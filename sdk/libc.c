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
// Standart Kütüphane Malloc (Arka planda Kernel Syscall'unu çağırır)
// ========================================================
// YENİ: USER-SPACE DİNAMİK BELLEK YÖNETİCİSİ (Kernel'den Bağımsız!)
// ========================================================
struct malloc_block {
    unsigned int size;
    int is_free;
    struct malloc_block* next;
};

// Uygulamanın kendi dünyasındaki Heap Başlangıcı
struct malloc_block* user_heap_head = 0;

// Kernel'den Sayfa (Page) Genişletme İsteği
void* sbrk(int increment) {
    unsigned int ret;
    __asm__ __volatile__ ("int $0x80" : "=a"(ret) : "a"(34), "b"(increment));
    return (void*)ret;
}

// Tamamen İzole Uygulama İçi Malloc Motoru (Next-Fit Algoritması)
void* malloc(unsigned int size) {
    if (size == 0) return 0;
    size = (size + 3) & ~3; // Hafızayı kusursuz çalışması için 4 bayta hizala

    // İlk defa RAM isteniyorsa, Çekirdekten sbrk ile yeni harita talep et!
    if (user_heap_head == 0) {
        user_heap_head = (struct malloc_block*)sbrk(sizeof(struct malloc_block) + size);
        user_heap_head->size = size;
        user_heap_head->is_free = 0;
        user_heap_head->next = 0;
        return (void*)((unsigned int)user_heap_head + sizeof(struct malloc_block));
    }

    struct malloc_block* curr = user_heap_head;
    struct malloc_block* last = curr;
    
    // Uygulama içinde daha önceden silinmiş (Free) boş bir delik var mı diye bak
    while (curr != 0) {
        if (curr->is_free && curr->size >= size) {
            curr->is_free = 0;
            return (void*)((unsigned int)curr + sizeof(struct malloc_block));
        }
        last = curr;
        curr = curr->next;
    }

    // Uygulamanın elindeki RAM yetmediyse, Çekirdekten haritayı daha da büyütmesini iste!
    struct malloc_block* new_block = (struct malloc_block*)sbrk(sizeof(struct malloc_block) + size);
    new_block->size = size;
    new_block->is_free = 0;
    new_block->next = 0;
    last->next = new_block;
    
    return (void*)((unsigned int)new_block + sizeof(struct malloc_block));
}

void free(void* ptr) {
    if (!ptr) return;
    // Göstericinin 1 adım gerisine gidip başlığı bul ve RAM'i tekrar serbest (Free) bırak
    struct malloc_block* block = (struct malloc_block*)((unsigned int)ptr - sizeof(struct malloc_block));
    block->is_free = 1;
}
void get_screen_info(unsigned int** fb, int* w, int* h) {
    sys_get_screen(fb, w, h);
}
void get_mouse_state(int* x, int* y, int* btn) {
    sys_get_mouse(x, y, btn);
}