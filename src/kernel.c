#include "vga.h"
#include "idt.h"
#include "memory.h"

void kernel_main(unsigned int magic, struct multiboot_info* mb_info) {
    clear_screen();  
    pic_remap();     
    init_idt();      
    __asm__ __volatile__ ("sti"); 
    
    print_string("ArdaOS surum 0.0.1 basariyla yuklendi!\n");

    if (magic != 0x2BADB002) {
        print_string("HATA: Multiboot uyumsuz!\n");
        return;
    }

    if (mb_info->flags & 0x01) {
        unsigned int total_memory_mb = (mb_info->mem_upper / 1024) + 1;
        print_string("Tespit Edilen RAM: "); print_number(total_memory_mb); print_string(" MB\n");
        
        for (int i = 0; i < 256; i++) {
            bitmap_set(i);
        }
    }
    
    init_paging();
    
    print_string("> "); 

    while(1) {
        __asm__ __volatile__ ("hlt"); 
    }
}