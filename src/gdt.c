#include "gdt.h"

// GDT Bellek Bloğu Tanımı
struct GDT_entry {
    unsigned short limit_low;
    unsigned short base_low;
    unsigned char base_middle;
    unsigned char access;
    unsigned char granularity;
    unsigned char base_high;
} __attribute__((packed));

struct GDT_pointer {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

struct GDT_entry gdt[3];
struct GDT_pointer gdt_ptr;

// Assembly'de yazacağımız harita yükleyici
extern void gdt_flush(unsigned int); 

void gdt_set_gate(int num, unsigned int base, unsigned int limit, unsigned char access, unsigned char gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[num].access = access;
}

void init_gdt() {
    gdt_ptr.limit = (sizeof(struct GDT_entry) * 3) - 1;
    gdt_ptr.base = (unsigned int)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);                // 1. Kural: Her zaman bir NULL kapısı olmalı
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // 2. Kural: Çekirdek Kod Alanı (0x08)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // 3. Kural: Çekirdek Veri Alanı (0x10)

    gdt_flush((unsigned int)&gdt_ptr);
}