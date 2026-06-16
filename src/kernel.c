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

unsigned int* vesa_framebuffer;
extern void outb(unsigned short port, unsigned char data);
extern unsigned char inb(unsigned short port);

// ==========================================
// 1. GLOBAL DEĞİŞKENLER VE DURUM YÖNETİMİ
// ==========================================
#define MAX_SHAPES 200
#define MAX_WINDOWS 4
int shape_count = 0;
int shape_x[MAX_SHAPES]; int shape_y[MAX_SHAPES];
int shape_w[MAX_SHAPES]; int shape_h[MAX_SHAPES];
unsigned int shape_color[MAX_SHAPES];

typedef struct {
    int id;
    int x, y, w, h;
    int is_open;
    int is_dragging;
    char title[32];
} window_t;
window_t windows[MAX_WINDOWS];

int focused_window = 0; 
int any_window_dragging = 0;
volatile int force_redraw = 0;
char last_game_key = 0;
int app_window_id = -1;
unsigned int current_app_base = 0;

int last_mouse_x = 0, last_mouse_y = 0;
unsigned int current_bg_color = 0x001B26;
char terminal_response[512] = "ArdaOS V0.2'ye Hos Geldiniz!";
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
    outb(0x70, reg);
    return inb(0x71);
}

unsigned char bcd_to_bin(unsigned char bcd) {
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}

int api_create_window(const char* title, int w, int h) {
    const char* real_title = title;
    if ((unsigned int)title < 0x100000) {
        real_title = (const char*)(current_app_base + (unsigned int)title);
    }

    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!windows[i].is_open) {
            windows[i].id = i;
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
            app_window_id = i;   
            force_redraw = 1;
            return i;            
        }
    }
    return -1; 
}

void api_add_shape(int x, int y, int w, int h, unsigned int color) {
    if (shape_count < MAX_SHAPES) {
        shape_x[shape_count] = x; shape_y[shape_count] = y;
        shape_w[shape_count] = w; shape_h[shape_count] = h;
        shape_color[shape_count] = color;
        shape_count++;
    }
    force_redraw = 1;
}

void api_clear_shapes() {
    shape_count = 0;
    force_redraw = 1;
}

volatile int app_needs_to_die = 0;
extern void kill_app_task(void);
unsigned int zombie_app_base = 0;

void background_task() {
    while(1) {
        __asm__ __volatile__("sti");
        task1_counter++; 
        yield();         
    }
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
    force_redraw = 0;
    
    draw_desktop(current_bg_color); 
    
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
        
        if (w_idx == 0) {
            draw_string_wrapped(windows[0].x + 10, windows[0].y + 40, windows[0].w - 20, terminal_response, 0x00000000, 0xFFFFFFFF); 
            draw_string_wrapped(windows[0].x + 10, windows[0].y + windows[0].h - 30, windows[0].w - 20, user_input, 0x000000AA, 0xFFFFFFFF); 
        } 
        else if (w_idx == 1) {
            char info_text[128] = "Sistem Calisma Suresi:\n";
            char sec_str[10];
            itoa(timer_ticks / 100, sec_str);
            strcat(info_text, sec_str);
            strcat(info_text, " Saniye\n\nBellek Durumu: OK\nMultitasking: Aktif");
            draw_string_wrapped(windows[1].x + 10, windows[1].y + 50, windows[1].w - 20, info_text, 0x00000000, 0xFFFFFFFF);
        }
        else if (app_window_id != -1 && w_idx == app_window_id) {
            draw_rect(windows[w_idx].x + 2, windows[w_idx].y + 32, windows[w_idx].w - 4, windows[w_idx].h - 34, 0x00000000);
            for (int s = 0; s < shape_count; s++) {
                draw_rect(windows[w_idx].x + shape_x[s], windows[w_idx].y + 32 + shape_y[s], shape_w[s], shape_h[s], shape_color[s]);
            }
        }
    }
    
    draw_cursor(mouse_x, mouse_y);
    swap_buffers();
}

// ==========================================
// 4. MOTOR 2: KOMUT İŞLEYİCİ (COMMAND ENGINE)
// ==========================================
void execute_command(char* cmd) {
    if (strcmp(cmd, "") != 0) {
        strcpy(cmd_history[history_count % MAX_HISTORY], cmd);
        history_count++;
        history_index = history_count; 
    }

    if (strcmp(cmd, "info") == 0) {
        strcpy(terminal_response, "Sistem: ArdaOS V0.2\nMimari: 32-bit x86\nCekirdek Durumu: Kararli\nGUI: Moduler");
    } 
    else if (strcmp(cmd, "temizle") == 0) {
        strcpy(terminal_response, ""); 
    } 
    else if (strcmp(cmd, "testapp") == 0) {
        unsigned char* app_memory = (unsigned char*)malloc(4096); 
        if (app_memory != 0) {
            int file_size = ardaos_read_file("TESTAPP ", "BIN", app_memory);
            if (file_size > 0) {
                current_app_base = (unsigned int)app_memory;
                void (*app_entry)() = (void (*)())app_memory;
                create_task(app_entry);
                strcpy(terminal_response, "[ BASARILI ] Uygulama yuklendi. Kendi penceresini acacak...");
            } else {
                free(app_memory);
                strcpy(terminal_response, "Hata: TESTAPP.BIN bulunamadi.");
            }
        } else {
            strcpy(terminal_response, "Hata: Yetersiz RAM!");
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
        strcpy(terminal_response, "KOMUTLAR:\n- help: Bu listeyi gosterir\n- info: Sistem detayi\n- temizle: Ekrani siler\n- renk [mavi/kirmizi]: Arkaplan\n- memorytest: RAM saglik testi\n- uptime: Calisma olay sayaci");
    }
    else if (strcmp(cmd, "ls") == 0 || strcmp(cmd, "dir") == 0) {
        ardaos_list_files(terminal_response);
    }
    else if (strcmp(cmd, "memorytest") == 0) {
        void* test_ptr = malloc(1024); 
        if (test_ptr != 0) {
            free(test_ptr); 
            strcpy(terminal_response, "[ BASARILI ]\n1 KB Heap bellegi sorunsuz ayrildi ve iade edildi.");
        } else {
            strcpy(terminal_response, "[ DIKKAT - BASARISIZ ]\nHeap uzerinde yeterli bellek kalmadi.");
        }
    }
    else if (strcmp(cmd, "uptime") == 0) {
        char sec_str[10];
        itoa(timer_ticks / 100, sec_str);
        strcpy(terminal_response, "Sistem Gercek Calisma Suresi: ");
        strcat(terminal_response, sec_str);
        strcat(terminal_response, " saniye");
    }
    else if (strcmp(cmd, "saat") == 0) {
        unsigned char h = bcd_to_bin(get_rtc_register(0x04));
        unsigned char m = bcd_to_bin(get_rtc_register(0x02));
        unsigned char s = bcd_to_bin(get_rtc_register(0x00));
        h = (h + 3) % 24;
        char hs[10], ms[10], ss[10];
        itoa(h, hs); itoa(m, ms); itoa(s, ss);
        strcpy(terminal_response, "Gercek Donanim Saati (CMOS UTC+3): ");
        strcat(terminal_response, hs); strcat(terminal_response, ":");
        strcat(terminal_response, ms); strcat(terminal_response, ":");
        strcat(terminal_response, ss);
    }
    else if (strncmp(cmd, "yanki ", 6) == 0) { 
        strcpy(terminal_response, "Sen dedin ki: ");
        strcat(terminal_response, cmd + 6); 
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
            strcpy(terminal_response, "Gecersiz islem! Ornek kullanim: hesapla 25 + 14");
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
            shape_count = 0; 
            strcpy(terminal_response, "Masaustu tuvali temizlendi!");
        }
        else if (strncmp(args, "dikdortgen ", 11) == 0) {
            if (shape_count < MAX_SHAPES) {
                int i = 11;
                while(args[i] == ' ') i++; 
                int x = atoi(&args[i]);
                
                while((args[i] >= '0' && args[i] <= '9') || args[i] == '-') i++;
                while(args[i] == ' ') i++; 
                int y = atoi(&args[i]);
                
                while((args[i] >= '0' && args[i] <= '9') || args[i] == '-') i++;
                while(args[i] == ' ') i++; 
                int w = atoi(&args[i]);
                
                while((args[i] >= '0' && args[i] <= '9') || args[i] == '-') i++;
                while(args[i] == ' ') i++; 
                int h = atoi(&args[i]);
                
                while((args[i] >= '0' && args[i] <= '9') || args[i] == '-') i++;
                while(args[i] == ' ') i++;
                
                unsigned int c = 0x00FFFFFF;
                if (strncmp(&args[i], "kirmizi", 7) == 0) c = 0x00FF2D55;
                else if (strncmp(&args[i], "yesil", 5) == 0) c = 0x0034C759;
                else if (strncmp(&args[i], "mavi", 4) == 0) c = 0x000078D7;
                else if (strncmp(&args[i], "sari", 4) == 0) c = 0x00FFCC00;

                shape_x[shape_count] = x; shape_y[shape_count] = y;
                shape_w[shape_count] = w; shape_h[shape_count] = h;
                shape_color[shape_count] = c; shape_count++;
                strcpy(terminal_response, "Sekil basariyla Ekran Listesine eklendi!");
            } else {
                strcpy(terminal_response, "Hata: Ekranda maksimum sekil sayisina ulasildi!");
            }
        } 
        else {
            strcpy(terminal_response, "Hata: 'ciz dikdortgen <x> <y> <w> <h> <renk>' kullanin.");
        }
    }
    else if (strcmp(cmd, "") == 0) { } 
    else {
        strcpy(terminal_response, "Hata: Bilinmeyen komut! 'help' yazarak komutlari gorebilirsiniz.");
    }
}

// ==========================================
// 5. MOTOR 3: GİRDİ YÖNETİCİSİ (INPUT ENGINE)
// ==========================================
void process_keyboard_events() {
    char kbd_char = get_keyboard_char();
    if (kbd_char != 0) {
        last_game_key = kbd_char;
        force_redraw = 1;
        if (!(app_window_id != -1 && windows[app_window_id].is_open && focused_window == app_window_id)) {
            if (kbd_char == '\n') { 
                char* cmd = &user_input[6]; 
                execute_command(cmd);
                strcpy(user_input, "Arda> ");
                input_idx = 6;
            } 
            else if (kbd_char == 17) { 
                if (history_count > 0 && history_index > 0) {
                    history_index--;
                    strcpy(user_input, "Arda> ");
                    strcat(user_input, cmd_history[history_index % MAX_HISTORY]);
                    input_idx = 6 + strlen(cmd_history[history_index % MAX_HISTORY]);
                }
            }
            else if (kbd_char == 18) { 
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
        force_redraw = 1;

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
                    focused_window = clicked_window; 
                    if (mouse_x >= windows[focused_window].x + windows[focused_window].w - 30 &&
                        mouse_x <= windows[focused_window].x + windows[focused_window].w &&
                        mouse_y >= windows[focused_window].y && mouse_y <= windows[focused_window].y + 30) {
                        windows[focused_window].is_open = 0; 
                    }
                    else if (mouse_y >= windows[focused_window].y && mouse_y <= windows[focused_window].y + 30) {
                        windows[focused_window].is_dragging = 1;
                        any_window_dragging = 1;
                    }
                }
            }

            if (any_window_dragging && windows[focused_window].is_dragging) {
                windows[focused_window].x += delta_x;
                windows[focused_window].y += delta_y;
            }
        } else {
            for (int i = 0; i < MAX_WINDOWS; i++) windows[i].is_dragging = 0;
            any_window_dragging = 0;
        }
        last_mouse_x = mouse_x; last_mouse_y = mouse_y;
    }
}

// ==========================================
// 7. ANA İŞLETİM SİSTEMİ BAŞLANGICI (KERNEL ENTRY)
// ==========================================
void kernel_main(unsigned int magic, struct multiboot_info* mb_info) {
    init_gdt();
    pic_remap();     
    init_idt();      
    init_mouse(); 

    if (magic != 0x2BADB002) return; 

    if (mb_info->flags & (1 << 12)) { 
        vesa_framebuffer = (unsigned int*)(unsigned int)mb_info->framebuffer_addr;
    }
    
    init_paging((unsigned int)vesa_framebuffer); 
    init_heap();
    init_tasking();               
    create_task(background_task);

    if (vesa_framebuffer != 0) {
        init_graphics(vesa_framebuffer, 1024, 768);
    }

    init_timer(100);
    __asm__ __volatile__ ("sti");

    windows[0].id = 0; windows[0].is_open = 1; windows[0].is_dragging = 0;
    windows[0].x = 100; windows[0].y = 100; windows[0].w = 450; windows[0].h = 350;
    strcpy(windows[0].title, "ArdaOS Terminali");

    windows[1].id = 1; windows[1].is_open = 1; windows[1].is_dragging = 0;
    windows[1].x = 600; windows[1].y = 150; windows[1].w = 300; windows[1].h = 200;
    strcpy(windows[1].title, "Sistem Monitoru");

    focused_window = 0;

    // Diski Test Et ve Sonucu Terminale Yaz
    char write_buffer[512] = "Merhaba Arda! Bu yazi tamamen RAM disindan, fiziksel Hard Diskten okunmustur!";
    ata_lba_write(5, 1, (unsigned short*)write_buffer);

    char read_buffer[512];
    for(int i=0; i<512; i++) read_buffer[i] = 0; 
    ata_lba_read(5, 1, (unsigned short*)read_buffer);

    strcpy(terminal_response, "ArdaOS Disk Testi Sonucu:\n[ ");
    strcat(terminal_response, read_buffer);
    strcat(terminal_response, " ]\n\n- info\n- temizle\n- hesapla");

    last_mouse_x = mouse_x; last_mouse_y = mouse_y;

    // ==========================================
    // SADECE 13 SATIRLIK KUSURSUZ ANA DÖNGÜ!
    // ==========================================
    while(1) {
        system_ticks++;
        if (app_needs_to_die || (app_window_id != -1 && windows[app_window_id].is_open == 0)) {

            kill_app_task(); // Görevi listeden tamamen kopar ve 4KB yığınını iade et

            //if (current_app_base != 0) {
            //   free((void*)current_app_base); // Uygulamanın 4KB kod belleğini iade et
            //   
            //}
            current_app_base = 0;
            if (app_window_id != -1) {
                windows[app_window_id].is_open = 0;
                app_window_id = -1;
            }

            shape_count = 0;
            focused_window = 0; // Odağı güvenlice terminale geri ver
            force_redraw = 1;
            app_needs_to_die = 0;
            strcpy(terminal_response, "[ SISTEM ] Uygulama basariyla kapatildi ve RAM temizlendi.");
        }
        int current_second = timer_ticks / 100;
        if (current_second != last_second) {
            last_second = current_second;
            force_redraw = 1; 
        }

        process_mouse_events();
        process_keyboard_events();
        render_gui();

        yield(); 
        __asm__ __volatile__ ("sti; hlt");
    }
}