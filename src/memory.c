#include "memory.h"
#include "vga.h"

#define BLOCK_SIZE 4096
#define TOTAL_BLOCKS 32768
#define BITMAP_SIZE (TOTAL_BLOCKS / 32)

unsigned int memory_bitmap[BITMAP_SIZE] = {0};

void bitmap_set(int bit) { memory_bitmap[bit / 32] |= (1 << (bit % 32)); }
void bitmap_clear(int bit) { memory_bitmap[bit / 32] &= ~(1 << (bit % 32)); }

int pmm_find_first_free_block() {
    for (int i = 0; i < BITMAP_SIZE; i++) {
        if (memory_bitmap[i] != 0xFFFFFFFF) {
            for (int bit = 0; bit < 32; bit++) {
                if (!(memory_bitmap[i] & (1 << bit))) return (i * 32) + bit;
            }
        }
    }
    return -1;
}

void* pmm_alloc_block() {
    int free_block = pmm_find_first_free_block();
    if (free_block == -1) { print_string("PANIK: OOM!\n"); return 0; }
    bitmap_set(free_block);
    return (void*)(free_block * BLOCK_SIZE);
}

void pmm_free_block(void* physical_address) {
    bitmap_clear((unsigned int)physical_address / BLOCK_SIZE);
}

unsigned int page_directory[1024] __attribute__((aligned(4096)));
unsigned int first_page_table[1024] __attribute__((aligned(4096)));
extern void enable_paging(unsigned int page_dir_address);

void init_paging() {
    for(int i = 0; i < 1024; i++) page_directory[i] = 0x00000002;
    for(unsigned int i = 0; i < 1024; i++) first_page_table[i] = (i * 4096) | 3;
    page_directory[0] = ((unsigned int)first_page_table) | 3;
    enable_paging((unsigned int)page_directory);
    print_string("Sanal Bellek (Paging) mimarisi basariyla aktif edildi!\n");
}