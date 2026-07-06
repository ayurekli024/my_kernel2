#ifndef MEMORY_H
#define MEMORY_H



void bitmap_set(int bit);
void* pmm_alloc_block();
void pmm_free_block(void* physical_address);
void init_paging(unsigned int framebuffer_addr);

// (önceki kodların altı)

// --- DİNAMİK BELLEK (HEAP) YAPILARI ---
struct block_header {
    unsigned int size;         // Bloğun içindeki kullanılabilir veri boyutu
    int is_free;               // 1 ise boş, 0 ise kullanımda
    struct block_header* next; // Listedeki bir sonraki bloğun adresi
};
extern unsigned int total_used_memory;
void init_heap();
void* malloc(unsigned int size);
void free(void* ptr);
void* api_get_shared_memory(void);
#endif // MEMORY_H bitişinin hemen üstünde olduğundan emin ol