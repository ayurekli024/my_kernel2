#include "idt.h"
#include "io.h"
#include "graphics.h" // En üste eklemeyi unutma!


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

// Mavi Ekran (BSOD) Motoru
void fault_handler(int int_no, int err_code) {
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
    
    __asm__ __volatile__ ("lidt %0" : : "m" (idt_ptr));
}