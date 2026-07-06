#include "idt.h"
#include "io.h"
#include "graphics.h" // En üste eklemeyi unutma!
#include "task.h"

struct IDT_entry {
    unsigned short offset_lowerbits; unsigned short selector;
    unsigned char zero; unsigned char type_attr; unsigned short offset_higherbits;
} __attribute__((packed));

struct IDT_pointer { unsigned short limit; unsigned int base; } __attribute__((packed));

struct IDT_entry idt[256];
struct IDT_pointer idt_ptr;

void idt_set_gate(unsigned char num, unsigned long base, unsigned short sel, unsigned char flags) {
    idt[num].offset_lowerbits = base & 0xFFFF;
    idt[num].offset_higherbits = (base >> 16) & 0xFFFF;
    idt[num].selector = sel; idt[num].zero = 0; idt[num].type_attr = flags;
}

void pic_remap(void) {
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    
    // --- GERÇEK DONANIMSAL İZOLASYON ---
    
    // MASTER PIC: Sadece Klavye (Bit 1) ve Slave Bağlantısı (Bit 2) "0" (AÇIK)
    // Diğer her şey "1" (KAPALI/MASKELİ). İkili: 11111001 = 0xF9
    outb(0x21, 0xF9); 
    
    // SLAVE PIC: Sadece Fare (Bit 4) "0" (AÇIK)
    // Diğer her şey "1" (KAPALI/MASKELİ). İkili: 11101111 = 0xEF
    outb(0xA1, 0xEF); 
}
// ... (Üst kısımdaki pic_remap ve struct tanımları aynı kalacak) ...
extern void api_add_shape(int x, int y, int w, int h, unsigned int color);
extern char last_game_key;
extern void api_clear_shapes(void);
extern int api_create_window(const char* title, int w, int h);
extern int api_get_key(void);
extern int api_write_file(const char*, const char*, unsigned char*);
extern int api_read_file(const char*, const char*, unsigned char*);
extern void api_exit_app(void);
// Yeni Eklenen Dış Bağlantılar
extern void isr_default_ex(void);
extern void isr_default_int(void);
extern void isr0(void);
extern void isr8(void);
extern void isr13(void);
extern void isr14(void);
extern void keyboard_handler(void);
extern void mouse_handler(void);
extern void timer_handler(void);
extern void syscall_handler(void);
extern void yield_handler(void);

extern task_t* current_task;
extern int task_to_kill;
extern void terminal_print(const char* text);
extern void yield(void);
// Mavi Ekran (BSOD) Motoru
void fault_handler(int int_no, int err_code) {
    (void)err_code;
    // ========================================================
    // YENİ: ZEKİ CELLAT (RING 3 KORUMASI)
    // ========================================================
    // Eğer çöken görev Kernel (0) veya System (1) değilse (Yani harici bir uygulamaysa)
    if (current_task != 0 && current_task->id >= 2) {
        
        if (int_no == 14) terminal_print("[ GUVENLIK ] Engellendi! Uygulama Kernel'e saldirdi (Page Fault).");
        else if (int_no == 13) terminal_print("[ GUVENLIK ] Engellendi! Yetkisiz donanim erisimi (GPF).");
        else terminal_print("[ GUVENLIK ] Uygulama kural ihlali yapti ve sonlandirildi!");

        // Görevi ana döngüdeki Cellat motoruna devret (Penceresi ve RAM'i silinecek)
        task_to_kill = current_task->id; 
        current_task->state = 1; 
        
        // Kazayla dönerse diye işlemciyi yormayan sonsuz bir karantina döngüsü
        while(1) {
            yield(); 
        }
        return; // İşlemci asla buraya ulaşmaz ama C dili kuralları gereği koyuyoruz
    }
    extern void reset_clipping_rect(void);
    extern void mark_screen_dirty(void);
    reset_clipping_rect();
    mark_screen_dirty();
    draw_rect(0, 0, 1024, 768, 0x000000AA);
    draw_string(50, 50, "FATAL ERROR: KERNEL PANIC", 0x00FFFFFF, 0x000000AA);
    draw_string(50, 80, "Isletim sisteminiz kritik bir hatayla karsilasti ve durduruldu.", 0x00FFFFFF, 0x000000AA);

    if (int_no == 0) {
        draw_string(50, 110, "HATA KODU: 0 (Divide by Zero)", 0x00FFFFFF, 0x000000AA);
    } else if (int_no == 6) {
        draw_string(50, 110, "HATA KODU: 6 (Invalid Opcode - Gecersiz CPU Komutu)", 0x00FFFFFF, 0x000000AA);
    } else if (int_no == 8) {
        draw_string(50, 110, "HATA KODU: 8 (Double Fault - Cifte Hata!)", 0x00FFFFFF, 0x000000AA);
    } else if (int_no == 11) {
        draw_string(50, 110, "HATA KODU: 11 (Segment Not Present)", 0x00FFFFFF, 0x000000AA);
    } else if (int_no == 13) {
        draw_string(50, 110, "HATA KODU: 13 (General Protection Fault)", 0x00FFFFFF, 0x000000AA);
    } else if (int_no == 14) {
        draw_string(50, 110, "HATA KODU: 14 (Page Fault - Gecersiz Bellek Erisimi)", 0x00FFFFFF, 0x000000AA);
    } else if (int_no == 255) {
        draw_string(50, 110, "HATA KODU: Yakalanmamis Bir CPU Istisnasi Olustu!", 0x00FFFFFF, 0x000000AA);
    } else {
        draw_string(50, 110, "HATA KODU: Bilinmeyen Istisna", 0x00FFFFFF, 0x000000AA);
    }

    draw_string(50, 150, "Lutfen sisteminizi fiziksel olarak yeniden baslatin.", 0x00FFFFFF, 0x000000AA);

    // YENİ: Mavi ekranı RAM'den çıkarıp gerçek monitöre itiyoruz!
    extern void swap_buffers(void);
    swap_buffers();

    while(1) {
        __asm__ __volatile__ ("cli; hlt");
    }
}

void init_idt(void) {
    idt_ptr.limit = (sizeof(struct IDT_entry) * 256) - 1;
    idt_ptr.base = (unsigned int)&idt;
    
    // 1. ZIRH: İlk 32 Kapı CPU Hatalarıdır! (Doğrudan Mavi Ekrana gider)
    for(int i = 0; i < 32; i++) {
        idt_set_gate(i, (unsigned long)isr_default_ex, 0x08, 0x8E);
    }
    
    // 2. ZIRH: 32-255 Arası Donanım Kesmeleridir! (Çökmeyi engeller, yola devam eder)
    for(int i = 32; i < 256; i++) {
        idt_set_gate(i, (unsigned long)isr_default_int, 0x08, 0x8E);
    }
    
    // Kendi yazdığımız özel hata yakalayıcıları üzerine zımbalıyoruz
    // 2. ADIM: Kendi kapılarımızı (üzerine yazarak) sisteme tanıtıyoruz
    idt_set_gate(0, (unsigned long)isr0, 0x08, 0x8E);
    idt_set_gate(8, (unsigned long)isr8, 0x08, 0x8E);   // YENİ: Sessiz kilitlenmeyi çözen kapı!
    idt_set_gate(13, (unsigned long)isr13, 0x08, 0x8E);
    idt_set_gate(14, (unsigned long)isr14, 0x08, 0x8E);
    idt_set_gate(32, (unsigned long)timer_handler, 0x08, 0x8E);
    // Kendi yazdığımız donanım sürücüleri
    idt_set_gate(33, (unsigned long)keyboard_handler, 0x08, 0x8E);
    idt_set_gate(44, (unsigned long)mouse_handler, 0x08, 0x8E);
    // YENİ: DPL 3 (0xEE) ile bu kesmeleri Ring 3 (Kullanıcı) moduna açıyoruz!
    idt_set_gate(128, (unsigned long)syscall_handler, 0x08, 0xEE); 
    idt_set_gate(129, (unsigned long)yield_handler, 0x08, 0xEE);
    __asm__ __volatile__ ("lidt %0" : : "m" (idt_ptr));
}
// ==========================================
// ARDAOS API MERKEZİ (SYSCALL ROUTER)
// ==========================================
int syscall_handler_main(unsigned int sys_num, unsigned int arg1, unsigned int arg2, unsigned int arg3, unsigned int arg4, unsigned int arg5) {
    // API No 1: Nokta Çiz (put_pixel)
    // Beklenen: arg1=x, arg2=y, arg3=color
    if (sys_num == 1) {
        put_pixel(arg1, arg2, arg3);
        return 1; // Başarılı
    }
    
    // API No 2: Dikdörtgen/Pencere Çiz (draw_rect)
    // Beklenen: arg1=x, arg2=y, arg3=w, arg4=h, arg5=color
    else if (sys_num == 2) {
        draw_rect(arg1, arg2, arg3, arg4, arg5);
        return 1; 
    }
    
    // API No 3: Ekrana Yazı Yaz (draw_string)
    // Beklenen: arg1=x, arg2=y, arg3=metin_adresi, arg4=fg_color, arg5=bg_color
    else if (sys_num == 3) {
        draw_string(arg1, arg2, (const char*)arg3, arg4, arg5);
        return 1;
    }
    else if (sys_num == 4) {
        yield();
        return 1;
    }
    
    // API No 5: Masaüstüne Kalıcı Şekil Ekle (Ekran silinse bile kalır)
    else if (sys_num == 5) {
        api_add_shape(arg1, arg2, arg3, arg4, arg5);
        return 1;
    }
     
    else if (sys_num == 6) {
        return api_get_key();
    }
    else if (sys_num == 7) {
        api_clear_shapes();
        return 1;
    }
    // API No 8: Harici Uygulamalar İçin Dinamik Pencere Oluşturucu
    else if (sys_num == 8) {
        return api_create_window((const char*)arg1, arg2, arg3);
    }
    // YENİ - API No 9: Manuel Çıkış ve RAM İadesi (sys_exit)
    // API No 9: Manuel Çıkış ve Zombi İşaretleme (sys_exit)
    else if (sys_num == 9) {
        extern void api_exit_app(void);
        api_exit_app(); // Çekirdeğe git ve CPU'yu devret
        return 1;
    }
    // API No 10: Dosya Yazma
    else if (sys_num == 10) {
        return api_write_file((const char*)arg1, (const char*)arg2, (unsigned char*)arg3);
    }
    // API No 11: Dosya Okuma
    else if (sys_num == 11) {
        return api_read_file((const char*)arg1, (const char*)arg2, (unsigned char*)arg3);
    }
    // API No 12: Terminale Mesaj Yazdirma (sys_print)
    else if (sys_num == 12) {
        extern void api_print(const char*);
        api_print((const char*)arg1);
        return 1;
    }
    // API No 13: Asenkron Tuş Okuma (Oyunlar için)
    else if (sys_num == 13) {
        extern int api_poll_key(void);
        return api_poll_key();
    }
    // Bilinmeyen API numarası gelirse hata kodu (-1) döndür
    return -1;
}
