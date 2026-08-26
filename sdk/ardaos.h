#ifndef ARDAOS_H
#define ARDAOS_H

#define MAX_WINDOWS 6
#define MAX_SHAPES_PER_WIN 100
#define MAX_DESKTOP_SHAPES 50
#define TERMINAL_MAX_LINES 16
#define TERMINAL_LINE_LEN 80

typedef struct {
    int id;
    int x, y, w, h;
    int is_open;
    int is_dragging;
    char title[32];
    int owner_task_id;
    int shape_count;
    int shape_x[MAX_SHAPES_PER_WIN]; int shape_y[MAX_SHAPES_PER_WIN];
    int shape_w[MAX_SHAPES_PER_WIN]; int shape_h[MAX_SHAPES_PER_WIN];
    unsigned int shape_color[MAX_SHAPES_PER_WIN];
    int is_minimized;
    int prev_x, prev_y, prev_w, prev_h;
    char text_content[1024];
} window_t;

// YENİ: Çekirdek ve WM.ELF'in ortak kullandığı devasa köprü!
typedef struct {
    window_t windows[MAX_WINDOWS];
    
    int desktop_shape_count;
    int desktop_shape_x[MAX_DESKTOP_SHAPES]; int desktop_shape_y[MAX_DESKTOP_SHAPES];
    int desktop_shape_w[MAX_DESKTOP_SHAPES]; int desktop_shape_h[MAX_DESKTOP_SHAPES];
    unsigned int desktop_shape_color[MAX_DESKTOP_SHAPES];
    
    int focused_window;
    int any_window_dragging;
    volatile int force_redraw;
    
    int context_menu_open;
    int context_menu_hover_idx;
    
    char terminal_lines[TERMINAL_MAX_LINES][TERMINAL_LINE_LEN];
    int terminal_line_count;
    char user_input[256];
    int input_idx;
    
    unsigned int current_bg_color;
    
    // Sistem Monitörü verileri Çekirdekten -> WM'e akar
    int sys_pid[32];
    int sys_state[32]; 
    int sys_cpu[32];
    int sys_task_count;
} gui_state_t;
static inline void sys_yield() {
    __asm__ __volatile__ ("int $129");
}
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
static inline char sys_poll_key() { 
    int key; 
    __asm__ __volatile__ ("int $0x80" : "=a"(key) : "a"(13)); 
    return (char)key; 
}
static inline void sys_print(const char* text) { __asm__ __volatile__ ("int $0x80" : : "a"(12), "b"((unsigned int)text)); }
// YENİ: IPC - Ortak Toplantı Odasının (Shared Memory) Anahtarını Al
static inline void* sys_shm_get() {
    void* shm_ptr;
    __asm__ __volatile__ ("int $0x80" : "=a"(shm_ptr) : "a"(14));
    return shm_ptr;
}
// YENİ VFS (POSIX) STANDARTLARI
static inline int sys_open(const char* name, const char* ext) {
    int fd;
    __asm__ __volatile__ ("int $0x80" : "=a"(fd) : "a"(11), "b"((unsigned int)name), "c"((unsigned int)ext));
    return fd;
}

static inline int sys_read(int fd, unsigned char* buffer, int count) {
    int ret;
    __asm__ __volatile__ ("int $0x80" : "=a"(ret) : "a"(15), "b"(fd), "c"((unsigned int)buffer), "d"(count));
    return ret;
}

static inline void sys_close(int fd) {
    __asm__ __volatile__ ("int $0x80" : : "a"(16), "b"(fd));
}
static inline int sys_write(int fd, unsigned char* buffer, int count) {
    int ret;
    __asm__ __volatile__ ("int $0x80" : "=a"(ret) : "a"(17), "b"(fd), "c"((unsigned int)buffer), "d"(count));
    return ret;
}
// YENİ: DİNAMİK BELLEK SİSTEM ÇAĞRILARI
static inline void* sys_malloc(unsigned int size) {
    void* ptr;
    __asm__ __volatile__ ("int $0x80" : "=a"(ptr) : "a"(19), "b"(size));
    return ptr;
}

static inline void sys_free(void* ptr) {
    __asm__ __volatile__ ("int $0x80" : : "a"(20), "b"((unsigned int)ptr));
}
static inline int sys_exec(const char* name, const char* args) {
    int pid; __asm__ __volatile__ ("int $0x80" : "=a"(pid) : "a"(21), "b"((unsigned int)name), "c"((unsigned int)args)); return pid;
}
static inline int sys_get_cmd(char* buffer) {
    int ready; __asm__ __volatile__ ("int $0x80" : "=a"(ready) : "a"(22), "b"((unsigned int)buffer)); return ready;
}
static inline void sys_get_process_list(char* buffer) {
    __asm__ __volatile__ ("int $0x80" : : "a"(23), "b"((unsigned int)buffer));
}
static inline void sys_clear_terminal() {
    __asm__ __volatile__ ("int $0x80" : : "a"(24));
}
static inline void sys_kill(int pid) {
    __asm__ __volatile__ ("int $0x80" : : "a"(25), "b"(pid));
}
static inline int sys_delete_file(const char* name, const char* ext) {
    int ret; __asm__ __volatile__ ("int $0x80" : "=a"(ret) : "a"(26), "b"((unsigned int)name), "c"((unsigned int)ext)); return ret;
}
static inline int sys_create_dir(const char* name) {
    int ret; __asm__ __volatile__ ("int $0x80" : "=a"(ret) : "a"(27), "b"((unsigned int)name)); return ret;
}
static inline void sys_list_files(char* buffer) {
    __asm__ __volatile__ ("int $0x80" : : "a"(28), "b"((unsigned int)buffer));
}
static inline void sys_system_action(int action_id, char* buffer) {
    __asm__ __volatile__ ("int $0x80" : : "a"(29), "b"(action_id), "c"((unsigned int)buffer));
}
static inline void sys_set_window_text(const char* text) {
    __asm__ __volatile__ ("int $0x80" : : "a"(30), "b"((unsigned int)text));
}
static inline void sys_get_screen(unsigned int** fb, int* w, int* h) {
    __asm__ __volatile__ ("int $0x80" : : "a"(31), "b"(fb), "c"(w), "d"(h));
}
static inline void sys_get_mouse(int* x, int* y, int* btn) {
    __asm__ __volatile__ ("int $0x80" : : "a"(32), "b"(x), "c"(y), "d"(btn));
}
static inline void sys_get_time(int* h, int* m) {
    __asm__ __volatile__ ("int $0x80" : : "a"(33), "b"(h), "c"(m));
}
static inline int sys_get_window_pos(int win_id, int* x, int* y, int* w, int* h) {
    int ret;
    // DİKKAT: arg5 için %edi (D) yazmacı kullanılır.
    __asm__ __volatile__ ("int $0x80" : "=a"(ret) : "a"(35), "b"(win_id), "c"(x), "d"(y), "S"(w), "D"(h));
    return ret;
}
// YENİ - IPC Pano Sistem Çağrıları
static inline void sys_set_clipboard(const char* text) {
    __asm__ __volatile__ ("int $0x80" : : "a"(36), "b"((unsigned int)text));
}
static inline void sys_get_clipboard(char* buffer) {
    __asm__ __volatile__ ("int $0x80" : : "a"(37), "b"((unsigned int)buffer));
}
#endif