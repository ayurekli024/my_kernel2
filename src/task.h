#ifndef TASK_H
#define TASK_H

typedef struct task {
    unsigned int esp;      
    unsigned int id;   
    unsigned int stack_base;    
    unsigned int app_base;     
    struct task* next;  
    int state;   
    unsigned int cpu_ticks; // YENİ: Anlık saniyedeki vuruş sayısı
    unsigned int cpu_usage;
} task_t;

extern task_t* current_task;
void init_tasking(void);
int create_task(void (*func)(void), unsigned int app_base, char* args);void yield(void);
void kill_task_by_id(int task_id);
void get_process_list(char* buffer); // YENI EKLENDI
void wake_task_by_id(int task_id); // YENİ EKLENDİ
extern task_t* ready_queue; // YENİ: Sistem monitörünün görevleri okuyabilmesi için
#endif