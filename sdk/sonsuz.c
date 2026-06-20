#include "ardaos.h"

__attribute__((section(".text.entry")))
void _start() {
    sys_print("Sonsuz Dongu Testi Basladi! OS Kilitlenmemeli!");
    
    // DİKKAT: İçinde sys_yield() YOK! Saf ve acımasız bir sonsuz döngü.
    // Eğer Preemptive mimarimiz çalışmıyorsa, ArdaOS bu satırda tamamen DONAR!
    while(1) {
        // İşlemciyi %100 meşgul et
    }
}