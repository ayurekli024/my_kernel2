#include "multiboot.h"
#include "idt.h"
#include "memory.h"
#include "timer.h"
#include "task.h"
#include "graphics.h"
#include "mouse.h"
#include "io.h"
#include "gdt.h"

unsigned int* vesa_framebuffer;
extern void outb(unsigned short port, unsigned char data);
extern unsigned char inb(unsigned short port);
// Masaüstü bileşenlerini çizen yardımcı fonksiyon
void draw_desktop(void) {
    // 1. Masaüstü Arka Planı (Koyu Turkuaz / Gece Mavisi) - 0x00RRGGBB
    draw_rect(0, 0, 1024, 768, 0x001B26); 

    // 2. Görev Çubuğu (Ekranın en alt kısmı, koyu gri)
    draw_rect(0, 728, 1024, 40, 0x00111A); 

    // 3. İlk Pencere (Beyaz zemin)
    draw_rect(300, 200, 400, 300, 0x00F0F0F0); 

    // 4. Pencere Başlığı (Titlebar - Klasik Mavi)
    draw_rect(300, 200, 400, 30, 0x000078D7); 
    
    // 5. Pencere Kapatma Butonu (Kırmızı)
    draw_rect(670, 205, 20, 20, 0x00E81123);

    // --- YAZI MOTORU METİNLERİ ---
    // Pencere başlığı yazıları (Şeffaf arka plan için 0xFFFFFFFF)
    draw_string(310, 211, "ArdaOS V0.1", 0x00FFFFFF, 0xFFFFFFFF);
    draw_string(676, 211, "X", 0x00FFFFFF, 0xFFFFFFFF); 

    // Pencerenin içindeki çok satırlı (\n) metin
    draw_string(320, 250, "Masaustu sistemine hos geldiniz!\nAlt satira gecis basariyla saglandi.\n\nSistem Durumu: Korumali Mod Aktif.", 0x00000000, 0xFFFFFFFF);
}

void kernel_main(unsigned int magic, struct multiboot_info* mb_info) {
    // 1. Her ihtimale karşı donanım kesmelerini geçici olarak kapatıyoruz
    __asm__ __volatile__ ("cli"); 
    init_gdt();
    pic_remap();     
    init_idt();      
    init_mouse(); // PS/2 Fare denetleyicisini kur

    if (magic != 0x2BADB002) return; 

    if (mb_info->flags & (1 << 12)) { 
        vesa_framebuffer = (unsigned int*)(unsigned int)mb_info->framebuffer_addr;
    }
    
    // 16 MB'lık dev zırhlı sayfalama (Paging) haritasını ve Heap'i aktif et
    init_paging((unsigned int)vesa_framebuffer); 
    init_heap();
    
    // Temel çizim motoru başlatılıyor
    if (vesa_framebuffer != 0) {
        init_graphics(vesa_framebuffer, 1024, 768);
        draw_desktop(); // Masaüstünü ilk kez çiz
    }

    __asm__ __volatile__ ("sti");
    // Eski fare koordinatını takip etmek için değişkenler
    int last_mouse_x = mouse_x;
    int last_mouse_y = mouse_y;

    while(1) {
        // Eğer fare hareket ettiyse ekranı güncelle
        if (mouse_x != last_mouse_x || mouse_y != last_mouse_y) {
            // Şimdilik basit bir temizlik: Farenin eski yerini arka plan rengine boya
            // (İleride bunun yerine double-buffering mimarisine geçeceğiz)
            draw_rect(last_mouse_x, last_mouse_y, 5, 5, 0x001B26); 
            
            // Yeni koordinatları güncelle
            last_mouse_x = mouse_x;
            last_mouse_y = mouse_y;
        }

        // Farenin bulunduğu güncel noktaya kırmızı imleci çiz
        draw_rect(mouse_x, mouse_y, 5, 5, 0x00FF0000); 

        // İşlemciyi bir sonraki donanım (fare/klavye) sinyaline kadar uyut
        __asm__ __volatile__ ("hlt"); 
    }
}