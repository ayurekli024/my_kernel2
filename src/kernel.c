#include "multiboot.h"
#include "vga.h"
#include "idt.h"
#include "memory.h"
#include "timer.h"
#include "task.h"
#include "graphics.h"

unsigned int* vesa_framebuffer;

void kernel_main(unsigned int magic, struct multiboot_info* mb_info) {
    pic_remap();     
    init_idt();      

    if (magic != 0x2BADB002) return; 

    if (mb_info->flags & (1 << 12)) { 
        vesa_framebuffer = (unsigned int*)(unsigned int)mb_info->framebuffer_addr;
    }

    // --- ARDA OS MASAÜSTÜ ARAYÜZÜ (GUI) ---
    if (vesa_framebuffer != 0) {
        // 1. Grafik motoruna ekranın boyutlarını bildir (1024x768)
        init_graphics(vesa_framebuffer, 1024, 768);

        // 2. Masaüstü Arka Planı (Koyu Turkuaz / Gece Mavisi)
        // Renk Formatı (ARGB): 0x00RRGGBB
        draw_rect(0, 0, 1024, 768, 0x001B26); 

        // 3. Görev Çubuğu (Ekranın en alt kısmı, koyu gri)
        draw_rect(0, 728, 1024, 40, 0x00111A); 

        // 4. Ekrana İlk Pencereyi Çiz! (Beyaz zemin)
        // X: 300, Y: 200 konumuna 400x300 ebatlarında bir pencere
        draw_rect(300, 200, 400, 300, 0x00F0F0F0); 

        // 5. Pencere Başlığı (Titlebar - Klasik Mavi)
        draw_rect(300, 200, 400, 30, 0x000078D7); 
        
        // 6. Pencere Kapatma Butonu (Kırmızı)
        // Başlığın sağ üst köşesine minik bir kare çiziyoruz
        draw_rect(670, 205, 20, 20, 0x00E81123);
    }

    while(1) {
        __asm__ __volatile__ ("hlt"); 
    }
}