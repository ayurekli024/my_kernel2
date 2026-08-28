#include "ardaos.h"
#include "libc.h"

void _start(char* args) {
    sys_create_window("Kumanda (Verici)", 200, 180);
    
    // 1. Aynı Boruyu (Pipe) Aç
    int fd = sys_open("MESAJ", "FIFO");
    
    sys_set_window_text("\n  Sinyal Gonder:");
    
    // 2. 3D Butonları Yarat
    sys_create_button(20, 60, 150, 30, "Selam Gonder");
    sys_create_button(20, 100, 150, 30, "Durum Raporu");
    
    while(1) {
        int ev_id;
        int ev_type = sys_poll_event(&ev_id);
        
        if (ev_type == 1) { // Butona Tıklandı
            // 3. Tıklanan butona göre boruya (Pipe) yazı yaz!
            if (ev_id == 0) {
                sys_write(fd, "Merhaba ArdaOS!", 15);
            } else if (ev_id == 1) {
                sys_write(fd, "Sistem 100% Stabil", 18);
            }
        }
        sys_yield();
    }
}