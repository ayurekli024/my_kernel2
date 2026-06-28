#include "sound.h"
#include "io.h"
#include "timer.h" // timer_ticks sayacımız için

extern void yield(void); // Multitasking motorumuzdan gelen sihirli değnek

// Sistemi dondurmadan (diğer pencereleri kitlemeden) bekletme fonksiyonu
void sleep(unsigned int ms) {
    // Zamanlayıcımız 100 Hz, yani 1 tick = 10 milisaniye
    unsigned int ticks_to_wait = ms / 10; 
    if (ticks_to_wait == 0) ticks_to_wait = 1;
    
    unsigned int target_ticks = timer_ticks + ticks_to_wait;
    
    while (timer_ticks < target_ticks) {
        // Beklerken işlemciyi boşa döndürme, sıradaki göreve devret!
        yield(); 
    }
}

// Belirtilen frekansta ses üretir
void play_sound(unsigned int frequency) {
    if (frequency == 0) return;
    
    unsigned int divisor = 1193180 / frequency;
    
    // PIT'i kare dalga (Square Wave) üreteci moduna geçir
    outb(0x43, 0xB6);
    outb(0x42, (unsigned char)(divisor & 0xFF));
    outb(0x42, (unsigned char)((divisor >> 8) & 0xFF));
    
    // Hoparlörü aç (Port 0x61'in 0. ve 1. bitlerini 1 yap)
    unsigned char tmp = inb(0x61);
    if (tmp != (tmp | 3)) {
        outb(0x61, tmp | 3);
    }
}

// Hoparlörü susturur
void nosound() {
    unsigned char tmp = inb(0x61) & 0xFC; // İlk iki biti sıfırla
    outb(0x61, tmp);
}

// Standart hata / uyarı biplemesi
void beep() {
    play_sound(1000); // 1000 Hz (Tiz bir bip)
    sleep(100);       // 100 milisaniye basılı tut
    nosound();        // Kapat
}