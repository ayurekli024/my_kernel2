#include "ardaos.h"
#include "libc.h" 

__attribute__((section(".text.entry")))
void _start(char* args) {
    sys_create_window("Malloc RAM Testi", 450, 200);
    printf("[ ISTEMCI ] Ufak Yigin (Stack) sinirlarindan kurtuluyoruz!");

    // 1. Çekirdekten 100 Kilobaytlık Dev Bir RAM İste!
    // Eğer bunu `char dev_dizi[100000];` olarak yapsaydık uygulama anında patlardı (Stack Overflow)
    char* dev_buffer = (char*)malloc(100000); 
    
    if (dev_buffer != 0) {
        printf("[ BASARILI ] Kernel 100 KB RAM tahsis etti! Adres: %d", (unsigned int)dev_buffer);
        
        // Belleğe yazma testi yapalım (Eğer sayfalama engellerse Zeki Cellat vurur!)
        dev_buffer[0] = 'M';
        dev_buffer[1] = 'e';
        dev_buffer[2] = 'r';
        dev_buffer[3] = 'h';
        dev_buffer[4] = 'a';
        dev_buffer[5] = 'b';
        dev_buffer[6] = 'a';
        dev_buffer[7] = '\0';
        
        printf("Bellek Testi Basarili. Icerik: %s", dev_buffer);
        
        // İşimiz bittiğinde mutlaka belleği iade etmeliyiz ki sistem sızdırmasın (Memory Leak)
        free(dev_buffer);
        printf("[ BASARILI ] Dev bellek blogu Kernel'e geri iade edildi.");
        
    } else {
        printf("[ HATA ] Malloc RAM tahsis edemedi!");
    }
    
    sys_exit();
    while(1);
}