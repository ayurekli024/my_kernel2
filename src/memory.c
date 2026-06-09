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
// ============================================================================
// DİNAMİK BELLEK YÖNETİCİSİ (HEAP / MALLOC / FREE)
// ============================================================================

#define HEAP_START 0x200000 // Heap başlangıç adresi (2 MB noktası)
#define HEAP_SIZE  1048576  // Başlangıç için 1 MB'lık esnek alan

struct block_header* heap_head;

// Heap sistemini ilk kez ayağa kaldıran fonksiyon
void init_heap() {
    heap_head = (struct block_header*) HEAP_START;
    // İlk başta tek bir devasa boş blok var
    heap_head->size = HEAP_SIZE - sizeof(struct block_header);
    heap_head->is_free = 1;
    heap_head->next = 0; // 0 = NULL
    
    print_string("Dinamik Bellek (Heap) 2 MB adresinde baslatildi.\n");
}

// First-Fit algoritması ile çalışan malloc fonksiyonumuz
void* malloc(unsigned int size) {
    if (size == 0) return 0;

    // Bellek hizalaması (Alignment) - İşlemci sağlığı için boyutu 4'ün katlarına yuvarla
    size = (size + 3) & ~3; 

    struct block_header* current = heap_head;

    while (current != 0) {
        // Eğer blok boşsa ve istediğimiz boyuta yetiyorsa
        if (current->is_free && current->size >= size) {
            
            // Eğer bulduğumuz boşluk, istediğimizden ÇOK daha büyükse onu ikiye böl (Split)
            // Bölmeye değmesi için en az bir header + 4 baytlık yer kalması lazım
            if (current->size > size + sizeof(struct block_header) + 4) {
                // Yeni boş bloğun adresini hesapla
                struct block_header* new_block = (struct block_header*)((unsigned int)current + sizeof(struct block_header) + size);
                
                new_block->size = current->size - size - sizeof(struct block_header);
                new_block->is_free = 1;
                new_block->next = current->next;
                
                // Mevcut bloğu daralt ve yeni bloğa bağla
                current->size = size;
                current->next = new_block;
            }
            
            // Bloğu "dolu" olarak işaretle ve verinin yazılacağı adresi kullanıcıya ver
            current->is_free = 0;
            return (void*)((unsigned int)current + sizeof(struct block_header));
        }
        current = current->next; // Bir sonraki bloğa geç (First-Fit döngüsü)
    }
    
    print_string("HATA: Heap uzerinde yeterli bellek kalmadi!\n");
    return 0; // NULL
}

// Tahsis edilen belleği sisteme geri iade eden fonksiyon
void free(void* ptr) {
    if (ptr == 0) return;

    // Kullanıcının veri adresinden geriye giderek gizli kimlik kartını (header) bul
    struct block_header* block = (struct block_header*)((unsigned int)ptr - sizeof(struct block_header));
    block->is_free = 1; // Bloğu serbest bırak
    
    // Parçalanmayı (Fragmentation) önlemek için Blok Birleştirme (Coalescing)
    // Tüm listeyi tara, yan yana duran iki "boş" blok varsa onları tek bir büyük blok yap
    struct block_header* current = heap_head;
    while (current != 0 && current->next != 0) {
        if (current->is_free && current->next->is_free) {
            current->size += current->next->size + sizeof(struct block_header);
            current->next = current->next->next;
        } else {
            current = current->next; // Sadece birleşme yoksa ilerle
        }
    }
}