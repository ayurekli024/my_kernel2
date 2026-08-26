#include "keyboard.h"
#include "io.h"

#define BUFFER_SIZE 256
char kbd_buffer[BUFFER_SIZE];
int kbd_head = 0;
int kbd_tail = 0;

// YENİ: Shift tuşunun basılı olup olmadığını tutan "Durum (State)" değişkenimiz
int shift_pressed = 0; 
int ctrl_pressed = 0;
// Normal QWERTY Haritası
const char kbd_us[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
     0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
     0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0,
  '*',  0,  ' ',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0, '-',   0,   0,   0, '+',   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0
};

// YENİ: Shift tuşuna basıldığında kullanılacak Büyük Harf / Sembol Haritası
const char kbd_us_shifted[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
  '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
     0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
     0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',   0,
  '*',  0,  ' ',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0, '-',   0,   0,   0, '+',   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,   0
};

void keyboard_handler_main() {
    unsigned char status = inb(0x64);
    
    if (status & 0x01) {
        unsigned char scancode = inb(0x60);
        
        // 1. KONTROL: Shift ve Ctrl bırakıldı mı? (Break Codes)
        if (scancode == 0x48 || scancode == 0x50) { } 
        else if (scancode == 0xAA || scancode == 0xB6) { shift_pressed = 0; return; }
        else if (scancode == 0x9D) { ctrl_pressed = 0; return; } // YENİ: Ctrl Bırakıldı

        // 2. KONTROL: Shift ve Ctrl basıldı mı? (Make Codes)
        if (scancode == 0x2A || scancode == 0x36) { shift_pressed = 1; return; }
        if (scancode == 0x1D) { ctrl_pressed = 1; return; } // YENİ: Ctrl Basıldı
        
        if (!(scancode == 0x48 || scancode == 0x50) && (scancode & 0x80)) return; 

        char c = 0;
        if (scancode == 0x48) c = 17; 
        else if (scancode == 0x50) c = 18; 
        else {
            // İŞTE SİHİR BURADA: Donanım kesmelerini gizli ASCII kodlarına çeviriyoruz!
            if (ctrl_pressed && scancode == 0x2E) c = 3;      // Ctrl + C (ASCII 3: ETX)
            else if (ctrl_pressed && scancode == 0x2F) c = 22; // Ctrl + V (ASCII 22: SYN)
            else if (shift_pressed) c = kbd_us_shifted[scancode];
            else c = kbd_us[scancode];
        }

        if (c != 0) {
            int next_head = (kbd_head + 1) % BUFFER_SIZE;
            if (next_head != kbd_tail) { 
                kbd_buffer[kbd_head] = c;
                kbd_head = next_head;
            }
        }
    }
}

char get_keyboard_char() {
    if (kbd_head == kbd_tail) return 0; 
    char c = kbd_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % BUFFER_SIZE;
    return c;
}