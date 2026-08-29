#include "ardaos.h"
#include "libc.h"

char response_buf[8192]; 
char clean_buf[8192];    

// ========================================================
// ARDAOS HTML RENDER MOTORU (DOM PARSER)
// ========================================================
void parse_and_render(const char* raw_data, char* clean_text) {
    int i = 0, out_idx = 0;
    int in_tag = 0;
    
    // 1. ADIM: HTTP Sunucu Başlıklarını (Headers) Atla
    while (raw_data[i] != '\0') {
        if (raw_data[i] == '\r' && raw_data[i+1] == '\n' && 
            raw_data[i+2] == '\r' && raw_data[i+3] == '\n') {
            i += 4; 
            break;
        }
        i++;
    }
    if (raw_data[i] == '\0') i = 0; 

    // 2. ADIM: HTML Etiketlerini Ayıkla ve Estetik Düzenle
    while (raw_data[i] != '\0' && out_idx < 8190) {
        if (raw_data[i] == '<') {
            in_tag = 1; 
            
            if ((raw_data[i+1] == 'b' || raw_data[i+1] == 'B') && 
                (raw_data[i+2] == 'r' || raw_data[i+2] == 'R')) {
                clean_text[out_idx++] = '\n';
            }
            else if ((raw_data[i+1] == 'l' || raw_data[i+1] == 'L') && 
                     (raw_data[i+2] == 'i' || raw_data[i+2] == 'I')) {
                clean_text[out_idx++] = '\n';
                clean_text[out_idx++] = '*';
                clean_text[out_idx++] = ' ';
            }
            else if ((raw_data[i+1] == 'p' || raw_data[i+1] == 'P') || 
                     (raw_data[i+1] == '/')) {
                if (out_idx > 0 && clean_text[out_idx-1] != '\n') {
                    clean_text[out_idx++] = '\n';
                }
            }
        } 
        else if (raw_data[i] == '>') {
            in_tag = 0; 
        } 
        else if (!in_tag) {
            if (raw_data[i] == '\t' || raw_data[i] == '\r') {
                if (out_idx > 0 && clean_text[out_idx-1] != ' ') clean_text[out_idx++] = ' ';
            } 
            else if (raw_data[i] == '\n') {
                if (out_idx > 0 && clean_text[out_idx-1] != ' ' && clean_text[out_idx-1] != '\n') {
                    clean_text[out_idx++] = ' ';
                }
            } 
            else {
                clean_text[out_idx++] = raw_data[i]; 
            }
        }
        i++;
    }
    clean_text[out_idx] = '\0';
}

void _start(char* args) {
    int win = sys_create_window("ArdaOS Tarayici (Google HTML Motoru)", 650, 500);
    sys_set_window_text("Adim 1: Aga baglaniliyor... (ARP Gonderildi)\n");

    char dummy[10];
    sys_system_action(3, dummy); // ARP Gönder

    // O Efsanevi Altın Oran!
    for(int i = 0; i < 50; i++) sys_yield();

    sys_set_window_text("Adim 2: Google'a (142.250.187.46) TCP SYN Gonderiliyor...\n");

    int fd = sys_open("NET", "TCP");
    if (fd < 0) {
        sys_set_window_text("HATA: Ag soketi acilamadi!\n");
        while(1) sys_yield();
    }

    const char* http_req = "GET / HTTP/1.1\r\nHost: google.com\r\nConnection: close\r\n\r\n";
    int req_len = strlen(http_req);

    int handshake_timeout = 0;
    int connected = 0;
    
    while (handshake_timeout < 15000) {
        if (sys_write(fd, (unsigned char*)http_req, req_len) >= 0) {
            connected = 1;
            break;
        }
        sys_yield();
        handshake_timeout++;
    }

    if (!connected) {
        sys_set_window_text("HATA: Baglanti kurulamadi! Lutfen uygulamayi kapatip tekrar deneyin.");
        while(1) sys_yield(); 
    }

    // İŞTE YENİLİK: Zaman Aşımı Çöpe Atıldı, Harika Bir Bekleme Animasyonu Eklendi!
    int read_bytes = 0;
    int tick = 0;
    char wait_msg[64] = "Adim 4: HTTP GET Gonderildi! Yanit bekleniyor...   ";
    sys_set_window_text(wait_msg);
    
    while (read_bytes == 0) {
        read_bytes = sys_read(fd, (unsigned char*)response_buf, 8000); 
        
        if (tick % 5000 == 0) {
            int dot_state = (tick / 5000) % 4;
            if (dot_state == 0) strcpy(wait_msg, "Adim 4: HTTP GET Gonderildi! Yanit bekleniyor.  ");
            else if (dot_state == 1) strcpy(wait_msg, "Adim 4: HTTP GET Gonderildi! Yanit bekleniyor.. ");
            else if (dot_state == 2) strcpy(wait_msg, "Adim 4: HTTP GET Gonderildi! Yanit bekleniyor...");
            else strcpy(wait_msg, "Adim 4: HTTP GET Gonderildi! Yanit bekleniyor   ");
            sys_set_window_text(wait_msg);
        }
        
        sys_yield();
        tick++;
    }

    if (read_bytes > 0) {
        parse_and_render(response_buf, clean_buf);
        sys_set_window_text(clean_buf);
    }

    sys_set_window_text(clean_buf); // Google'ın HTML'den temizlenmiş halini çiz

    // KLAVYE DİNLEYİCİSİ (COPY MOTORU)
    while(1) {
        char k = sys_poll_key(); // Çekirdekten tuş çek
        if (k == 'c' || k == 'C') {
            sys_set_clipboard(clean_buf); // 4KB'a kadar olan metni çekirdeğe yolla!
            
            // Kullanıcıya kopyalandığını hissettirmek için ekrana kısa bir bilgi bas
            sys_set_window_text("\n\n[ BILGI ] Web sayfasi basariyla Panoya KOPYALANDI!\n\nTerminale gecip 'pano' yazarak yapistirabilirsiniz.");
        }
        sys_sleep(10);
    }
}