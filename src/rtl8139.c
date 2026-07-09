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
    unsigned short status = inw(rtl_io_base + 0x3E); // ISR Register
    
    // YENİ: Eğer paket başarıyla GÖNDERİLDİYSE (Transmit OK)
    if (status & 0x04) {
        terminal_print("[ INTERNET ] ARP Paketi Basariyla Gonderildi (TX OK)!");
    }
    
    // Eğer paket başarıyla ALINDIysa (Receive OK)
    if (status & 0x01) {
        unsigned short rx_status = *(unsigned short*)(rx_buffer + rx_offset);
        unsigned short rx_length = *(unsigned short*)(rx_buffer + rx_offset + 2);
        unsigned char* packet = rx_buffer + rx_offset + 4;
        unsigned short ether_type = (packet[12] << 8) | packet[13]; 
        
        char msg[128] = "[ INTERNET ] Paket Yakalandi! Boyut: ";
        char len_str[10]; itoa(rx_length, len_str);
        strcat(msg, len_str);
        
        if (ether_type == 0x0800) strcat(msg, " Bayt (Tur: IPv4)");
        else if (ether_type == 0x0806) strcat(msg, " Bayt (Tur: ARP Yaniti!)");
        else if (ether_type == 0x86DD) strcat(msg, " Bayt (Tur: IPv6)");
        else strcat(msg, " Bayt (Tur: Diger)");
        
        terminal_print(msg);
        
        rx_offset = (rx_offset + rx_length + 4 + 3) & ~3;
        outw(rtl_io_base + 0x38, rx_offset - 16); 
    }
    
    outw(rtl_io_base + 0x3E, 0x05); // ISR'yi temizle
    outb(0x20, 0x20); outb(0xA0, 0x20); // EOI (Interrupt Bitti)
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