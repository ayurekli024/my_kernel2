#include "ardaos.h"

// DÜZELTME: Bu fonksiyonun kesinlikle en baştan başlamasını emrediyoruz
void __attribute__((section(".entry"))) _start() {
    // Yemyeşil karemiz
    add_shape(700, 400, 100, 100, 0x0034C759);
    
    // Uygulamayı uyut ve sırayı çekirdeğe sal
    sys_halt();
}