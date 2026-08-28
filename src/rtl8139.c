#include "rtl8139.h"
#include "pci.h"
#include "io.h"
#include "../sdk/ardaos.h"
// Dışarıdan Alınan Kernel Fonksiyonları
extern void* dma_alloc(unsigned int size);
extern void terminal_print(const char* text);
extern void api_print(const char* text);
extern void itoa(int n, char s[]);
extern void strcpy(char* dest, const char* src);
extern void strcat(char* dest, const char* src);
extern gui_state_t* gui;

unsigned int rtl_io_base = 0;
unsigned char mac_address[6];
unsigned char* rx_buffer;
// --- GERİ GELEN UDP DEĞİŞKENLERİ ---
unsigned char udp_inbox[2048];
int udp_inbox_size = 0;
volatile int udp_inbox_ready = 0;

// --- YUKARI TAŞINAN ARP DEĞİŞKENLERİ VE İMZASI ---
extern unsigned char router_mac[6];
extern int arp_resolved;
void rtl8139_send_arp(void);

// ... (udp_inbox kısmı ve mac_address tanımları üstte kalacak) ...

// =========================================================
// YENİ: TCP SOKET YÖNETİCİSİ (MAX 16 EŞZAMANLI BAĞLANTI)
// =========================================================

tcp_socket_t tcp_sockets[16] = {0}; // = {0} ekleyerek tüm soketleri kesin olarak sıfırla!
unsigned short next_local_port = 55556;

int net_socket_create() {
    for (int i = 0; i < 16; i++) {
        if (tcp_sockets[i].active == 0) {
            tcp_sockets[i].active = 1;
            tcp_sockets[i].state = TCP_CLOSED;
            tcp_sockets[i].local_port = next_local_port++;
            tcp_sockets[i].seq = 0x11223344 + i;
            tcp_sockets[i].ack = 0;
            tcp_sockets[i].rx_size = 0;
            tcp_sockets[i].rx_ready = 0;
            return i; // Soket Numarasını (Bilet) dön
        }
    }
    return -1;
}

int net_tcp_connect(int sock_id, unsigned char* ip, unsigned short port) {
    if (sock_id < 0 || sock_id >= 16 || !tcp_sockets[sock_id].active) return -1;
    if (arp_resolved == 0) rtl8139_send_arp(); // Önce ARP çöz!
    
    for (int i = 0; i < 4; i++) tcp_sockets[sock_id].remote_ip[i] = ip[i];
    tcp_sockets[sock_id].remote_port = port;
    tcp_sockets[sock_id].state = TCP_SYN_SENT;
    
    // 0x02 = SYN Bayrağı
    net_tcp_send(sock_id, 0x02, 0, 0); 
    return 0;
}
void rtl8139_send_tcp(unsigned char flags, unsigned char* data, int data_len);

void init_rtl8139() {
    unsigned char bus, slot;
    if (pci_get_device(0x10EC, 0x8139, &bus, &slot)) {
        terminal_print("[ AĞ KARTI ] Realtek RTL8139 Ethernet Karti Bulundu!");
        unsigned int bar0 = pci_read_config_dword(bus, slot, 0, 0x10);
        rtl_io_base = bar0 & ~3; 
        unsigned int command_reg = pci_read_config_dword(bus, slot, 0, 0x04);
        pci_write_config_dword(bus, slot, 0, 0x04, command_reg | 0x04); 
        outb(rtl_io_base + 0x52, 0x0);
        outb(rtl_io_base + 0x37, 0x10);
        while((inb(rtl_io_base + 0x37) & 0x10) != 0) {}
        rx_buffer = (unsigned char*)dma_alloc(8192 + 16 + 1500); 
        outl(rtl_io_base + 0x30, (unsigned int)rx_buffer); 
        for (int i = 0; i < 6; i++) mac_address[i] = inb(rtl_io_base + i);
        terminal_print("[ AĞ KARTI ] Mac Adresi Okundu.");
        outw(rtl_io_base + 0x3C, 0x0005);
        outl(rtl_io_base + 0x44, 0xF | (1 << 7));
        outb(rtl_io_base + 0x37, 0x0C);
        terminal_print("[ AG KARTI ] RTL8139 Ag Dinlemeye Basladi!");
    } else {
        terminal_print("[ AG KARTI HATA ] RTL8139 PCI uzerinde bulunamadi.");
    }
}

// =========================================================
// İŞTE BÜYÜK ZIRH: 4 FARKLI İLETİM TAMPONU! (Çarpışma Bitti)
// =========================================================
static int tx_descriptor = 0;
static unsigned char* tx_buffers[4] = {0, 0, 0, 0};

unsigned char router_mac[6] = {0};
int arp_resolved = 0;

unsigned short net_checksum(unsigned char* data, int len) {
    unsigned int sum = 0;
    for (int i = 0; i < len; i += 2) sum += (data[i] << 8) | data[i+1];
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return ~sum;
}

void rtl8139_send_arp() {
    if (tx_buffers[0] == 0) for(int i=0; i<4; i++) tx_buffers[i] = (unsigned char*)dma_alloc(2048);
    unsigned char* current_tx = tx_buffers[tx_descriptor]; // Kanalına özel hafıza!
    
    unsigned char arp_packet[64] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
        mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5], 
        0x08, 0x06, 0x00, 0x01, 0x08, 0x00, 0x06, 0x04, 0x00, 0x01, 
        mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5],
        10, 0, 2, 15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 10, 0, 2, 2
    };
    for(int i = 42; i < 64; i++) arp_packet[i] = 0;
    for(int i = 0; i < 64; i++) current_tx[i] = arp_packet[i];

    outl(rtl_io_base + 0x20 + (tx_descriptor * 4), (unsigned int)current_tx);
    outl(rtl_io_base + 0x10 + (tx_descriptor * 4), 64);
    tx_descriptor = (tx_descriptor + 1) % 4;
}

static unsigned int rx_offset = 0;

void rtl8139_handler_main() {
    unsigned short status = inw(rtl_io_base + 0x3E); 
    if (status & 0x01) {
        unsigned short rx_length = *(unsigned short*)(rx_buffer + rx_offset + 2);
        unsigned char* packet = rx_buffer + rx_offset + 4;
        unsigned short ether_type = (packet[12] << 8) | packet[13]; 
        
        char msg[128] = "[ INTERNET ] Paket: ";
        char len_str[10]; itoa(rx_length, len_str); strcat(msg, len_str);
        
        if (ether_type == 0x0806) { 
            unsigned short arp_opcode = (packet[20] << 8) | packet[21];
            if (arp_opcode == 2) { 
                for(int i = 0; i < 6; i++) router_mac[i] = packet[22+i];
                arp_resolved = 1; 
                strcat(msg, " Bayt (Tur: ARP YANITI - MAC Kaydedildi!)");
            } else strcat(msg, " Bayt (Tur: ARP ISTEGI)");
        } 
        else if (ether_type == 0x0800) { 
            if (packet[23] == 0x01) { 
                if (packet[34] == 0x00) strcat(msg, " Bayt - [ PING YANITI (PONG) ]");
                else strcat(msg, " Bayt (Tur: ICMP Istegi)");
            } 
            else if (packet[23] == 0x11) { 
                unsigned short udp_len = (packet[38] << 8) | packet[39];
                udp_inbox_size = udp_len - 8; 
                for(int i=0; i<udp_inbox_size; i++) udp_inbox[i] = packet[42+i];
                udp_inbox[udp_inbox_size] = '\0'; 
                udp_inbox_ready = 1; 
                strcat(msg, " Bayt - [ UDP SOKET VERISI ALINDI! ]");
            }
            else if (packet[23] == 0x06) { 
                int ip_hdr_len = (packet[14] & 0x0F) * 4;
                int tcp_hdr_start = 14 + ip_hdr_len;
                unsigned char flags = packet[tcp_hdr_start + 13];
                
                unsigned short dest_port = (packet[tcp_hdr_start+2] << 8) | packet[tcp_hdr_start+3];
                unsigned int incoming_seq = (packet[tcp_hdr_start+4]<<24) | (packet[tcp_hdr_start+5]<<16) | (packet[tcp_hdr_start+6]<<8) | packet[tcp_hdr_start+7];
                
                int tcp_hdr_len = (packet[tcp_hdr_start + 12] >> 4) * 4;
                int data_start = tcp_hdr_start + tcp_hdr_len;
                int total_len = (packet[16]<<8) | packet[17];
                int data_len = total_len - ip_hdr_len - tcp_hdr_len;

                // Hedef Porta Göre Doğru Soketi Bul (Demultiplexing)
                int s_id = -1;
                for (int s = 0; s < 16; s++) {
                    if (tcp_sockets[s].active && tcp_sockets[s].local_port == dest_port) { s_id = s; break; }
                }

                if (s_id != -1) {
                    tcp_socket_t* sock = &tcp_sockets[s_id];
                    
                    if ((flags & 0x02) && (flags & 0x10)) { // SYN-ACK
                        sock->ack = incoming_seq + 1; 
                        sock->state = TCP_ESTABLISHED; 
                        net_tcp_send(s_id, 0x10, 0, 0); // ACK yolla
                        strcat(msg, " Bayt - [ TCP SYN-ACK ]");
                    } 
                    else if (data_len > 0) { // HTTP veya Veri
                        sock->ack = incoming_seq + data_len;
                        for(int i=0; i<data_len; i++) {
                            if (sock->rx_size < 8190) sock->rx_buf[sock->rx_size++] = packet[data_start+i];
                        }
                        sock->rx_buf[sock->rx_size] = '\0';
                        sock->rx_ready = 1; 
                        net_tcp_send(s_id, 0x10, 0, 0); // ACK yolla
                        strcat(msg, " Bayt - [ TCP DATA ]");
                    }
                    else if (flags & 0x01) { // FIN
                        sock->ack = incoming_seq + 1;
                        net_tcp_send(s_id, 0x11, 0, 0); // FIN-ACK
                        sock->state = TCP_CLOSED;
                        strcat(msg, " Bayt - [ TCP FIN ]");
                    }
                }
            }
            else strcat(msg, " Bayt (Tur: IPv4 Diger)");
        }
        else strcat(msg, " Bayt (Tur: Diger)");
        
        terminal_print(msg);
        rx_offset = (rx_offset + rx_length + 4 + 3) & ~3;
        outw(rtl_io_base + 0x38, rx_offset - 16); 
    }
    outw(rtl_io_base + 0x3E, 0x05); 
    outb(0x20, 0x20); outb(0xA0, 0x20); 
    if (gui) gui->net_activity_timer = 20;
}

__attribute__((naked)) void rtl8139_handler(void) {
    __asm__ __volatile__ ("pusha\n call rtl8139_handler_main\n popa\n iret\n");
}

void rtl8139_send_ping() {
    if (tx_buffers[0] == 0) for(int i=0; i<4; i++) tx_buffers[i] = (unsigned char*)dma_alloc(2048);
    unsigned char* current_tx = tx_buffers[tx_descriptor];

    unsigned char ping_packet[74] = {0}; 
    for(int i=0; i<6; i++) ping_packet[i] = router_mac[i]; 
    for(int i=0; i<6; i++) ping_packet[6+i] = mac_address[i]; 
    ping_packet[12] = 0x08; ping_packet[13] = 0x00; 
    ping_packet[14] = 0x45; ping_packet[15] = 0x00; 
    ping_packet[16] = 0x00; ping_packet[17] = 0x3C; 
    ping_packet[18] = 0xAB; ping_packet[19] = 0xCD; 
    ping_packet[20] = 0x00; ping_packet[21] = 0x00; 
    ping_packet[22] = 0x40; ping_packet[23] = 0x01; 
    ping_packet[26] = 10; ping_packet[27] = 0; ping_packet[28] = 2; ping_packet[29] = 15; 
    ping_packet[30] = 10; ping_packet[31] = 0; ping_packet[32] = 2; ping_packet[33] = 2;  
    
    unsigned short ip_csum = net_checksum(&ping_packet[14], 20);
    ping_packet[24] = (ip_csum >> 8) & 0xFF; ping_packet[25] = ip_csum & 0xFF;
    
    ping_packet[34] = 0x08; ping_packet[35] = 0x00; 
    ping_packet[38] = 0x00; ping_packet[39] = 0x01; 
    ping_packet[40] = 0x00; ping_packet[41] = 0x01; 
    
    const char* payload = "ArdaOS ICMP Ping Test Paketi!!!"; 
    for(int i=0; i<32; i++) ping_packet[42+i] = payload[i];
    
    unsigned short icmp_csum = net_checksum(&ping_packet[34], 40);
    ping_packet[36] = (icmp_csum >> 8) & 0xFF; ping_packet[37] = icmp_csum & 0xFF;
    
    for(int i = 0; i < 74; i++) current_tx[i] = ping_packet[i];
    outl(rtl_io_base + 0x20 + (tx_descriptor * 4), (unsigned int)current_tx);
    outl(rtl_io_base + 0x10 + (tx_descriptor * 4), 74);
    tx_descriptor = (tx_descriptor + 1) % 4;
}

void rtl8139_send_udp(unsigned char* dest_ip, unsigned short dest_port, unsigned short src_port, unsigned char* data, int data_len) {
    if (tx_buffers[0] == 0) for(int i=0; i<4; i++) tx_buffers[i] = (unsigned char*)dma_alloc(2048);
    if (arp_resolved == 0) return; 
    unsigned char* current_tx = tx_buffers[tx_descriptor];

    int total_len = 14 + 20 + 8 + data_len;
    if (total_len < 60) total_len = 60; 
    
    static unsigned char packet[2048];
    for (int i = 0; i < 2048; i++) packet[i] = 0; 

    for(int i=0; i<6; i++) packet[i] = router_mac[i];
    for(int i=0; i<6; i++) packet[6+i] = mac_address[i];
    packet[12] = 0x08; packet[13] = 0x00; 

    packet[14] = 0x45; packet[15] = 0x00;
    packet[16] = ((20 + 8 + data_len) >> 8) & 0xFF; packet[17] = (20 + 8 + data_len) & 0xFF;
    packet[18] = 0x00; packet[19] = 0x00;
    packet[20] = 0x00; packet[21] = 0x00;
    packet[22] = 0x40; packet[23] = 0x11; 
    
    packet[26] = 10; packet[27] = 0; packet[28] = 2; packet[29] = 15; 
    for(int i=0; i<4; i++) packet[30+i] = dest_ip[i]; 

    unsigned short ip_csum = net_checksum(&packet[14], 20);
    packet[24] = (ip_csum >> 8) & 0xFF; packet[25] = ip_csum & 0xFF;

    packet[34] = (src_port >> 8) & 0xFF; packet[35] = src_port & 0xFF;
    packet[36] = (dest_port >> 8) & 0xFF; packet[37] = dest_port & 0xFF;
    packet[38] = ((8 + data_len) >> 8) & 0xFF; packet[39] = (8 + data_len) & 0xFF;
    packet[40] = 0x00; packet[41] = 0x00; 

    for(int i=0; i<data_len; i++) packet[42+i] = data[i];

    for(int i=0; i<total_len; i++) current_tx[i] = packet[i];
    outl(rtl_io_base + 0x20 + (tx_descriptor * 4), (unsigned int)current_tx);
    outl(rtl_io_base + 0x10 + (tx_descriptor * 4), total_len);
    tx_descriptor = (tx_descriptor + 1) % 4;
    if (gui) gui->net_activity_timer = 20;
}

// YENİ: İsmi güncellendi ve sock_id parametresi eklendi
int net_tcp_send(int sock_id, unsigned char flags, unsigned char* data, int data_len) {
    // Güvenlik: Geçersiz veya kapalı bir sokete yazmayı engelle
    if (sock_id < 0 || sock_id >= 16 || !tcp_sockets[sock_id].active) return -1;
    if (tx_buffers[0] == 0) for(int i=0; i<4; i++) tx_buffers[i] = (unsigned char*)dma_alloc(2048);
    if (arp_resolved == 0) return -1; 
    
    // Hedef soket bilgilerini global değişkenler yerine struct üzerinden çekiyoruz
    tcp_socket_t* sock = &tcp_sockets[sock_id];
    unsigned char* current_tx = tx_buffers[tx_descriptor];

    static unsigned char packet[2048];
    for (int i = 0; i < 2048; i++) packet[i] = 0;

    int tcp_len = 20 + data_len;
    int ip_len = 20 + tcp_len;     
    int frame_len = 14 + ip_len;   
    
    if (frame_len < 60) frame_len = 60; 

    for(int i=0; i<6; i++) packet[i] = router_mac[i];
    for(int i=0; i<6; i++) packet[6+i] = mac_address[i];
    packet[12] = 0x08; packet[13] = 0x00;

    packet[14] = 0x45; packet[15] = 0x00;
    packet[16] = ip_len >> 8; packet[17] = ip_len & 0xFF;
    packet[18] = 0x12; packet[19] = 0x34; 
    packet[20] = 0x40; packet[21] = 0x00; 
    packet[22] = 0x40; packet[23] = 0x06;
    
    unsigned char src_ip[4] = {10, 0, 2, 15};
    for(int i=0; i<4; i++) { 
        packet[26+i] = src_ip[i]; 
        packet[30+i] = sock->remote_ip[i]; // GLOBAL YERİNE SOKET IP'Sİ
    }
    
    unsigned short ip_csum = net_checksum(&packet[14], 20);
    packet[24] = ip_csum >> 8; packet[25] = ip_csum & 0xFF;

    // GLOBAL YERİNE SOKET PORTLARI
    packet[34] = sock->local_port >> 8; packet[35] = sock->local_port & 0xFF;
    packet[36] = sock->remote_port >> 8;  packet[37] = sock->remote_port & 0xFF;
    
    // GLOBAL YERİNE SOKET SEQUENCE (SIRA) VE ACK NUMARALARI
    packet[38] = (sock->seq >> 24) & 0xFF; packet[39] = (sock->seq >> 16) & 0xFF;
    packet[40] = (sock->seq >> 8) & 0xFF;  packet[41] = sock->seq & 0xFF;
    
    packet[42] = (sock->ack >> 24) & 0xFF; packet[43] = (sock->ack >> 16) & 0xFF;
    packet[44] = (sock->ack >> 8) & 0xFF;  packet[45] = sock->ack & 0xFF;
    
    packet[46] = 0x50; 
    packet[47] = flags; 
    packet[48] = 0xFA; packet[49] = 0xF0; 
    
    for(int i=0; i<data_len; i++) packet[54+i] = data[i];

    unsigned int csum = 0;
    csum += (src_ip[0]<<8)|src_ip[1]; csum += (src_ip[2]<<8)|src_ip[3];
    csum += (sock->remote_ip[0]<<8)|sock->remote_ip[1]; // GLOBAL YERİNE SOKET IP'Sİ
    csum += (sock->remote_ip[2]<<8)|sock->remote_ip[3];
    csum += 0x0006; csum += tcp_len;
    for(int i=0; i<tcp_len; i+=2) {
        unsigned short word = (packet[34+i]<<8) | (i+1 < tcp_len ? packet[34+i+1] : 0);
        csum += word;
    }
    while(csum >> 16) csum = (csum & 0xFFFF) + (csum >> 16);
    csum = ~csum;
    packet[50] = csum >> 8; packet[51] = csum & 0xFF;

    for(int i=0; i<frame_len; i++) current_tx[i] = packet[i];
    outl(rtl_io_base + 0x20 + (tx_descriptor * 4), (unsigned int)current_tx);
    outl(rtl_io_base + 0x10 + (tx_descriptor * 4), frame_len);
    tx_descriptor = (tx_descriptor + 1) % 4;
    
    // GLOBAL YERİNE SOKET SEQUENCE GÜNCELLEMESİ
    if (data_len > 0) sock->seq += data_len;
    else if (flags & 0x02 || flags & 0x01) sock->seq += 1; // SYN veya FIN ise 1 artır
    if (gui) gui->net_activity_timer = 20;
    return data_len;
}