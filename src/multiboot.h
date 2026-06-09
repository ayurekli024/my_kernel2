#ifndef MULTIBOOT_H
#define MULTIBOOT_H

// GRUB'ın işletim sistemimize donanım ve grafik bilgilerini ilettiği standart yapı
struct multiboot_info {
    unsigned int flags;
    unsigned int mem_lower;
    unsigned int mem_upper;
    unsigned int boot_device;
    unsigned int cmdline;
    unsigned int mods_count;
    unsigned int mods_addr;
    unsigned int num;
    unsigned int size;
    unsigned int addr;
    unsigned int shndx;
    unsigned int mmap_length;
    unsigned int mmap_addr;
    unsigned int drives_length;
    unsigned int drives_addr;
    unsigned int config_table;
    unsigned int boot_loader_name;
    unsigned int apm_table;

    // VBE (Video BIOS Extension) Bilgileri
    unsigned int vbe_control_info;
    unsigned int vbe_mode_info;
    unsigned short vbe_mode;
    unsigned short vbe_interface_seg;
    unsigned short vbe_interface_off;
    unsigned short vbe_interface_len;

    // BİZE LAZIM OLAN GRAFİK (FRAMEBUFFER) BİLGİLERİ BURADA
    unsigned long long framebuffer_addr;
    unsigned int framebuffer_pitch;
    unsigned int framebuffer_width;
    unsigned int framebuffer_height;
    unsigned char framebuffer_bpp;
    unsigned char framebuffer_type;
    unsigned char color_info[6];
} __attribute__((packed));

#endif