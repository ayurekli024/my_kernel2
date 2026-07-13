#include "rtl8139.h"
#include "pci.h"
#include "io.h"

// Dışarıdan Alınan Kernel Fonksiyonları
extern void* dma_alloc(unsigned int size);
extern void terminal_print(const char* text);
extern void api_print(const char* text);
extern void itoa(int n, char s[]);
extern void strcpy(char* dest, const char* src);
extern void strcat(char* dest, const char* src);

unsigned int rtl_io_base = 0;
unsigned char mac_address[6];
unsigned char* rx_buffer;

void init_rtl8139() {
    unsigned char bus, slot;
    
    // Realtek (Vendor: 0x10EC) RTL8139 (Device: 0x8139) kartını PCI üzerinde ara
    if (pci_get_device(0x10EC, 0x8139, &bus, &slot)) {
        terminal_print("[ AĞ KARTI ] Realtek RTL8139 Ethernet Karti Bulundu!");
        
        // 1. Port Adresini Al (BAR0)
        unsigned int bar0 = pci_read_config_dword(bus, slot, 0, 0x10);
        rtl_io_base = bar0 & ~3; // Alt 2 biti temizle (I/O Port adresi)
        
        // 2. PCI Bus Mastering'i Aç (Kartın doğrudan DMA yapabilmesi için)
        unsigned int command_reg = pci_read_config_dword(bus, slot, 0, 0x04);
        pci_write_config_dword(bus, slot, 0, 0x04, command_reg | 0x04); // Bus Master bitini (Bit 2) 1 yap
        
        // 3. Ağ kartını uyandır (0x52 portuna 0 yazarak)
        outb(rtl_io_base + 0x52, 0x0);
        
        // 4. Yazılımsal Reset At (0x37 portuna 0x10 yazarak)
        outb(rtl_io_base + 0x37, 0x10);
        while((inb(rtl_io_base + 0x37) & 0x10) != 0) {
            // Reset bitinin sıfırlanmasını (kartın hazır olmasını) bekle
        }
        
        // 5. Gelen paketler için DMA (Doğrudan Bellek Erişimi) tamponu oluştur
        rx_buffer = (unsigned char*)dma_alloc(8192 + 16 + 1500); // RTL8139 Standardı 
        outl(rtl_io_base + 0x30, (unsigned int)rx_buffer); // RBSTART Register'ına adresi ver
        
        // 6. MAC Adresini Oku ve Terminale Bas
        for (int i = 0; i < 6; i++) {
            mac_address[i] = inb(rtl_io_base + i);
        }
        terminal_print("[ AĞ KARTI ] Mac Adresi Okundu.");
        
        // 7. Kesmeleri (Interrupts) Aç - Sadece Gelen ve Giden paketleri (ROK ve TOK) dinle
        outw(rtl_io_base + 0x3C, 0x0005);
        
        // 8. Hangi Paketleri Kabul Edeceğini Seç (Broadcast ve Match paketleri)
        outl(rtl_io_base + 0x44, 0xF | (1 << 7));
        
        // 9. Son olarak kartı dinlemeye ve göndermeye (RX ve TX) aç!
        outb(rtl_io_base + 0x37, 0x0C);
        terminal_print("[ AĞ KARTI ] RTL8139 Ag Dinlemeye Basladi!");
    } else {
        terminal_print("[ AĞ KARTI HATA ] RTL8139 PCI uzerinde bulunamadi.");
    }
}
// O anki müsait iletim (Transmit) kanalını takip etmek için (RTL8139'da 4 kanal vardır: 0-3)
static int tx_descriptor = 0;
static unsigned char* tx_buffer = 0;
// --- YENİ EKLENECEKLER ---
// Yönlendiricinin MAC adresini tutacağımız değişken ve ARP'nin çözülüp çözülmediği bilgisi
unsigned char router_mac[6] = {0};
int arp_resolved = 0;

// İnternet paketlerinin bozuk olup olmadığını anlayan meşhur Checksum algoritması
unsigned short net_checksum(unsigned char* data, int len) {
    unsigned int sum = 0;
    for (int i = 0; i < len; i += 2) {
        sum += (data[i] << 8) | data[i+1];
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return ~sum;
}
void rtl8139_send_arp() {
    // Sadece ilk seferde TX tamponu için DMA belleği ayır (Sürekli RAM harcamamak için)
    if (tx_buffer == 0) {
        tx_buffer = (unsigned char*)dma_alloc(64);
    }
    
    // QEMU'nun sanal yönlendiricisine (10.0.2.2) kimliğini soran bir ARP paketi
    unsigned char arp_packet[64] = {
        // --- 1. ETHERNET BAŞLIĞI (14 Bayt) ---
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Hedef MAC (Broadcast - Herkese)
        mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5], // Kaynak MAC (Biz)
        0x08, 0x06, // EtherType (0x0806 = ARP Paketi)
        
        // --- 2. ARP GÖVDESİ (28 Bayt) ---
        0x00, 0x01, // Donanım Tipi (Ethernet)
        0x08, 0x00, // Protokol Tipi (IPv4)
        0x06,       // Donanım Adresi Boyutu (MAC = 6 Bayt)
        0x04,       // Protokol Adresi Boyutu (IP = 4 Bayt)
        0x00, 0x01, // İşlem Kodu (1 = ARP İsteği / Request)
        
        // Gönderen Bilgileri (Bizim MAC ve varsayılan QEMU IP'miz 10.0.2.15)
        mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5],
        10, 0, 2, 15,
        
        // Hedef Bilgileri (Hedef MAC'i bilmediğimiz için 00, Hedef IP: QEMU Yönlendiricisi 10.0.2.2)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        10, 0, 2, 2
    };
    
    // Kalan kısmı sıfırla doldur (Minimum Ethernet çerçevesi 60-64 bayt olmalıdır)
    for(int i = 42; i < 64; i++) arp_packet[i] = 0;
    
    // Paketi Ağ Kartının doğrudan okuyabileceği DMA belleğine kopyala
    for(int i = 0; i < 64; i++) tx_buffer[i] = arp_packet[i];

    // Kartın ateşleme mekanizmasına paketin hafızadaki adresini (TSAD) ve boyutunu (TSD) veriyoruz
    outl(rtl_io_base + 0x20 + (tx_descriptor * 4), (unsigned int)tx_buffer);
    outl(rtl_io_base + 0x10 + (tx_descriptor * 4), 64);

    // Bir sonraki paket için sıradaki iletim kanalına geç (0, 1, 2, 3, 0...)
    tx_descriptor = (tx_descriptor + 1) % 4;
}
// DMA Belleğinde nerede kaldığımızı takip eden kalıcı (static) imleç
static unsigned int rx_offset = 0;

void rtl8139_handler_main() {
    unsigned short status = inw(rtl_io_base + 0x3E); 
    
    if (status & 0x04) {
        // terminal_print("[ INTERNET ] Paket Gonderildi (TX OK)"); // Ekranda kalabalık yapmasın diye gizleyebiliriz
    }
    
    if (status & 0x01) {
        unsigned short rx_length = *(unsigned short*)(rx_buffer + rx_offset + 2);
        unsigned char* packet = rx_buffer + rx_offset + 4;
        unsigned short ether_type = (packet[12] << 8) | packet[13]; 
        
        char msg[128] = "[ INTERNET ] Paket: ";
        char len_str[10]; itoa(rx_length, len_str);
        strcat(msg, len_str);
        
        // ==========================================
        // YENİ: PAKET AYRIŞTIRICI (PACKET PARSER)
        // ==========================================
        if (ether_type == 0x0806) { // Eğer ARP ise
            unsigned short arp_opcode = (packet[20] << 8) | packet[21];
            if (arp_opcode == 2) { // 2 = ARP Reply (Cevap)
                // QEMU Yönlendiricisinin MAC Adresini (22-27 baytları arası) Hafızaya Kaydet!
                for(int i = 0; i < 6; i++) router_mac[i] = packet[22+i];
                arp_resolved = 1; 
                strcat(msg, " Bayt (Tur: ARP YANITI - MAC Kaydedildi!)");
            } else {
                strcat(msg, " Bayt (Tur: ARP ISTEGI)");
            }
        } 
        else if (ether_type == 0x0800) { // Eğer IPv4 ise
            if (packet[23] == 0x01) { // Eğer Protokol ICMP (Ping) ise
                if (packet[34] == 0x00) { // 0 = Echo Reply (PONG!)
                    strcat(msg, " Bayt - [ PING YANITI ALINDI (PONG)!!! ]");
                } else {
                    strcat(msg, " Bayt (Tur: ICMP İstegi)");
                }
            } else {
                strcat(msg, " Bayt (Tur: IPv4 Diger)");
            }
        }
        else strcat(msg, " Bayt (Tur: Diger)");
        
        terminal_print(msg);
        
        rx_offset = (rx_offset + rx_length + 4 + 3) & ~3;
        outw(rtl_io_base + 0x38, rx_offset - 16); 
    }
    
    outw(rtl_io_base + 0x3E, 0x05); 
    outb(0x20, 0x20); outb(0xA0, 0x20); 
}

// İŞTE EKSİK PARÇA: Linker'ın aradığı Kesme Sıçrama Tahtası (Naked Interrupt Wrapper)
// boot.asm ile uğraşmamak için Assembly wrapper'ını doğrudan C içinde GCC ile yazıyoruz!
__attribute__((naked)) void rtl8139_handler(void) {
    __asm__ __volatile__ (
        "pusha\n"                    // Tüm CPU yazmaçlarını yedekle
        "call rtl8139_handler_main\n" // Asıl C kodumuzu çağır
        "popa\n"                     // Yazmaçları geri yükle
        "iret\n"                    // Kesmeden (Interrupt) donanımsal olarak çık
    );
}
void rtl8139_send_ping() {
    if (tx_buffer == 0) tx_buffer = (unsigned char*)dma_alloc(128);

    unsigned char ping_packet[74] = {0}; // 14 (Eth) + 20 (IP) + 40 (ICMP+Data)
    
    // 1. ETHERNET KATMANI (Layer 2)
    for(int i=0; i<6; i++) ping_packet[i] = router_mac[i]; // Hedef (Router MAC)
    for(int i=0; i<6; i++) ping_packet[6+i] = mac_address[i]; // Kaynak (Biz)
    ping_packet[12] = 0x08; ping_packet[13] = 0x00; // Type: IPv4
    
    // 2. IPv4 KATMANI (Layer 3)
    ping_packet[14] = 0x45; // Version 4, Header Length 5
    ping_packet[15] = 0x00; // DSCP
    ping_packet[16] = 0x00; ping_packet[17] = 0x3C; // Toplam Boyut: 60 bayt (20 IP + 40 ICMP)
    ping_packet[18] = 0xAB; ping_packet[19] = 0xCD; // ID
    ping_packet[20] = 0x00; ping_packet[21] = 0x00; // Flags/Frag
    ping_packet[22] = 0x40; // TTL (Time to Live) = 64
    ping_packet[23] = 0x01; // Protokol = ICMP
    // Kaynak IP (Biz: 10.0.2.15)
    ping_packet[26] = 10; ping_packet[27] = 0; ping_packet[28] = 2; ping_packet[29] = 15; 
    // Hedef IP (QEMU Router: 10.0.2.2)
    ping_packet[30] = 10; ping_packet[31] = 0; ping_packet[32] = 2; ping_packet[33] = 2;  
    
    // IP Checksum Hesapla ve Yaz (Hayati önem taşır, bozuksa router paketi çöpe atar)
    unsigned short ip_csum = net_checksum(&ping_packet[14], 20);
    ping_packet[24] = (ip_csum >> 8) & 0xFF;
    ping_packet[25] = ip_csum & 0xFF;
    
    // 3. ICMP KATMANI (Ping)
    ping_packet[34] = 0x08; // Type = 8 (Echo Request)
    ping_packet[35] = 0x00; // Code = 0
    ping_packet[38] = 0x00; ping_packet[39] = 0x01; // ID
    ping_packet[40] = 0x00; ping_packet[41] = 0x01; // Sequence
    
    // Paketin Yükü (Data) - İnternette yankılanacak o meşhur metin!
    const char* payload = "ArdaOS ICMP Ping Test Paketi!!!"; // 31 Karakter
    for(int i=0; i<32; i++) ping_packet[42+i] = payload[i];
    
    // ICMP Checksum Hesapla ve Yaz
    unsigned short icmp_csum = net_checksum(&ping_packet[34], 40);
    ping_packet[36] = (icmp_csum >> 8) & 0xFF;
    ping_packet[37] = icmp_csum & 0xFF;
    
    // DMA'ya kopyala ve Ağ Kartını Ateşle!
    for(int i = 0; i < 74; i++) tx_buffer[i] = ping_packet[i];
    outl(rtl_io_base + 0x20 + (tx_descriptor * 4), (unsigned int)tx_buffer);
    outl(rtl_io_base + 0x10 + (tx_descriptor * 4), 74);
    tx_descriptor = (tx_descriptor + 1) % 4;
}