#include "timer.h"
#include "io.h"

// YENİ: volatile eklendi
volatile unsigned int timer_ticks = 0;

void init_timer(unsigned int frequency) {
    unsigned int divisor = 1193180 / frequency;

    // PIT yongasını ayarla
    outb(0x43, 0x36);
    outb(0x40, (unsigned char)(divisor & 0xFF));
    outb(0x40, (unsigned char)((divisor >> 8) & 0xFF));
    
    // YENİ: Anakartın (PIC) IRQ0 (Saat) kanalındaki susturucuyu (Mask) kesin olarak kaldır!
    // 0x21 portu Master PIC'in veri portudur. En sağdaki biti (Bit 0) '0' yaparak saati açıyoruz.
    outb(0x21, inb(0x21) & 0xFE); 
}

void timer_handler_main() {
    timer_ticks++;
}