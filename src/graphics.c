#include "graphics.h"
#include "font.h"


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
void draw_char(int x, int y, char c, unsigned int fg_color, unsigned int bg_color) {
    // Geçersiz bir karakter istenirse boşluk çiz
    if ((unsigned char)c > 127) c = ' ';

    // Font tablosundan bu harfin 8 satırlık haritasını çek
    const unsigned char* glyph = font8x8[(int)c];

    for (int row = 0; row < 8; row++) {
        unsigned char line = glyph[row];
        for (int col = 0; col < 8; col++) {
            // İlgili bit 1 ise (yazı rengi), 0 ise (arka plan rengi)
            // Bit kaydırma (shift) işlemi ile karakteri soldan sağa tarıyoruz
            if ((line >> (7 - col)) & 1) {
                put_pixel(x + col, y + row, fg_color);
            } else {
                // Şeffaf arka plan istemiyorsak arka planı boya
                if (bg_color != 0xFFFFFFFF) { 
                    put_pixel(x + col, y + row, bg_color);
                }
            }
        }
    }
}

// Bir metni (String) harf harf ekrana yazdıran fonksiyon
// Bir metni (String) harf harf ekrana yazdıran fonksiyon
void draw_string(int x, int y, const char* str, unsigned int fg_color, unsigned int bg_color) {
    int start_x = x; // Satır başına dönmek için başlangıç X noktasını hafızaya al
    
    while (*str) {
        if (*str == '\n') {
            x = start_x; // Kalemi satırın en başına çek
            y += 12;     // Kalemi bir alt satıra indir (8 piksel harf + 4 piksel boşluk)
        } else {
            draw_char(x, y, *str, fg_color, bg_color);
            x += 8;      // Her normal harften sonra kalemi 8 piksel sağa kaydır
        }
        str++;
    }
}