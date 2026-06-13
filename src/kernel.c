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
#include "string.h"

unsigned int* vesa_framebuffer;
extern void outb(unsigned short port, unsigned char data);
extern unsigned char inb(unsigned short port);

// Masaüstü bileşenlerini DİNAMİK koordinatlarla çizen yardımcı fonksiyon
// YENİ: Masaüstü arka plan rengi artık dışarıdan parametre alıyor!
void draw_desktop(int win_x, int win_y, int win_w, int win_h, unsigned int desktop_bg) {
    // 1. Dinamik Masaüstü Arka Planı
    draw_rect(0, 0, 1024, 768, desktop_bg); 

    draw_rect(0, 728, 1024, 40, 0x00111A); // Görev Çubuğu
    draw_rect(win_x, win_y, win_w, win_h, 0x00F0F0F0); // Pencere
    draw_rect(win_x, win_y, win_w, 30, 0x000078D7); // Başlık Çubuğu
    draw_rect(win_x + win_w - 30, win_y, 30, 30, 0x00FF2D55); // Kapatma Butonu
    draw_string(win_x + 10, win_y + 8, "ArdaOS Terminali", 0x00FFFFFF, 0x000078D7);
}
int task1_counter = 0;

// İKİNCİ ÇEKİRDEK GÖREVİ (Sonsuz Döngü 2)
void background_task() {
    while(1) {
        task1_counter++; // Kendi işini yap
        yield();         // İşlemciyi nazikçe masaüstü döngüsüne geri ver
    }
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
    init_tasking();               // YENİ: Görev Yöneticisini Başlat (Kernel = Task 0)
    create_task(background_task);
    // Temel çizim motoru başlatılıyor
    if (vesa_framebuffer != 0) {
        init_graphics(vesa_framebuffer, 1024, 768);
    }

    // Donanım kesmelerini (Interrupts) serbest bırak
   // ... (Üst kısımdaki init_gdt, paging, heap kodları aynı)
    init_timer(100);
    __asm__ __volatile__ ("sti");

    int win_x = 300, win_y = 200, win_w = 400, win_h = 300;
    int is_dragging = 0;
    int last_mouse_x = mouse_x, last_mouse_y = mouse_y;

    // YENİ: Terminal Mimarisi
    unsigned int current_bg_color = 0x001B26; // Varsayılan arka plan
    char terminal_response[512] = "ArdaOS V0.1'e Hos Geldiniz!\n\nKullanilabilir Komutlar:\n- info\n- temizle\n- renk mavi\n- renk kirmizi";
    
    // Kullanıcının yazı yazdığı alan (Prompt)
    char user_input[256] = "Arda> "; 
    int input_idx = 6; // İmleç "Arda> " yazısından sonra başlıyor

    draw_desktop(win_x, win_y, win_w, win_h, current_bg_color); 
    draw_string_wrapped(win_x + 10, win_y + 40, win_w - 20, terminal_response, 0x00000000, 0xFFFFFFFF);
    draw_string_wrapped(win_x + 10, win_y + 250, win_w - 20, user_input, 0x000000AA, 0xFFFFFFFF);
    draw_cursor(mouse_x, mouse_y); 
    swap_buffers(); 
    unsigned int system_ticks = 0;
    int last_second = -1;
    while(1) {
        system_ticks++; 
        int needs_redraw = 0;

        // 1. OLAY: SAAT KONTROLÜ
        int current_second = timer_ticks / 100;
        if (current_second != last_second) {
            last_second = current_second;
            needs_redraw = 1; 
        }

        // 2. OLAY: FARE KONTROLÜ
        int delta_x = mouse_x - last_mouse_x;
        int delta_y = mouse_y - last_mouse_y;

        if (delta_x != 0 || delta_y != 0) {
            needs_redraw = 1;
            if (mouse_left_button) {
                if (!is_dragging && mouse_x >= win_x && mouse_x < (win_x + win_w) && mouse_y >= win_y && mouse_y < (win_y + 30)) {
                    is_dragging = 1; 
                }
                if (is_dragging) { win_x += delta_x; win_y += delta_y; }
            } else { is_dragging = 0; }
            last_mouse_x = mouse_x; last_mouse_y = mouse_y;
        }

        // 3. OLAY: KLAVYE VE KOMUT PARSER
        char kbd_char = get_keyboard_char();
        if (kbd_char != 0) {
            needs_redraw = 1;

            if (kbd_char == '\n') { 
                char* cmd = &user_input[6]; 

                if (strcmp(cmd, "info") == 0) {
                    strcpy(terminal_response, "Sistem: ArdaOS V0.1\nMimari: 32-bit x86\nCekirdek Durumu: Kararli\nGUI: Double-Buffered");
                } 
                else if (strcmp(cmd, "temizle") == 0) {
                    strcpy(terminal_response, ""); 
                } 
                else if (strcmp(cmd, "renk mavi") == 0) {
                    current_bg_color = 0x000000AA;
                    strcpy(terminal_response, "Masaustu rengi mavi olarak degistirildi.");
                } 
                else if (strcmp(cmd, "renk kirmizi") == 0) {
                    current_bg_color = 0x00AA0000;
                    strcpy(terminal_response, "Masaustu rengi kirmizi olarak degistirildi.");
                } 
                else if (strcmp(cmd, "help") == 0) {
                    strcpy(terminal_response, "KOMUTLAR:\n- help: Bu listeyi gosterir\n- info: Sistem detayi\n- temizle: Ekrani siler\n- renk [mavi/kirmizi]: Arkaplan\n- memorytest: RAM saglik testi\n- uptime: Calisma olay sayaci");
                }
                else if (strcmp(cmd, "memorytest") == 0) {
                    void* test_ptr = malloc(1024); 
                    if (test_ptr != 0) {
                        free(test_ptr); 
                        strcpy(terminal_response, "[ BASARILI ]\n1 KB Heap bellegi sorunsuz ayrildi ve iade edildi.");
                    } else {
                        strcpy(terminal_response, "[ DIKKAT - BASARISIZ ]\nHeap uzerinde yeterli bellek kalmadi. OOM Riski!");
                    }
                }
                else if (strcmp(cmd, "uptime") == 0) {
                    char sec_str[10];
                    itoa(current_second, sec_str);
                    strcpy(terminal_response, "Sistem Gercek Calisma Suresi: ");
                    strcat(terminal_response, sec_str);
                    strcat(terminal_response, " saniye");
                }
                else if (strcmp(cmd, "") == 0) { } 
                else {
                    strcpy(terminal_response, "Hata: Bilinmeyen komut! 'help' yazarak komutlari gorebilirsiniz.");
                }

                strcpy(user_input, "Arda> ");
                input_idx = 6;
            } 
            else if (kbd_char == '\b' && input_idx > 6) { 
                input_idx--;
                user_input[input_idx] = '\0';
            } 
            else if (input_idx < 255 && kbd_char != '\b' && kbd_char != '\n') { 
                user_input[input_idx] = kbd_char;
                input_idx++;
                user_input[input_idx] = '\0'; 
            }
        }

        // --- 4. EKRANI GÜNCELLE (EKSİK OLAN KISIM BURASIYDI!) ---
        if (needs_redraw) {
            // Masaüstünü çizerek eski pikselleri (izleri) temizle
            draw_desktop(win_x, win_y, win_w, win_h, current_bg_color);
            
            // Terminal yazılarını pencerene bas
            draw_string_wrapped(win_x + 10, win_y + 40, win_w - 20, terminal_response, 0x00000000, 0xFFFFFFFF); 
            draw_string_wrapped(win_x + 10, win_y + 250, win_w - 20, user_input, 0x000000AA, 0xFFFFFFFF); 
            
            // Görev Çubuğuna Saat Widget'ı Çiziliyor
            char taskbar_time[32] = "Uptime: ";
            char sec_str[10];
            itoa(current_second, sec_str);
            strcat(taskbar_time, sec_str);
            strcat(taskbar_time, " sn");
            draw_string(900, 740, taskbar_time, 0x00FFFFFF, 0x00111A);

            // Arka plan görevinin çalıştığını kanıtlayan canlı sayaç
            char bg_text[32] = "Arka Plan (Task 1): ";
            char count_str[10];
            itoa(task1_counter, count_str);
            strcat(bg_text, count_str);
            draw_string(10, 740, bg_text, 0x0000FF00, 0x00111A); 

            // İmleci bas ve ekranı monitöre fırlat
            draw_cursor(mouse_x, mouse_y);
            swap_buffers();
        }

        // İşlemciyi diğer göreve devret ve uyu
        yield(); 
        __asm__ __volatile__ ("hlt"); 
    }
}