#include "vga.h"
#include "io.h"

int cursor_x = 0;
int cursor_y = 0;
const int SCREEN_WIDTH = 80;
const int SCREEN_HEIGHT = 25;

void update_hardware_cursor(int x, int y) {
    unsigned short position = (y * SCREEN_WIDTH) + x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(position & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((position >> 8) & 0xFF));
}

void clear_screen(void) {
    char *video_memory = (char*) 0xB8000;
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        video_memory[i * 2] = ' ';
        video_memory[i * 2 + 1] = 0x07;
    }
    cursor_x = 0;
    cursor_y = 0;
    update_hardware_cursor(cursor_x, cursor_y);
}

// Ekran dolduğunda tüm satırları bir yukarı kaydıran ve son satırı temizleyen fonksiyon
void scroll_screen(void) {
    char *video_memory = (char*) 0xB8000;
    
    // 1. Adım: 1. satırdan 24. satıra kadar olan tüm veriyi 0. satıra (bir üst satıra) taşı
    // Her satır = 80 karakter * 2 bayt (ASCII + Renk) = 160 bayttır.
    for (int i = 0; i < SCREEN_WIDTH * (SCREEN_HEIGHT - 1) * 2; i++) {
        video_memory[i] = video_memory[i + (SCREEN_WIDTH * 2)];
    }
    
    // 2. Adım: En alt satırı (24. satır) tamamen boşluk (' ') karakteriyle doldurarak temizle
    int last_line_offset = SCREEN_WIDTH * (SCREEN_HEIGHT - 1) * 2;
    for (int i = 0; i < SCREEN_WIDTH; i++) {
        video_memory[last_line_offset + (i * 2)] = ' ';
        video_memory[last_line_offset + (i * 2) + 1] = 0x07; // Standart gri renk
    }
    
    // 3. Adım: İmleci temizlenen bu en alt satıra sabitle
    cursor_y = SCREEN_HEIGHT - 1;
}
void put_char(char c) {
    char *video_memory = (char*) 0xB8000;
    if (c == '\b') {
        if (cursor_x > 0) cursor_x--;
        else if (cursor_y > 0) { cursor_y--; cursor_x = SCREEN_WIDTH - 1; }
        int offset = (cursor_y * SCREEN_WIDTH + cursor_x) * 2;
        video_memory[offset] = ' '; video_memory[offset + 1] = 0x0A;
        update_hardware_cursor(cursor_x, cursor_y);
        return; 
    } 
    else if (c == '\n') { cursor_x = 0; cursor_y++; } 
    else {
        int offset = (cursor_y * SCREEN_WIDTH + cursor_x) * 2;
        video_memory[offset] = c; video_memory[offset + 1] = 0x0A;
        cursor_x++;
    }
    if (cursor_x >= SCREEN_WIDTH) { cursor_x = 0; cursor_y++; }
    if (cursor_y >= SCREEN_HEIGHT) { 
        scroll_screen();
    }
    update_hardware_cursor(cursor_x, cursor_y);
}

void print_string(const char *str) {
    for (int i = 0; str[i] != '\0'; i++) put_char(str[i]);
}

void print_number(unsigned int num) {
    if (num == 0) { put_char('0'); return; }
    char buf[16]; int i = 0;
    while (num > 0) { buf[i++] = (num % 10) + '0'; num /= 10; }
    while (i > 0) put_char(buf[--i]);
}