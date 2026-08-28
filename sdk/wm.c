#include "ardaos.h"
#include "libc.h"
#include "../src/font.h"
#include "../src/cursor.h"

gui_state_t* gui;

unsigned int* framebuffer;
unsigned int* back_buffer;
int screen_width = 1024, screen_height = 768;

int clip_min_x = 0, clip_min_y = 0, clip_max_x = 1024, clip_max_y = 768;
int dirty_min_x = 0, dirty_min_y = 0, dirty_max_x = 1024, dirty_max_y = 768;
int last_mouse_x = 0, last_mouse_y = 0;
int mouse_x = 0, mouse_y = 0, mouse_left_button = 0;
int blink_counter = 0; 

// Derleyici için fonksiyon tanıtımları
void draw_rect(int x, int y, int width, int height, unsigned int color);
void draw_string(int x, int y, const char* str, unsigned int fg_color, unsigned int bg_color);

// ========================================================
// 16-BİT DUVAR KAĞIDI VE DEDEKTİF (DEBUG) MOTORU
// ========================================================
unsigned short* wallpaper_buffer = (unsigned short*)0xE10000; 
int wallpaper_loaded = 0; 

// Canlı Hata Ayıklama (Debug) Değişkenleri
int dbg_fd = -1;
int dbg_read = -1;
int dbg_w = 0;
int dbg_h = 0;
short dbg_bpp = 0;
char dbg_sig[3] = "??";

void load_wallpaper() {
    dbg_fd = sys_open("ARKA    ", "BMP");
    if (dbg_fd < 0) return;

    // Yeni RAM istemek yok! Geçici olarak back_buffer'ı tepsi gibi kullanıyoruz.
    unsigned char* raw_bmp = (unsigned char*)back_buffer; 
    dbg_read = sys_read(dbg_fd, raw_bmp, 2360000);
    sys_close(dbg_fd);

    // Eğer donanımdan en azından başlık kısmı (54 bayt) geldiyse değerleri çek
    if (dbg_read > 54) {
        dbg_sig[0] = raw_bmp[0];
        dbg_sig[1] = raw_bmp[1];
        dbg_sig[2] = '\0';
        dbg_w = *(int*)&raw_bmp[18];
        dbg_h = *(int*)&raw_bmp[22];
        dbg_bpp = *(short*)&raw_bmp[28];

        if (raw_bmp[0] == 'B' && raw_bmp[1] == 'M' && dbg_bpp >= 24 && dbg_w == 1024) {
            int abs_h = (dbg_h < 0) ? -dbg_h : dbg_h;
            int row_padded = (dbg_w * (dbg_bpp / 8) + 3) & (~3);

            for (int y = 0; y < abs_h && y < 768; y++) {
                int bmp_y = (dbg_h > 0) ? (abs_h - 1 - y) : y; 
                for (int x = 0; x < dbg_w; x++) {
                    int p = 54 + (bmp_y * row_padded) + (x * (dbg_bpp / 8));
                    unsigned char b = raw_bmp[p];
                    unsigned char g = raw_bmp[p+1];
                    unsigned char r = raw_bmp[p+2];
                    wallpaper_buffer[y * 1024 + x] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
                }
            }
            wallpaper_loaded = 1;
        }
    }
    
    // Geçici tepsiyi temizle ki ekranda çöp pikseller kalmasın
    for(int i = 0; i < 1024 * 768; i++) back_buffer[i] = gui->current_bg_color;
}

void draw_wallpaper() {
    if (!wallpaper_loaded) {
        draw_rect(0, 0, 1024, 768, gui->current_bg_color);
        return;
    }
    
    int start_x = (dirty_min_x > 0) ? dirty_min_x : 0; 
    int start_y = (dirty_min_y > 0) ? dirty_min_y : 0;
    int end_x = (dirty_max_x < screen_width) ? dirty_max_x : screen_width - 1; 
    int end_y = (dirty_max_y < screen_height) ? dirty_max_y : screen_height - 1;
    if (start_x > end_x || start_y > end_y) return; 

    for (int i = start_y; i <= end_y; i++) {
        for (int j = start_x; j <= end_x; j++) {
            unsigned short c16 = wallpaper_buffer[i * 1024 + j];
            unsigned int r = (c16 >> 8) & 0xF8;
            unsigned int g = (c16 >> 3) & 0xFC;
            unsigned int b = (c16 << 3) & 0xF8;
            back_buffer[i * screen_width + j] = (r << 16) | (g << 8) | b;
        }
    }
}

// ========================================================
// ÇİZİM VE KESME (CLIPPING) MOTORLARI
// ========================================================
void set_clipping_rect(int x, int y, int w, int h) {
    clip_min_x = (x < 0) ? 0 : x; clip_min_y = (y < 0) ? 0 : y;
    clip_max_x = (x + w - 1 >= screen_width) ? screen_width - 1 : x + w - 1;
    clip_max_y = (y + h - 1 >= screen_height) ? screen_height - 1 : y + h - 1;
}
void reset_clipping_rect() {
    clip_min_x = 0; clip_min_y = 0;
    clip_max_x = screen_width - 1; clip_max_y = screen_height - 1;
}
void add_dirty_rect(int x, int y, int w, int h) {
    if (x < dirty_min_x) dirty_min_x = (x < 0) ? 0 : x;
    if (y < dirty_min_y) dirty_min_y = (y < 0) ? 0 : y;
    if (x + w - 1 > dirty_max_x) dirty_max_x = (x + w - 1 >= screen_width) ? screen_width - 1 : x + w - 1;
    if (y + h - 1 > dirty_max_y) dirty_max_y = (y + h - 1 >= screen_height) ? screen_height - 1 : y + h - 1;
}
void mark_screen_dirty() {
    dirty_min_x = 0; dirty_min_y = 0;
    dirty_max_x = screen_width - 1; dirty_max_y = screen_height - 1;
}
void put_pixel(int x, int y, unsigned int color) {
    if (x < clip_min_x || x > clip_max_x || y < clip_min_y || y > clip_max_y) return;
    if (x < dirty_min_x || x > dirty_max_x || y < dirty_min_y || y > dirty_max_y) return;
    back_buffer[y * screen_width + x] = color; 
}
void swap_buffers(void) {
    int s_min_x = (dirty_min_x < 0) ? 0 : dirty_min_x;
    int s_min_y = (dirty_min_y < 0) ? 0 : dirty_min_y;
    int s_max_x = (dirty_max_x >= screen_width) ? screen_width - 1 : dirty_max_x;
    int s_max_y = (dirty_max_y >= screen_height) ? screen_height - 1 : dirty_max_y;
    if (s_min_x > s_max_x || s_min_y > s_max_y) return; 

    for (int y = s_min_y; y <= s_max_y; y++) {
        for (int x = s_min_x; x <= s_max_x; x++) {
            framebuffer[y * screen_width + x] = back_buffer[y * screen_width + x];
        }
    }
    dirty_min_x = screen_width; dirty_min_y = screen_height;
    dirty_max_x = 0; dirty_max_y = 0;
}
void draw_rect(int x, int y, int width, int height, unsigned int color) {
    int start_x = (x > clip_min_x) ? x : clip_min_x; int start_y = (y > clip_min_y) ? y : clip_min_y;
    int end_x = (x + width - 1 < clip_max_x) ? x + width - 1 : clip_max_x; int end_y = (y + height - 1 < clip_max_y) ? y + height - 1 : clip_max_y;
    start_x = (start_x > dirty_min_x) ? start_x : dirty_min_x; start_y = (start_y > dirty_min_y) ? start_y : dirty_min_y;
    end_x = (end_x < dirty_max_x) ? end_x : dirty_max_x; end_y = (end_y < dirty_max_y) ? end_y : dirty_max_y;
    if (start_x > end_x || start_y > end_y) return; 

    for (int i = start_y; i <= end_y; i++) {
        for (int j = start_x; j <= end_x; j++) {
            back_buffer[i * screen_width + j] = color;
        }
    }
}
void draw_char(int x, int y, char c, unsigned int fg_color, unsigned int bg_color) {
    if ((unsigned char)c > 127) c = ' ';
    const unsigned char* glyph = font8x8[(int)c];
    for (int row = 0; row < 8; row++) {
        unsigned char line = glyph[row];
        for (int col = 0; col < 8; col++) {
            if ((line >> (7 - col)) & 1) put_pixel(x + col, y + row, fg_color);
            else if (bg_color != 0xFFFFFFFF) put_pixel(x + col, y + row, bg_color);
        }
    }
}
void draw_string(int x, int y, const char* str, unsigned int fg_color, unsigned int bg_color) {
    int start_x = x; 
    while (*str) {
        if (*str == '\n') { x = start_x; y += 12; } 
        else { draw_char(x, y, *str, fg_color, bg_color); x += 8; }
        str++;
    }
}
void draw_cursor(int x, int y) {
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            unsigned char pt = arrow_cursor[row][col];
            if (pt == 1) put_pixel(x + col, y + row, 0x00000000); 
            else if (pt == 2) put_pixel(x + col, y + row, 0x00FFFFFF); 
        }
    }
}

// ========================================================
// KULLANICI ARAYÜZÜ (GUI) BİLEŞENLERİ
// ========================================================
void draw_window(window_t* win) {
    if (!win->is_open) return;
    draw_rect(win->x, win->y, win->w, win->h, 0x00F0F0F0); 
    unsigned int title_color = (win->id == gui->focused_window) ? 0x000078D7 : 0x00777777;
    draw_rect(win->x, win->y, win->w, 30, title_color); 
    draw_rect(win->x + win->w - 30, win->y, 30, 30, 0x00FF2D55); 
    draw_string(win->x + 10, win->y + 8, win->title, 0x00FFFFFF, title_color);
}
void draw_taskbar() {
    draw_rect(0, 728, 1024, 40, 0x00111A);
    draw_rect(10, 732, 35, 32, 0x00333333); draw_string(22, 742, "<", 0x00FFFFFF, 0x00333333);
    draw_rect(55, 732, 40, 32, 0x00333333); draw_string(71, 742, "=", 0x00FFFFFF, 0x00333333);
    draw_rect(105, 732, 35, 32, 0x00333333); draw_string(117, 742, ">", 0x00FFFFFF, 0x00333333);
    // YENİ: Ağ Aktivite Ledi (Saatin Yanında)
    if (gui->net_activity_timer > 0) {
        // Veri akışı varsa Parlak Yeşil LED yanar
        draw_rect(935, 742, 10, 10, 0x0000FF00); 
        gui->net_activity_timer--;
        gui->force_redraw = 1; // Yanıp sönme animasyonunu akıcı tutmak için ekranı yenilemeye zorla
    } else {
        // Boştayken Sönük Gri LED
        draw_rect(935, 742, 10, 10, 0x00333333); 
    }

    int h, m;
    sys_get_time(&h, &m);
    char hs[10], ms[10]; itoa(h, hs); itoa(m, ms);
    char time_str[16] = "";
    if (h < 10) strcat(time_str, "0"); strcat(time_str, hs); strcat(time_str, ":");
    if (m < 10) strcat(time_str, "0"); strcat(time_str, ms);
    draw_string(960, 740, time_str, 0x00FFFFFF, 0x00111A);

    int btn_x = 150; 
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (gui->windows[i].is_open) {
            unsigned int btn_color = (i == gui->focused_window) ? 0x000078D7 : 0x00333333;
            draw_rect(btn_x, 732, 120, 32, btn_color);
            char short_title[12]; int t = 0;
            while(gui->windows[i].title[t] != '\0' && t < 10) { short_title[t] = gui->windows[i].title[t]; t++; }
            short_title[t] = '\0';
            draw_string(btn_x + 10, 742, short_title, 0x00FFFFFF, btn_color);
            btn_x += 130; 
        }
    }
}
void draw_context_menu() {
    if (!gui->context_menu_open || gui->focused_window < 0 || !gui->windows[gui->focused_window].is_open) return;
    int menu_x = 55, menu_y = 728 - 90, menu_w = 120;
    draw_rect(menu_x, menu_y, menu_w, 90, 0x00222222);
    unsigned int c0 = (gui->context_menu_hover_idx == 0) ? 0x00555555 : 0x00222222;
    draw_rect(menu_x, menu_y, menu_w, 30, c0); draw_string(menu_x + 10, menu_y + 8, "Kucult", 0x00FFFFFF, c0);
    unsigned int c1 = (gui->context_menu_hover_idx == 1) ? 0x00555555 : 0x00222222;
    draw_rect(menu_x, menu_y + 30, menu_w, 30, c1); draw_string(menu_x + 10, menu_y + 38, "Tam Ekran", 0x00FFFFFF, c1);
    unsigned int c2 = (gui->context_menu_hover_idx == 2) ? 0x00FF2D55 : 0x00222222; 
    draw_rect(menu_x, menu_y + 60, menu_w, 30, c2); draw_string(menu_x + 10, menu_y + 68, "Kapat", 0x00FFFFFF, c2);
}

// ========================================================
// FARE VE RENDER DÖNGÜLERİ (ANA MOTOR)
// ========================================================
void process_mouse_events() {
    sys_get_mouse(&mouse_x, &mouse_y, &mouse_left_button);
    static int is_selecting = 0;
    int delta_x = mouse_x - last_mouse_x; int delta_y = mouse_y - last_mouse_y;

    if (delta_x != 0 || delta_y != 0 || mouse_left_button) {
        add_dirty_rect(last_mouse_x, last_mouse_y, 16, 16);

        if (gui->context_menu_open) {
            int old_hover = gui->context_menu_hover_idx;
            if (mouse_x >= 55 && mouse_x <= 175 && mouse_y >= 638 && mouse_y < 728) gui->context_menu_hover_idx = (mouse_y - 638) / 30; 
            else gui->context_menu_hover_idx = -1;
            if (old_hover != gui->context_menu_hover_idx) gui->force_redraw = 1;
        }

        if (mouse_left_button) {
            if (gui->focused_window >= 2 && !gui->any_window_dragging && !gui->context_menu_open) {
                int w_idx = gui->focused_window;
                int txt_x = gui->windows[w_idx].x + 10;
                int txt_y = gui->windows[w_idx].y + 40;
                // Buton isabet (Hit) kontrolü
                for (int b = 0; b < gui->windows[w_idx].button_count; b++) {
                    int bx = gui->windows[w_idx].x + gui->windows[w_idx].button_x[b];
                    int by = gui->windows[w_idx].y + 32 + gui->windows[w_idx].button_y[b];
                    if (mouse_x >= bx && mouse_x <= bx + gui->windows[w_idx].button_w[b] &&
                        mouse_y >= by && mouse_y <= by + gui->windows[w_idx].button_h[b]) {
                        gui->windows[w_idx].button_state[b] = 1; // Butonu göçert
                        gui->force_redraw = 1;
                    }
                }
                // Fare pencerelerin metin alanının içindeyse
                if (mouse_x >= txt_x && mouse_y >= txt_y && 
                    mouse_x <= gui->windows[w_idx].x + gui->windows[w_idx].w - 10 &&
                    mouse_y <= gui->windows[w_idx].y + gui->windows[w_idx].h - 10) {
                    
                    int col = (mouse_x - txt_x) / 8;
                    int row = (mouse_y - txt_y) / 16;
                    int chars_per_line = (gui->windows[w_idx].w - 20) / 8;
                    int index = (row * chars_per_line) + col;
                    
                    if (!is_selecting) { gui->windows[w_idx].sel_start = index; is_selecting = 1; }
                    gui->windows[w_idx].sel_end = index;
                    gui->force_redraw = 1;
                }
            }
            if (!gui->any_window_dragging) {
                if (gui->context_menu_open) {
                    if (mouse_x >= 55 && mouse_x <= 175 && mouse_y >= 638 && mouse_y < 728) {
                        int action = (mouse_y - 638) / 30;
                        if (action == 0) {
                            gui->windows[gui->focused_window].is_minimized = 1;
                            for (int i = 0; i < MAX_WINDOWS; i++) { if (gui->windows[i].is_open && !gui->windows[i].is_minimized) { gui->focused_window = i; break; } }
                        } 
                        else if (action == 1) {
                            if (gui->windows[gui->focused_window].w == 1024) {
                                gui->windows[gui->focused_window].x = gui->windows[gui->focused_window].prev_x; gui->windows[gui->focused_window].y = gui->windows[gui->focused_window].prev_y;
                                gui->windows[gui->focused_window].w = gui->windows[gui->focused_window].prev_w; gui->windows[gui->focused_window].h = gui->windows[gui->focused_window].prev_h;
                            } else {
                                gui->windows[gui->focused_window].prev_x = gui->windows[gui->focused_window].x; gui->windows[gui->focused_window].prev_y = gui->windows[gui->focused_window].y;
                                gui->windows[gui->focused_window].prev_w = gui->windows[gui->focused_window].w; gui->windows[gui->focused_window].prev_h = gui->windows[gui->focused_window].h;
                                gui->windows[gui->focused_window].x = 0; gui->windows[gui->focused_window].y = 0;
                                gui->windows[gui->focused_window].w = 1024; gui->windows[gui->focused_window].h = 728; 
                            }
                        }
                        else if (action == 2) {
                            sys_kill(gui->windows[gui->focused_window].owner_task_id); 
                            gui->windows[gui->focused_window].is_open = 0; 
                            for (int i = 0; i < MAX_WINDOWS; i++) { if (gui->windows[i].is_open && !gui->windows[i].is_minimized) { gui->focused_window = i; break; } }
                        }
                    }
                    gui->context_menu_open = 0; gui->force_redraw = 1; last_mouse_x = mouse_x; last_mouse_y = mouse_y;
                    return; 
                }

                if (mouse_y >= 728) {
                    if (mouse_x >= 10 && mouse_x <= 45) {
                        int prev_idx = gui->focused_window;
                        for (int i = 1; i < MAX_WINDOWS; i++) {
                            int check = (gui->focused_window - i + MAX_WINDOWS) % MAX_WINDOWS;
                            if (gui->windows[check].is_open) { prev_idx = check; break; }
                        }
                        if (prev_idx != gui->focused_window) { gui->windows[prev_idx].is_minimized = 0; gui->focused_window = prev_idx; gui->force_redraw = 1; }
                    }
                    else if (mouse_x >= 55 && mouse_x <= 95) { if (gui->focused_window >= 0 && gui->windows[gui->focused_window].is_open) { gui->context_menu_open = 1; gui->force_redraw = 1; } }
                    else if (mouse_x >= 105 && mouse_x <= 140) {
                        int next_idx = gui->focused_window;
                        for (int i = 1; i < MAX_WINDOWS; i++) {
                            int check = (gui->focused_window + i) % MAX_WINDOWS;
                            if (gui->windows[check].is_open) { next_idx = check; break; }
                        }
                        if (next_idx != gui->focused_window) { gui->windows[next_idx].is_minimized = 0; gui->focused_window = next_idx; gui->force_redraw = 1; }
                    }
                    else if (mouse_x >= 150 && mouse_x < 930) {
                        int btn_index = (mouse_x - 150) / 130; int current_idx = 0;
                        for (int i = 0; i < MAX_WINDOWS; i++) {
                            if (gui->windows[i].is_open) {
                                if (current_idx == btn_index) { gui->windows[i].is_minimized = 0; if (gui->focused_window != i) gui->focused_window = i; gui->force_redraw = 1; break; }
                                current_idx++;
                            }
                        }
                    }
                    last_mouse_x = mouse_x; last_mouse_y = mouse_y; return;
                } 
                
                int clicked_window = -1;
                if (gui->windows[gui->focused_window].is_open && !gui->windows[gui->focused_window].is_minimized &&
                    mouse_x >= gui->windows[gui->focused_window].x && mouse_x <= gui->windows[gui->focused_window].x + gui->windows[gui->focused_window].w &&
                    mouse_y >= gui->windows[gui->focused_window].y && mouse_y <= gui->windows[gui->focused_window].y + gui->windows[gui->focused_window].h) {
                    clicked_window = gui->focused_window;
                } else {
                    for (int i = 0; i < MAX_WINDOWS; i++) {
                        if (gui->windows[i].is_open && i != gui->focused_window && !gui->windows[i].is_minimized &&
                            mouse_x >= gui->windows[i].x && mouse_x <= gui->windows[i].x + gui->windows[i].w &&
                            mouse_y >= gui->windows[i].y && mouse_y <= gui->windows[i].y + gui->windows[i].h) { clicked_window = i; break; }
                    }
                }

                if (clicked_window != -1) {
                    if (gui->focused_window != clicked_window) { gui->focused_window = clicked_window; gui->force_redraw = 1; }
                    if (mouse_x >= gui->windows[gui->focused_window].x + gui->windows[gui->focused_window].w - 30 &&
                        mouse_x <= gui->windows[gui->focused_window].x + gui->windows[gui->focused_window].w &&
                        mouse_y >= gui->windows[gui->focused_window].y && mouse_y <= gui->windows[gui->focused_window].y + 30) {
                        sys_kill(gui->windows[gui->focused_window].owner_task_id);
                        gui->windows[gui->focused_window].is_open = 0;
                        gui->force_redraw = 1; 
                    }
                    else if (mouse_y >= gui->windows[gui->focused_window].y && mouse_y <= gui->windows[gui->focused_window].y + 30) {
                        gui->windows[gui->focused_window].is_dragging = 1; gui->any_window_dragging = 1;
                    }
                }
            }

            if (gui->any_window_dragging && gui->windows[gui->focused_window].is_dragging) {
                add_dirty_rect(gui->windows[gui->focused_window].x, gui->windows[gui->focused_window].y, gui->windows[gui->focused_window].w, gui->windows[gui->focused_window].h);
                gui->windows[gui->focused_window].x += delta_x; gui->windows[gui->focused_window].y += delta_y;
                add_dirty_rect(gui->windows[gui->focused_window].x, gui->windows[gui->focused_window].y, gui->windows[gui->focused_window].w, gui->windows[gui->focused_window].h);
            }
        } else {
            // Fare sol tuşu bırakıldığında basılı butonları serbest bırak ve olay (Event) fırlat!
            for (int i = 0; i < MAX_WINDOWS; i++) {
                gui->windows[i].is_dragging = 0;
                if (gui->windows[i].is_open) {
                    for(int b = 0; b < gui->windows[i].button_count; b++) {
                        if (gui->windows[i].button_state[b] == 1) {
                            gui->windows[i].button_state[b] = 0; // Butonu eski haline getir
                            gui->force_redraw = 1;
                            
                            // UYGULAMAYA SİNYAL FIRLAT!
                            if (gui->windows[i].event_count < 20) {
                                gui->windows[i].event_type[gui->windows[i].event_count] = 1; 
                                gui->windows[i].event_id[gui->windows[i].event_count] = b;
                                gui->windows[i].event_count++;
                            }
                        }
                    }
                }
            }
            gui->any_window_dragging = 0;
            is_selecting = 0; // Seçim modunu sıfırla
        }
        
        add_dirty_rect(mouse_x, mouse_y, 16, 16);
        if (gui->force_redraw == 0) gui->force_redraw = 2; 
        last_mouse_x = mouse_x; last_mouse_y = mouse_y;
    }
}

void render_gui() {
    if (!gui->force_redraw) return;
    if (gui->force_redraw == 1) mark_screen_dirty(); 
    gui->force_redraw = 0;
    
    draw_wallpaper(); 
    
    for (int s = 0; s < gui->desktop_shape_count; s++) {
        draw_rect(gui->desktop_shape_x[s], gui->desktop_shape_y[s], gui->desktop_shape_w[s], gui->desktop_shape_h[s], gui->desktop_shape_color[s]);
    }
    
    int draw_order[MAX_WINDOWS]; int order_idx = 0;
    for (int i = 0; i < MAX_WINDOWS; i++) { if (i != gui->focused_window) draw_order[order_idx++] = i; }
    draw_order[order_idx++] = gui->focused_window;

    for (int i = 0; i < MAX_WINDOWS; i++) {
        int w_idx = draw_order[i];
        if (!gui->windows[w_idx].is_open || gui->windows[w_idx].is_minimized) continue;
        
        draw_window(&gui->windows[w_idx]); 
        set_clipping_rect(gui->windows[w_idx].x + 2, gui->windows[w_idx].y + 32, gui->windows[w_idx].w - 4, gui->windows[w_idx].h - 34);

        if (w_idx == 0) { 
            for (int line = 0; line < gui->terminal_line_count; line++) {
                draw_string(gui->windows[0].x + 10, gui->windows[0].y + 40 + (line * 16), gui->terminal_lines[line], 0x00000000, 0xFFFFFFFF);
            }
            draw_string(gui->windows[0].x + 10, gui->windows[0].y + gui->windows[0].h - 25, gui->user_input, 0x000000AA, 0xFFFFFFFF); 
        } 
        else if (w_idx == 1) { 
            draw_string(gui->windows[1].x + 10, gui->windows[1].y + 40, "PID  DURUM   CPU%", 0x00000000, 0xFFFFFFFF);
            draw_rect(gui->windows[1].x + 10, gui->windows[1].y + 55, gui->windows[1].w - 20, 2, 0x00BBBBBB);
            int y_offset = 65;
            for (int m = 0; m < gui->sys_task_count && y_offset < gui->windows[1].h - 20; m++) {
                char pid_str[4]; itoa(gui->sys_pid[m], pid_str);
                draw_string(gui->windows[1].x + 10, gui->windows[1].y + y_offset, pid_str, 0x00000000, 0xFFFFFFFF);
                const char* state_str = (gui->sys_state[m] == 0) ? "RUN" : "BLK";
                unsigned int state_color = (gui->sys_state[m] == 0) ? 0x0000AA00 : 0x00AA0000;
                draw_string(gui->windows[1].x + 45, gui->windows[1].y + y_offset, state_str, state_color, 0xFFFFFFFF);
                char usage_str[8]; itoa(gui->sys_cpu[m], usage_str); strcat(usage_str, "%");
                draw_string(gui->windows[1].x + 100, gui->windows[1].y + y_offset, usage_str, 0x00000000, 0xFFFFFFFF);
                unsigned int bar_color = (gui->sys_cpu[m] > 50) ? ((gui->sys_cpu[m] > 80) ? 0x00FF0000 : 0x00FFAA00) : 0x0000AA00;
                int bar_width = gui->sys_cpu[m] * 1.3; if (bar_width > 130) bar_width = 130;  
                if (bar_width > 0) draw_rect(gui->windows[1].x + 150, gui->windows[1].y + y_offset, bar_width, 10, bar_color);
                y_offset += 20;
            }
        }
        else if (w_idx >= 2) { 
            draw_rect(gui->windows[w_idx].x + 2, gui->windows[w_idx].y + 32, gui->windows[w_idx].w - 4, gui->windows[w_idx].h - 34, 0x00F0F0F0);
            for (int s = 0; s < gui->windows[w_idx].shape_count; s++) {
                draw_rect(gui->windows[w_idx].x + gui->windows[w_idx].shape_x[s], gui->windows[w_idx].y + 32 + gui->windows[w_idx].shape_y[s], 
                          gui->windows[w_idx].shape_w[s], gui->windows[w_idx].shape_h[s], gui->windows[w_idx].shape_color[s]);
            }
            
            int txt_x = gui->windows[w_idx].x + 10; int txt_y = gui->windows[w_idx].y + 40;
            int cursor_x = txt_x; int cursor_y = txt_y;
            
            // YENİ: Harf harf çizim ve Mavi Vurgu (Highlight) Motoru
            int s_min = (gui->windows[w_idx].sel_start < gui->windows[w_idx].sel_end) ? gui->windows[w_idx].sel_start : gui->windows[w_idx].sel_end;
            int s_max = (gui->windows[w_idx].sel_start > gui->windows[w_idx].sel_end) ? gui->windows[w_idx].sel_start : gui->windows[w_idx].sel_end;

            for (int c = 0; gui->windows[w_idx].text_content[c] != '\0'; c++) {
                if (gui->windows[w_idx].text_content[c] == '\n' || cursor_x + 8 > gui->windows[w_idx].x + gui->windows[w_idx].w - 10) {
                    cursor_x = txt_x; cursor_y += 16; 
                    if (gui->windows[w_idx].text_content[c] == '\n') continue;
                }
                
                unsigned int bg = 0x00F0F0F0; unsigned int fg = 0x00000000;
                
                // Eğer harf fare ile seçilen aralığa düşüyorsa, Mavi Arka Plan - Beyaz Yazı yap!
                if (gui->windows[w_idx].sel_start != -1 && c >= s_min && c <= s_max) {
                    bg = 0x000078D7; fg = 0x00FFFFFF; 
                }
                
                draw_char(cursor_x, cursor_y, gui->windows[w_idx].text_content[c], fg, bg);
                cursor_x += 8;
            }
            if (w_idx == gui->focused_window && blink_counter < 50) draw_rect(cursor_x, cursor_y, 8, 16, 0x00000000); 
        }
        // YENİ: 3D Buton Render Motoru
            for (int b = 0; b < gui->windows[w_idx].button_count; b++) {
                int bx = gui->windows[w_idx].x + gui->windows[w_idx].button_x[b];
                int by = gui->windows[w_idx].y + 32 + gui->windows[w_idx].button_y[b];
                int bw = gui->windows[w_idx].button_w[b];
                int bh = gui->windows[w_idx].button_h[b];
                int state = gui->windows[w_idx].button_state[b];
                
                draw_rect(bx, by, bw, bh, 0x00D4D0C8); // Win95 Gövde Rengi
                
                // 3D Gölgeler (Basılıysa Işık Yön Değiştirir)
                unsigned int c_top = (state == 1) ? 0x00808080 : 0x00FFFFFF;
                unsigned int c_bot = (state == 1) ? 0x00FFFFFF : 0x00404040;
                
                draw_rect(bx, by, bw, 2, c_top); 
                draw_rect(bx, by, 2, bh, c_top); 
                draw_rect(bx, by + bh - 2, bw, 2, c_bot); 
                draw_rect(bx + bw - 2, by, 2, bh, c_bot); 
                
                // Metni ortala ve tıklandığında 1 piksel kaydır
                int t_len = 0; while (gui->windows[w_idx].button_text[b][t_len]) t_len++;
                int tx = bx + (bw - (t_len * 8)) / 2;
                int ty = by + (bh - 8) / 2;
                if (state == 1) { tx++; ty++; } 
                draw_string(tx, ty, gui->windows[w_idx].button_text[b], 0x00000000, 0x00D4D0C8);
            }
        reset_clipping_rect();
    }
    
    // İŞTE EFSANE ZIRH: Ekrana Canlı Debug Basıyoruz!
    char debug_msg[128] = "BMP HATA AYIKLAMA -> FD: ";
    char tmp[16];
    itoa(dbg_fd, tmp); strcat(debug_msg, tmp);
    strcat(debug_msg, " | OKUNAN: "); itoa(dbg_read, tmp); strcat(debug_msg, tmp);
    strcat(debug_msg, " | BPP: "); itoa(dbg_bpp, tmp); strcat(debug_msg, tmp);
    strcat(debug_msg, " | BOYUT: "); itoa(dbg_w, tmp); strcat(debug_msg, tmp);
    strcat(debug_msg, "x"); itoa(dbg_h, tmp); strcat(debug_msg, tmp);
    strcat(debug_msg, " | IMZA: "); strcat(debug_msg, dbg_sig);

    // Debug şeridini ekranın en üstüne siyah kutu içine çiz
    draw_rect(0, 0, 1024, 20, 0x00000000);
    draw_string(5, 5, debug_msg, 0x00FFFFFF, 0x00000000);

    draw_taskbar();
    draw_context_menu(); 
    draw_cursor(mouse_x, mouse_y);
    swap_buffers();
}

void _start(char* args) {
    gui = (gui_state_t*)sys_shm_get(); 
    sys_get_screen(&framebuffer, &screen_width, &screen_height); 
    back_buffer = (unsigned int*)sys_malloc(screen_width * screen_height * 4); 
    
    load_wallpaper();
    
    reset_clipping_rect(); mark_screen_dirty();
    
    while(1) {
        blink_counter++; if (blink_counter > 100) blink_counter = 0;
        
        process_mouse_events();
        render_gui();
        
        sys_yield();
    }
}