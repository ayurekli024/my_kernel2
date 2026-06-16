#ifndef TASK_H
#define TASK_H

// PCB (Process Control Block) - Görev Kontrol Bloğu
typedef struct task {
    unsigned int esp;      // Görevin kaldığı yer (Stack Pointer)
    unsigned int id;   
    unsigned int stack_base;    
    struct task* next;     // Sonraki görev (Dairesel liste için)
} task_t;

void init_tasking(void);
void create_task(void (*func)(void));
void yield(void);

#endif