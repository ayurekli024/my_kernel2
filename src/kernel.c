#include "multiboot.h"
#include "idt.h"
#include "memory.h"
#include "timer.h"
#include "task.h"
#include "graphics.h"
#include "mouse.h"
#include "io.h"
#include "gdt.h"
#include "keyboard.h"
#include "string.h"
#include "disk.h"
#include "sound.h"
unsigned int* vesa_framebuffer;
extern void outb(unsigned short port, unsigned char data);
extern unsigned char inb(unsigned short port);
// ELF dosyaları için güvenli (Gölge haritada kaybolmayan) statik disk okuma alanı
unsigned char elf_load_buffer[65536];
// ==========================================
// 1. GLOBAL DEĞİŞKENLER VE DURUM YÖNETİMİ
// ==========================================
#define MAX_DESKTOP_SHAPES 50
#define MAX_WINDOWS 6 // Limit 6'ya çıkarıldı
#define MAX_SHAPES_PER_WIN 100 // Her pencerenin özel tuval limiti

// Masaüstü Şekilleri (Terminalden "ciz" komutu ile çizilenler)
int desktop_shape_count = 0;
int desktop_shape_x[MAX_DESKTOP_SHAPES]; int desktop_shape_y[MAX_DESKTOP_SHAPES];
int desktop_shape_w[MAX_DESKTOP_SHAPES]; int desktop_shape_h[MAX_DESKTOP_SHAPES];
unsigned int desktop_shape_color[MAX_DESKTOP_SHAPES];

// Kapsüllü (OOP) Pencere Mimarisi
typedef struct {
    int id;
    int x, y, w, h;
    int is_open;
    int is_dragging;
    char title[32];
    int owner_task_id; // Bu pencere hangi göreve (Task) ait?
    
    // Her pencerenin kendine ait bağımsız grafik hafızası
    int shape_count;
    int shape_x[MAX_SHAPES_PER_WIN]; int shape_y[MAX_SHAPES_PER_WIN];
    int shape_w[MAX_SHAPES_PER_WIN]; int shape_h[MAX_SHAPES_PER_WIN];
    unsigned int shape_color[MAX_SHAPES_PER_WIN];
} window_t;

window_t windows[MAX_WINDOWS];

int focused_window = 0; 
int any_window_dragging = 0;
volatile int force_redraw = 0;
char last_game_key = 0;

volatile int app_needs_to_die = 0; 
int task_to_kill = -1; // Cellat motorunun hedefini tutar

int last_mouse_x = 0, last_mouse_y = 0;
unsigned int current_bg_color = 0x001B26;
// YENİ: TERMINAL SCROLLING MOTORU
#define TERMINAL_MAX_LINES 16
#define TERMINAL_LINE_LEN 80
char terminal_lines[TERMINAL_MAX_LINES][TERMINAL_LINE_LEN];
int terminal_line_count = 0;

char terminal_response[1024] = ""; // execute_command için geçici tampon
char user_input[256] = "Arda> "; 
int input_idx = 6;
unsigned int system_ticks = 0;
int last_second = -1;

#define MAX_HISTORY 10
char cmd_history[MAX_HISTORY][256];
int history_count = 0; 
int history_index = 0;
int task1_counter = 0;

// ==========================================
// 2. YARDIMCI FONKSİYONLAR VE API'LER
// ==========================================
unsigned char get_rtc_register(int reg) {
    outb(0x70, reg); return inb(0x71);
}

unsigned char bcd_to_bin(unsigned char bcd) {
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}

// MULTITASKING API: Pencereyi açan Task'ı sahiplendirir
int api_create_window(const char* title, int w, int h) {
    unsigned int active_app_base = current_task->app_base;
    const char* real_title = title;
    
    if ((unsigned int)title < 0x100000) {
        real_title = (const char*)(active_app_base + (unsigned int)title);
    }

    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!windows[i].is_open) {
            windows[i].id = i;
            windows[i].owner_task_id = current_task->id; 
            windows[i].shape_count = 0;                  
            windows[i].x = 200 + (i * 20); 
            windows[i].y = 120 + (i * 20);
            windows[i].w = w; windows[i].h = h;
            windows[i].is_open = 1; windows[i].is_dragging = 0;
            
            int j = 0;
            while (real_title[j] != '\0' && j < 31) {
                windows[i].title[j] = real_title[j];
                j++;
            }
            windows[i].title[j] = '\0';
            
            focused_window = i;  
            force_redraw = 1;
            return i;            
        }
    }
    return -1; 
}

// MULTITASKING API: Şekilleri evrensel listeye değil, Task'a ait olan pencerenin tuvaline çizer!
void api_add_shape(int x, int y, int w, int h, unsigned int color) {
    for (int i = 2; i < MAX_WINDOWS; i++) {
        if (windows[i].is_open && windows[i].owner_task_id == current_task->id) {
            if (windows[i].shape_count < MAX_SHAPES_PER_WIN) {
                int s = windows[i].shape_count;
                windows[i].shape_x[s] = x; windows[i].shape_y[s] = y;
                windows[i].shape_w[s] = w; windows[i].shape_h[s] = h;
                windows[i].shape_color[s] = color;
                windows[i].shape_count++;
                force_redraw = 1;
            }
            return;
        }
    }
}

void api_clear_shapes() {
    for (int i = 2; i < MAX_WINDOWS; i++) {
        if (windows[i].is_open && windows[i].owner_task_id == current_task->id) {
            windows[i].shape_count = 0;
            force_redraw = 1;
            return;
        }
    }
}
// MULTITASKING API: Sadece odaklanmış pencerenin sahibi klavyeyi okuyabilir!
int api_get_key() {
    if (focused_window >= 2 && windows[focused_window].is_open) {
        if (windows[focused_window].owner_task_id == current_task->id) {
            char k = last_game_key;
            last_game_key = 0;
            
            // Eğer tuş geldiyse, görevin durumu zaten RUNNABLE (0) kalsın ve tuşu döndür
            if (k != 0) return k;
            
            // TUŞ YOKSA GÖREVİ UYUT! (İşlemci yemesin)
            current_task->state = 1; // 1 = BLOCKED
            return 0;
        }
    }
    return 0; 
}
// MULTITASKING API: Görevi UYUTMADAN (Asenkron) tuşu okur (Oyunlar için)
int api_poll_key() {
    if (focused_window >= 2 && windows[focused_window].is_open) {
        if (windows[focused_window].owner_task_id == current_task->id) {
            char k = last_game_key;
            last_game_key = 0;
            return k; // Tuş yoksa bile 0 döndür ama UYUTMA!
        }
    }
    return 0; 
}
// MULTITASKING API: Harici uygulamanın güvenli şekilde dosya yazması
int api_write_file(const char* name, const char* ext, unsigned char* buffer) {
    unsigned int base = current_task->app_base;
    const char* real_name = (unsigned int)name < 0x100000 ? (const char*)(base + (unsigned int)name) : name;
    const char* real_ext = (unsigned int)ext < 0x100000 ? (const char*)(base + (unsigned int)ext) : ext;
    unsigned char* real_buffer = (unsigned int)buffer < 0x100000 ? (unsigned char*)(base + (unsigned int)buffer) : buffer;
    
    // KESİN ZIRH: Uygulamadan boyut bekleme, donanım standardı olan 512'yi (1 Sektör) zorla!
    return ardaos_write_file(real_name, real_ext, 512, real_buffer);
}

// MULTITASKING API: Harici uygulamanın güvenli şekilde dosya okuması
int api_read_file(const char* name, const char* ext, unsigned char* buffer) {
    unsigned int base = current_task->app_base;
    const char* real_name = (unsigned int)name < 0x100000 ? (const char*)(base + (unsigned int)name) : name;
    const char* real_ext = (unsigned int)ext < 0x100000 ? (const char*)(base + (unsigned int)ext) : ext;
    unsigned char* real_buffer = (unsigned int)buffer < 0x100000 ? (unsigned char*)(base + (unsigned int)buffer) : buffer;
    return ardaos_read_file(real_name, real_ext, real_buffer);
}
// ========================================================
// VFS KÖPRÜSÜ: SANAL DONANIMLARI OKUMA (Hardware Abstraction)
// ========================================================
int kernel_read_keyboard(unsigned char* buffer) {
    if (focused_window >= 2 && windows[focused_window].is_open) {
        if (windows[focused_window].owner_task_id == current_task->id) {
            char k = last_game_key;
            last_game_key = 0; // Tuşu okuduk, sıfırla
            
            if (k != 0) {
                buffer[0] = k;
                return 1; // 1 Bayt okundu
            }
            
            // İşlemciyi yormamak için görev uyutulur
            current_task->state = 1; // BLOCKED
            return 0; // 0 Bayt okundu (Uygulama tekrar denemeli)
        }
    }
    return -1; // Yetki yok
}
// TERMINAL YAZDIRMA VE KAYDIRMA (SCROLLING) MOTORU
void terminal_print(const char* text) {
    int i = 0;
    char temp_line[TERMINAL_LINE_LEN];
    int t_idx = 0;
    
    while(text[i] != '\0') {
        // Eğer alt satıra geçiş (\n) geldiyse veya satır sonuna ulaşıldıysa
        if (text[i] == '\n' || t_idx >= TERMINAL_LINE_LEN - 1) {
            temp_line[t_idx] = '\0';
            
            // Ekran doluysa tüm satırları 1 blok yukarı kaydır!
            if (terminal_line_count >= TERMINAL_MAX_LINES) {
                for (int j = 1; j < TERMINAL_MAX_LINES; j++) {
                    strcpy(terminal_lines[j-1], terminal_lines[j]);
                }
                terminal_line_count = TERMINAL_MAX_LINES - 1;
            }
            strcpy(terminal_lines[terminal_line_count], temp_line);
            terminal_line_count++;
            
            t_idx = 0;
            if (text[i] == '\n') { i++; continue; }
        }
        temp_line[t_idx++] = text[i++];
    }
    
    // Kalan son harfleri de bas
    if (t_idx > 0) {
        temp_line[t_idx] = '\0';
        if (terminal_line_count >= TERMINAL_MAX_LINES) {
            for (int j = 1; j < TERMINAL_MAX_LINES; j++) {
                strcpy(terminal_lines[j-1], terminal_lines[j]);
            }
            terminal_line_count = TERMINAL_MAX_LINES - 1;
        }
        strcpy(terminal_lines[terminal_line_count], temp_line);
        terminal_line_count++;
    }
    force_redraw = 1;
}
// MULTITASKING API: Uygulamalarin Terminale Mesaj Gondermesi
void api_print(const char* text) {
    unsigned int base = current_task->app_base;
    const char* real_text = (unsigned int)text < 0x100000 ? (const char*)(base + (unsigned int)text) : text;
    
    // Artık üstüne yazmıyor, kaydırarak ekliyor!
    terminal_print(real_text); 
}
// SYSCALL (API No 9) Tetiklendiğinde çalışır
void api_exit_app() {
    task_to_kill = current_task->id; // Hedefi (kendini) belirle
    
    // YENİ: Uygulamaya geri dönme! İşlemciyi (CPU) anında sıradaki göreve devret!
    extern void yield(void);
    yield(); 
}

extern void kill_task_by_id(int task_id);

void background_task() {
    while(1) { __asm__ __volatile__("sti"); task1_counter++; yield(); }
}

// ==========================================
// 3. MOTOR 1: ÇİZİM VE GRAFİK (RENDER ENGINE)
// ==========================================
void draw_window(window_t* win) {
    if (!win->is_open) return;
    draw_rect(win->x, win->y, win->w, win->h, 0x00F0F0F0); 
    unsigned int title_color = (win->id == focused_window) ? 0x000078D7 : 0x00777777;
    draw_rect(win->x, win->y, win->w, 30, title_color); 
    draw_rect(win->x + win->w - 30, win->y, 30, 30, 0x00FF2D55); 
    draw_string(win->x + 10, win->y + 8, win->title, 0x00FFFFFF, title_color);
}

void draw_desktop(unsigned int desktop_bg) {
    draw_rect(0, 0, 1024, 768, desktop_bg); 
}
void draw_taskbar() {
    // 1. Zemin: Görev çubuğunun arka planı
    draw_rect(0, 728, 1024, 40, 0x00111A);
    
    // --- YENİ: SOL BÖLGE (Hızlı Kontrol Butonları) ---
    // 1. Buton: Sol Ok (<) | X: 10, Y: 732
    draw_rect(10, 732, 35, 32, 0x00333333);
    draw_string(22, 742, "<", 0x00FFFFFF, 0x00333333);

    // 2. Buton: Seçenekler Menüsü (=) | X: 55, Y: 732
    draw_rect(55, 732, 40, 32, 0x00333333);
    draw_string(71, 742, "=", 0x00FFFFFF, 0x00333333); // İstersen "O" veya "M" de yapabilirsin

    // 3. Buton: Sağ Ok (>) | X: 105, Y: 732
    draw_rect(105, 732, 35, 32, 0x00333333);
    draw_string(117, 742, ">", 0x00FFFFFF, 0x00333333);
    // ------------------------------------------------
    
    // 2. SAĞ BÖLGE: Canlı Saat
    unsigned char h = bcd_to_bin(get_rtc_register(0x04));
    h = (h + 3) % 24; // GMT+3 Türkiye Saati
    unsigned char m = bcd_to_bin(get_rtc_register(0x02));
    
    char hs[10], ms[10];
    itoa(h, hs); itoa(m, ms);
    
    char time_str[16] = "";
    if (h < 10) strcat(time_str, "0");
    strcat(time_str, hs);
    strcat(time_str, ":");
    if (m < 10) strcat(time_str, "0");
    strcat(time_str, ms);
    
    // Saati sağ alt köşeye (X:960, Y:740) yaz
    draw_string(960, 740, time_str, 0x00FFFFFF, 0x00111A);

    // 3. ORTA BÖLGE: Açık Pencerelerin Butonları
    int btn_x = 150; // Sol tarafı (0-150) Başlat Menüsü için boş bıraktık
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (windows[i].is_open) {
            // Aktif olan pencerenin butonu ArdaOS Mavisi, diğerleri koyu gri olsun
            unsigned int btn_color = (i == focused_window) ? 0x000078D7 : 0x00333333;
            
            draw_rect(btn_x, 732, 120, 32, btn_color);
            
            // Pencere isminin ilk birkaç harfini butonun içine yaz
            char short_title[12];
            int t = 0;
            while(windows[i].title[t] != '\0' && t < 10) {
                short_title[t] = windows[i].title[t];
                t++;
            }
            short_title[t] = '\0';
            
            draw_string(btn_x + 10, 742, short_title, 0x00FFFFFF, btn_color);
            
            btn_x += 130; // Bir sonraki buton için X'i sağa kaydır
        }
    }
}
void render_gui() {
    if (!force_redraw) return;
    
    // Eğer işletim sistemi 'Full Redraw' (1) istediyse tüm ekranı kirlet
    if (force_redraw == 1) mark_screen_dirty(); 
    force_redraw = 0;
    
    draw_desktop(current_bg_color); 
    
    for (int s = 0; s < desktop_shape_count; s++) {
        draw_rect(desktop_shape_x[s], desktop_shape_y[s], desktop_shape_w[s], desktop_shape_h[s], desktop_shape_color[s]);
    }
    
    int draw_order[MAX_WINDOWS];
    int order_idx = 0;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (i != focused_window) draw_order[order_idx++] = i;
    }
    draw_order[order_idx++] = focused_window;

    for (int i = 0; i < MAX_WINDOWS; i++) {
        int w_idx = draw_order[i];
        if (!windows[w_idx].is_open) continue;
        
        draw_window(&windows[w_idx]); 
        
        // ==========================================
        // YENİ: PENCERE İÇERİĞİ İÇİN BOUNDARY CLIPPING
        // ==========================================
        // Uygulamalar şekilleri veya yazıları pencerenin dışına taşıramaz!
        set_clipping_rect(windows[w_idx].x + 2, windows[w_idx].y + 32, windows[w_idx].w - 4, windows[w_idx].h - 34);

        if (w_idx == 0) {
            for (int line = 0; line < terminal_line_count; line++) {
                draw_string(windows[0].x + 10, windows[0].y + 40 + (line * 16), terminal_lines[line], 0x00000000, 0xFFFFFFFF);
            }
            draw_string(windows[0].x + 10, windows[0].y + windows[0].h - 25, user_input, 0x000000AA, 0xFFFFFFFF); 
        } 
        else if (w_idx == 1) {
            // (Sistem Monitörü UI Kodları - BURAYI KESMEDEN AYNEN KORU)
            draw_string(windows[1].x + 10, windows[1].y + 40, "PID  DURUM   CPU%", 0x00000000, 0xFFFFFFFF);
            draw_rect(windows[1].x + 10, windows[1].y + 55, windows[1].w - 20, 2, 0x00BBBBBB);

            if (ready_queue != 0) {
                task_t* task_array[32]; int task_count = 0; task_t* curr = ready_queue;
                do { if (task_count < 32) task_array[task_count++] = curr; curr = curr->next; } while (curr != ready_queue);
                for (int m = 0; m < task_count - 1; m++) {
                    for (int j = 0; j < task_count - m - 1; j++) {
                        if (task_array[j]->id > task_array[j+1]->id) {
                            task_t* temp = task_array[j]; task_array[j] = task_array[j+1]; task_array[j+1] = temp;
                        }
                    }
                }
                int y_offset = 65;
                for (int m = 0; m < task_count && y_offset < windows[1].h - 20; m++) {
                    task_t* t = task_array[m];
                    char pid_str[4]; itoa(t->id, pid_str);
                    draw_string(windows[1].x + 10, windows[1].y + y_offset, pid_str, 0x00000000, 0xFFFFFFFF);
                    const char* state_str = (t->state == 0) ? "RUN" : "BLK";
                    unsigned int state_color = (t->state == 0) ? 0x0000AA00 : 0x00AA0000;
                    draw_string(windows[1].x + 45, windows[1].y + y_offset, state_str, state_color, 0xFFFFFFFF);
                    char usage_str[8]; itoa(t->cpu_usage, usage_str); strcat(usage_str, "%");
                    draw_string(windows[1].x + 100, windows[1].y + y_offset, usage_str, 0x00000000, 0xFFFFFFFF);
                    unsigned int bar_color = (t->cpu_usage > 50) ? ((t->cpu_usage > 80) ? 0x00FF0000 : 0x00FFAA00) : 0x0000AA00;
                    int bar_width = t->cpu_usage * 1.3; if (bar_width > 130) bar_width = 130;  
                    if (bar_width > 0) draw_rect(windows[1].x + 150, windows[1].y + y_offset, bar_width, 10, bar_color);
                    y_offset += 20;
                }
            }
        }
        else if (w_idx >= 2) {
            draw_rect(windows[w_idx].x + 2, windows[w_idx].y + 32, windows[w_idx].w - 4, windows[w_idx].h - 34, 0x00000000);
            for (int s = 0; s < windows[w_idx].shape_count; s++) {
                draw_rect(windows[w_idx].x + windows[w_idx].shape_x[s], 
                          windows[w_idx].y + 32 + windows[w_idx].shape_y[s], 
                          windows[w_idx].shape_w[s], 
                          windows[w_idx].shape_h[s], 
                          windows[w_idx].shape_color[s]);
            }
        }
        
        // Pencere içerik çizimi bitti, dışarıya taşma zırhını kaldır:
        reset_clipping_rect();
    }
    draw_taskbar();
    draw_cursor(mouse_x, mouse_y);
    swap_buffers();
}

// ==========================================
// 4. MOTOR 2: KOMUT İŞLEYİCİ (COMMAND ENGINE)
// ==========================================
// YENİ: Shell ile Kernel'in Haberleşme Deposu
char pending_command[256] = "";
volatile int command_ready = 0;

// YENİ: Sadece Uygulama Yüklemeyi Bilen Saf Çekirdek Fonksiyonu
int api_exec_app(const char* name, const char* args) {
    char raw_name[16];
    int i = 0;
    while(name[i] != '\0' && name[i] != '.' && i < 15) {
        raw_name[i] = name[i];
        i++;
    }
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
        // Diskten okunan programı yeni bir Görev (Task) olarak başlat
        int pid = create_task((void (*)())elf_load_buffer, (unsigned int)elf_load_buffer, (char*)args);
        return pid;
    }
    return -1;
}

// Shell'in ekranı temizleyebilmesi için Kernel köprüsü
void api_clear_terminal() {
    extern int terminal_line_count;
    extern volatile int force_redraw;
    terminal_line_count = 0;
    force_redraw = 1;
}
// ==========================================
// 5. MOTOR 3: GİRDİ YÖNETİCİSİ (INPUT ENGINE)
// ==========================================
void process_keyboard_events() {
    char kbd_char = get_keyboard_char();
    if (kbd_char != 0) {
        force_redraw = 1;
        
        if (focused_window >= 2 && windows[focused_window].is_open) {
            wake_task_by_id(windows[focused_window].owner_task_id);
        }
        
        if (focused_window >= 2 && windows[focused_window].is_open) {
            last_game_key = kbd_char;
        } else {
            last_game_key = 0; 
            
            if (kbd_char == '\n') { 
                terminal_print(user_input); 
                
                // YENİ: Komutu işletmek yerine Shell.elf'e (Ring 3) devret!
                strcpy(pending_command, &user_input[6]);
                command_ready = 1; 
                
                strcpy(user_input, "Arda> ");
                input_idx = 6;
            }
            else if (kbd_char == 17) { // GEÇMİŞ KOMUT: YUKARI OK 
                if (history_count > 0 && history_index > 0) {
                    history_index--;
                    strcpy(user_input, "Arda> ");
                    strcat(user_input, cmd_history[history_index % MAX_HISTORY]);
                    input_idx = 6 + strlen(cmd_history[history_index % MAX_HISTORY]);
                }
            }
            else if (kbd_char == 18) { // GEÇMİŞ KOMUT: AŞAĞI OK 
                if (history_index < history_count) {
                    history_index++;
                    if (history_index == history_count) {
                        strcpy(user_input, "Arda> ");
                        input_idx = 6;
                    } else {
                        strcpy(user_input, "Arda> ");
                        strcat(user_input, cmd_history[history_index % MAX_HISTORY]);
                        input_idx = 6 + strlen(cmd_history[history_index % MAX_HISTORY]);
                    }
                }
            }
            else if (kbd_char == '\b' && input_idx > 6) { 
                input_idx--;
                user_input[input_idx] = '\0';
            } 
            else if (input_idx < 255 && kbd_char != '\b' && kbd_char != '\n') { 
                user_input[input_idx] = kbd_char;
                input_idx++;
                user_input[input_idx] = '\0'; 
            }
        }
    }
}

// ==========================================
// 6. MOTOR 4: PENCERE VE FARE YÖNETİCİSİ (WINDOW ENGINE)
// ==========================================
void process_mouse_events() {
    int delta_x = mouse_x - last_mouse_x;
    int delta_y = mouse_y - last_mouse_y;

    if (delta_x != 0 || delta_y != 0) {
        
        // YENİ: Sadece farenin 'Eski' konumunu kirlet (Silinmesi için)
        add_dirty_rect(last_mouse_x, last_mouse_y, 16, 16);

        if (mouse_left_button) {
            if (!any_window_dragging) {
                if (mouse_y >= 728) {
                    // SOL BÖLGE - Buton 1: Önceki Uygulama (<)
                    if (mouse_x >= 10 && mouse_x <= 45) {
                        int prev_idx = focused_window;
                        for (int i = 1; i < MAX_WINDOWS; i++) {
                            // Modulo (kalan bulma) ile dizinin başına dönebilen geri sayım
                            int check = (focused_window - i + MAX_WINDOWS) % MAX_WINDOWS;
                            if (windows[check].is_open) {
                                prev_idx = check;
                                break;
                            }
                        }
                        if (prev_idx != focused_window) {
                            focused_window = prev_idx;
                            force_redraw = 1;
                        }
                    }
                    
                    // SOL BÖLGE - Buton 2: Seçenekler (=)
                    else if (mouse_x >= 55 && mouse_x <= 95) {
                        // Şimdilik test amaçlı: Aktif pencereyi anında KAPAT!
                        if (focused_window >= 0 && windows[focused_window].is_open) {
                            windows[focused_window].is_open = 0;
                            force_redraw = 1;
                            
                            // Ekran boş kalmasın, açık başka bir pencere varsa ona odaklan
                            for (int i = 0; i < MAX_WINDOWS; i++) {
                                if (windows[i].is_open) { focused_window = i; break; }
                            }
                        }
                    }
                    
                    // SOL BÖLGE - Buton 3: Sonraki Uygulama (>)
                    else if (mouse_x >= 105 && mouse_x <= 140) {
                        int next_idx = focused_window;
                        for (int i = 1; i < MAX_WINDOWS; i++) {
                            // Dizinin sonuna gelince başa dönen ileri sayım
                            int check = (focused_window + i) % MAX_WINDOWS;
                            if (windows[check].is_open) {
                                next_idx = check;
                                break;
                            }
                        }
                        if (next_idx != focused_window) {
                            focused_window = next_idx;
                            force_redraw = 1;
                        }
                    }
                    
                    // ORTA BÖLGE - Pencerelerin Kendi Butonları
                    else if (mouse_x >= 150 && mouse_x < 930) {
                        int btn_index = (mouse_x - 150) / 130;
                        int current_idx = 0;
                        for (int i = 0; i < MAX_WINDOWS; i++) {
                            if (windows[i].is_open) {
                                if (current_idx == btn_index) {
                                    if (focused_window != i) {
                                        focused_window = i; 
                                        force_redraw = 1; 
                                    }
                                    break;
                                }
                                current_idx++;
                            }
                        }
                    }
                    
                    last_mouse_x = mouse_x; last_mouse_y = mouse_y;
                    return;
                } 
                int clicked_window = -1;
                if (windows[focused_window].is_open &&
                    mouse_x >= windows[focused_window].x && mouse_x <= windows[focused_window].x + windows[focused_window].w &&
                    mouse_y >= windows[focused_window].y && mouse_y <= windows[focused_window].y + windows[focused_window].h) {
                    clicked_window = focused_window;
                } else {
                    for (int i = 0; i < MAX_WINDOWS; i++) {
                        if (windows[i].is_open && i != focused_window &&
                            mouse_x >= windows[i].x && mouse_x <= windows[i].x + windows[i].w &&
                            mouse_y >= windows[i].y && mouse_y <= windows[i].y + windows[i].h) {
                            clicked_window = i; break;
                        }
                    }
                }

                if (clicked_window != -1) {
                    if (focused_window != clicked_window) {
                        focused_window = clicked_window; 
                        force_redraw = 1; // Z-Order değiştiği için Full Redraw iste!
                    }

                    if (mouse_x >= windows[focused_window].x + windows[focused_window].w - 30 &&
                        mouse_x <= windows[focused_window].x + windows[focused_window].w &&
                        mouse_y >= windows[focused_window].y && mouse_y <= windows[focused_window].y + 30) {
                        
                        if (focused_window >= 2) {
                            task_to_kill = windows[focused_window].owner_task_id;
                        } else {
                            windows[focused_window].is_open = 0;
                        }
                        force_redraw = 1; // Ekrandan obje silindi, Full Redraw iste!
                    }
                    else if (mouse_y >= windows[focused_window].y && mouse_y <= windows[focused_window].y + 30) {
                        windows[focused_window].is_dragging = 1;
                        any_window_dragging = 1;
                    }
                }
            }

            if (any_window_dragging && windows[focused_window].is_dragging) {
                // Sürükleme başladı! Pencerenin ESKİ konumunu kirlet
                add_dirty_rect(windows[focused_window].x, windows[focused_window].y, windows[focused_window].w, windows[focused_window].h);
                
                windows[focused_window].x += delta_x;
                windows[focused_window].y += delta_y;
                
                // Pencerenin YENİ konumunu kirlet
                add_dirty_rect(windows[focused_window].x, windows[focused_window].y, windows[focused_window].w, windows[focused_window].h);
            }
        } else {
            for (int i = 0; i < MAX_WINDOWS; i++) windows[i].is_dragging = 0;
            any_window_dragging = 0;
        }
        
        // Farenin YENİ konumunu kirlet
        add_dirty_rect(mouse_x, mouse_y, 16, 16);

        // Sistemin Full Redraw'a (1) ihtiyacı yoksa, onu Fast Redraw'a (2) al
        if (force_redraw == 0) force_redraw = 2; 
        
        last_mouse_x = mouse_x; last_mouse_y = mouse_y;
    }
}

// ==========================================
// 7. ANA İŞLETİM SİSTEMİ BAŞLANGICI (KERNEL ENTRY)
// ==========================================
void kernel_main(unsigned int magic, struct multiboot_info* mb_info) {
    init_gdt(); pic_remap(); init_idt(); init_mouse(); 
    if (magic != 0x2BADB002) return; 
    if (mb_info->flags & (1 << 12)) vesa_framebuffer = (unsigned int*)(unsigned int)mb_info->framebuffer_addr;
    init_paging((unsigned int)vesa_framebuffer); 
    init_heap(); init_tasking(); create_task(background_task, 0, "");

    if (vesa_framebuffer != 0) init_graphics(vesa_framebuffer, 1024, 768);
    init_timer(100);
    init_disk();
    // YENİ: PCI Veriyolu ve Ağ Kartı Başlatıcıları
    // (Henüz dosyaları oluşturmadığımız için şimdilik yorum satırı yapıyoruz)
    //extern void init_pci(void);
    extern void init_rtl8139(void);
    //init_pci();
    init_rtl8139();
    __asm__ __volatile__ ("sti");

    windows[0].id = 0; windows[0].is_open = 1; windows[0].is_dragging = 0;
    windows[0].x = 100; windows[0].y = 100; windows[0].w = 450; windows[0].h = 350;
    strcpy(windows[0].title, "ArdaOS Terminali");
    
    windows[1].id = 1; windows[1].is_open = 1; windows[1].is_dragging = 0;
    windows[1].x = 600; windows[1].y = 150; windows[1].w = 300; windows[1].h = 200;
    strcpy(windows[1].title, "Sistem Monitoru");

    focused_window = 0; last_mouse_x = mouse_x; last_mouse_y = mouse_y;
    
    // Terminali başlat
    terminal_print("ArdaOS V0.5 Multitasking'e Hos Geldiniz!");
    // İŞLETİM SİSTEMİ AYAĞA KALKTIĞINDA İLK OLARAK SHELL'İ BAŞLAT!
    api_exec_app("shell.elf", "");
    // ==========================================
    // MULTITASKING ANA DÖNGÜSÜ
    // ==========================================
    while(1) {
        system_ticks++;
        
        // API 9'dan (Q tuşu sys_exit) gelen ölüm fermanı
        if (app_needs_to_die) {
            task_to_kill = current_task->id;
            app_needs_to_die = 0;
        }

        // KUSURSUZ CELLAT MOTORU: Hedeflenen ID'yi temizle
        if (task_to_kill != -1) {
            kill_task_by_id(task_to_kill); 
            
            for (int i = 2; i < MAX_WINDOWS; i++) {
                if (windows[i].is_open && windows[i].owner_task_id == task_to_kill) {
                    windows[i].is_open = 0;
                    if (focused_window == i) focused_window = 0;
                }
            }
            task_to_kill = -1;
            force_redraw = 1;
            strcpy(terminal_response, "[ SISTEM ] Uygulama sonlandirildi.");
        }

        int current_second = timer_ticks / 100;
        if (current_second != last_second) {
            last_second = current_second; 
            
            // --- CANLI CPU HESAPLAMA MOTORU ---
            if (ready_queue != 0) {
                task_t* curr = ready_queue;
                do {
                    // 100 Tick = %100. Sayacı direkt yüzdeye kopyala!
                    curr->cpu_usage = curr->cpu_ticks; 
                    curr->cpu_ticks = 0; // Bir sonraki saniye için sıfırla
                    
                    curr = curr->next;
                } while (curr != ready_queue);
            }
            force_redraw = 1; 
        }

        process_mouse_events();
        process_keyboard_events();
        render_gui();

        yield(); 
        __asm__ __volatile__ ("sti; hlt");
    }
}