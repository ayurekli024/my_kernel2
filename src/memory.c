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

// ============================================================================
// SAYFALAMA (PAGING) TABLOLARI VE YENİ İZOLASYON MİMARİSİ
// ============================================================================
unsigned int page_directory[1024] __attribute__((aligned(4096)));
unsigned int first_page_table[1024] __attribute__((aligned(4096))); // 0 - 4 MB
unsigned int heap_page_table[1024] __attribute__((aligned(4096)));  // 8 - 12 MB
unsigned int dma_page_table[1024] __attribute__((aligned(4096)));   // 12 - 16 MB

unsigned int vbe_page_tables[4][1024] __attribute__((aligned(4096)));
extern void enable_paging(unsigned int page_dir_address);

void init_paging(unsigned int framebuffer_addr) {
    for(int i = 0; i < 1024; i++) page_directory[i] = 0x00000002;
    
    // 1. Tablo (0 - 4 MB) -> Kernel, VGA ve BSS
    for(unsigned int i = 0; i < 1024; i++) {
        first_page_table[i] = (i * 4096) | 3;
    }
    
    // PD[1] (4 - 8 MB) BİLEREK BOŞ BIRAKILDI! 
    // Burası sadece uygulamalara (0x400000 adresine) tahsis edilecek.
    
    // 3. Tablo (8 - 12 MB) -> Kernel Heap (Dinamik Bellek buraya taşındı)
    for(unsigned int i = 0; i < 1024; i++) {
        heap_page_table[i] = (0x800000 + (i * 4096)) | 7;
    }

    // 4. Tablo (12 - 16 MB) -> Ağ Kartı DMA & IPC Toplantı Odası
    for(unsigned int i = 0; i < 1024; i++) {
        dma_page_table[i] = (0xC00000 + (i * 4096)) | 7;
    }

    // Dizinleri (Directory) yerleştir
    page_directory[0] = ((unsigned int)first_page_table) | 7; 
    // page_directory[1] BOŞ KALDI!
    page_directory[2] = ((unsigned int)heap_page_table) | 7; 
    page_directory[3] = ((unsigned int)dma_page_table) | 7; 
    
    // Grafik Kartı Belleği (Genelde 16MB üstündedir, dinamik hesaplanır)
    if (framebuffer_addr != 0) {
        unsigned int pd_index = framebuffer_addr >> 22;
        if (pd_index > 3) { 
            for (int t = 0; t < 4; t++) { 
                unsigned int block_start = (pd_index + t) << 22; 
                for(int i = 0; i < 1024; i++) vbe_page_tables[t][i] = (block_start + (i * 4096)) | 7; 
                page_directory[pd_index + t] = ((unsigned int)vbe_page_tables[t]) | 7;
            }
        }
    }
    enable_paging((unsigned int)page_directory);
}

// IPC Toplantı Odasının Adresi
void* api_get_shared_memory(void) {
    return (void*)0xE00000; // 14. MB'a güvenli bölgeye taşındı
}

// ============================================================================
// DMA (DOĞRUDAN BELLEK ERİŞİMİ) YÖNETİCİSİ - AĞ KARTI İÇİN
// ============================================================================
unsigned int dma_memory_pointer = 0xC00000; // 12. MB'a taşındı

void* dma_alloc(unsigned int size) {
    void* ptr = (void*)dma_memory_pointer;
    dma_memory_pointer += size;
    if (dma_memory_pointer % 4 != 0) {
        dma_memory_pointer += (4 - (dma_memory_pointer % 4));
    }
    return ptr;
}

// ============================================================================
// DİNAMİK BELLEK YÖNETİCİSİ (HEAP / MALLOC / FREE)
// ============================================================================
#define HEAP_START 0x800000 // BÜYÜK GÖÇ: Heap başlangıcı 8. MB'a alındı!
#define HEAP_SIZE  4194304  // Tam 4 MB'lık devasa alan

struct block_header* heap_head;
struct block_header* next_fit_ptr;
unsigned int total_used_memory = 0;

void init_heap() {
    heap_head = (struct block_header*) HEAP_START;
    heap_head->size = HEAP_SIZE - sizeof(struct block_header);
    heap_head->is_free = 1;
    heap_head->next = 0; 
    next_fit_ptr = heap_head; 
    print_string("Dinamik Bellek (Heap) 8. MB adresine guvenle tasindi.\n");
}

void* malloc(unsigned int size) {
    if (size == 0) return 0;
    size = (size + 3) & ~3; 

    struct block_header* current = next_fit_ptr;
    struct block_header* start_search = current; 
    int wrapped = 0; 

    while (current != 0) {
        if (current->is_free && current->size >= size) {
            if (current->size > size + sizeof(struct block_header) + 4) {
                struct block_header* new_block = (struct block_header*)((unsigned int)current + sizeof(struct block_header) + size);
                new_block->size = current->size - size - sizeof(struct block_header);
                new_block->is_free = 1;
                new_block->next = current->next;
                
                current->size = size;
                current->next = new_block;
            }
            current->is_free = 0;
            
            next_fit_ptr = current->next;
            if (next_fit_ptr == 0) next_fit_ptr = heap_head;
            total_used_memory += current->size;
            return (void*)((unsigned int)current + sizeof(struct block_header));
        }
        current = current->next; 

        if (current == 0 && !wrapped) {
            current = heap_head;
            wrapped = 1;
        }
        if (wrapped && current == start_search) break;
    }
    print_string("HATA: Heap uzerinde yeterli bellek kalmadi!\n");
    return 0; 
}

void free(void* ptr) {
    if (ptr == 0) return;
    struct block_header* block = (struct block_header*)((unsigned int)ptr - sizeof(struct block_header));
    block->is_free = 1; 
    total_used_memory -= block->size;

    struct block_header* current = heap_head;
    while (current != 0 && current->next != 0) {
        if (current->is_free && current->next->is_free) {
            current->size += current->next->size + sizeof(struct block_header);
            current->next = current->next->next;
        } else {
            current = current->next; 
        }
    }
    next_fit_ptr = heap_head;
}

// ==========================================
// UYGULAMALAR İÇİN İZOLE FİZİKSEL BELLEK (PER-PROCESS PAGING)
// ==========================================
extern unsigned int page_directory[1024];

// YENİ ZIRH: MMU (Sayfa Tabloları) için kusursuz 4096 bayt hizalı bellek ayırıcı
void* alloc_page_aligned() {
    extern unsigned int dma_memory_pointer;
    // Eğer adres 4096'nın tam katı değilse, onu zorla bir sonraki 4096'ya yuvarla!
    if (dma_memory_pointer % 4096 != 0) {
        dma_memory_pointer += (4096 - (dma_memory_pointer % 4096));
    }
    void* ptr = (void*)dma_memory_pointer;
    dma_memory_pointer += 4096; // 1 Tablo = 4 KB
    return ptr;
}

// ---------------------------------------------------------
unsigned int task_page_dirs[8][1024] __attribute__((aligned(4096)));
int next_task_id = 0;

unsigned int* create_task_page_dir() {
    if (next_task_id >= 8) return (unsigned int*)page_directory; 
    
    unsigned int* pd = task_page_dirs[next_task_id];
    
    // Sadece Çekirdek haritasını (Kernel, Heap, DMA) klonla.
    // Uygulamanın sayfalarını Akıllı Yükleyici (map_vaddr_to_paddr) kendisi hatasız ekleyecek!
    for(int i = 0; i < 1024; i++) {
        pd[i] = page_directory[i];
    }
    
    next_task_id++;
    return pd;
}
// ============================================================================
// YENİ: UYGULAMALAR İÇİN SINIRSIZ FİZİKSEL HAFIZA HAVUZU (16. MB'DAN BAŞLAR)
// ============================================================================
// 16 MB (0x1000000) sınırı: Kernel, DMA ve Ortak Hafıza'nın (GUI) tamamen bittiği yer!
unsigned int user_phys_memory = 0x1000000; 

void* alloc_user_page() {
    void* ptr = (void*)user_phys_memory;
    user_phys_memory += 4096; // Her istekte 4KB (1 Sayfa) ver
    return ptr;
}