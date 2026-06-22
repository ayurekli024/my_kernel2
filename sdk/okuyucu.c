#include "ardaos.h"

char read_buffer[513];

__attribute__((section(".text.entry")))
void _start(char* args) {  // YENİ: Çekirdek buraya gizlice parametreyi yollayacak!
    char f_name[9] = "SKOR    "; // Parametre yoksa Varsayılan İsim
    char f_ext[4]  = "TXT";      // Varsayılan Uzantı
    
    // Eğer terminalden bir argüman girildiyse (Örn: "TEST.TXT")
    if (args != 0 && args[0] != '\0') {
        for(int i = 0; i < 8; i++) f_name[i] = ' '; // İçini temizle
        for(int i = 0; i < 3; i++) f_ext[i] = ' ';
        
        int i = 0, j = 0;
        // Noktaya kadar olan kısmı f_name içine al ve büyük harfe çevir
        while(args[i] != '.' && args[i] != '\0' && j < 8) {
            char c = args[i++];
            if (c >= 'a' && c <= 'z') c -= 32; 
            f_name[j++] = c;
        }
        if (args[i] == '.') {
            i++; 
            j = 0;
            // Noktadan sonrasını f_ext içine al
            while(args[i] != '\0' && j < 3) {
                char c = args[i++];
                if (c >= 'a' && c <= 'z') c -= 32;
                f_ext[j++] = c;
            }
        }
        f_name[8] = '\0'; f_ext[3] = '\0';
    }

    // ARTIK DOSYAYI DİNAMİK OLARAK OKUYORUZ!
    int file_size = sys_read_file(f_name, f_ext, (unsigned char*)read_buffer);
    
    if (file_size > 0) {
        read_buffer[file_size < 512 ? file_size : 512] = '\0'; 
        sys_print(read_buffer);
    } else {
        sys_print("HATA: Belirtilen dosya diskte bulunamadi!");
    }
    
    for (volatile int i = 0; i < 5000000; i++) { sys_yield(); }
    sys_exit();
    while(1);
}       