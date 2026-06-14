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
// ==========================================
// EKRAN LİSTESİ (DISPLAY LIST) HAFIZASI
// ==========================================
#define MAX_SHAPES 10
int shape_count = 0;
int shape_x[MAX_SHAPES];
int shape_y[MAX_SHAPES];
int shape_w[MAX_SHAPES];
int shape_h[MAX_SHAPES];
unsigned int shape_color[MAX_SHAPES];
// Masaüstü bileşenlerini DİNAMİK koordinatlarla çizen yardımcı fonksiyon
// YENİ: Masaüstü arka plan rengi artık dışarıdan parametre alıyor!
void draw_desktop(int win_x, int win_y, int win_w, int win_h, unsigned int desktop_bg) {
    // 1. KATMAN: Masaüstü Arka Planı (En alt)
    draw_rect(0, 0, 1024, 768, desktop_bg); 

    // 2. KATMAN: Terminalden Gelen Özel Şekiller (Masaüstüne yapışık)
    for (int s = 0; s < shape_count; s++) {
        draw_rect(shape_x[s], shape_y[s], shape_w[s], shape_h[s], shape_color[s]);
    }

    // 3. KATMAN: Görev Çubuğu (Üstte)
    draw_rect(0, 728, 1024, 40, 0x00111A); 

    // 4. KATMAN: İşletim Sistemi Penceresi (En üstte - Sürüklenebilir)
    draw_rect(win_x, win_y, win_w, win_h, 0x00F0F0F0); 
    draw_rect(win_x, win_y, win_w, 30, 0x000078D7); 
    draw_rect(win_x + win_w - 30, win_y, 30, 30, 0x00FF2D55); 
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
    // CMOS yongasından (Gerçek Zamanlı Saat) veri okuyan fonksiyon
    unsigned char get_rtc_register(int reg) {
        outb(0x70, reg);
        return inb(0x71);
    }

    // CMOS verileri BCD (Binary Coded Decimal) formatındadır, onu normal sayılara çeviririz
    unsigned char bcd_to_bin(unsigned char bcd) {
        return (bcd & 0x0F) + ((bcd >> 4) * 10);
    }
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
                else if (strcmp(cmd, "saat") == 0) {
                    unsigned char h = bcd_to_bin(get_rtc_register(0x04));
                    unsigned char m = bcd_to_bin(get_rtc_register(0x02));
                    unsigned char s = bcd_to_bin(get_rtc_register(0x00));
                    
                    // UTC saatini Türkiye Saatine (UTC+3) uyarlıyoruz (Basitçe +3 ekliyoruz, 24'ü geçerse mod alıyoruz)
                    h = (h + 3) % 24;
                    
                    char hs[10], ms[10], ss[10];
                    itoa(h, hs); itoa(m, ms); itoa(s, ss);
                    
                    strcpy(terminal_response, "Gercek Donanim Saati (CMOS UTC+3): ");
                    strcat(terminal_response, hs); strcat(terminal_response, ":");
                    strcat(terminal_response, ms); strcat(terminal_response, ":");
                    strcat(terminal_response, ss);
                }
                
                // 2. YENİ KOMUT: Yankı (Parametre Okuma)
                // Eğer komut "yanki " ile başlıyorsa (ilk 6 karakter)
                else if (strncmp(cmd, "yanki ", 6) == 0) { 
                    strcpy(terminal_response, "Sen dedin ki: ");
                    strcat(terminal_response, cmd + 6); // 6. karakterden sonrasını al
                }
                
                // 3. YENİ KOMUT: Hesap Makinesi
                // Eğer komut "hesapla " ile başlıyorsa (ilk 8 karakter)
                else if (strncmp(cmd, "hesapla ", 8) == 0) {
                    int i = 8; // Komutun verisi 8. indisten başlıyor
                    
                    // İlk sayıyı al (Boşlukları atla)
                    // İlk sayıyı al (Boşlukları atla)
                    while(cmd[i] == ' ') i++;
                    int num1 = atoi(&cmd[i]);
                    
                    // DÜZELTME: Öncelik hatasını engellemek için parantezler eklendi
                    while((cmd[i] >= '0' && cmd[i] <= '9') || cmd[i] == '-') i++;
                    
                    while(cmd[i] == ' ') i++; // Boşlukları atla
                    char op = cmd[i];
                    i++;
                    
                    // İkinci sayıyı al
                    while(cmd[i] == ' ') i++;
                    int num2 = atoi(&cmd[i]);

                    int result = 0;
                    int valid = 1;
                    
                    // Mantıksal İşlemler
                    if (op == '+') result = num1 + num2;
                    else if (op == '-') result = num1 - num2;
                    else if (op == '*') result = num1 * num2;
                    else if (op == '/') {
                        if (num2 == 0) { valid = 0; strcpy(terminal_response, "Hata: Sifira bolme yapilamaz!"); }
                        else result = num1 / num2;
                    } else {
                        valid = 0;
                        strcpy(terminal_response, "Gecersiz islem! Ornek kullanim: hesapla 25 + 14");
                    }

                    if (valid) {
                        char res_str[32];
                        itoa(result, res_str);
                        strcpy(terminal_response, "Islem Sonucu: ");
                        strcat(terminal_response, res_str);
                    }
                }
                // YENİ KOMUT: Grafik API - Şekil Çizme
                else if (strncmp(cmd, "ciz ", 4) == 0) {
                    char* args = cmd + 4; // "ciz " kelimesinden sonrasını al
                    
                    if (strncmp(args, "temizle", 7) == 0) {
                        shape_count = 0; // Sayacı sıfırlamak şekilleri ekrandan siler!
                        strcpy(terminal_response, "Masaustu tuvali temizlendi!");
                    }
                    else if (strncmp(args, "dikdortgen ", 11) == 0) {
                        if (shape_count < MAX_SHAPES) {
                            int i = 11;
                            
                            // Parametreleri Boşluklardan Atlayarak Ayıkla
                            while(args[i] == ' ') i++;
                            int x = atoi(&args[i]);
                            while((args[i] >= '0' && args[i] <= '9') || args[i] == '-') i++;
                            
                            while(args[i] == ' ') i++;
                            int y = atoi(&args[i]);
                            while((args[i] >= '0' && args[i] <= '9') || args[i] == '-') i++;
                            
                            while(args[i] == ' ') i++;
                            int w = atoi(&args[i]);
                            while((args[i] >= '0' && args[i] <= '9') || args[i] == '-') i++;
                            
                            while(args[i] == ' ') i++;
                            int h = atoi(&args[i]);
                            while((args[i] >= '0' && args[i] <= '9') || args[i] == '-') i++;
                            
                            // Rengi Belirle
                            while(args[i] == ' ') i++;
                            unsigned int c = 0x00FFFFFF; // Varsayılan: Beyaz
                            if (strncmp(&args[i], "kirmizi", 7) == 0) c = 0x00FF2D55;
                            else if (strncmp(&args[i], "yesil", 5) == 0) c = 0x0034C759;
                            else if (strncmp(&args[i], "mavi", 4) == 0) c = 0x000078D7;
                            else if (strncmp(&args[i], "sari", 4) == 0) c = 0x00FFCC00;

                            // RAM'deki listeye kaydet
                            shape_x[shape_count] = x;
                            shape_y[shape_count] = y;
                            shape_w[shape_count] = w;
                            shape_h[shape_count] = h;
                            shape_color[shape_count] = c;
                            shape_count++;

                            strcpy(terminal_response, "Sekil basariyla Ekran Listesine eklendi!");
                        } else {
                            strcpy(terminal_response, "Hata: Ekranda maksimum sekil sayisina (10) ulasildi!");
                        }
                    } 
                    else {
                        strcpy(terminal_response, "Hata: 'ciz dikdortgen <x> <y> <w> <h> <renk>' kullanin.");
                    }
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