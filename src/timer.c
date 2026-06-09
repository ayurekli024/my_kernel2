#include "timer.h"
#include "io.h"
#include "vga.h"
#include "task.h"
// Sistemin açılışından bu yana geçen "tik" sayısı (Volatile olmalı ki derleyici optimize edip silmesin)
volatile unsigned int timer_ticks = 0;
unsigned int timer_freq = 100;

// Saniyede 100 kez donanımdan gelen kesme (Interrupt) fonksiyonu
void timer_handler_main(void) {
    timer_ticks++;
    outb(0x20, 0x20); // İşlemciye kesmenin bittiğini bildir (EOI sinyali)

    // PREEMPTIVE MULTITASKING: Her tik (10 ms) geldiğinde programı zorla değiştir!
    schedule(); 
}

void init_timer(unsigned int freq) {
    timer_freq = freq;
    
    // 1.193182 MHz'lik ana saati istediğimiz frekansa bölüyoruz
    unsigned int divisor = 1193180 / freq;

    // 0x43 komut portuna: Kanal 0, Alt/Üst Bayt, Mod 3 (Kare Dalga) sinyali yolluyoruz
    outb(0x43, 0x36);
    
    // 0x40 veri portuna bölen sayımızın önce alt, sonra üst 8 bitini gönderiyoruz
    outb(0x40, (unsigned char)(divisor & 0xFF));
    outb(0x40, (unsigned char)((divisor >> 8) & 0xFF));

    print_string("Sistem Saati (PIT) ");
    print_number(freq);
    print_string(" Hz ile baslatildi.\n");
}

// Tik sayısını frekansa bölerek saniyeyi buluyoruz
unsigned int get_uptime() {
    return timer_ticks / timer_freq; 
}

// Belirtilen milisaniye kadar işlemciyi bekleten fonksiyon
// Belirtilen milisaniye kadar işlemciyi bekleten fonksiyon
void sleep(unsigned int ms) {
    // İşlem başındaki başlangıç zamanımızı kaydediyoruz
    unsigned int start = timer_ticks;
    
    // Daha güvenli matematik: Önce çarp, sonra böl (Küsürat kayıplarını önler)
    unsigned int ticks_to_wait = (ms * timer_freq) / 1000;
    
    // Eğer süre çok kısaysa en az 1 tik (10ms) beklemesini garanti altına al
    if (ticks_to_wait == 0) ticks_to_wait = 1;
    
    // Başlangıçtan bu yana geçen tik sayısı, hedefi aşana kadar bekle
    while ((timer_ticks - start) < ticks_to_wait) {
        // ÇÖZÜM 1: İşlemciyi uyuturken kesmelerin (interrupt) açık olduğundan emin ol
        // ÇÖZÜM 2: Eğer QEMU hala yavaş çalışıyorsa buradaki "sti; hlt" kısmını silip
        // sadece "nop" (No Operation) yazarak Busy-Wait (Meşgul Bekleme) yapabilirsin.
        __asm__ __volatile__ ("sti; nop"); 
    }
}