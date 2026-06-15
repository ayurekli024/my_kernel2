#ifndef ARDAOS_H
#define ARDAOS_H

static inline void sys_yield() {
    __asm__ __volatile__ ("int $0x80" : : "a"(4));
}

static inline void add_shape(int x, int y, int w, int h, unsigned int color) {
    __asm__ __volatile__ (
        "int $0x80"
        : 
        : "a"(5), "b"(x), "c"(y), "d"(w), "S"(h), "D"(color)
    );
}

static inline void sys_halt() {
    while(1) {
        __asm__ __volatile__ ("sti; hlt"); // Kesme kapısını aç ve işlemciyi dinlendir
        sys_yield(); 
    }
}

#endif