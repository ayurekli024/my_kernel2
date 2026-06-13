#include "multiboot.h"
#include "idt.h"
#include "memory.h"
#include "timer.h"
#include "task.h"
#include "graphics.h"
#include "mouse.h"
#include "io.h"
#include "gdt.h"
#include "keyboard.h"

unsigned int* vesa_framebuffer;
extern void outb(unsigned short port, unsigned char data);
extern unsigned char inb(unsigned short port);

// Masaüstü bileşenlerini DİNAMİK koordinatlarla çizen yardımcı fonksiyon
void draw_desktop(int win_x, int win_y, int win_w, int win_h) {
    // 1. Masaüstü Arka Planı (Koyu Turkuaz)
    draw_rect(0, 0, 1024, 768, 0x001B26); 

    // 2. Görev Çubuğu (Koyu gri)
    draw_rect(0, 728, 1024, 40, 0x00111A); 

    // 3. Dinamik Pencere Gövdesi (Beyaz zemin)
    draw_rect(win_x, win_y, win_w, win_h, 0x00F0F0F0); 

    // 4. Dinamik Pencere Başlığı (Titlebar - Klasik Mavi)
    // Yüksekliği 30 piksel, sürükleme alanımız burası olacak!
    draw_rect(win_x, win_y, win_w, 30, 0x000078D7); 
    
    // 5. Pencere Kapatma Butonu (Kırmızı)
    draw_rect(win_x + win_w - 30, win_y, 30, 30, 0x00FF2D55);
    
    // Pencere Başlık Yazısı
    draw_string(win_x + 10, win_y + 8, "ArdaOS Terminali", 0x00FFFFFF, 0x000078D7);
}

// DÜZELTME: multiboot_info_t yerine projenin orijinal yapısı olan struct multiboot_info* kullanıldı
void kernel_main(unsigned int magic, struct multiboot_info* mb_info) {
    // Temel donanım ve segmentasyon kurulumları
    init_gdt();
    pic_remap();     
    init_idt();      
    init_mouse(); // PS/2 Fare denetleyicisini kur

    // Artık bu kontrol doğru çalışacak ve sistemi kapatmayacak!
    if (magic != 0x2BADB002) return; 

    if (mb_info->flags & (1 << 12)) { 
        vesa_framebuffer = (unsigned int*)(unsigned int)mb_info->framebuffer_addr;
    }
    
    // DÜZELTME: Satır sonundaki hatalı \n temizlendi
    init_paging((unsigned int)vesa_framebuffer); 
    init_heap();
    
    // Temel çizim motoru başlatılıyor
    if (vesa_framebuffer != 0) {
        init_graphics(vesa_framebuffer, 1024, 768);
    }

    // Donanım kesmelerini (Interrupts) serbest bırak
    __asm__ __volatile__ ("sti");

    // PENCERE KOORDİNAT DEĞİŞKENLERİ
    int win_x = 300;
    int win_w = 400;
    int win_y = 200;
    int win_h = 300;
    int is_dragging = 0; // Sürükleme aktif mi?

    int last_mouse_x = mouse_x;
    int last_mouse_y = mouse_y;

    char user_input[256] = "ArdaOS Terminaline Yazin: "; 
    int input_idx = 26; 

    // İLK AÇILIŞ ÇİZİMİ
    draw_desktop(win_x, win_y, win_w, win_h); 
    draw_string_wrapped(win_x + 20, win_y + 50, win_w - 40, user_input, 0x00000000, 0xFFFFFFFF);
    draw_cursor(mouse_x, mouse_y); 
    swap_buffers(); 

    // ANA PENCERE YÖNETİCİSİ DÖNGÜSÜ
    while(1) {
        int needs_redraw = 0;

        // Farenin anlık hareket miktarını (Delta) hesapla
        int delta_x = mouse_x - last_mouse_x;
        int delta_y = mouse_y - last_mouse_y;

        // 1. DURUM: Fare hareket etti mi?
        if (delta_x != 0 || delta_y != 0) {
            needs_redraw = 1;

            // Eğer sol buton BASILIYSA sürükleme kontrolü yap
            if (mouse_left_button) {
                // Eğer halihazırda sürükleme başlamadıysa, farenin başlık çubuğunda olup olmadığını kontrol et
                if (!is_dragging) {
                    if (mouse_x >= win_x && mouse_x < (win_x + win_w) &&
                        mouse_y >= win_y && mouse_y < (win_y + 30)) {
                        is_dragging = 1; // Sürüklemeyi başlat.
                    }
                }
                
                // Sürükleme aktifse pencere konumunu farenin hareketi kadar kaydır
                if (is_dragging) {
                    win_x += delta_x;
                    win_y += delta_y;
                }
            } else {
                // Sol buton bırakıldığı an sürüklemeyi bitir
                is_dragging = 0;
            }

            last_mouse_x = mouse_x;
            last_mouse_y = mouse_y;
        }

        // 2. DURUM: Klavyeden tuşa basıldı mı?
        char kbd_char = get_keyboard_char();
        if (kbd_char != 0) {
            if (kbd_char == '\b' && input_idx > 0) { 
                input_idx--;
                user_input[input_idx] = '\0';
            } else if (input_idx < 255 && kbd_char != '\b') { 
                user_input[input_idx] = kbd_char;
                input_idx++;
                user_input[input_idx] = '\0'; 
            }
            needs_redraw = 1; 
        }

        // ÇİZİM VE EKRAN GÜNCELLEME (BUFFER FLIP)
        if (needs_redraw) {
            // Masaüstünü yeni koordinatlarla RAM'de çiz
            draw_desktop(win_x, win_y, win_w, win_h);
            
            // Terminal yazısını pencerenin içine göre hizalayarak çiz
            draw_string_wrapped(win_x + 20, win_y + 50, win_w - 40, user_input, 0x00000000, 0xFFFFFFFF); 
            
            // İmleci bas ve hayali ekranı monitöre fırlat!
            draw_cursor(mouse_x, mouse_y);
            swap_buffers();
        }

        __asm__ __volatile__ ("hlt"); 
    }
}