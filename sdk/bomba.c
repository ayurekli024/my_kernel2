#include "ardaos.h"

__attribute__((section(".text.entry")))
void _start(char* args) {
    sys_create_window("Kotu Amacli Yazilim", 300, 150);
    sys_print("BOMBA CALISTI! 3 Saniye icinde sisteme saldiracak...");

    // YENİ: sys_yield() SİLİNDİ! Sadece işlemciyi boş yere yoruyoruz.
    // İşletim sisteminin donanım saati (IRQ0) zaten arka planda sistemi yaşatmaya devam edecek!
    for (volatile int i = 0; i < 80000000; i++) { 
        // İçi tamamen boş
    }

    // SALDIRI BAŞLIYOR! (Ring 3'ten Ring 0'ın kalbine yazmaya çalış)
    volatile unsigned int* kernel_memory = (unsigned int*)0x00000000;
    
    *kernel_memory = 0xDEADBEEF; // BUM!

    sys_print("Eger bu yaziyi goruyorsan, Ring 3 CALISMIYOR demektir!");
    
    sys_exit();
    while(1);
}