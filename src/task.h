#ifndef TASK_H
#define TASK_H

// PCB (Process Control Block) - Görev Kontrol Bloğu
typedef struct task {
    unsigned int esp;      
    unsigned int id;   
    unsigned int stack_base;    
    unsigned int app_base;     // YENİ: Uygulamanın RAM'deki kod adresi (Relokasyon için)
    struct task* next;     
} task_t;

extern task_t* current_task;   // YENİ: Çekirdeğin o an kimin çalıştığını bilmesi için

void init_tasking(void);
int create_task(void (*func)(void), unsigned int app_base); // YENİ: Dönüş tipi ID oldu
void yield(void);

#endif