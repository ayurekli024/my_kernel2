#ifndef MEMORY_H
#define MEMORY_H

struct multiboot_info {
    unsigned int flags;
    unsigned int mem_lower;
    unsigned int mem_upper;
} __attribute__((packed));

void bitmap_set(int bit);
void* pmm_alloc_block();
void pmm_free_block(void* physical_address);
void init_paging();

// (önceki kodların altı)

// --- DİNAMİK BELLEK (HEAP) YAPILARI ---
struct block_header {
    unsigned int size;         // Bloğun içindeki kullanılabilir veri boyutu
    int is_free;               // 1 ise boş, 0 ise kullanımda
    struct block_header* next; // Listedeki bir sonraki bloğun adresi
};

void init_heap();
void* malloc(unsigned int size);
void free(void* ptr);

#endif // MEMORY_H bitişinin hemen üstünde olduğundan emin ol