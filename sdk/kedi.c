#include "ardaos.h"

__attribute__((section(".text.entry")))
void _start(char* args) {
    char read_buffer[513];
    char f_name[9] = "        ";
    char f_ext[4]  = "   ";

    // 1. Terminalden girilen dosya adını FAT formatına çevir (Örn: "kedi.bin NOTLAR.TXT")
    if (args != 0 && args[0] != '\0') {
        int i = 0, j = 0;
        while(args[i] != '.' && args[i] != '\0' && j < 8) {
            char c = args[i++];
            if (c >= 'a' && c <= 'z') c -= 32;
            f_name[j++] = c;
        }
        if (args[i] == '.') {
            i++; j = 0;
            while(args[i] != '\0' && j < 3) {
                char c = args[i++];
                if (c >= 'a' && c <= 'z') c -= 32;
                f_ext[j++] = c;
            }
        }
        f_name[8] = '\0'; f_ext[3] = '\0';
    } else {
        sys_print("HATA: Lutfen okunacak dosyayi belirtin (Orn: kedi.bin TEST.TXT)");
        sys_exit();
        while(1); 
    }

    // ==========================================================
    // YENİ VFS (Sanal Dosya Sistemi) TESTİ!
    // ==========================================================
    
    // A) Dosyayı aç ve İşletim Sisteminden Bilet (FD) al
    int fd = sys_open(f_name, f_ext);

    if (fd >= 0) {
        // B) Doğrudan disk sektörleriyle değil, aldığımız FD (Bilet) ile okuma yapıyoruz!
        int bytes_read = sys_read(fd, (unsigned char*)read_buffer, 512);

        if (bytes_read > 0) {
            read_buffer[bytes_read] = '\0'; // Metnin sonunu işaretle
            sys_print("--- DOSYA ICERIGI ---");
            sys_print(read_buffer);
        } else {
            sys_print("Dosya tamamen bos.");
        }

        // C) İşi bitince bileti (FD) işletim sistemine iade et
        sys_close(fd); 
        
    } else {
        sys_print("HATA: VFS belirtilen dosyayi bulamadi veya acamadi!");
    }

    // Görevi sonlandır ve RAM'i sisteme iade et
    sys_exit();
    while(1);
}