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
#include "disk.h"

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
// Dışarıdan gelen API istekleri için kalıcı şekil ekleyici
void api_add_shape(int x, int y, int w, int h, unsigned int color) {
    if (shape_count < MAX_SHAPES) {
        shape_x[shape_count] = x;
        shape_y[shape_count] = y;
        shape_w[shape_count] = w;
        shape_h[shape_count] = h;
        shape_color[shape_count] = color;
        shape_count++;
    }
}
// ==========================================
    // KOMUT GEÇMİŞİ (COMMAND HISTORY)
    // ==========================================
    #define MAX_HISTORY 10
    char cmd_history[MAX_HISTORY][256];
    int history_count = 0; // Toplam kaç komut yazıldı
    int history_index = 0; // Ok tuşlarıyla gezinirken nerede olduğumuz
    // ==========================================
// PENCERE YÖNETİCİSİ (WINDOW MANAGER)
// ==========================================
#define MAX_WINDOWS 2

typedef struct {
    int id;
    int x, y, w, h;
    int is_open;
    int is_dragging;
    char title[32];
} window_t;

window_t windows[MAX_WINDOWS];
int focused_window = 0; // Hangi pencere aktif?
int any_window_dragging = 0; // Herhangi bir pencere sürükleniyor mu?

// SADECE pencereyi çizen yeni, modüler fonksiyonumuz
void draw_window(window_t* win) {
    if (!win->is_open) return;
    
    // Pencere Gövdesi
    draw_rect(win->x, win->y, win->w, win->h, 0x00F0F0F0); 
    
    // Z-Index Görselliği: Seçili olan pencerenin başlığı Mavi, arkadakilerin Gri olsun
    unsigned int title_color = (win->id == focused_window) ? 0x000078D7 : 0x00777777;
    draw_rect(win->x, win->y, win->w, 30, title_color); 
    
    // Kırmızı Kapatma (X) Butonu
    draw_rect(win->x + win->w - 30, win->y, 30, 30, 0x00FF2D55); 
    
    // Başlık Yazısı
    draw_string(win->x + 10, win->y + 8, win->title, 0x00FFFFFF, title_color);
}

// ARTIK SADECE ARKA PLANI VE ŞEKİLLERİ ÇİZER
void draw_desktop(unsigned int desktop_bg) {
    // 1. Masaüstü Arka Planı
    draw_rect(0, 0, 1024, 768, desktop_bg); 

    // 2. Terminalden Gelen Özel Şekiller
    for (int s = 0; s < shape_count; s++) {
        draw_rect(shape_x[s], shape_y[s], shape_w[s], shape_h[s], shape_color[s]);
    }

    // 3. Görev Çubuğu
    draw_rect(0, 728, 1024, 40, 0x00111A); 
}
int task1_counter = 0;

// İKİNCİ ÇEKİRDEK GÖREVİ (Sonsuz Döngü 2)
void background_task() {
    while(1) {
        __asm__ __volatile__("sti");
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

    init_timer(100);
    __asm__ __volatile__ ("sti");

    // ESKİ SABİT DEĞİŞKENLERİ VE İLK ÇİZİM KODLARINI SİLDİK
    int last_mouse_x = mouse_x, last_mouse_y = mouse_y;

    // Terminal Mimarisi
    unsigned int current_bg_color = 0x001B26; // Varsayılan arka plan
    char terminal_response[512] = "ArdaOS V0.1'e Hos Geldiniz!\n\nKullanilabilir Komutlar:\n- info\n- temizle\n- renk mavi\n- renk kirmizi";
    char user_input[256] = "Arda> "; 
    int input_idx = 6; 

    // Saat için CMOS fonksiyonları (Zaten eklemiştin)
    unsigned char get_rtc_register(int reg) {
        outb(0x70, reg);
        return inb(0x71);
    }
    unsigned char bcd_to_bin(unsigned char bcd) {
        return (bcd & 0x0F) + ((bcd >> 4) * 10);
    }

    // Sayaçlar
    unsigned int system_ticks = 0;
    int last_second = -1;

    // ==========================================
    // PENCERELERİ KUR VE BAŞLAT
    // ==========================================
    
    // Pencere 0: Terminalimiz
    windows[0].id = 0; windows[0].is_open = 1; windows[0].is_dragging = 0;
    windows[0].x = 100; windows[0].y = 100; windows[0].w = 450; windows[0].h = 350;
    strcpy(windows[0].title, "ArdaOS Terminali");

    // Pencere 1: Canlı Sistem Monitörü
    windows[1].id = 1; windows[1].is_open = 1; windows[1].is_dragging = 0;
    windows[1].x = 600; windows[1].y = 150; windows[1].w = 300; windows[1].h = 200;
    strcpy(windows[1].title, "Sistem Monitoru");

    focused_window = 0;
    // ==========================================
    // HARD DISK (ATA PIO) OKUMA/YAZMA TESTİ
    // ==========================================
    
    // 1. Diske yazılacak veriyi hazırla (512 bayt boyutunda, çünkü sektörler 512 bayttır)
    char write_buffer[512] = "Merhaba Arda! Bu yazi tamamen RAM disindan, fiziksel Hard Diskten okunmustur!";
    
    // 2. Diskin 5. Sektörüne (LBA 5) bu yazıyı kaydet!
    ata_lba_write(5, 1, (unsigned short*)write_buffer);

    // 3. Yazının gerçekten diske gidip gitmediğini kanıtlamak için boş bir okuma alanı yarat
    char read_buffer[512];
    for(int i=0; i<512; i++) read_buffer[i] = 0; // İçi bomboş olsun

    // 4. Diskin 5. Sektöründen veriyi oku ve boş olan read_buffer'a aktar
    ata_lba_read(5, 1, (unsigned short*)read_buffer);

    // 5. Okunan bu veriyi doğrudan terminalin varsayılan karşılama mesajına ekle!
    strcpy(terminal_response, "ArdaOS Disk Testi Sonucu:\n[ ");
    strcat(terminal_response, read_buffer);
    strcat(terminal_response, " ]\n\n- info\n- temizle\n- hesapla");
    // (Buradan sonra while(1) döngün başlıyor, oraya dokunma!)
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
        // --- 2. OLAY: GELİŞMİŞ FARE KONTROLÜ VE Z-INDEX (ESKİ FARE KODUNU BUNUNLA DEĞİŞTİR) ---
        int delta_x = mouse_x - last_mouse_x;
        int delta_y = mouse_y - last_mouse_y;

        if (delta_x != 0 || delta_y != 0) {
            needs_redraw = 1;

            if (mouse_left_button) {
                if (!any_window_dragging) { 
                    int clicked_window = -1;

                    // Önce Z-Index'te en üstte olan (Focuslanmış) pencereye tıklanmış mı bak
                    if (windows[focused_window].is_open &&
                        mouse_x >= windows[focused_window].x && mouse_x <= windows[focused_window].x + windows[focused_window].w &&
                        mouse_y >= windows[focused_window].y && mouse_y <= windows[focused_window].y + windows[focused_window].h) {
                        clicked_window = focused_window;
                    } else {
                        // Diğer pencerelere bak
                        for (int i = 0; i < MAX_WINDOWS; i++) {
                            if (windows[i].is_open && i != focused_window &&
                                mouse_x >= windows[i].x && mouse_x <= windows[i].x + windows[i].w &&
                                mouse_y >= windows[i].y && mouse_y <= windows[i].y + windows[i].h) {
                                clicked_window = i;
                                break;
                            }
                        }
                    }

                    // Eğer bir pencereye tıklandıysa:
                    if (clicked_window != -1) {
                        focused_window = clicked_window; // O pencereyi öne getir (Focus)

                        // 1. Kırmızı X Butonuna mı tıkladı?
                        if (mouse_x >= windows[focused_window].x + windows[focused_window].w - 30 &&
                            mouse_x <= windows[focused_window].x + windows[focused_window].w &&
                            mouse_y >= windows[focused_window].y && mouse_y <= windows[focused_window].y + 30) {
                            windows[focused_window].is_open = 0; // Pencereyi KAPAT!
                        }
                        // 2. Başlık çubuğuna mı tıkladı? (Sürükleme)
                        else if (mouse_y >= windows[focused_window].y && mouse_y <= windows[focused_window].y + 30) {
                            windows[focused_window].is_dragging = 1;
                            any_window_dragging = 1;
                        }
                    }
                }

                // Sürükleme işlemi gerçekleşiyorsa pencere koordinatlarını güncelle
                if (any_window_dragging && windows[focused_window].is_dragging) {
                    windows[focused_window].x += delta_x;
                    windows[focused_window].y += delta_y;
                }
            } else {
                // Sol tık bırakıldığında sürüklemeleri sıfırla
                for (int i = 0; i < MAX_WINDOWS; i++) windows[i].is_dragging = 0;
                any_window_dragging = 0;
            }
            last_mouse_x = mouse_x; last_mouse_y = mouse_y;
        }

        // 3. OLAY: KLAVYE VE KOMUT PARSER
        char kbd_char = get_keyboard_char();
        if (kbd_char != 0) {
            needs_redraw = 1;

            if (kbd_char == '\n') { 
                char* cmd = &user_input[6]; 
                if (strcmp(cmd, "") != 0) {
                    strcpy(cmd_history[history_count % MAX_HISTORY], cmd);
                    history_count++;
                    history_index = history_count; // İmleci en sona (yeni boşluğa) al
                }
                if (strcmp(cmd, "info") == 0) {
                    strcpy(terminal_response, "Sistem: ArdaOS V0.1\nMimari: 32-bit x86\nCekirdek Durumu: Kararli\nGUI: Double-Buffered");
                } 
                else if (strcmp(cmd, "temizle") == 0) {
                    strcpy(terminal_response, ""); 
                } 
                // ====================================================
                // YENİ: HARİCİ UYGULAMA YÜKLEYİCİ (EXECUTABLE LOADER)
                // ====================================================
                else if (strcmp(cmd, "testapp") == 0) {
                    // 1. Uygulama için RAM'de yer ayır (4 KB)
                    unsigned char* app_memory = (unsigned char*)malloc(4096); 
                    
                    if (app_memory != 0) {
                        // 2. Diske git ve FAT16 indeksimizden dosyayı bulup RAM'e çek
                        int file_size = ardaos_read_file("TESTAPP ", "BIN", app_memory);
                        
                        if (file_size > 0) {
                            // 3. RAM'deki bu rastgele byte yığınını bir "Fonksiyon İşaretçisine" (Function Pointer) çevir
                            void (*app_entry)() = (void (*)())app_memory;
                            
                            // 4. Çekirdeğin Çoklu Görev (Multitasking) motoruna yeni bir görev olarak ekle!
                            create_task(app_entry);
                            
                            strcpy(terminal_response, "[ BASARILI ] TESTAPP.BIN diskten okundu ve RAM'de calistirildi!\nEkrandaki yesil kareye bakin.");
                        } else {
                            free(app_memory); // Dosya yoksa RAM'i geri ver
                            strcpy(terminal_response, "Hata: TESTAPP.BIN sanal diskte (c.img) bulunamadi.");
                        }
                    } else {
                        strcpy(terminal_response, "Hata: Uygulamayi yuklemek icin yeterli RAM yok!");
                    }
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
            else if (kbd_char == 17) { 
                if (history_count > 0 && history_index > 0) {
                    history_index--; // Bir önceki komuta git
                    strcpy(user_input, "Arda> ");
                    strcat(user_input, cmd_history[history_index % MAX_HISTORY]);
                    input_idx = 6 + strlen(cmd_history[history_index % MAX_HISTORY]);
                }
            }
            // YENİ: AŞAĞI OK TUŞU (Günümüze Dön)
            else if (kbd_char == 18) { 
                if (history_index < history_count) {
                    history_index++; // Bir sonraki komuta git
                    if (history_index == history_count) {
                        // En sona (günümüze) geldiysek satırı temizle
                        strcpy(user_input, "Arda> ");
                        input_idx = 6;
                    } else {
                        // Aradaki bir komuttaysak onu yazdır
                        strcpy(user_input, "Arda> ");
                        strcat(user_input, cmd_history[history_index % MAX_HISTORY]);
                        input_idx = 6 + strlen(cmd_history[history_index % MAX_HISTORY]);
                    }
                }
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

        // --- 4. EKRANI GÜNCELLE ---
        if (needs_redraw) {
            draw_desktop(current_bg_color); // Arka plan
            
            // DÜZELTME: Pencereleri ve içeriklerini Z-Index sırasına göre (Arkadakinden öndekine) çizen kuyruk mantığı
            int draw_order[MAX_WINDOWS];
            int order_idx = 0;
            
            // 1. Önce arkadaki pencereleri kuyruğa ekle
            for (int i = 0; i < MAX_WINDOWS; i++) {
                if (i != focused_window) draw_order[order_idx++] = i;
            }
            // 2. En son odaklanılan (Focus) pencereyi kuyruğa ekle ki en üste çizilsin
            draw_order[order_idx++] = focused_window;

            // 3. Kuyruğu sırayla çiz (İskelet + İçerik)
            for (int i = 0; i < MAX_WINDOWS; i++) {
                int w_idx = draw_order[i];
                if (!windows[w_idx].is_open) continue;
                
                draw_window(&windows[w_idx]); // Önce pencerenin iskeleti
                
                // Hemen ardından SADECE o pencerenin içeriğini yazdır!
                if (w_idx == 0) {
                    draw_string_wrapped(windows[0].x + 10, windows[0].y + 40, windows[0].w - 20, terminal_response, 0x00000000, 0xFFFFFFFF); 
                    draw_string_wrapped(windows[0].x + 10, windows[0].y + windows[0].h - 30, windows[0].w - 20, user_input, 0x000000AA, 0xFFFFFFFF); 
                } 
                else if (w_idx == 1) {
                    char info_text[128] = "Sistem Calisma Suresi:\n";
                    char sec_str[10];
                    itoa(current_second, sec_str);
                    strcat(info_text, sec_str);
                    strcat(info_text, " Saniye\n\nBellek Durumu: OK\nMultitasking: Aktif");
                    draw_string_wrapped(windows[1].x + 10, windows[1].y + 50, windows[1].w - 20, info_text, 0x00000000, 0xFFFFFFFF);
                }
            }

            // --- HARİCİ UYGULAMA SİMÜLASYONU (SYSCALL TESTİ) ---
            int syscall_result;
            __asm__ __volatile__ (
                "mov $2, %%eax \n\t"          
                "mov $400, %%ebx \n\t"        
                "mov $50, %%ecx \n\t"         
                "mov $120, %%edx \n\t"        
                "mov $120, %%esi \n\t"        
                "mov $0x00FF00FF, %%edi \n\t" 
                "int $0x80 \n\t"              
                "mov %%eax, %0"               
                : "=r" (syscall_result)
                :
                : "eax", "ebx", "ecx", "edx", "esi", "edi"
            );
            
            draw_cursor(mouse_x, mouse_y);
            swap_buffers();
        }

        yield(); 
        __asm__ __volatile__ ("sti; hlt");
    }
}