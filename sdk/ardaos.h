#ifndef ARDAOS_H
#define ARDAOS_H

static inline void sys_yield() {
    __asm__ __volatile__ ("int $0x80" : : "a"(4));
}

static inline void add_shape(int x, int y, int w, int h, unsigned int color) {
    __asm__ __volatile__ ("int $0x80" : : "a"(5), "b"(x), "c"(y), "d"(w), "S"(h), "D"(color));
}

// YENİ: Klavyeden o an basılan tuşu okur
static inline char sys_get_key() {
    int key;
    __asm__ __volatile__ ("int $0x80" : "=a"(key) : "a"(6));
    return (char)key;
}

// YENİ: Çizilen tüm şekilleri ekrandan siler (Animasyon yaratmak için)
static inline void sys_clear_shapes() {
    __asm__ __volatile__ ("int $0x80" : : "a"(7));
}

static inline void sys_halt() {
    while(1) {
        __asm__ __volatile__ ("sti; hlt"); 
        sys_yield(); 
    }
}

#endif