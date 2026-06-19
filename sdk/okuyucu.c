#include "ardaos.h"

char read_buffer[513];

__attribute__((section(".text.entry")))
void _start() {
    int file_size = sys_read_file("SKOR    ", "TXT", (unsigned char*)read_buffer);
    
    if (file_size > 0) {
        // Güvenlik kilidi (Maksimum 512 Byte)
        read_buffer[file_size < 512 ? file_size : 512] = '\0'; 
        
        // BAŞKA HİÇBİR ŞEY YAZDIRMA! Sadece dosyanın içini ekrana bas ki yazılar ezilmesin.
        sys_print(read_buffer);
    } else {
        sys_print("HATA: SKOR.TXT diskte bulunamadi veya okunamadi!");
    }
    
    // GUI penceresi olmadığı için sys_get_key çalışmaz. 
    // Bunun yerine metni senin okuyabilmen için ekranda 5-10 saniye asılı tutuyoruz:
    for (volatile int i = 0; i < 5000000; i++) { 
        sys_yield(); 
    }

    sys_exit();
    while(1);
}