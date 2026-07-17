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
    draw_rect(0, 728, 1024, 40, 0x00111A);  
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
    draw_cursor(mouse_x, mouse_y);
    swap_buffers();
}

// ==========================================
// 4. MOTOR 2: KOMUT İŞLEYİCİ (COMMAND ENGINE)
// ==========================================
void execute_command(char* cmd) {
    terminal_response[0] = '\0';
    
    // YENİ: Komutu ve parametreyi ayır (Örn: "okuyucu.bin SKOR.TXT")
    char first_word[32];
    char app_args[128] = "";
    int f_idx = 0;
    while (cmd[f_idx] != ' ' && cmd[f_idx] != '\0' && f_idx < 31) {
        first_word[f_idx] = cmd[f_idx];
        f_idx++;
    }
    first_word[f_idx] = '\0';
    if (cmd[f_idx] == ' ') {
        strcpy(app_args, &cmd[f_idx + 1]);
    }
    int fw_len = strlen(first_word);
    if (strcmp(cmd, "") != 0) {
        strcpy(cmd_history[history_count % MAX_HISTORY], cmd);
        history_count++;
        history_index = history_count; 
    }

    if (strcmp(cmd, "info") == 0) {
        strcpy(terminal_response, "Sistem: ArdaOS V0.3\nMimari: 32-bit x86\nOzel: Gercek Multitasking");
    } 
    else if (strcmp(cmd, "temizle") == 0) {
        terminal_line_count = 0; // Terminali gerçekten temizle!
        terminal_response[0] = '\0'; 
    }
    // ========================================================
    // YENİ: DONANIMSAL SES (PC SPEAKER) KOMUTLARI
    // ========================================================
    else if (strcmp(first_word, "bip") == 0) {
        beep();
        strcpy(terminal_response, "Bip sesi calindi!");
    }
    else if (strcmp(first_word, "melodi") == 0) {
        strcpy(terminal_response, "8-bit Nostalji Melodisi caliniyor...");
        terminal_print(terminal_response); // Metni hemen ekrana bas
        terminal_response[0] = '\0';       // Tamponu temizle ki iki kez basmasın
        
        // Efsanevi Super Mario Giriş Melodisi
        play_sound(659); sleep(150); nosound(); sleep(50);  // Mi
        play_sound(659); sleep(150); nosound(); sleep(150); // Mi
        play_sound(659); sleep(150); nosound(); sleep(150); // Mi
        play_sound(523); sleep(150); nosound(); sleep(50);  // Do
        play_sound(659); sleep(150); nosound(); sleep(150); // Mi
        play_sound(784); sleep(300); nosound(); sleep(300); // Sol (İnce)
        play_sound(392); sleep(300); nosound(); sleep(300); // Sol (Kalın)
    }
    // ========================================================
    // DİNAMİK UYGULAMA YÜKLEYİCİ (DYNAMIC EXECUTION ENGINE)
    // ========================================================
    // Eğer girilen komutun sonu ".bin" veya ".BIN" ile bitiyorsa
    // ========================================================
    // DİNAMİK UYGULAMA YÜKLEYİCİ (Parametre Destekli)
    // ========================================================
    // İlk kelime ".bin" veya ".BIN" ile bitiyorsa
    // DİNAMİK UYGULAMA YÜKLEYİCİ (Parametre ve ELF Destekli)
    else if (fw_len > 4 && 
            (strcmp(first_word + fw_len - 4, ".bin") == 0 || strcmp(first_word + fw_len - 4, ".BIN") == 0 ||
             strcmp(first_word + fw_len - 4, ".elf") == 0 || strcmp(first_word + fw_len - 4, ".ELF") == 0)) {
        
        char raw_name[16];
        strcpy(raw_name, first_word);
        raw_name[fw_len - 4] = '\0'; 
        
        char target_ext[4];
        if (first_word[fw_len - 1] == 'n' || first_word[fw_len - 1] == 'N') strcpy(target_ext, "BIN");
        else strcpy(target_ext, "ELF"); // FAT16 için doğru uzantıyı belirle

        char fat_name[9] = "        "; 
        for(int i = 0; i < 8 && raw_name[i] != '\0'; i++) {
            fat_name[i] = raw_name[i];
            if(fat_name[i] >= 'a' && fat_name[i] <= 'z') fat_name[i] -= 32;
        }
        fat_name[8] = '\0';

        // ELF dosyaları blok hizalamaları yüzünden 4 KB'ı aşabilir, limiti yükselttik
        unsigned char* app_memory = (unsigned char*)malloc(16384); 
        if (app_memory != 0) {
            int file_size = ardaos_read_file(fat_name, target_ext, app_memory);
            if (file_size > 0) {
                // create_task artık arka planda CR3'ü büküp kendi ELF kontrolünü yapacak!
                create_task((void (*)())app_memory, (unsigned int)app_memory, app_args);
                
                strcpy(terminal_response, "[ SISTEM ] ");
                strcat(terminal_response, raw_name);
                if (app_args[0] != '\0') strcat(terminal_response, " argumanlarla baslatildi.");
                else strcat(terminal_response, " baslatildi.");
            } else {
                free(app_memory);
                strcpy(terminal_response, "Hata: ");
                strcat(terminal_response, first_word);
                strcat(terminal_response, " diskte bulunamadi.");
            }
        } else {
            strcpy(terminal_response, "Hata: Uygulama baslatmak icin yetersiz RAM!");
        }
    }
    else if (strcmp(cmd, "renk mavi") == 0) {
        current_bg_color = 0x000000AA;
        strcpy(terminal_response, "Masaustu rengi mavi olarak degistirildi.");
    } 
    else if (strcmp(cmd, "renk kirmizi") == 0) {
        current_bg_color = 0x00AA0000;
        strcpy(terminal_response, "Masaustu rengi kirmizi olarak degistirildi.");
    } 
    else if (strcmp(cmd, "help") == 0) {
        strcpy(terminal_response, 
            "--- ARDAOS KOMUT LISTESI ---\n"
            "[ SISTEM ] info, help, temizle, saat, uptime, ram, memorytest\n"
            "[ GOREV  ] ps (Gorevleri listele), kill <PID>, <uygulama>.bin [arg]\n"
            "[ DISK   ] ls (veya dir), yaz <DOSYA.UZT> <metin>, rm <DOSYA.UZT>, mkdir <AD>\n"
            "[ GRAFIK ] renk <mavi/kirmizi>, ciz dikdortgen <x y w h rnk>, ciz temizle\n"
            "[ DIGER  ] hesapla <a+b>, yanki <mesaj>, bip, melodi"
        );
    }
    // ========================================================
    // YENİ: GÖREV YÖNETİCİSİ KOMUTLARI
    // ========================================================
    else if (strcmp(cmd, "ps") == 0) {
        // task.c'deki fonksiyonu çağırıp sonucu terminale yazdır
        get_process_list(terminal_response);
    }
    else if (strncmp(cmd, "kill ", 5) == 0) {
        // "kill 3" yazıldığında 3 rakamını ayrıştır
        int i = 5;
        while(cmd[i] == ' ') i++;
        int target_pid = atoi(&cmd[i]);
        
        // PID 0 (Kernel) ve PID 1 (Arka Plan Sayacı) sistemin kalbidir, dokunulamaz!
        if (target_pid == 0 || target_pid == 1) {
            strcpy(terminal_response, "[ HATA ] KERNEL veya SYSTEM gorevleri oldurulemez!");
        } else {
            // Ana döngüdeki "Cellat Motoru"na hedefi bildir!
            task_to_kill = target_pid; 
            
            strcpy(terminal_response, "[ SISTEM ] Kill sinyali gonderildi: PID ");
            char pid_str[10];
            itoa(target_pid, pid_str);
            strcat(terminal_response, pid_str);
        }
    }
    else if (strcmp(cmd, "ls") == 0 || strcmp(cmd, "dir") == 0) {
        ardaos_list_files(terminal_response);
    }
    else if (strcmp(cmd, "memorytest") == 0) {
        void* test_ptr = malloc(1024); 
        if (test_ptr != 0) {
            free(test_ptr); 
            strcpy(terminal_response, "[ BASARILI ] 1 KB Heap bellegi iade edildi.");
        } else {
            strcpy(terminal_response, "[ HATA ] Yetersiz Heap bellegi.");
        }
    }
    else if (strcmp(cmd, "ram") == 0) {
        char mem_str[16];
        itoa(total_used_memory, mem_str);
        strcpy(terminal_response, "[ SISTEM RAM ] Kullanilan: ");
        strcat(terminal_response, mem_str);
        strcat(terminal_response, " Bayt / 5242880 Bayt (5 MB)");
    }
    else if (strcmp(cmd, "uptime") == 0) {
        char sec_str[10];
        itoa(timer_ticks / 100, sec_str);
        strcpy(terminal_response, "Sistem Gercek Calisma Suresi: ");
        strcat(terminal_response, sec_str);
        strcat(terminal_response, " saniye");
    }
    // ========================================================
    // YENİ: DOSYA YAZMA (TOUCH / ECHO >)
    // ========================================================
    else if (strcmp(first_word, "yaz") == 0) {
        if (app_args[0] == '\0') {
            strcpy(terminal_response, "Hata: Kullanim -> yaz DOSYA.TXT Icerik metni...");
        } else {
            char fat_name[9] = "        ";
            char fat_ext[4] = "   ";
            char file_content[512] = {0}; // Maksimum 1 sektörlük (512 bayt) metin

            int i = 0, k = 0;
            
            // 1. Dosya adını 8 karakter FAT formatına çevir
            while (app_args[i] != '.' && app_args[i] != ' ' && app_args[i] != '\0' && k < 8) {
                char c = app_args[i++];
                if (c >= 'a' && c <= 'z') c -= 32;
                fat_name[k++] = c;
            }
            
            // 2. Noktayı atla ve uzantıyı 3 karakter FAT formatına çevir
            if (app_args[i] == '.') {
                i++; k = 0;
                while (app_args[i] != ' ' && app_args[i] != '\0' && k < 3) {
                    char c = app_args[i++];
                    if (c >= 'a' && c <= 'z') c -= 32;
                    fat_ext[k++] = c;
                }
            }

            // 3. Dosya adından sonraki boşlukları atla (İçeriğe geçiş)
            while (app_args[i] == ' ') i++;

            // 4. Geri kalan tüm metni dosya içeriği olarak kopyala
            int c_idx = 0;
            while (app_args[i] != '\0' && c_idx < 511) {
                file_content[c_idx++] = app_args[i++];
            }
            file_content[c_idx] = '\0'; // Metnin sonunu belirle

            // 5. İçerik boş mu kontrol et, değilse diske fırlat!
            if (c_idx == 0) {
                 strcpy(terminal_response, "Hata: Dosyaya yazilacak icerik bos olamaz!");
            } else {
                 if (ardaos_write_file(fat_name, fat_ext, c_idx, (unsigned char*)file_content) == 0) {
                     strcpy(terminal_response, "[ BASARILI ] Dosya diske yazildi.");
                 } else {
                     strcpy(terminal_response, "[ HATA ] Dosya diske yazilamadi (Disk dolu olabilir).");
                 }
            }
        }
    }
    // ========================================================
    // YENİ: DOSYA SİLME (RM) VE KLASÖR AÇMA (MKDIR)
    // ========================================================
    else if (strcmp(first_word, "rm") == 0) {
        if (app_args[0] == '\0') {
            strcpy(terminal_response, "Hata: Silinecek dosyayi belirtin (Orn: rm SKOR.TXT)");
        } else {
            char fat_name[9] = "        "; 
            char fat_ext[4] = "   ";
            int i = 0, k = 0;
            // Dosya adını 8 karakter FAT formatına çevir
            while (app_args[i] != '.' && app_args[i] != '\0' && k < 8) {
                char c = app_args[i++];
                if (c >= 'a' && c <= 'z') c -= 32;
                fat_name[k++] = c;
            }
            if (app_args[i] == '.') {
                i++; k = 0;
                // Uzantıyı 3 karakter FAT formatına çevir
                while (app_args[i] != '\0' && k < 3) {
                    char c = app_args[i++];
                    if (c >= 'a' && c <= 'z') c -= 32;
                    fat_ext[k++] = c;
                }
            }
            fat_name[8] = '\0'; fat_ext[3] = '\0';
            
            if (ardaos_delete_file(fat_name, fat_ext) == 0) {
                strcpy(terminal_response, "[ BASARILI ] Dosya diskten kalici olarak silindi.");
            } else {
                strcpy(terminal_response, "[ HATA ] Dosya diskte bulunamadi!");
            }
        }
    }
    else if (strcmp(first_word, "mkdir") == 0) {
        if (app_args[0] == '\0') {
            strcpy(terminal_response, "Hata: Klasor adini belirtin (Orn: mkdir OYUNLAR)");
        } else {
            char fat_name[9] = "        ";
            int i = 0;
            // Klasör adını 8 karakter FAT formatına çevir
            while(app_args[i] != ' ' && app_args[i] != '\0' && i < 8) {
                char c = app_args[i];
                if (c >= 'a' && c <= 'z') c -= 32;
                fat_name[i] = c;
                i++;
            }
            fat_name[8] = '\0';
            
            if (ardaos_create_dir(fat_name) == 0) {
                strcpy(terminal_response, "[ BASARILI ] Klasor diskte olusturuldu.");
            } else {
                strcpy(terminal_response, "[ HATA ] Klasor olusturulamadi (Disk dolu).");
            }
        }
    }
    else if (strcmp(cmd, "saat") == 0) {
        unsigned char h = bcd_to_bin(get_rtc_register(0x04));
        unsigned char m = bcd_to_bin(get_rtc_register(0x02));
        unsigned char s = bcd_to_bin(get_rtc_register(0x00));
        h = (h + 3) % 24;
        char hs[10], ms[10], ss[10];
        itoa(h, hs); itoa(m, ms); itoa(s, ss);
        strcpy(terminal_response, "Gercek Donanim Saati: ");
        strcat(terminal_response, hs); strcat(terminal_response, ":");
        strcat(terminal_response, ms); strcat(terminal_response, ":");
        strcat(terminal_response, ss);
    }
    else if (strncmp(cmd, "yanki ", 6) == 0) { 
        strcpy(terminal_response, "Sen dedin ki: ");
        strcat(terminal_response, cmd + 6); 
    }
    else if (strcmp(first_word, "ping") == 0) {
        extern int arp_resolved;
        
        // Eğer MAC adresini henüz bilmiyorsak (arp_resolved == 0)
        if (arp_resolved == 0) {
            extern void rtl8139_send_arp(void);
            rtl8139_send_arp();
            strcpy(terminal_response, "[ SISTEM ] MAC adresi bilinmiyor. ARP Istegi atildi.");
        } 
        // Eğer MAC adresini biliyorsak, GERÇEK İNTERNET PING'ini fırlat!
        else {
            extern void rtl8139_send_ping(void);
            rtl8139_send_ping();
            strcpy(terminal_response, "[ SISTEM ] GERCEK ICMP PING PAKETI FIRLATILDI!");
        }
    }
    else if (strncmp(cmd, "hesapla ", 8) == 0) {
        int i = 8; 
        while(cmd[i] == ' ') i++;
        int num1 = atoi(&cmd[i]);
        while((cmd[i] >= '0' && cmd[i] <= '9') || cmd[i] == '-') i++;
        while(cmd[i] == ' ') i++; 
        char op = cmd[i];
        i++;
        while(cmd[i] == ' ') i++;
        int num2 = atoi(&cmd[i]);

        int result = 0; int valid = 1;
        if (op == '+') result = num1 + num2;
        else if (op == '-') result = num1 - num2;
        else if (op == '*') result = num1 * num2;
        else if (op == '/') {
            if (num2 == 0) { valid = 0; strcpy(terminal_response, "Hata: Sifira bolme yapilamaz!"); }
            else result = num1 / num2;
        } else {
            valid = 0;
            strcpy(terminal_response, "Gecersiz islem! Ornek: hesapla 25 + 14");
        }

        if (valid) {
            char res_str[32];
            itoa(result, res_str);
            strcpy(terminal_response, "Islem Sonucu: ");
            strcat(terminal_response, res_str);
        }
    }
    else if (strncmp(cmd, "ciz ", 4) == 0) {
        char* args = cmd + 4; 
        if (strncmp(args, "temizle", 7) == 0) {
            desktop_shape_count = 0; 
            strcpy(terminal_response, "Masaustu tuvali temizlendi!");
        }
        else if (strncmp(args, "dikdortgen ", 11) == 0) {
            if (desktop_shape_count < MAX_DESKTOP_SHAPES) {
                int i = 11;
                while(args[i] == ' ') i++; int x = atoi(&args[i]);
                while((args[i] >= '0' && args[i] <= '9') || args[i] == '-') i++;
                while(args[i] == ' ') i++; int y = atoi(&args[i]);
                while((args[i] >= '0' && args[i] <= '9') || args[i] == '-') i++;
                while(args[i] == ' ') i++; int w = atoi(&args[i]);
                while((args[i] >= '0' && args[i] <= '9') || args[i] == '-') i++;
                while(args[i] == ' ') i++; int h = atoi(&args[i]);
                while((args[i] >= '0' && args[i] <= '9') || args[i] == '-') i++;
                while(args[i] == ' ') i++;
                
                unsigned int c = 0x00FFFFFF;
                if (strncmp(&args[i], "kirmizi", 7) == 0) c = 0x00FF2D55;
                else if (strncmp(&args[i], "yesil", 5) == 0) c = 0x0034C759;
                else if (strncmp(&args[i], "mavi", 4) == 0) c = 0x000078D7;
                else if (strncmp(&args[i], "sari", 4) == 0) c = 0x00FFCC00;

                desktop_shape_x[desktop_shape_count] = x; desktop_shape_y[desktop_shape_count] = y;
                desktop_shape_w[desktop_shape_count] = w; desktop_shape_h[desktop_shape_count] = h;
                desktop_shape_color[desktop_shape_count] = c; desktop_shape_count++;
                strcpy(terminal_response, "Masaustu Sekli Eklendi!");
            }
        } 
    }
    else if (strcmp(cmd, "") == 0) { } 
    else {
        strcpy(terminal_response, "Hata: Bilinmeyen komut! 'help' yazarak komutlari gorebilirsiniz.");
    }
    if (terminal_response[0] != '\0') {
        terminal_print(terminal_response);
    }
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
                terminal_print(user_input); // YENİ: Yazdığımız komutu ekranda kaydır
                
                char* cmd = &user_input[6]; 
                execute_command(cmd);
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