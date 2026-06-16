#include "task.h"
#include "memory.h" 

task_t* current_task;
task_t* ready_queue;
int next_pid = 1;

extern void task_switch(unsigned int *prev_esp, unsigned int next_esp);

void init_tasking() {
    current_task = (task_t*)malloc(sizeof(task_t));
    current_task->id = 0;
    current_task->app_base = 0; // Çekirdeğin adresi 0'dır
    current_task->next = current_task; 
    ready_queue = current_task;
}

// DÜZELTME: Artık görevi oluştururken kod adresini alıyor ve ID döndürüyor
int create_task(void (*func)(void), unsigned int app_base) {
    task_t* new_task = (task_t*)malloc(sizeof(task_t));
    new_task->id = next_pid++;
    new_task->app_base = app_base; // Görevin kendi belleğini kaydet
    
    unsigned int* stack = (unsigned int*)malloc(4096);
    new_task->stack_base = (unsigned int)stack;
    unsigned int* stack_top = (unsigned int*)((unsigned int)stack + 4096); 
    
    *(--stack_top) = (unsigned int)func; 
    *(--stack_top) = 0; // EBP
    *(--stack_top) = 0; // EBX
    *(--stack_top) = 0; // ESI
    *(--stack_top) = 0; // EDI
    
    new_task->esp = (unsigned int)stack_top;
    
    new_task->next = ready_queue->next;
    ready_queue->next = new_task;
    
    return new_task->id; // Hangi görev (PID) oluştuğunu döndür
}

void yield() {
    task_t* prev = current_task;
    current_task = current_task->next;
    if (prev != current_task) {
        task_switch(&prev->esp, current_task->esp);
    }
}

// YENİ: Sadece istenen ID'ye sahip görevi donduran Nokta Atışı Cellat!
void kill_task_by_id(int task_id) {
    task_t* curr = ready_queue;
    if (curr == 0) return;
    
    do {
        if (curr->next->id == task_id) { 
            task_t* target = curr->next;
            curr->next = target->next; // Görevi zincirden sonsuza dek kopar!
            return; 
        }
        curr = curr->next;
    } while (curr != ready_queue);
}