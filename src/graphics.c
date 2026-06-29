#include "graphics.h"
#include "font.h"
#include "memory.h"
#include "cursor.h" // Ok imlecimiz

volatile unsigned int* framebuffer;
unsigned int* back_buffer = 0;
int screen_width;
int screen_height;

// ========================================================
// 1. SINIR KIRPMA (BOUNDARY CLIPPING)
// ========================================================
int clip_min_x = 0, clip_min_y = 0;
int clip_max_x = 1024, clip_max_y = 768;

void set_clipping_rect(int x, int y, int w, int h) {
    clip_min_x = (x < 0) ? 0 : x;
    clip_min_y = (y < 0) ? 0 : y;
    clip_max_x = (x + w - 1 >= screen_width) ? screen_width - 1 : x + w - 1;
    clip_max_y = (y + h - 1 >= screen_height) ? screen_height - 1 : y + h - 1;
}

void reset_clipping_rect() {
    clip_min_x = 0; clip_min_y = 0;
    clip_max_x = screen_width - 1; clip_max_y = screen_height - 1;
}

// ========================================================
// 2. KİRLİ BÖLGELER (DIRTY RECTANGLES - DAMAGE REGIONS)
// ========================================================
int dirty_min_x = 0, dirty_min_y = 0;
int dirty_max_x = 1024, dirty_max_y = 768;

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

void init_graphics(unsigned int* fb, int width, int height) {
    framebuffer = fb; screen_width = width; screen_height = height;
    back_buffer = (unsigned int*)malloc(width * height * sizeof(unsigned int));
    reset_clipping_rect(); mark_screen_dirty();
}

// ========================================================
// 3. OPTİMİZE EDİLMİŞ PİKSEL MOTORU
// ========================================================
void put_pixel(int x, int y, unsigned int color) {
    // 1. Piksel pencerenin sınırları dışındaysa ÇİZME! (Clipping)
    if (x < clip_min_x || x > clip_max_x || y < clip_min_y || y > clip_max_y) return;
    
    // 2. Piksel ekranın değişmeyen (temiz) bir kısmındaysa ÇİZME! (Damage Region)
    if (x < dirty_min_x || x > dirty_max_x || y < dirty_min_y || y > dirty_max_y) return;

    if (back_buffer) back_buffer[y * screen_width + x] = color; 
    else if (framebuffer) framebuffer[y * screen_width + x] = color; 
}

void swap_buffers(void) {
    if (!back_buffer || !framebuffer) return;
    
    // YENİ: 3 MB'ın tamamını kopyalamak yerine, SADECE değişen "Kirli Dikdörtgeni" kopyala!
    int s_min_x = (dirty_min_x < 0) ? 0 : dirty_min_x;
    int s_min_y = (dirty_min_y < 0) ? 0 : dirty_min_y;
    int s_max_x = (dirty_max_x >= screen_width) ? screen_width - 1 : dirty_max_x;
    int s_max_y = (dirty_max_y >= screen_height) ? screen_height - 1 : dirty_max_y;

    if (s_min_x > s_max_x || s_min_y > s_max_y) return; // Değişen bir şey yoksa doğrudan çık!

    for (int y = s_min_y; y <= s_max_y; y++) {
        for (int x = s_min_x; x <= s_max_x; x++) {
            framebuffer[y * screen_width + x] = back_buffer[y * screen_width + x];
        }
    }
    
    // Ekran güncellendi, kirli bölgeyi sıfırla (Kesişmesi imkansız değerler veriyoruz)
    dirty_min_x = screen_width; dirty_min_y = screen_height;
    dirty_max_x = 0; dirty_max_y = 0;
}

// O(1) Reddedici Zeki Dikdörtgen Algoritması
void draw_rect(int x, int y, int width, int height, unsigned int color) {
    int start_x = (x > clip_min_x) ? x : clip_min_x;
    int start_y = (y > clip_min_y) ? y : clip_min_y;
    int end_x = (x + width - 1 < clip_max_x) ? x + width - 1 : clip_max_x;
    int end_y = (y + height - 1 < clip_max_y) ? y + height - 1 : clip_max_y;

    start_x = (start_x > dirty_min_x) ? start_x : dirty_min_x;
    start_y = (start_y > dirty_min_y) ? start_y : dirty_min_y;
    end_x = (end_x < dirty_max_x) ? end_x : dirty_max_x;
    end_y = (end_y < dirty_max_y) ? end_y : dirty_max_y;

    if (start_x > end_x || start_y > end_y) return; // Çizim alanı güvenli bölgeyle hiç kesişmiyorsa döngüyü tamamen iptal et!

    for (int i = start_y; i <= end_y; i++) {
        for (int j = start_x; j <= end_x; j++) {
            if (back_buffer) back_buffer[i * screen_width + j] = color;
            else if (framebuffer) framebuffer[i * screen_width + j] = color;
        }
    }
}

void draw_char(int x, int y, char c, unsigned int fg_color, unsigned int bg_color) {
    if ((unsigned char)c > 127) c = ' ';
    const unsigned char* glyph = font8x8[(int)c];
    for (int row = 0; row < 8; row++) {
        unsigned char line = glyph[row];
        for (int col = 0; col < 8; col++) {
            if ((line >> (7 - col)) & 1) {
                put_pixel(x + col, y + row, fg_color);
            } else {
                if (bg_color != 0xFFFFFFFF) put_pixel(x + col, y + row, bg_color);
            }
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

void draw_string_wrapped(int x, int y, int max_w, const char* str, unsigned int fg_color, unsigned int bg_color) {
    int start_x = x; 
    while (*str) {
        if (*str == '\n') { x = start_x; y += 12; } 
        else {
            if ((x - start_x) + 8 > max_w) { x = start_x; y += 12; }
            draw_char(x, y, *str, fg_color, bg_color);
            x += 8;
        }
        str++;
    }
}

void draw_cursor(int x, int y) {
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            unsigned char pixel_type = arrow_cursor[row][col];
            if (pixel_type == 1) put_pixel(x + col, y + row, 0x00000000); 
            else if (pixel_type == 2) put_pixel(x + col, y + row, 0x00FFFFFF); 
        }
    }
}