#include "multiboot.h"
#include "idt.h"
#include "memory.h"
#include "timer.h"
#include "task.h"
#include "io.h"
#include "gdt.h"
#include "keyboard.h"
#include "string.h"
#include "disk.h"
#include "sound.h"
#include "mouse.h"
#include "../sdk/ardaos.h" // YENI: Arayuz yapilari cagriliyor!

unsigned int* vesa_framebuffer;
extern void outb(unsigned short port, unsigned char data);
extern unsigned char inb(unsigned short port);
unsigned char elf_load_buffer[65536];

// ==========================================
// 1. GLOBAL DEĞİŞKENLER (ORTAK HAFIZAYA TAŞINDI)
// ==========================================
// YENI: Tum GUI degiskenleri Shared Memory'de (0xE00000) tutuluyor!
gui_state_t* gui = (gui_state_t*)0xE00000;

char last_game_key = 0;
volatile int app_needs_to_die = 0; 
int task_to_kill = -1; 
char terminal_response[1024] = ""; 
unsigned int system_ticks = 0;
int last_second = -1;

#define MAX_HISTORY 10
char cmd_history[MAX_HISTORY][256];
int history_count = 0; 
int history_index = 0;
int task1_counter = 0;

char pending_command[256] = "";
volatile int command_ready = 0;
char* system_clipboard; // Pano için sadece işaretçi, RAM Heap'ten gelecek!
// ==========================================
// 2. YARDIMCI FONKSİYONLAR VE API'LER
// ==========================================
unsigned char get_rtc_register(int reg) { outb(0x70, reg); return inb(0x71); }
unsigned char bcd_to_bin(unsigned char bcd) { return (bcd & 0x0F) + ((bcd >> 4) * 10); }

int api_create_window(const char* title, int w, int h) {
    unsigned int active_app_base = current_task->app_base;
    const char* real_title = title;
    if ((unsigned int)title < 0x100000) real_title = (const char*)(active_app_base + (unsigned int)title);

    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!gui->windows[i].is_open) {
            gui->windows[i].id = i;
            gui->windows[i].owner_task_id = current_task->id; 
            gui->windows[i].shape_count = 0;                  
            gui->windows[i].x = 200 + (i * 20); 
            gui->windows[i].y = 120 + (i * 20);
            gui->windows[i].w = w; gui->windows[i].h = h;
            gui->windows[i].is_open = 1; gui->windows[i].is_dragging = 0;
            gui->windows[i].is_minimized = 0;
            gui->windows[i].button_count = 0; // YENİ: Butonları sıfırla
            gui->windows[i].event_count = 0;
            gui->windows[i].text_content[0] = '\0';
            
            int j = 0;
            while (real_title[j] != '\0' && j < 31) { gui->windows[i].title[j] = real_title[j]; j++; }
            gui->windows[i].title[j] = '\0';
            
            gui->focused_window = i;  
            gui->force_redraw = 1;
            return i;            
        }
    }
    return -1; 
}

void api_add_shape(int x, int y, int w, int h, unsigned int color) {
    for (int i = 2; i < MAX_WINDOWS; i++) {
        if (gui->windows[i].is_open && gui->windows[i].owner_task_id == current_task->id) {
            if (gui->windows[i].shape_count < MAX_SHAPES_PER_WIN) {
                int s = gui->windows[i].shape_count;
                gui->windows[i].shape_x[s] = x; gui->windows[i].shape_y[s] = y;
                gui->windows[i].shape_w[s] = w; gui->windows[i].shape_h[s] = h;
                gui->windows[i].shape_color[s] = color;
                gui->windows[i].shape_count++;
                gui->force_redraw = 1;
            }
            return;
        }
    }
}

void api_clear_shapes() {
    for (int i = 2; i < MAX_WINDOWS; i++) {
        if (gui->windows[i].is_open && gui->windows[i].owner_task_id == current_task->id) {
            gui->windows[i].shape_count = 0;
            gui->force_redraw = 1;
            return;
        }
    }
}

int api_get_key() {
    if (gui->focused_window >= 2 && gui->windows[gui->focused_window].is_open) {
        if (gui->windows[gui->focused_window].owner_task_id == current_task->id) {
            char k = last_game_key;
            last_game_key = 0;
            if (k != 0) return k;
            current_task->state = 1; // BLOCKED
            return 0;
        }
    }
    return 0; 
}

int api_poll_key() {
    if (gui->focused_window >= 2 && gui->windows[gui->focused_window].is_open) {
        if (gui->windows[gui->focused_window].owner_task_id == current_task->id) {
            char k = last_game_key;
            last_game_key = 0;
            return k; 
        }
    }
    return 0; 
}

int api_write_file(const char* name, const char* ext, unsigned char* buffer) {
    unsigned int base = current_task->app_base;
    const char* real_name = (unsigned int)name < 0x100000 ? (const char*)(base + (unsigned int)name) : name;
    const char* real_ext = (unsigned int)ext < 0x100000 ? (const char*)(base + (unsigned int)ext) : ext;
    unsigned char* real_buffer = (unsigned int)buffer < 0x100000 ? (unsigned char*)(base + (unsigned int)buffer) : buffer;
    
    int actual_size = 0;
    while(real_buffer[actual_size] != '\0' && actual_size < 10240) actual_size++;
    actual_size++; 
    return ardaos_write_file(real_name, real_ext, actual_size, real_buffer);
}

int api_read_file(const char* name, const char* ext, unsigned char* buffer) {
    unsigned int base = current_task->app_base;
    const char* real_name = (unsigned int)name < 0x100000 ? (const char*)(base + (unsigned int)name) : name;
    const char* real_ext = (unsigned int)ext < 0x100000 ? (const char*)(base + (unsigned int)ext) : ext;
    unsigned char* real_buffer = (unsigned int)buffer < 0x100000 ? (unsigned char*)(base + (unsigned int)buffer) : buffer;
    return ardaos_read_file(real_name, real_ext, real_buffer);
}

void api_set_window_text(const char* text) {
    for (int i = 2; i < MAX_WINDOWS; i++) {
        if (gui->windows[i].is_open && gui->windows[i].owner_task_id == current_task->id) {
            int k = 0;
            while (text[k] != '\0' && k < 1023) { gui->windows[i].text_content[k] = text[k]; k++; }
            gui->windows[i].text_content[k] = '\0';
            gui->force_redraw = 1;
            return;
        }
    }
}

int kernel_read_keyboard(unsigned char* buffer) {
    if (gui->focused_window >= 2 && gui->windows[gui->focused_window].is_open) {
        if (gui->windows[gui->focused_window].owner_task_id == current_task->id) {
            char k = last_game_key;
            last_game_key = 0; 
            if (k != 0) { buffer[0] = k; return 1; }
            current_task->state = 1; 
            return 0; 
        }
    }
    return -1; 
}

void terminal_print(const char* text) {
    int i = 0;
    char temp_line[TERMINAL_LINE_LEN];
    int t_idx = 0;
    while(text[i] != '\0') {
        if (text[i] == '\n' || t_idx >= TERMINAL_LINE_LEN - 1) {
            temp_line[t_idx] = '\0';
            if (gui->terminal_line_count >= TERMINAL_MAX_LINES) {
                for (int j = 1; j < TERMINAL_MAX_LINES; j++) strcpy(gui->terminal_lines[j-1], gui->terminal_lines[j]);
                gui->terminal_line_count = TERMINAL_MAX_LINES - 1;
            }
            strcpy(gui->terminal_lines[gui->terminal_line_count], temp_line);
            gui->terminal_line_count++;
            t_idx = 0;
            if (text[i] == '\n') { i++; continue; }
        }
        temp_line[t_idx++] = text[i++];
    }
    if (t_idx > 0) {
        temp_line[t_idx] = '\0';
        if (gui->terminal_line_count >= TERMINAL_MAX_LINES) {
            for (int j = 1; j < TERMINAL_MAX_LINES; j++) strcpy(gui->terminal_lines[j-1], gui->terminal_lines[j]);
            gui->terminal_line_count = TERMINAL_MAX_LINES - 1;
        }
        strcpy(gui->terminal_lines[gui->terminal_line_count], temp_line);
        gui->terminal_line_count++;
    }
    gui->force_redraw = 1;
}

void api_print(const char* text) {
    unsigned int base = current_task->app_base;
    const char* real_text = (unsigned int)text < 0x100000 ? (const char*)(base + (unsigned int)text) : text;
    terminal_print(real_text); 
}

void api_exit_app() {
    task_to_kill = current_task->id; 
    extern void yield(void);
    yield(); 
}

extern void kill_task_by_id(int task_id);
void background_task() { while(1) { __asm__ __volatile__("sti"); task1_counter++; yield(); } }

int api_exec_app(const char* name, const char* args) {
    char raw_name[16];
    int i = 0;
    while(name[i] != '\0' && name[i] != '.' && i < 15) { raw_name[i] = name[i]; i++; }
    raw_name[i] = '\0';
    
    char ext[4] = "ELF";
    int len = strlen(name);
    if (len > 4 && (name[len-1] == 'n' || name[len-1] == 'N')) strcpy(ext, "BIN");

    char fat_name[9] = "        "; 
    for(int j = 0; j < 8 && raw_name[j] != '\0'; j++) {
        fat_name[j] = raw_name[j];
        if(fat_name[j] >= 'a' && fat_name[j] <= 'z') fat_name[j] -= 32;
    }
    fat_name[8] = '\0';

    extern unsigned char elf_load_buffer[];
    int file_size = ardaos_read_file(fat_name, ext, elf_load_buffer);
    
    if (file_size > 0) {
        extern void* malloc(unsigned int);
        
        // UYGULAMA İZOLASYON ZIRHI: Uygulamaları Heap'ten ayır ve hizala!
        // 64KB uygulama için + 4KB tolerans payı alıyoruz
        unsigned char* raw_mem = (unsigned char*)malloc(65536 + 4096); 
        
        // Adresi zorla 4096'nın katlarına (Sayfa Hizalaması) yuvarlıyoruz
        unsigned char* app_memory = (unsigned char*)(((unsigned int)raw_mem + 4096) & 0xFFFFF000);
        
        // Zeki Cellat uygulamayı silebilsin diye orijinal malloc adresini bir kenara saklıyoruz
        *((unsigned int*)(app_memory - 4)) = (unsigned int)raw_mem;

        for (int k = 0; k < 65536; k++) app_memory[k] = 0; 
        
        int pid = create_task((void (*)())elf_load_buffer, (unsigned int)app_memory, (char*)args);
        return pid;
    }
    return -1;
}

void api_clear_terminal() {
    gui->terminal_line_count = 0;
    gui->force_redraw = 1;
}

// ==========================================
// 3. MOTOR 2: GİRDİ YÖNETİCİSİ (INPUT ENGINE)
// ==========================================
void process_keyboard_events() {
    char kbd_char = get_keyboard_char();
    if (kbd_char != 0) {
        gui->force_redraw = 1;
        
        if (gui->focused_window >= 2 && gui->windows[gui->focused_window].is_open) {
            wake_task_by_id(gui->windows[gui->focused_window].owner_task_id);
        }
        // --- YENİ: ÇEKİRDEK SEVİYESİ CTRL+C (KOPYALA) ---
        if (kbd_char == 3) { 
            if (gui->focused_window >= 2 && gui->windows[gui->focused_window].sel_start != -1) {
                int w_idx = gui->focused_window;
                int s_min = (gui->windows[w_idx].sel_start < gui->windows[w_idx].sel_end) ? gui->windows[w_idx].sel_start : gui->windows[w_idx].sel_end;
                int s_max = (gui->windows[w_idx].sel_start > gui->windows[w_idx].sel_end) ? gui->windows[w_idx].sel_start : gui->windows[w_idx].sel_end;
                
                extern char* system_clipboard;
                int copy_idx = 0;
                for (int i = s_min; i <= s_max && copy_idx < 4095; i++) {
                    char c = gui->windows[w_idx].text_content[i];
                    if (c == '\0') break;
                    system_clipboard[copy_idx++] = c;
                }
                system_clipboard[copy_idx] = '\0';
                terminal_print("[ SISTEM ] Metin basariyla panoya KOPYALANDI! (Ctrl+C)");
            }
        }
        // --- YENİ: ÇEKİRDEK SEVİYESİ CTRL+V (YAPIŞTIR) ---
        else if (kbd_char == 22) { 
            if (gui->focused_window == 0) { // Sadece Terminale Yapıştır (Şimdilik)
                extern char* system_clipboard;
                int i = 0;
                while(system_clipboard[i] != '\0' && gui->input_idx < 255) {
                    if (system_clipboard[i] != '\n') { // Satır sonları terminal girdisini bozmasın
                        gui->user_input[gui->input_idx++] = system_clipboard[i];
                    }
                    i++;
                }
                gui->user_input[gui->input_idx] = '\0';
            }
        }
        if (gui->focused_window >= 2 && gui->windows[gui->focused_window].is_open) {
            last_game_key = kbd_char;
        } else {
            last_game_key = 0; 
            
            if (kbd_char == '\n') { 
                terminal_print(gui->user_input); 
                
                // Kullanıcının yazdığı komutu al ("Arda> " kısmını atla)
                char* entered_cmd = &(gui->user_input)[6];
                
                // YENİ: Komutu Geçmişe (History) Kaydet
                if (entered_cmd[0] != '\0') {
                    extern void strcpy(char*, const char*);
                    strcpy(cmd_history[history_count % MAX_HISTORY], entered_cmd);
                    history_count++;
                    history_index = history_count; // İndeksi her zaman en sona (temiz satıra) al
                }

                // Uygulamanın çekmesi için komutu hazırla
                strcpy(pending_command, entered_cmd);
                command_ready = 1; 
                
                // Terminal satırını sıfırla
                strcpy(gui->user_input, "Arda> ");
                gui->input_idx = 6;
            }
            else if (kbd_char == 17) { 
                if (history_count > 0 && history_index > 0) {
                    history_index--;
                    strcpy(gui->user_input, "Arda> ");
                    strcat(gui->user_input, cmd_history[history_index % MAX_HISTORY]);
                    gui->input_idx = 6 + strlen(cmd_history[history_index % MAX_HISTORY]);
                }
            }
            else if (kbd_char == 18) { 
                if (history_index < history_count) {
                    history_index++;
                    if (history_index == history_count) {
                        strcpy(gui->user_input, "Arda> ");
                        gui->input_idx = 6;
                    } else {
                        strcpy(gui->user_input, "Arda> ");
                        strcat(gui->user_input, cmd_history[history_index % MAX_HISTORY]);
                        gui->input_idx = 6 + strlen(cmd_history[history_index % MAX_HISTORY]);
                    }
                }
            }
            else if (kbd_char == '\b' && gui->input_idx > 6) { 
                gui->input_idx--;
                gui->user_input[gui->input_idx] = '\0';
            } 
            else if (gui->input_idx < 255 && kbd_char != '\b' && kbd_char != '\n') { 
                gui->user_input[gui->input_idx] = kbd_char;
                gui->input_idx++;
                gui->user_input[gui->input_idx] = '\0'; 
            }
        }
    }
}

// ==========================================
// 4. ANA İŞLETİM SİSTEMİ BAŞLANGICI
// ==========================================
void kernel_main(unsigned int magic, struct multiboot_info* mb_info) {
    init_gdt(); pic_remap(); init_idt(); init_mouse(); 
    if (magic != 0x2BADB002) return; 
    if (mb_info->flags & (1 << 12)) vesa_framebuffer = (unsigned int*)(unsigned int)mb_info->framebuffer_addr;
    
    init_paging((unsigned int)vesa_framebuffer); 
    init_heap(); init_tasking(); create_task(background_task, 0, "");
    // YENİ: Panoyu BSS'te değil, güvenli Heap belleğinde (8. MB) oluşturuyoruz!
    system_clipboard = (char*)malloc(4096);
    for(int i = 0; i < 4096; i++) system_clipboard[i] = '\0';
    // ZEKİ CELLAT ZIRHI: Kernel Panic çizebilsin diye grafiği Kernel'de de başlat!
    extern void init_graphics(unsigned int*, int, int);
    if (vesa_framebuffer != 0) init_graphics(vesa_framebuffer, 1024, 768);
    init_timer(100); init_disk();
    extern void init_rtl8139(void);
    init_rtl8139();
    
    // GUI Hafızasını Temizle ve Başlat
    for (int i = 0; i < sizeof(gui_state_t); i++) ((char*)gui)[i] = 0;
    
    gui->current_bg_color = 0x001B26;
    gui->windows[0].id = 0; gui->windows[0].is_open = 1; gui->windows[0].is_minimized = 0;
    gui->windows[0].x = 100; gui->windows[0].y = 100; gui->windows[0].w = 450; gui->windows[0].h = 350;
    strcpy(gui->windows[0].title, "ArdaOS Terminali");
    
    gui->windows[1].id = 1; gui->windows[1].is_open = 1; gui->windows[1].is_minimized = 0;
    gui->windows[1].x = 600; gui->windows[1].y = 150; gui->windows[1].w = 300; gui->windows[1].h = 200;
    strcpy(gui->windows[1].title, "Sistem Monitoru");

    gui->focused_window = 0; 
    gui->force_redraw = 1;
    strcpy(gui->user_input, "Arda> ");
    gui->input_idx = 6;
    
    terminal_print("ArdaOS V1.1 Multitasking'e Hos Geldiniz!");
    __asm__ __volatile__ ("sti");
    
    // İşletim sistemi ayağa kalktığında ilk iş WM.ELF (Arayüz) ve SHELL.ELF (Terminal) başlar!
    api_exec_app("wm.elf", "");
    api_exec_app("shell.elf", "");

    while(1) {
        system_ticks++;
        if (app_needs_to_die) { task_to_kill = current_task->id; app_needs_to_die = 0; }

        if (task_to_kill != -1) {
            kill_task_by_id(task_to_kill); 
            for (int i = 2; i < MAX_WINDOWS; i++) {
                if (gui->windows[i].is_open && gui->windows[i].owner_task_id == task_to_kill) {
                    gui->windows[i].is_open = 0;
                    if (gui->focused_window == i) gui->focused_window = 0;
                }
            }
            task_to_kill = -1;
            gui->force_redraw = 1;
            strcpy(terminal_response, "[ SISTEM ] Uygulama sonlandirildi.");
        }

        int current_second = timer_ticks / 100;
        if (current_second != last_second) {
            last_second = current_second; 
            if (ready_queue != 0) {
                task_t* curr = ready_queue;
                gui->sys_task_count = 0;
                do {
                    if (gui->sys_task_count < 32) {
                        gui->sys_pid[gui->sys_task_count] = curr->id;
                        gui->sys_state[gui->sys_task_count] = curr->state;
                        gui->sys_cpu[gui->sys_task_count] = curr->cpu_ticks;
                        gui->sys_task_count++;
                    }
                    curr->cpu_usage = curr->cpu_ticks; 
                    curr->cpu_ticks = 0; 
                    curr = curr->next;
                } while (curr != ready_queue);
            }
            gui->force_redraw = 1; 
        }

        process_keyboard_events();
        
        // DİKKAT: RENDER_GUI VE MOUSE_EVENTS ARTIK BURADA YOK! ONLARI WM.ELF YAPACAK!
        
        yield(); 
        __asm__ __volatile__ ("sti; hlt");
    }
}