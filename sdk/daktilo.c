#include "ardaos.h"

__attribute__((section(".text.entry")))
void _start(char* args) {
    sys_create_window("VFS Sanal Klavye (DEV.KBD)", 400, 300);
    sys_print("[ DAKTILO ] /dev/kbd sanal donanimi aciliyor...");

    // İŞTE SİHİR BURADA: Hiçbir özel klavye syscall'u yok. 
    // Sadece standart bir "Sanal Dosya" açıyoruz!
    int fd = sys_open("DEV", "KBD");
    
    if (fd >= 0) {
        sys_print("[ BASARILI ] Klavye dosyasi acildi. Yazin (Cikis: q):");
        
        while(1) {
            unsigned char kbd_buffer[2]; 
            
            // Dosyadan standart VFS yöntemiyle oku (Arka planda donanımdan gelecek)
            int bytes_read = sys_read(fd, kbd_buffer, 1);
            
            if (bytes_read == 1) {
                kbd_buffer[1] = '\0';
                sys_print((char*)kbd_buffer); // Okunan tuşu terminale bas
                
                if (kbd_buffer[0] == 'q' || kbd_buffer[0] == 'Q') {
                    break; // q'ya basıldığında sonsuz akıştan (Stream) çık
                }
            } else {
                // Eğer tuş basılmadıysa sıramızı devredelim ki sistem yorulmasın
                sys_yield();
            }
        }
        sys_close(fd);
    } else {
        sys_print("[ HATA ] Cihaz dosyasi acilamadi!");
    }
    
    sys_print("Daktilo sonlandirildi.");
    sys_exit();
    while(1);
}