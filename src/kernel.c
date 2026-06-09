#include "vga.h"
#include "idt.h"
#include "memory.h"
#include "timer.h"
#include "task.h"
#include "multiboot.h"

unsigned int* vesa_framebuffer;
void task_A() {
    // İLK ÇALIŞMA SİHRİ: Zamanlayıcının (PIT) bizi tekrar vurabilmesi için kapıları açıyoruz!
    __asm__ __volatile__ ("sti"); 

    while(1) {
        print_string("A");
        // yield(); SİLDİK! Artık insiyatif programda değil.
    }
}

void task_B() {
    __asm__ __volatile__ ("sti"); 

    while(1) {
        print_string("B");
        // yield(); SİLDİK!
    }
}
void kernel_main(unsigned int magic, struct multiboot_info* mb_info) {
    // DİKKAT: clear_screen(); SİLİNDİ! (Artık metin modunda değiliz)
    
    pic_remap();     
    init_idt();      
    __asm__ __volatile__ ("sti"); 

    if (magic != 0x2BADB002) {
        return; // Hata durumunda yapacak bir şey yok, ekranımız yok
    }

    // GRUB'ın grafik modunu başarıyla açıp açmadığını kontrol et
    if (mb_info->flags & (1 << 12)) { // 12. bit Framebuffer bilgisini belirtir
        // 64-bitlik adresi 32-bitlik işaretçimize sığdırıyoruz
        vesa_framebuffer = (unsigned int*)(unsigned int)mb_info->framebuffer_addr;
    }

    if (mb_info->flags & 0x01) {
        unsigned int total_memory_mb = (mb_info->mem_upper / 1024) + 1;
        for (int i = 0; i < 256; i++) {
            bitmap_set(i);
        }
    }
    
    init_paging();
    init_heap();
    init_timer(100); 
    
    // --- GRAFİK TESTİ: EKRANI MAVİYE BOYAYALIM ---
    // Eğer vesa_framebuffer başarıyla alındıysa, tüm pikselleri tek tek boyuyoruz.
    // 32-bit renk formatı: 0x00RRGGBB (Kırmızı, Yeşil, Mavi)
    if (vesa_framebuffer != 0) {
        for (int y = 0; y < 768; y++) {
            for (int x = 0; x < 1024; x++) {
                vesa_framebuffer[y * 1024 + x] = 0x000000FF; // Saf Mavi
            }
        }
    }

    while(1) {
        __asm__ __volatile__ ("hlt"); 
    }
}