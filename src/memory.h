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

#endif