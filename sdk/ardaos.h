#ifndef ARDAOS_H
#define ARDAOS_H

static inline void sys_yield() { __asm__ __volatile__ ("int $0x80" : : "a"(4)); }
static inline void add_shape(int x, int y, int w, int h, unsigned int color) { __asm__ __volatile__ ("int $0x80" : : "a"(5), "b"(x), "c"(y), "d"(w), "S"(h), "D"(color)); }
static inline char sys_get_key() { int key; __asm__ __volatile__ ("int $0x80" : "=a"(key) : "a"(6)); return (char)key; }
static inline void sys_clear_shapes() { __asm__ __volatile__ ("int $0x80" : : "a"(7)); }
static inline void sys_halt() { while(1) { __asm__ __volatile__ ("sti; hlt"); sys_yield(); } }
static inline int sys_create_window(const char* title, int w, int h) {
    int win_id; __asm__ __volatile__ ("int $0x80" : "=a"(win_id) : "a"(8), "b"(title), "c"(w), "d"(h)); return win_id;
}
static inline void sys_exit() { __asm__ __volatile__ ("int $0x80" : : "a"(9)); }

// EFSANE ZIRH: GCC'nin bozmaması için Boyut (size) parametresini sildik!
// BOYUT PARAMETRESİ KALDIRILDI! Buffer artık EDX ("d") yazmacından gidiyor.
static inline int sys_write_file(const char* name, const char* ext, unsigned char* buffer) {
    int ret;
    __asm__ __volatile__ ("int $0x80" : "=a"(ret) : "a"(10), "b"((unsigned int)name), "c"((unsigned int)ext), "d"((unsigned int)buffer));
    return ret;
}

static inline int sys_read_file(const char* name, const char* ext, unsigned char* buffer) {
    int ret;
    __asm__ __volatile__ ("int $0x80" : "=a"(ret) : "a"(11), "b"((unsigned int)name), "c"((unsigned int)ext), "d"((unsigned int)buffer));
    return ret;
}
static inline void sys_print(const char* text) { __asm__ __volatile__ ("int $0x80" : : "a"(12), "b"((unsigned int)text)); }
#endif