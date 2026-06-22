#include "task.h"
#include "memory.h" 
#include "string.h"
task_t* current_task;
task_t* ready_queue;
int next_pid = 1;

void init_tasking() {
    current_task = (task_t*)malloc(sizeof(task_t));
    current_task->id = 0;
    current_task->app_base = 0; 
    current_task->next = current_task; 
    ready_queue = current_task;
}

int create_task(void (*func)(void), unsigned int app_base, char* args) {
    task_t* new_task = (task_t*)malloc(sizeof(task_t));
    new_task->id = next_pid++;
    new_task->app_base = app_base; 
    
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
    
    unsigned int* stack_top = (unsigned int*)stack_top_addr;
    
    // =========================================================
    // 2. CDECL STANDARDI: PARAMETRE VE DÖNÜŞ ADRESİ
    // =========================================================
    *(--stack_top) = (unsigned int)target_args; // [ESP + 4]: 1. Argüman (char* args)
    *(--stack_top) = 0x00000000;                // [ESP + 0]: Sahte Dönüş Adresi (sys_exit çağırmazsa çöker)
    
    // --- KUSURSUZ ZIRH: SAHTE DONANIM KESMESİ (FORGED IRET FRAME) ---
    *(--stack_top) = 0x202; // EFLAGS
    *(--stack_top) = 0x08;  // CS
    *(--stack_top) = (unsigned int)func; // EIP (Fonksiyon adresi)
    
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