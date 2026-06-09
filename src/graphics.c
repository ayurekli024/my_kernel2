#include "graphics.h"

unsigned int* framebuffer;
int screen_width;
int screen_height;

void init_graphics(unsigned int* fb, int width, int height) {
    framebuffer = fb;
    screen_width = width;
    screen_height = height;
}

// 1024x768 koordinat sisteminde güvenli piksel çizimi
void put_pixel(int x, int y, unsigned int color) {
    // Çizim sınırı kontrolü (Piksellerin ekran dışına taşıp sistemi çökertmesini önler)
    if (x >= 0 && x < screen_width && y >= 0 && y < screen_height) {
        framebuffer[y * screen_width + x] = color;
    }
}

// Belirtilen koordinatlara, belirtilen boyutlarda içi dolu dikdörtgen çizer
void draw_rect(int x, int y, int width, int height, unsigned int color) {
    for (int i = y; i < y + height; i++) {
        for (int j = x; j < x + width; j++) {
            put_pixel(j, i, color);
        }
    }
}