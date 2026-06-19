#ifndef TASK_H
#define TASK_H

typedef struct task {
    unsigned int esp;      
    unsigned int id;   
    unsigned int stack_base;    
    unsigned int app_base;     
    struct task* next;     
} task_t;

extern task_t* current_task;
void init_tasking(void);
int create_task(void (*func)(void), unsigned int app_base); 
void yield(void);
void kill_task_by_id(int task_id);

#endif