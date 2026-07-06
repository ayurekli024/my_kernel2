#include "gdt.h"

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

// GDT'yi 6 kapıya çıkardık: NULL, KERNEL_CS, KERNEL_DS, USER_CS, USER_DS, TSS
struct GDT_entry gdt[6];
struct GDT_pointer gdt_ptr;
struct tss_entry_struct tss_entry;

extern void gdt_flush(unsigned int); 
extern void tss_flush(void); // Assembly'den gelecek TSS yükleyici

void gdt_set_gate(int num, unsigned int base, unsigned int limit, unsigned char access, unsigned char gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[num].access = access;
}

// Yeni görev başladığında, Çekirdek yığınını (ESP0) TSS'ye yazar
void set_kernel_stack(unsigned int stack) {
    tss_entry.esp0 = stack;
}

void write_tss(int num, unsigned short ss0, unsigned int esp0) {
    unsigned int base = (unsigned int)&tss_entry;
    unsigned int limit = base + sizeof(tss_entry);

    // TSS Kapısını GDT'ye Ekle (Access: 0x89 -> TSS Present & Executable)
    gdt_set_gate(num, base, limit, 0x89, 0x40);

    // Belleği sıfırla
    // Belleği sıfırla
    for (unsigned int i = 0; i < sizeof(tss_entry); i++) {
        ((unsigned char*)&tss_entry)[i] = 0;
    }

    tss_entry.ss0 = ss0;
    tss_entry.esp0 = esp0;
    
    // TSS'in diğer alanlarını varsayılan ayarla
    tss_entry.cs = 0x08 | 0x3; // Ring 0 çalıştırılır ama 3 yetkisiyle çağrılabilir
    tss_entry.ss = tss_entry.ds = tss_entry.es = tss_entry.fs = tss_entry.gs = 0x10 | 0x3;
}

void init_gdt() {
    gdt_ptr.limit = (sizeof(struct GDT_entry) * 6) - 1;
    gdt_ptr.base = (unsigned int)&gdt;

    // 1. Kural: NULL Kapısı
    gdt_set_gate(0, 0, 0, 0, 0);                
    // 2. Kural: Çekirdek Kod Alanı (0x08)
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); 
    // 3. Kural: Çekirdek Veri Alanı (0x10)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); 
    
    // ===============================================
    // YENİ: KULLANICI MODU KAPILARI (RING 3 - DPL 3)
    // ===============================================
    // 4. Kural: Kullanıcı Kod Alanı (0x18 | 3 = 0x1B) - Access: 0xFA
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); 
    // 5. Kural: Kullanıcı Veri Alanı (0x20 | 3 = 0x23) - Access: 0xF2
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); 

    // 6. Kural: TSS (Görev Durum Segmenti) Kapısı (0x28)
    // Geçici olarak ESP0'ı sıfır veriyoruz, task.c içinde dinamik atanacak
    write_tss(5, 0x10, 0);

    gdt_flush((unsigned int)&gdt_ptr);
    tss_flush(); // İşlemciye TSS kapısının numarasını (0x2B) öğretiyoruz
}