#include "ardaos.h"
#include "libc.h" // YENİ: Kendi Standart Kütüphanemiz!

__attribute__((section(".text.entry")))
void _start(char* args) {
    sys_create_window("Google DNS Istemcisi", 450, 200);
    printf("[ DNS ] Google.com IP adresi sorgulaniyor...");

    int socket_fd = sys_open("NET", "UDP");
    
    if (socket_fd >= 0) {
        unsigned char dns_query[] = {
            0xAB, 0xCD, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
            0x03, 'w', 'w', 'w', 0x06, 'g', 'o', 'o', 'g', 'l', 'e', 0x03, 'c', 'o', 'm', 
            0x00, 0x00, 0x01, 0x00, 0x01  
        };

        sys_write(socket_fd, dns_query, 32);
        
        unsigned char gelen_veri[512];
        int zaman_asimi = 0;
        
        while(zaman_asimi < 50000) { 
            int okunan = sys_read(socket_fd, gelen_veri, 512);
            if (okunan > 0) {
                printf("--- DNS SUNUCUSUNDAN CEVAP GELDI! ---");
                
                unsigned char ip1 = gelen_veri[okunan - 4];
                unsigned char ip2 = gelen_veri[okunan - 3];
                unsigned char ip3 = gelen_veri[okunan - 2];
                unsigned char ip4 = gelen_veri[okunan - 1];
                
                // İŞTE YENİ GÜCÜMÜZ: Tıpkı Linux'taki gibi tek satırda muazzam formatlama!
                printf("Google IP Adresi: %d.%d.%d.%d", ip1, ip2, ip3, ip4);
                break;
            }
            sys_yield();
            zaman_asimi++;
        }
        
        if (zaman_asimi >= 50000) printf("DNS Yanit vermedi (Zaman Asimi).");
        
        sys_close(socket_fd);
    } 
    
    sys_exit();
    while(1);
}