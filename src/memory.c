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

// memory.c içindeki mevcut tanımlar
unsigned int page_directory[1024] __attribute__((aligned(4096)));
unsigned int first_page_table[1024] __attribute__((aligned(4096)));

// YENİ: Çekirdek kodları ve fontlar büyüdüğü için 2. bir çekirdek sayfa tablosu ekliyoruz
unsigned int second_page_table[1024] __attribute__((aligned(4096)));

unsigned int vbe_page_tables[4][1024] __attribute__((aligned(4096)));
extern void enable_paging(unsigned int page_dir_address);

void init_paging(unsigned int framebuffer_addr) {
    // Tüm dizini temizle
    for(int i = 0; i < 1024; i++) page_directory[i] = 0x00000002;
    
    // 1. Tablo: İlk 4 MB'lık alan (0x00000000 - 0x003FFFFF)
    for(unsigned int i = 0; i < 1024; i++) first_page_table[i] = (i * 4096) | 3;
    for(unsigned int i = 0; i < 1024; i++) second_page_table[i] = (0x400000 + (i * 4096)) | 3; // 4 MB ile 8 MB arası

    page_directory[0] = ((unsigned int)first_page_table) | 3;
    page_directory[1] = ((unsigned int)second_page_table) | 3; // Çekirdeğe 8 MB yer açtık!
    
    // --- Geri kalan Ekran Kartı (VBE) haritalama kodun AYNEN KALACAK ---
    if (framebuffer_addr != 0) {
        unsigned int pd_index = framebuffer_addr >> 22;
        if (pd_index > 1) { // Çekirdeği ezmemek için
            for (int t = 0; t < 4; t++) { 
                unsigned int block_start = (pd_index + t) << 22; 
                for(int i = 0; i < 1024; i++) {
                    vbe_page_tables[t][i] = (block_start + (i * 4096)) | 3; 
                }
                page_directory[pd_index + t] = ((unsigned int)vbe_page_tables[t]) | 3;
            }
        }
    }
    
    enable_paging((unsigned int)page_directory);
}
// ============================================================================
// DİNAMİK BELLEK YÖNETİCİSİ (HEAP / MALLOC / FREE)
// ============================================================================

#define HEAP_START 0x200000 // Heap başlangıç adresi (2 MB noktası)
// YENİ: 1 MB'lık alanı 5 MB'a (5242880 bayt) çıkardık! (3 MB ekran + 2 MB ekstra)
#define HEAP_SIZE  5242880  

struct block_header* heap_head;
struct block_header* next_fit_ptr;
// Heap sistemini ilk kez ayağa kaldıran fonksiyon
void init_heap() {
    heap_head = (struct block_header*) HEAP_START;
    heap_head->size = HEAP_SIZE - sizeof(struct block_header);
    heap_head->is_free = 1;
    heap_head->next = 0; 
    
    // Başlangıçta son kalınan yer, doğal olarak heap'in en başıdır
    next_fit_ptr = heap_head; 
    
    print_string("Dinamik Bellek (Heap) 2 MB adresinde baslatildi (Next-Fit aktif).\n");
}

// First-Fit algoritması ile çalışan malloc fonksiyonumuz
// Next-Fit algoritması ile çalışan malloc fonksiyonumuz
void* malloc(unsigned int size) {
    if (size == 0) return 0;

    size = (size + 3) & ~3; // 4 bayt hizalama

    // Aramaya en son kaldığımız yerden başlıyoruz
    struct block_header* current = next_fit_ptr;
    struct block_header* start_search = current; // Döngünün sonsuza girmemesi için başladığımız yeri kaydediyoruz
    int wrapped = 0; // Listenin başına dönüp dönmediğimizi takip eden bayrak

    while (current != 0) {
        // Eğer blok boşsa ve istediğimiz boyuta yetiyorsa
        if (current->is_free && current->size >= size) {
            
            // Split (Bölme) işlemi (Eski kod ile tamamen aynı)
            if (current->size > size + sizeof(struct block_header) + 4) {
                struct block_header* new_block = (struct block_header*)((unsigned int)current + sizeof(struct block_header) + size);
                new_block->size = current->size - size - sizeof(struct block_header);
                new_block->is_free = 1;
                new_block->next = current->next;
                
                current->size = size;
                current->next = new_block;
            }
            
            current->is_free = 0;
            
            // --- NEXT-FIT SİHRİ BURADA ---
            // Bir dahaki sefere aramaya bu bloğun hemen sonrasından başla
            next_fit_ptr = current->next;
            // Eğer heap'in en sonuna geldiysek, işaretçiyi tekrar başa sar
            if (next_fit_ptr == 0) {
                next_fit_ptr = heap_head;
            }
            
            return (void*)((unsigned int)current + sizeof(struct block_header));
        }
        
        current = current->next; // Bir sonraki bloğa geç

        // --- DAİRESEL ARAMA (WRAP-AROUND) ---
        // Eğer listenin sonuna geldiysek ve daha önce başa dönmediysek, başa dön!
        if (current == 0 && !wrapped) {
            current = heap_head;
            wrapped = 1;
        }

        // Eğer başa dönüp tüm listeyi taradıysak ve başladığımız yere geri geldiysek, yer yok demektir.
        if (wrapped && current == start_search) {
            break;
        }
    }
    
    print_string("HATA: Heap uzerinde yeterli bellek kalmadi!\n");
    return 0; 
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
    next_fit_ptr = heap_head;
}