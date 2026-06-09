#include "idt.h"
#include "io.h"
#include "vga.h" // En üste eklemeyi unutma!


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
    outb(0x21, 0xFC); outb(0xA1, 0xFF); 
}
extern void timer_handler(void);
extern void keyboard_handler(void); 
extern void isr0(void);

void init_idt(void) {
    idt_ptr.limit = (sizeof(struct IDT_entry) * 256) - 1;
    idt_ptr.base = (unsigned int)&idt;
    
    // YENİ: İstisna 0 (Sıfıra Bölme) -> 0. Sıra
    idt_set_gate(0, (unsigned long)isr0, 0x08, 0x8E);
    
    // IRQ0 (Saat) -> 32. Kesme
    idt_set_gate(32, (unsigned long)timer_handler, 0x08, 0x8E);
    
    // IRQ1 (Klavye) -> 33. Kesme
    idt_set_gate(33, (unsigned long)keyboard_handler, 0x08, 0x8E);
    
    __asm__ __volatile__ ("lidt %0" : : "m" (idt_ptr));
}
// Ölümcül hataları ekrana yazdıran Kernel Panic motoru
void fault_handler(int int_no, int err_code) {
    clear_screen();
    
    print_string("==================================================\n");
    print_string("                   KERNEL PANIC                   \n");
    print_string("==================================================\n");
    
    if (int_no == 0) {
        print_string("HATA KODU 0: Sifira Bolme (Divide by Zero) Istisnasi!\n");
    } else {
        print_string("Bilinmeyen bir islemci hatasi olustu.\n");
    }

    print_string("\nIsletim sistemi guvenlik amaciyla durduruldu.\n");
    print_string("Lutfen sisteminizi yeniden baslatin.\n");

    // İşlemciyi sonsuza kadar kilitliyoruz. Artık klavye veya saat çalışmayacak.
    while(1) {
        __asm__ __volatile__ ("cli; hlt");
    }
}