#ifndef GDT_H
#define GDT_H

// TSS (Task State Segment) - Donanım düzeyinde yığın değiştirici
struct tss_entry_struct {
    unsigned int prev_tss;
    unsigned int esp0; // Çekirdek (Ring 0) yığınının adresi
    unsigned int ss0;  // Çekirdek Veri Segmenti seçicisi (0x10)
    unsigned int esp1, ss1, esp2, ss2;
    unsigned int cr3;
    unsigned int eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
    unsigned int es, cs, ss, ds, fs, gs;
    unsigned int ldt;
    unsigned short trap, iomap_base;
} __attribute__((packed));

void init_gdt(void);
void set_kernel_stack(unsigned int stack); // Görev değiştiğinde TSS'i güncelleyecek fonksiyon

#endif