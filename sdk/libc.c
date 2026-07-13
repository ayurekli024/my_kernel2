#include "libc.h"
#include "ardaos.h" 

// ========================================================
// YENİ: ARDAOS BELLEK ÇEVİRMENİ (RELOCATION MOTORU)
// ========================================================
// Çekirdekten uygulamanın RAM'deki gerçek başlangıç adresini alır
unsigned int get_app_base() {
    unsigned int base;
    __asm__ __volatile__ ("int $0x80" : "=a"(base) : "a"(18));
    return base;
}

// Derleyicinin ürettiği 0 tabanlı sahte adresleri, RAM'deki gerçek fiziksel adreslere çevirir
char* PTR(const void* ptr) {
    unsigned int p = (unsigned int)ptr;
    // Eğer adres 1 MB'ın (0x100000) altındaysa, bu bir .rodata veya .data (kod) adresidir.
    if (p < 0x100000) return (char*)(p + get_app_base());
    // Eğer 1 MB'ın üstündeyse zaten Yığın (Stack) veya DMA adresidir, dokunma.
    return (char*)p;
}

// ========================================================
// GÜNCELLENMİŞ STANDART KÜTÜPHANE FONKSİYONLARI
// ========================================================
int strlen(const char* str) {
    char* real_str = PTR(str); // Güvenli çeviri
    int len = 0;
    while (real_str[len]) len++;
    return len;
}

void strcpy(char* dest, const char* src) {
    char* real_dest = PTR(dest);
    char* real_src = PTR(src);
    int i = 0;
    while (real_src[i]) { real_dest[i] = real_src[i]; i++; }
    real_dest[i] = '\0';
}

void strcat(char* dest, const char* src) {
    char* real_dest = PTR(dest);
    char* real_src = PTR(src);
    int i = strlen(real_dest); 
    int j = 0;
    while (real_src[j]) { real_dest[i+j] = real_src[j]; j++; }
    real_dest[i+j] = '\0';
}

void memset(void* dest, int val, int len) {
    unsigned char* ptr = (unsigned char*)PTR(dest);
    while (len--) *ptr++ = val;
}

void itoa(int n, char s[]) {
    char* real_s = PTR(s);
    int i = 0;
    if (n == 0) { real_s[i++] = '0'; real_s[i] = '\0'; return; }
    while (n > 0) { real_s[i++] = (n % 10) + '0'; n /= 10; }
    real_s[i] = '\0';
    int start = 0, end = i - 1;
    while (start < end) { 
        char temp = real_s[start]; 
        real_s[start] = real_s[end]; 
        real_s[end] = temp; 
        start++; end--; 
    }
}

void printf(const char* format, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, format);

    char buffer[512]; 
    int buf_idx = 0;
    
    // Format metninin adresini gerçek RAM adresine çevir
    char* real_format = PTR(format);

    for (int i = 0; real_format[i] != '\0'; i++) {
        if (real_format[i] == '%' && real_format[i+1] != '\0') {
            i++; 
            
            if (real_format[i] == 'd') { 
                int val = __builtin_va_arg(args, int);
                char num_str[16];
                itoa(val, num_str); 
                char* real_num_str = PTR(num_str);
                int j = 0;
                while(real_num_str[j]) { buffer[buf_idx++] = real_num_str[j++]; }
            } 
            else if (real_format[i] == 's') { 
                char* str = __builtin_va_arg(args, char*);
                char* real_str = PTR(str); // Dışarıdan gelen metni de mutlaka çevir!
                int j = 0;
                while(real_str[j]) { buffer[buf_idx++] = real_str[j++]; }
            }
        } 
        else {
            buffer[buf_idx++] = real_format[i];
        }
    }
    buffer[buf_idx] = '\0'; 
    __builtin_va_end(args);

    // Buffer (Yığın) zaten 1MB'ın üzerinde olduğu için sys_print buna itiraz etmeden güvenle yazdıracak!
    sys_print(buffer); 
}