#include "keyboard.h"
#include "io.h"

#define BUFFER_SIZE 256
char kbd_buffer[BUFFER_SIZE];
int kbd_head = 0;
int kbd_tail = 0;

// YENİ: Shift tuşunun basılı olup olmadığını tutan "Durum (State)" değişkenimiz
int shift_pressed = 0; 

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
        
        // 1. KONTROL: Shift tuşları bırakıldı mı? (Break Codes: 0xAA veya 0xB6)
        if (scancode == 0x48 || scancode == 0x50) {
            // Aşağıda işleyeceğimiz için burayı boş geçiyoruz, Break kontrolüne takılmasın
        } else {
            if (scancode == 0xAA || scancode == 0xB6) {
                shift_pressed = 0; // Shift'ten elini çekti, durumu sıfırla
                return;
            }
        }

        // 2. KONTROL: Shift tuşlarına basıldı mı? (Make Codes: 0x2A veya 0x36)
        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = 1; // Shift basılı, durumu aktifleştir
            return;
        }
        
        // 3. KONTROL: Diğer tuşların bırakılma (Break) olaylarını yoksay
        // Ok tuşlarının make code'ları 0x80'den küçük olduğu için buraya takılmazlar
        if (!(scancode == 0x48 || scancode == 0x50) && (scancode & 0x80)) {
            return; 
        }

        // 4. KARAR: Hangi haritayı kullanacağız?
        char c = 0;
        
        // DÜZELTME: Ok tuşlarını doğrudan karakter değişkenine (c) atıyoruz!
        if (scancode == 0x48) {
            c = 17; // Yukarı Ok kodu
        } else if (scancode == 0x50) {
            c = 18; // Aşağı Ok kodu
        } else {
            if (shift_pressed) {
                c = kbd_us_shifted[scancode]; // Shift basılıysa büyük harf
            } else {
                c = kbd_us[scancode];         // Değilse normal harf
            }
        }

        // Karakteri (veya ok tuşu şifresini) kuyruğa (Buffer) güvenle ekle
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