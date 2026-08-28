#include "ardaos.h"
#include "libc.h"

void _start(char* args) {
    sys_create_window("Mesaj Panosu (Alici)", 250, 150);
    
    // 1. Boruyu (Pipe) Aç
    int fd = sys_open("MESAJ", "FIFO");
    
    char display_text[256] = "\n  [ Yayin Bekleniyor... ]\n  -----------------------";
    sys_set_window_text(display_text);

    while(1) {
        char buf[64];
        // 2. Borudan Veri Oku (Veri yoksa 0 döner ve geçer)
        int n = sys_read(fd, buf, 63);
        if (n > 0) {
            buf[n] = '\0';
            
            // Gelen veriyi ekrana bas
            char new_text[256] = "\n  YENI MESAJ:\n  > ";
            strcat(new_text, buf);
            sys_set_window_text(new_text);
        }
        sys_yield(); // Çekirdeği yorma
    }
}