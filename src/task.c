#include "task.h"
#include "memory.h" 
#include "string.h"
task_t* current_task;
task_t* ready_queue;
int next_pid = 1;
extern void set_kernel_stack(unsigned int stack);
// ELF motorunun varlığından derleyiciye önceden haber veriyoruz
unsigned int load_elf_segments(unsigned char* elf_data, unsigned int* page_dir, unsigned char* phys_base);
void update_tss_esp0() {
    if (current_task != 0) {
        // YENİ: Kernel Stack alanı 8KB'ın en tepesine çekildi
        set_kernel_stack(current_task->stack_base + 8192);
    }
}
void init_tasking() {
    current_task = (task_t*)malloc(sizeof(task_t));
    current_task->id = 0;
    current_task->app_base = 0;
    
    current_task->cpu_ticks = 0; 
    current_task->cpu_usage = 0; 

    // ==========================================
    // YENİ EKLENEN KISIM: KERNEL HAFIZA HARİTASI
    // ==========================================
    extern unsigned int page_directory[1024];
    current_task->cr3 = (unsigned int)page_directory; 
    // ==========================================

    current_task->next = current_task; 
    ready_queue = current_task;
}

int create_task(void (*func)(void), unsigned int app_base, char* args) {
    task_t* new_task = (task_t*)malloc(sizeof(task_t));
    new_task->id = next_pid++;
    new_task->app_base = app_base; 
    // init_tasking() ve create_task() içindeki atamaların arasına ekle:
    new_task->cpu_ticks = 0;
    new_task->cpu_usage = 0;
    new_task->state = 0; // Yeni görev varsayılan olarak "Çalışabilir" başlar
    // YENİ ZIRH: Stack boyutu 4KB'dan 8KB'a çıkarıldı! 
    unsigned int* stack = (unsigned int*)malloc(8192);
    new_task->stack_base = (unsigned int)stack;
    
    // Stack'in en tepesi artık 8192
    unsigned int stack_top_addr = (unsigned int)stack + 8192;
    
    // =========================================================
    // 1. ARGÜMANI YIĞINA (STACK) GİZLİCE KOPYALA
    // =========================================================
    char* target_args = 0;
    if (args != 0 && args[0] != '\0') {
        int arg_len = strlen(args) + 1;
        stack_top_addr -= arg_len;
        strcpy((char*)stack_top_addr, args); // Metni stack'in tepesine yaz
        target_args = (char*)stack_top_addr; // Adresini kaydet
        stack_top_addr &= ~3;                // Stack'i GCC'nin sevdiği gibi 4 bayta hizala
    }
    
    // ... (stack_top_addr ayarları ve strcpy kısımları aynı kalacak) ...
    
    unsigned int* stack_top = (unsigned int*)stack_top_addr;
    
    // Hangi yetki seviyesinde olduğumuzu anla
    int is_user = (app_base != 0); // Arka plan sayacı (0) hariç herkes Ring 3'e düşecek!
    // ==========================================
    // YENİ EKLENEN KISIM: DIŞ FONKSİYON BAĞLANTILARI
    // ==========================================
    extern unsigned int* create_task_page_dir(void);
    extern unsigned int page_directory[1024];
    if (is_user) {
        new_task->cr3 = (unsigned int)create_task_page_dir();
        unsigned int kernel_cr3;
        
        __asm__ __volatile__("cli"); 
        __asm__ __volatile__("mov %%cr3, %0" : "=r"(kernel_cr3) : : "memory"); 
        __asm__ __volatile__("mov %0, %%cr3" : : "r"(new_task->cr3) : "memory");

        unsigned int actual_entry_point = (unsigned int)func;
        
        // YENİ: Akıllı Yükleyici Çağrısı! (Ham Veri, Uygulamanın CR3 Haritası, Fiziksel RAM)
        unsigned int elf_entry = load_elf_segments((unsigned char*)func, (unsigned int*)new_task->cr3, (unsigned char*)app_base);
        if (elf_entry != 0) {
            actual_entry_point = elf_entry; 
        }

        __asm__ __volatile__("mov %0, %%cr3" : : "r"(kernel_cr3)); 
        __asm__ __volatile__("sti");
        
        unsigned int* user_stack_top = (unsigned int*)(new_task->stack_base + 4096);
        *(--user_stack_top) = (unsigned int)target_args;
        *(--user_stack_top) = 0x00000000;

        *(--stack_top) = 0x23; 
        *(--stack_top) = (unsigned int)user_stack_top; 
        *(--stack_top) = 0x202; 
        *(--stack_top) = 0x1B; 
        *(--stack_top) = actual_entry_point; // DÜZELTME: Doğru tespit edilen EIP yazılıyor
    } else {
        // ==========================================
        // YENİ: KERNEL GÖREVLERİ ANA HARİTAYI KULLANIR (CR3)
        // ==========================================
        new_task->cr3 = (unsigned int)page_directory;

        // --- RING 0 (ÇEKİRDEK MODU) SAHTE IRET ÇERÇEVESİ ---
        // Çekirdek görevleri User ESP kullanmaz, argümanları kendi yığınına ister
        *(--stack_top) = (unsigned int)target_args;
        *(--stack_top) = 0x00000000;

        *(--stack_top) = 0x202; // EFLAGS
        *(--stack_top) = 0x08;  // Kernel CS
        *(--stack_top) = (unsigned int)func; // EIP
    }

    // --- PUSHA FRAME (8 Yazmaç) ---
    *(--stack_top) = 0; // EAX
    *(--stack_top) = 0; // ECX
    *(--stack_top) = 0; // EDX
    *(--stack_top) = 0; // EBX
    *(--stack_top) = 0; // ESP
    *(--stack_top) = 0; // EBP
    *(--stack_top) = 0; // ESI
    *(--stack_top) = 0; // EDI
    
    new_task->esp = (unsigned int)stack_top;
    
    new_task->next = ready_queue->next;
    ready_queue->next = new_task;
    
    return new_task->id; 
}
// YENİ: Uyuyan bir görevi dışarıdan uyandırma servisi
void wake_task_by_id(int task_id) {
    if (ready_queue == 0) return;
    
    task_t* curr = ready_queue;
    do {
        if (curr->id == task_id) {
            curr->state = 0; // 0 = RUNNABLE (Uykudan uyandır)
            return;
        }
        curr = curr->next;
    } while (curr != ready_queue);
}
// ARTIK TASK_SWITCH YOK! Yield doğrudan donanım kesmesini tetikler.
void yield() {
    __asm__ __volatile__ ("int $129"); // Saati bozmayan yeni görev değiştiricimiz!
}

// KUSURSUZ CELLAT: Statik hafızayı silmesini engelleyen zırh!
void kill_task_by_id(int task_id) {
    task_t* curr = ready_queue;
    if (curr == 0) return;
    do {
        if (curr->next->id == task_id) { 
            task_t* target = curr->next;
            
            if (ready_queue == target) {
                ready_queue = target->next;
            }
            curr->next = target->next; 
            
            if (target->stack_base != 0) {
                extern void free(void*);
                free((void*)target->stack_base);
            }
            
            if (target->id >= 2 && target->app_base != 0) {
                extern void free(void*);
                // KUSURSUZ CELLAT: Hizalanmış bellekten 4 bayt geriye bakarak orijinal adresi bul ve onu sil!
                unsigned int original_ptr = *((unsigned int*)(target->app_base - 4));
                free((void*)original_ptr);
            }
            
            extern void free(void*);
            free(target);
            return; 
        }
        curr = curr->next;
    } while (curr != ready_queue);
}

// =========================================================================
// 1. ZIRH: SANAL BELLEK YÖNLENDİRİCİSİ (Virtual to Physical Memory Mapper)
// =========================================================================
void map_vaddr_to_paddr(unsigned int* page_dir, unsigned int vaddr, unsigned int paddr) {
    unsigned int pdindex = vaddr >> 22;
    unsigned int ptindex = (vaddr >> 12) & 0x03FF;
    
    unsigned int* pt;
    if ((page_dir[pdindex] & 1) == 0) { // Page Table (Sayfa Tablosu) yoksa yarat
        
        // KUSURSUZ ZIRH: Malloc kullanılamaz çünkü MMU 4096 katları ister!
        extern void* alloc_page_aligned(void);
        pt = (unsigned int*)alloc_page_aligned();
        
        for(int i = 0; i < 1024; i++) pt[i] = 0;
        page_dir[pdindex] = ((unsigned int)pt) | 7; // Present, R/W, User
    } else {
        pt = (unsigned int*)(page_dir[pdindex] & 0xFFFFF000);
    }
    pt[ptindex] = (paddr & 0xFFFFF000) | 7; // Present, R/W, User
}

// =========================================================================
// 2. ZIRH: AKILLI ELF YÜKLEYİCİ (Fiziksel RAM'e yazar, Sanal RAM'e bağlar)
// =========================================================================
unsigned int load_elf_segments(unsigned char* elf_data, unsigned int* page_dir, unsigned char* phys_base) {
    elf32_ehdr_t* header = (elf32_ehdr_t*)elf_data;
    if (header->e_ident[0] != 0x7F) return 0; // ELF değilse çık
    
    elf32_phdr_t* phdr = (elf32_phdr_t*)(elf_data + header->e_phoff);
    for (int i = 0; i < header->e_phnum; i++) {
        if (phdr[i].p_type == 1) { // PT_LOAD (Yüklenebilir Segment)
            unsigned int vaddr = phdr[i].p_vaddr;
            unsigned int memsz = phdr[i].p_memsz;
            unsigned int filesz = phdr[i].p_filesz;
            unsigned int offset = phdr[i].p_offset;
            
            // A) Fiziksel RAM'e (malloc ile alınan yere) kopyala
            unsigned char* dest = phys_base + offset;
            unsigned char* src = elf_data + offset;
            for (unsigned int j = 0; j < filesz; j++) dest[j] = src[j];
            for (unsigned int j = filesz; j < memsz; j++) dest[j] = 0;
            
            // B) MMU İLLÜZYONU: Fiziksel RAM'i, uygulamanın beklediği Sanal Adrese (vaddr) bağla!
            unsigned int pages = (memsz / 4096) + 1;
            for (unsigned int p = 0; p < pages; p++) {
                map_vaddr_to_paddr(page_dir, vaddr + (p * 4096), (unsigned int)(phys_base + offset + (p * 4096)));
            }
        }
    }
    return header->e_entry; 
}
void get_process_list(char* buffer) {
    strcpy(buffer, "PID | DURUM  | BELLEK ADRESI\n");
    strcat(buffer, "-----------------------------\n");
    
    if (ready_queue == 0) {
        strcat(buffer, "Calisan gorev yok.\n");
        return;
    }
    
    task_t* curr = ready_queue;
    do {
        char pid_str[10];
        itoa(curr->id, pid_str);
        
        strcat(buffer, " ");
        strcat(buffer, pid_str);
        if (curr->id < 10) strcat(buffer, "  | ");
        else strcat(buffer, " | ");
        
        // Çekirdek mi, sistem görevi mi yoksa harici uygulama mı?
        if (curr->id == 0) strcat(buffer, "KERNEL | ");
        else if (curr->id == 1) strcat(buffer, "SYSTEM | ");
        else strcat(buffer, "APP    | ");
        
        // Bellek adresini profesyonelce Hex (0x...) formatında yaz
        char hex_str[16] = "0x";
        unsigned int addr = curr->app_base;
        char hex_chars[] = "0123456789ABCDEF";
        int idx = 2;
        for (int i = 28; i >= 0; i -= 4) {
            hex_str[idx++] = hex_chars[(addr >> i) & 0x0F];
        }
        hex_str[idx] = '\0';
        
        strcat(buffer, hex_str);
        strcat(buffer, "\n");
        
        curr = curr->next;
    } while (curr != ready_queue);
}
