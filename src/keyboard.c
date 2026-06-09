#include "keyboard.h"
#include "io.h"
#include "vga.h"
#include "memory.h"
#include "timer.h"
#include "string.h"
// --- SHELL (KOMUT YORUMLAYICI) DEĞİŞKENLERİ VE FONKSİYONLARI ---
char command_buffer[256]; // Kullanıcının yazdığı satırı tutacak bellek
int buffer_index = 0;     // Bellekteki mevcut konumumuz

// Standart C kütüphanesindeki strcmp fonksiyonunun kendi üretimimiz olan versiyonu


// Enter'a basıldığında buffer'daki yazıyı komut olarak değerlendiren fonksiyon
void execute_command(void) {
    if (buffer_index == 0) return; 
    
    command_buffer[buffer_index] = '\0'; 

    // --- ARGÜMAN AYRIŞTIRICI (PARSER) ---
    char cmd[32];      // Komutun kendisi (örn: "echo")
    char arg[224];     // Parametre kısmı (örn: "merhaba dunya")
    int i = 0, j = 0;
    
    // 1. İlk boşluğa (' ') kadar olan kısmı cmd dizisine kopyala
    while (command_buffer[i] != ' ' && command_buffer[i] != '\0' && i < 31) {
        cmd[i] = command_buffer[i];
        i++;
    }
    cmd[i] = '\0'; // Komutu sonlandır

    // 2. Eğer boşluk varsa, geri kalan her şeyi arg dizisine kopyala
    if (command_buffer[i] == ' ') {
        i++; // Boşluğu atla
        while (command_buffer[i] != '\0' && j < 223) {
            arg[j] = command_buffer[i];
            i++; j++;
        }
    }
    arg[j] = '\0'; // Argümanı sonlandır

    // --- YENİ KOMUT MANTIKLARI (cmd ve arg kullanarak) ---
    
    if (strcmp(cmd, "help") == 0) {
        print_string("Komutlar: help, clear, memorytest, uptime, echo [metin], bekle [ms]\n");
    } 
    else if (strcmp(cmd, "clear") == 0) {
        clear_screen();
    } 
    // YENİ: Dışarıdan metin alan echo komutu
    else if (strcmp(cmd, "echo") == 0) {
        if (strlen(arg) > 0) {
            print_string(arg);
            print_string("\n");
        } else {
            print_string("Kullanim: echo [yazilacak_metin]\n");
        }
    }
    // YENİ: Dışarıdan süre alan dinamik bekleme komutu
    else if (strcmp(cmd, "bekle") == 0) {
        int bekleme_suresi = atoi(arg); // Metni (örn: "2500") sayıya (2500) çevir
        if (bekleme_suresi > 0) {
            print_string("Bekleniyor: "); print_number(bekleme_suresi); print_string(" ms...\n");
            sleep(bekleme_suresi);
            print_string("Zaman doldu!\n");
        } else {
            print_string("Kullanim: bekle [milisaniye]\n");
        }
    }
    else if (strcmp(cmd, "memorytest") == 0) {
        print_string("--- Dinamik Bellek (Next-Fit) Testi ---\n");

        void* metin_alani = malloc(15); 
        void* sayi_alani = malloc(4);   
        
        print_string("1. Malloc (15 bayt) Adresi: "); print_number((unsigned int)metin_alani); print_string("\n");
        print_string("2. Malloc (4 bayt) Adresi:  "); print_number((unsigned int)sayi_alani); print_string("\n");
        
        free(metin_alani);
        print_string("1. Alan serbest birakildi (free).\n");
        
        // Next-Fit sayesinde bu yeni alan, en son kalınan yer olan 2. alanın ilerisine yazılmalı!
        void* yeni_alan = malloc(10); 
        print_string("Yeni Malloc (10 bayt) Adresi: "); print_number((unsigned int)yeni_alan); print_string("\n");

        // Test sonrası bellek sızıntısı olmaması için temizlik
        free(sayi_alani);
        free(yeni_alan);
        print_string("---------------------------------------\n");
    }
    else if (strcmp(cmd, "uptime") == 0) {
        print_string("Sistem: ");
        print_number(get_uptime());
        print_string(" saniyedir acik.\n");
    }
    // ... (önceki komutların altı)
    else if (strcmp(cmd, "crashtest") == 0) {
        print_string("Sistem bilerek cokertiliyor...\n");
        // Derleyicinin bu hatayı önceden fark edip silmemesi (optimize etmemesi) için volatile kullanıyoruz
        volatile int a = 10;
        volatile int b = 0;
        int c = a / b; // BAM! İşlemci burada donanımsal olarak patlayacak.
        print_number(c); 
    }
    // ...
    else {
        print_string("Bilinmeyen komut: ");
        print_string(cmd);
        print_string("\n");
    }

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