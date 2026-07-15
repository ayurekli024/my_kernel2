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
// YENİ: UDP Gelen Kutusu
unsigned char udp_inbox[2048];
int udp_inbox_size = 0;
volatile int udp_inbox_ready = 0;
// ==========================================
// YENİ: TCP MOTORU DURUM DEĞİŞKENLERİ
// ==========================================
unsigned char tcp_dest_ip[4] = {0};
unsigned short tcp_dest_port = 80; // HTTP Portu
unsigned short tcp_local_port = 55556;
unsigned int tcp_seq = 0x11223344; // Bizim Sıra Numaramız (Rastgele başlar)
unsigned int tcp_ack = 0;          // Karşı Tarafın Bize Yolladığı Sıra
int tcp_state = 0; // 0: KAPALI, 1: SYN_SENT, 2: ESTABLISHED
unsigned char tcp_inbox[8192]; // Gelen HTML kodlarını tutacağımız dev depo
int tcp_inbox_size = 0;
volatile int tcp_inbox_ready = 0;

void rtl8139_send_tcp(unsigned char flags, unsigned char* data, int data_len);

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
        terminal_print("[ AG KARTI ] RTL8139 Ag Dinlemeye Basladi!");
    } else {
        terminal_print("[ AG KARTI HATA ] RTL8139 PCI uzerinde bulunamadi.");
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
    // DÜZELTME 1: Çakışmaları önlemek için TX tamponunu hep 2048 bayt ayırıyoruz
    if (tx_buffer == 0) {
        tx_buffer = (unsigned char*)dma_alloc(2048);
    }
    
    unsigned char arp_packet[64] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
        mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5], 
        0x08, 0x06, 
        0x00, 0x01, 
        0x08, 0x00, 
        0x06,       
        0x04,       
        0x00, 0x01, 
        mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5],
        10, 0, 2, 15,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        10, 0, 2, 2
    };
    
    for(int i = 42; i < 64; i++) arp_packet[i] = 0;
    for(int i = 0; i < 64; i++) tx_buffer[i] = arp_packet[i];

    outl(rtl_io_base + 0x20 + (tx_descriptor * 4), (unsigned int)tx_buffer);
    outl(rtl_io_base + 0x10 + (tx_descriptor * 4), 64);
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
            if (packet[23] == 0x01) { // Protokol ICMP ise
                if (packet[34] == 0x00) strcat(msg, " Bayt - [ PING YANITI (PONG) ]");
                else strcat(msg, " Bayt (Tur: ICMP İstegi)");
            } 
            else if (packet[23] == 0x11) { // YENİ: Protokol UDP ise!
                unsigned short udp_len = (packet[38] << 8) | packet[39];
                udp_inbox_size = udp_len - 8; // 8 bayt UDP başlığını çıkar
                
                // Paketi uygulamanın okuyabilmesi için Gelen Kutusuna kopyala
                for(int i=0; i<udp_inbox_size; i++) {
                    udp_inbox[i] = packet[42+i];
                }
                udp_inbox[udp_inbox_size] = '\0'; // Metin gibi okunabilsin diye
                udp_inbox_ready = 1; // VFS'ye "Veri Hazır" bayrağını çek

                strcat(msg, " Bayt - [ UDP SOKET VERISI ALINDI! ]");
            }
            else if (packet[23] == 0x06) { // Protokol TCP (6) ise!
                int ip_hdr_len = (packet[14] & 0x0F) * 4;
                int tcp_hdr_start = 14 + ip_hdr_len;
                unsigned char flags = packet[tcp_hdr_start + 13];
                
                unsigned int incoming_seq = (packet[tcp_hdr_start+4]<<24) | (packet[tcp_hdr_start+5]<<16) | (packet[tcp_hdr_start+6]<<8) | packet[tcp_hdr_start+7];
                
                if ((flags & 0x02) && (flags & 0x10)) { // SYN-ACK (Bağlantı Kabul Edildi!)
                    tcp_ack = incoming_seq + 1; 
                    tcp_state = 2; // DURUM: ESTABLISHED (Bağlandı!)
                    rtl8139_send_tcp(0x10, 0, 0); // Karşıya "Aldım" (ACK) paketi fırlat
                    strcat(msg, " Bayt - [ TCP SYN-ACK ALINDI, EL SIKISILDI! ]");
                } 
                else if (flags & 0x08) { // PSH (İçinde HTTP Verisi Var!)
                    int tcp_hdr_len = (packet[tcp_hdr_start + 12] >> 4) * 4;
                    int data_start = tcp_hdr_start + tcp_hdr_len;
                    int total_len = (packet[16]<<8) | packet[17];
                    int data_len = total_len - ip_hdr_len - tcp_hdr_len;
                    
                    if (data_len > 0) {
                        tcp_ack = incoming_seq + data_len;
                        for(int i=0; i<data_len; i++) tcp_inbox[tcp_inbox_size++] = packet[data_start+i];
                        tcp_inbox[tcp_inbox_size] = '\0';
                        tcp_inbox_ready = 1; // Uygulamayı uyar!
                        
                        rtl8139_send_tcp(0x10, 0, 0); // Veriyi aldığımıza dair ACK fırlat
                    }
                    strcat(msg, " Bayt - [ TCP VERISI (HTTP) ALINDI! ]");
                }
                else {
                    // YENİ: Anlaşılmayan tüm bayrakları (Örn: RST-ACK olan 20'yi) ekrana bas!
                    strcat(msg, " Bayt - [ TCP BAYRAK: ");
                    char f_str[10]; itoa(flags, f_str);
                    strcat(msg, f_str);
                    strcat(msg, " ]");
                }
            }
            else {
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
    // DÜZELTME 2: Tampon boyutu 2048
    if (tx_buffer == 0) tx_buffer = (unsigned char*)dma_alloc(2048);

    unsigned char ping_packet[74] = {0}; 
    
    for(int i=0; i<6; i++) ping_packet[i] = router_mac[i]; 
    for(int i=0; i<6; i++) ping_packet[6+i] = mac_address[i]; 
    ping_packet[12] = 0x08; ping_packet[13] = 0x00; 
    
    ping_packet[14] = 0x45; 
    ping_packet[15] = 0x00; 
    ping_packet[16] = 0x00; ping_packet[17] = 0x3C; 
    ping_packet[18] = 0xAB; ping_packet[19] = 0xCD; 
    ping_packet[20] = 0x00; ping_packet[21] = 0x00; 
    ping_packet[22] = 0x40; 
    ping_packet[23] = 0x01; 
    ping_packet[26] = 10; ping_packet[27] = 0; ping_packet[28] = 2; ping_packet[29] = 15; 
    ping_packet[30] = 10; ping_packet[31] = 0; ping_packet[32] = 2; ping_packet[33] = 2;  
    
    unsigned short ip_csum = net_checksum(&ping_packet[14], 20);
    ping_packet[24] = (ip_csum >> 8) & 0xFF;
    ping_packet[25] = ip_csum & 0xFF;
    
    ping_packet[34] = 0x08; 
    ping_packet[35] = 0x00; 
    ping_packet[38] = 0x00; ping_packet[39] = 0x01; 
    ping_packet[40] = 0x00; ping_packet[41] = 0x01; 
    
    const char* payload = "ArdaOS ICMP Ping Test Paketi!!!"; 
    for(int i=0; i<32; i++) ping_packet[42+i] = payload[i];
    
    unsigned short icmp_csum = net_checksum(&ping_packet[34], 40);
    ping_packet[36] = (icmp_csum >> 8) & 0xFF;
    ping_packet[37] = icmp_csum & 0xFF;
    
    for(int i = 0; i < 74; i++) tx_buffer[i] = ping_packet[i];
    outl(rtl_io_base + 0x20 + (tx_descriptor * 4), (unsigned int)tx_buffer);
    outl(rtl_io_base + 0x10 + (tx_descriptor * 4), 74);
    tx_descriptor = (tx_descriptor + 1) % 4;
}
void rtl8139_send_udp(unsigned char* dest_ip, unsigned short dest_port, unsigned short src_port, unsigned char* data, int data_len) {
    if (tx_buffer == 0) tx_buffer = (unsigned char*)dma_alloc(2048);
    if (arp_resolved == 0) return; 

    int total_len = 14 + 20 + 8 + data_len;
    if (total_len < 60) total_len = 60; 
    
    // KRİTİK DÜZELTME: "static" kelimesi bu devasa dizinin Kernel yığınını (Stack) patlatmasını engeller!
    static unsigned char packet[2048];
    for (int i = 0; i < 2048; i++) packet[i] = 0; // Her gönderimde içini temizle

    for(int i=0; i<6; i++) packet[i] = router_mac[i];
    for(int i=0; i<6; i++) packet[6+i] = mac_address[i];
    packet[12] = 0x08; packet[13] = 0x00; 

    packet[14] = 0x45; packet[15] = 0x00;
    packet[16] = ((20 + 8 + data_len) >> 8) & 0xFF;
    packet[17] = (20 + 8 + data_len) & 0xFF;
    packet[18] = 0x00; packet[19] = 0x00;
    packet[20] = 0x00; packet[21] = 0x00;
    packet[22] = 0x40; 
    packet[23] = 0x11; 
    
    packet[26] = 10; packet[27] = 0; packet[28] = 2; packet[29] = 15; 
    for(int i=0; i<4; i++) packet[30+i] = dest_ip[i]; 

    unsigned short ip_csum = net_checksum(&packet[14], 20);
    packet[24] = (ip_csum >> 8) & 0xFF; packet[25] = ip_csum & 0xFF;

    packet[34] = (src_port >> 8) & 0xFF; packet[35] = src_port & 0xFF;
    packet[36] = (dest_port >> 8) & 0xFF; packet[37] = dest_port & 0xFF;
    packet[38] = ((8 + data_len) >> 8) & 0xFF; packet[39] = (8 + data_len) & 0xFF;
    packet[40] = 0x00; packet[41] = 0x00; 

    for(int i=0; i<data_len; i++) packet[42+i] = data[i];

    for(int i=0; i<total_len; i++) tx_buffer[i] = packet[i];
    outl(rtl_io_base + 0x20 + (tx_descriptor * 4), (unsigned int)tx_buffer);
    outl(rtl_io_base + 0x10 + (tx_descriptor * 4), total_len);
    tx_descriptor = (tx_descriptor + 1) % 4;
}
// ==========================================
// YENİ: TCP PAKET ÜRETİCİSİ VE GÖNDERİCİSİ
// ==========================================
void rtl8139_send_tcp(unsigned char flags, unsigned char* data, int data_len) {
    if (tx_buffer == 0) tx_buffer = (unsigned char*)dma_alloc(2048);
    
    // GÜVENLİK AĞI: Eğer ARP çözülmediyse (ping atılmadıysa) iptal et
    if (arp_resolved == 0) return; 

    static unsigned char packet[2048];
    for (int i = 0; i < 2048; i++) packet[i] = 0;

    int tcp_len = 20 + data_len;
    int ip_len = 20 + tcp_len;     // KRİTİK: Gerçek IP Boyutu (Padding HARİÇ!)
    int frame_len = 14 + ip_len;   // Donanım için toplam çerçeve boyutu
    
    // Donanım (Ethernet) minimum 60 bayt ister, fazlasını sıfırlarla (padding) doldururuz
    if (frame_len < 60) frame_len = 60; 

    // 1. ETHERNET BAŞLIĞI
    for(int i=0; i<6; i++) packet[i] = router_mac[i];
    for(int i=0; i<6; i++) packet[6+i] = mac_address[i];
    packet[12] = 0x08; packet[13] = 0x00;

    // 2. IPv4 BAŞLIĞI (Tuzak Buradaydı! Artık saf ip_len kullanıyoruz)
    // 2. IPv4 BAŞLIĞI
    packet[14] = 0x45; packet[15] = 0x00;
    packet[16] = ip_len >> 8; 
    packet[17] = ip_len & 0xFF;
    packet[18] = 0x12; packet[19] = 0x34; // YENİ: ID 0 Olmasın! Rastgele bir kimlik atadık
    packet[20] = 0x40; packet[21] = 0x00; // YENİ: DF (Don't Fragment) Bayrağı
    packet[22] = 0x40; packet[23] = 0x06;
    
    unsigned char src_ip[4] = {10, 0, 2, 15};
    for(int i=0; i<4; i++) { packet[26+i] = src_ip[i]; packet[30+i] = tcp_dest_ip[i]; }
    
    unsigned short ip_csum = net_checksum(&packet[14], 20);
    packet[24] = ip_csum >> 8; packet[25] = ip_csum & 0xFF;

    // 3. TCP BAŞLIĞI
    packet[34] = tcp_local_port >> 8; packet[35] = tcp_local_port & 0xFF;
    packet[36] = tcp_dest_port >> 8;  packet[37] = tcp_dest_port & 0xFF;
    
    packet[38] = (tcp_seq >> 24) & 0xFF; packet[39] = (tcp_seq >> 16) & 0xFF;
    packet[40] = (tcp_seq >> 8) & 0xFF;  packet[41] = tcp_seq & 0xFF;
    
    packet[42] = (tcp_ack >> 24) & 0xFF; packet[43] = (tcp_ack >> 16) & 0xFF;
    packet[44] = (tcp_ack >> 8) & 0xFF;  packet[45] = tcp_ack & 0xFF;
    
    packet[46] = 0x50; 
    packet[47] = flags; 
    packet[48] = 0xFA; packet[49] = 0xF0; 
    
    for(int i=0; i<data_len; i++) packet[54+i] = data[i];

    // TCP CHECKSUM (Bu hesaplama ip_len kullanılarak yapıldığı için artık kusursuz!)
    unsigned int csum = 0;
    csum += (src_ip[0]<<8)|src_ip[1]; csum += (src_ip[2]<<8)|src_ip[3];
    csum += (tcp_dest_ip[0]<<8)|tcp_dest_ip[1]; csum += (tcp_dest_ip[2]<<8)|tcp_dest_ip[3];
    csum += 0x0006; csum += tcp_len;
    for(int i=0; i<tcp_len; i+=2) {
        unsigned short word = (packet[34+i]<<8) | (i+1 < tcp_len ? packet[34+i+1] : 0);
        csum += word;
    }
    while(csum >> 16) csum = (csum & 0xFFFF) + (csum >> 16);
    csum = ~csum;
    packet[50] = csum >> 8; packet[51] = csum & 0xFF;

    // Donanıma Gönder
    for(int i=0; i<frame_len; i++) tx_buffer[i] = packet[i];
    outl(rtl_io_base + 0x20 + (tx_descriptor * 4), (unsigned int)tx_buffer);
    outl(rtl_io_base + 0x10 + (tx_descriptor * 4), frame_len);
    tx_descriptor = (tx_descriptor + 1) % 4;
    
    if (data_len > 0) tcp_seq += data_len;
    else if (flags & 0x02 || flags & 0x01) tcp_seq += 1;
}