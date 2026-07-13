#include "ardaos.h"

// --- RING 3 (KULLANICI ALANI) YARDIMCI FONKSİYONLARI ---

// Uygulama içi (User-Space) sayı-metin dönüştürücü
void app_itoa(int n, char s[]) {
    int i = 0;
    if (n == 0) {
        s[i++] = '0';
        s[i] = '\0';
        return;
    }
    while (n > 0) {
        s[i++] = (n % 10) + '0';
        n /= 10;
    }
    s[i] = '\0';
    // String'i tersine çevir (Çünkü rakamları sondan başa aldık)
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = s[start];
        s[start] = s[end];
        s[end] = temp;
        start++;
        end--;
    }
}

// Uygulama içi metin birleştirici
void app_strcat(char* dest, const char* src) {
    int i = 0;
    while (dest[i] != '\0') i++; // İlk metnin sonunu bul
    int j = 0;
    while (src[j] != '\0') {
        dest[i+j] = src[j]; // İkinci metni ucuna ekle
        j++;
    }
    dest[i+j] = '\0';
}

// --- ANA UYGULAMA ---

__attribute__((section(".text.entry")))
void _start(char* args) {
    sys_create_window("Google DNS Istemcisi", 450, 200);
    sys_print("[ DNS ] Google.com IP adresi sorgulaniyor...");

    int socket_fd = sys_open("NET", "UDP");
    
    if (socket_fd >= 0) {
        // Gerçek bir DNS Sorgu Paketi (www.google.com'un IP Adresi Nedir?)
        unsigned char dns_query[] = {
            0xAB, 0xCD, // Sorgu ID (Rastgele)
            0x01, 0x00, // Bayraklar: Standart Sorgu
            0x00, 0x01, // Soru Sayısı: 1
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
            0x03, 'w', 'w', 'w',
            0x06, 'g', 'o', 'o', 'g', 'l', 'e',
            0x03, 'c', 'o', 'm',
            0x00, // İsim sonu
            0x00, 0x01, // Tip: A Kaydı (IPv4)
            0x00, 0x01  // Sınıf: IN (Internet)
        };

        sys_write(socket_fd, dns_query, 32);
        
        unsigned char gelen_veri[512];
        int zaman_asimi = 0;
        
        while(zaman_asimi < 50000) { 
            int okunan = sys_read(socket_fd, gelen_veri, 512);
            if (okunan > 0) {
                sys_print("--- DNS SUNUCUSUNDAN CEVAP GELDI! ---");
                
                // IP adresi paketin son 4 baytında!
                unsigned char ip1 = gelen_veri[okunan - 4];
                unsigned char ip2 = gelen_veri[okunan - 3];
                unsigned char ip3 = gelen_veri[okunan - 2];
                unsigned char ip4 = gelen_veri[okunan - 1];
                
                char ip_msg[64];
                ip_msg[0] = 'I'; ip_msg[1] = 'P'; ip_msg[2] = ':'; ip_msg[3] = ' '; ip_msg[4] = '\0';
                
                char dot[2];
                dot[0] = '.'; dot[1] = '\0';
                
                char num_str[4];
                
                // Noktaları doğrudan `.rodata`'dan okumak yerine Stack'teki dot dizisinden okuyoruz
                app_itoa(ip1, num_str); app_strcat(ip_msg, num_str); app_strcat(ip_msg, dot);
                app_itoa(ip2, num_str); app_strcat(ip_msg, num_str); app_strcat(ip_msg, dot);
                app_itoa(ip3, num_str); app_strcat(ip_msg, num_str); app_strcat(ip_msg, dot);
                app_itoa(ip4, num_str); app_strcat(ip_msg, num_str);
                
                // Ve terminale bas! (sys_print çekirdek komutu olduğu için çeviriyi kendisi yapar)
                sys_print(ip_msg);
                break;
            }
            sys_yield();
            zaman_asimi++;
        }
        
        if (zaman_asimi >= 50000) sys_print("DNS Yanit vermedi (Zaman Asimi).");
        
        sys_close(socket_fd);
    } 
    
    sys_exit();
    while(1);
}