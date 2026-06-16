#include "task.h"
#include "memory.h" // malloc için

task_t* current_task;
task_t* ready_queue;
int next_pid = 1;

extern void task_switch(unsigned int *prev_esp, unsigned int next_esp);

void init_tasking() {
    // 1. O an çalışan Kernel_main döngümüzü "Task 0" olarak tescilliyoruz
    current_task = (task_t*)malloc(sizeof(task_t));
    current_task->id = 0;
    current_task->next = current_task; // Kuyrukta şimdilik sadece kendisi var
    ready_queue = current_task;
}

void create_task(void (*func)(void)) {
    task_t* new_task = (task_t*)malloc(sizeof(task_t));
    
    new_task->id = next_pid++;
    
    // Her yeni görev için 4 KB'lık özel bir yığın (Stack) tahsis et
    unsigned int* stack = (unsigned int*)malloc(4096);
    new_task->stack_base = (unsigned int)stack;
    unsigned int* stack_top = (unsigned int*)((unsigned int)stack + 4096); // Yığınlar yukarıdan aşağı büyür
    
    // İşlemciyi kandırmak için yığına sahte kayıtlar (Fake Registers) diziyoruz
    *(--stack_top) = (unsigned int)func; // RET komutunun atlayacağı adres (Fonksiyonun kendisi)
    *(--stack_top) = 0; // EBP
    *(--stack_top) = 0; // EBX
    *(--stack_top) = 0; // ESI
    *(--stack_top) = 0; // EDI
    
    new_task->esp = (unsigned int)stack_top;
    
    // Yeni görevi listeye ekle (Round Robin Dairesi)
    new_task->next = ready_queue->next;
    ready_queue->next = new_task;
}

// O anki görevi durdurup işlemciyi sıradaki göreve devreden sihirli fonksiyon
void yield() {
    task_t* prev = current_task;
    current_task = current_task->next;
    if (prev != current_task) {
        task_switch(&prev->esp, current_task->esp);
    }
}
// YENİ: Silinecek görevi tutan Zombie işaretçisi
// Çekirdek (0) ve Arka plan (1) hariç her görevi acımasızca silen fonksiyon
void kill_app_task() {
    task_t* curr = ready_queue;
    if (curr == 0) return;
    
    do {
        // Eğer görev ID'si 1'den büyükse (yani bu bir harici uygulamaysa)
        if (curr->next->id > 1) { 
            task_t* target = curr->next;
            curr->next = target->next; // Görevi zincirden sonsuza dek kopar!
            
            //free((void*)target->stack_base); // Görevin 4KB'lık yığınını iade et
            //free(target);                    // Görevin kimlik kartını iade et
            return; // İşi bitir
        }
        curr = curr->next;
    } while (curr != ready_queue);
}