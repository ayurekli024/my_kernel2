#include "graphics.h"
#include "font.h"
#include "memory.h"
#include "cursor.h" // Ok imlecimiz

// YENİ: volatile ekleyerek derleyicinin bu değişkeni silmesini kökten engelliyoruz!
volatile unsigned int* framebuffer;
unsigned int* back_buffer = 0;
int screen_width;
int screen_height;

void init_graphics(unsigned int* fb, int width, int height) {
    framebuffer = fb;
    screen_width = width;
    screen_height = height;
    
    // 3 MB'lık hayali tuvali Heap'ten al
    back_buffer = (unsigned int*)malloc(width * height * sizeof(unsigned int));
}

// ZIRHLI PİKSEL MOTORU
void put_pixel(int x, int y, unsigned int color) {
    if (x >= 0 && x < screen_width && y >= 0 && y < screen_height) {
        if (back_buffer) {
            // Her şey yolundaysa Hayali Tuvale (RAM) çiz (İz bırakmayı engeller)
            back_buffer[y * screen_width + x] = color; 
        } else if (framebuffer) {
            // Eğer sistem çökmüşse veya bellek kalmamışsa DOĞRUDAN ekrana çiz!
            framebuffer[y * screen_width + x] = color; 
        }
    }
}

void swap_buffers(void) {
    if (!back_buffer || !framebuffer) return;
    for (int i = 0; i < screen_width * screen_height; i++) {
        framebuffer[i] = back_buffer[i];
    }
}

void draw_rect(int x, int y, int width, int height, unsigned int color) {
    for (int i = y; i < y + height; i++) {
        for (int j = x; j < x + width; j++) {
            put_pixel(j, i, color);
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