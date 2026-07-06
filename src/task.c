#include "task.h"
#include "memory.h" 
#include "string.h"
task_t* current_task;
task_t* ready_queue;
int next_pid = 1;
extern void set_kernel_stack(unsigned int stack);
void update_tss_esp0() {
    if (current_task != 0) {
        set_kernel_stack(current_task->stack_base + 4096);
    }
}
void init_tasking() {
    current_task = (task_t*)malloc(sizeof(task_t));
    current_task->id = 0;
    current_task->app_base = 0;
    // init_tasking() ve create_task() içindeki atamaların arasına ekle:
    current_task->cpu_ticks = 0; // (create_task için new_task->cpu_ticks = 0;)
    current_task->cpu_usage = 0; // (create_task için new_task->cpu_usage = 0;) 
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
    unsigned int* stack = (unsigned int*)malloc(4096);
    new_task->stack_base = (unsigned int)stack;
    unsigned int stack_top_addr = (unsigned int)stack + 4096; 
    
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
    
    // ...
    unsigned int* stack_top = (unsigned int*)stack_top_addr;
    
    // CDECL STANDARDI: PARAMETRE VE DÖNÜŞ ADRESİ
    *(--stack_top) = (unsigned int)target_args; 
    *(--stack_top) = 0x00000000;                
    
    // Hangi yetki seviyesinde olduğumuzu anla
    int is_user = (app_base != 0); // Arka plan sayacı (0) hariç herkes Ring 3'e düşecek!

    if (is_user) {
        // --- RING 3 (KULLANICI MODU) SAHTE IRET ÇERÇEVESİ ---
        *(--stack_top) = 0x23; // User SS
        
        // YENİ: Uygulamaya kendi 4KB'lık bloğunun alt 3KB'ını veriyoruz, üst 1KB Çekirdeğe kalıyor!
        *(--stack_top) = new_task->stack_base + 3072; 
        
        *(--stack_top) = 0x202; // EFLAGS
        *(--stack_top) = 0x1B; // User CS
        *(--stack_top) = (unsigned int)func; // EIP
    } else {
        // --- RING 0 (ÇEKİRDEK MODU) SAHTE IRET ÇERÇEVESİ ---
        // İşlemci Ring 0'da kalacağı için Stack değişmez, 3 parametre bekler.
        *(--stack_top) = 0x202; // EFLAGS
        *(--stack_top) = 0x08;  // Kernel CS
        *(--stack_top) = (unsigned int)func; // EIP
    }
    
    // --- PUSHA FRAME (8 Yazmaç) ---
    // ... (Aşağıdaki pusha frame aynı kalacak)
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

void kill_task_by_id(int task_id) {
    task_t* curr = ready_queue;
    if (curr == 0) return;
    
    do {
        if (curr->next->id == task_id) { 
            task_t* target = curr->next;
            curr->next = target->next; 
            
            // =========================================================
            // SEÇENEK 1: KUSURSUZ RAM İADESİ (TRUE DEALLOCATION)
            // =========================================================
            // 1. Görevin 4 KB'lık Yığın (Stack) belleğini iade et
            if (target->stack_base != 0) {
                free((void*)target->stack_base);
            }
            
            // 2. Harici uygulama ise (PID >= 2), 4 KB'lık Kod belleğini iade et
            if (target->id >= 2 && target->app_base != 0) {
                free((void*)target->app_base);
            }
            
            // 3. Görevin kendi kayıt bloğunu (task_t) sistemden sil
            free(target);
            
            return; 
        }
        curr = curr->next;
    } while (curr != ready_queue);
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