#include "keyboard.h"
#include "io.h"
#include "vga.h"

// --- SHELL (KOMUT YORUMLAYICI) DEĞİŞKENLERİ VE FONKSİYONLARI ---
char command_buffer[256]; // Kullanıcının yazdığı satırı tutacak bellek
int buffer_index = 0;     // Bellekteki mevcut konumumuz

// Standart C kütüphanesindeki strcmp fonksiyonunun kendi üretimimiz olan versiyonu
int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// Enter'a basıldığında buffer'daki yazıyı komut olarak değerlendiren fonksiyon
void execute_command(void) {
    if (buffer_index == 0) return; // Boş basıldıysa hiçbir şey yapma
    
    command_buffer[buffer_index] = '\0'; // String dizisini kurallara uygun sonlandır

    // Komutları kontrol et
    if (strcmp(command_buffer, "help") == 0) {
        print_string("Kullanilabilir komutlar: help, clear, merhaba\n");
    } 
    else if (strcmp(command_buffer, "clear") == 0) {
        clear_screen();
    } 
    else if (strcmp(command_buffer, "merhaba") == 0) {
        print_string("Sisteme hos geldin! Ilk komutun basariyla calisti.\n");
    } 
    else {
        print_string("Bilinmeyen komut: ");
        print_string(command_buffer);
        print_string("\n");
    }

    // Komut işlendikten sonra buffer'ı sıfırla
    buffer_index = 0; 
}

// ============================================================================
// KLAVYE SÜRÜCÜSÜ VE TARAMA TABLOSU (SCANCODE MAP)
// ============================================================================

// Standart US Klavye düzeni
unsigned char keyboard_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	'9', '0', '-', '=', '\b',	
  '\t', 'q', 'w', 'e', 'r',	't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0, '\\', 
  'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0, '*',   0, ' ',	
    0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  '-',   0,   0,   0, '+',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

// Shift tuşuna basılıyken kullanılacak alternatif tarama tablosu
unsigned char keyboard_map_shifted[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',   
  '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',   
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',   0, '|', 
  'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',   0, '*',   0, ' ', 
    0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  '-',   0,   0,   0, '+',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

// Shift tuşunun basılı olup olmadığını takip eden küresel durum değişkeni
int shift_pressed = 0;

// Klavyeden bir tuşa basıldığında tetiklenen asıl fonksiyon
void keyboard_handler_main(void) {
    unsigned char status = inb(0x64);
    unsigned char keycode;

    if (status & 0x01) {
        keycode = inb(0x60);
        
        if (keycode == 0x2A || keycode == 0x36) {
            shift_pressed = 1;
        } else if (keycode == 0xAA || keycode == 0xB6) {
            shift_pressed = 0;
        } else if (!(keycode & 0x80)) {
            char c = shift_pressed ? keyboard_map_shifted[keycode] : keyboard_map[keycode];
            
            if (c != 0) {
                // EĞER ENTER'A BASILDIYSA
                if (c == '\n') {
                    put_char('\n');
                    execute_command();
                    print_string("> "); // Yeni komut için prompt yazdır
                } 
                // EĞER SİLME (BACKSPACE) TUŞUNA BASILDIYSA
                else if (c == '\b') {
                    if (buffer_index > 0) {
                        buffer_index--; // Bellekten sil
                        put_char('\b'); // Ekrandan sil
                    }
                } 
                // NORMAL BİR HARF YAZILDIYSA
                else {
                    if (buffer_index < 255) { // Bellek sınırını koru
                        command_buffer[buffer_index++] = c;
                        put_char(c);
                    }
                }
            }
        }
    }
    outb(0x20, 0x20); // EOI
}