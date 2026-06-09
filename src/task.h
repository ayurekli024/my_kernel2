#ifndef TASK_H
#define TASK_H

void init_multitasking();
void create_task(void (*task_func)());
void schedule(); // yield() yerine bunu kullanacağız

#endif