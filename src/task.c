#include "task.h"
#include "memory.h"
#include "vga.h"

// Görev Kontrol Bloğu (TCB - Thread Control Block)
typedef struct {
    unsigned int esp;  // Görevin uykuya daldığı yığın (stack) adresi
    int active;
} thread_t;

#define MAX_TASKS 4
thread_t tasks[MAX_TASKS];
int current_task = -1;
int task_count = 0;

// Assembly dosyamızdaki fonksiyonu C'ye tanıtıyoruz
extern void switch_task(unsigned int *old_esp, unsigned int new_esp);

void init_multitasking() {
    // İşletim sisteminin o an çalıştığı ana kodu "Görev 0" olarak kaydet
    tasks[0].active = 1;
    current_task = 0;
    task_count = 1;
    print_string("Multitasking (Coklu Gorev) baslatildi.\n");
}

void create_task(void (*task_func)()) {
    if (task_count >= MAX_TASKS) return;

    // Her görev için dinamik bellekten (Heap) 4 KB'lık yığın (Stack) tahsis et
    unsigned int *stack = (unsigned int *)malloc(4096);
    unsigned int stack_top = (unsigned int)stack + 4096;

    // Yığını, sanki bir fonksiyon çağrılmış ve kesilmiş gibi "Sahte (Fake)" değerlerle doldur
    // 1. İşlemcinin geri döneceği adres (Fonksiyonun başlangıç noktası)
    stack_top -= 4;
    *((unsigned int *)stack_top) = (unsigned int)task_func;

    // 2. switch_task içindeki 4 adet 'pop' işlemi için sahte yazmaç değerleri (0)
    stack_top -= 4; *((unsigned int *)stack_top) = 0; // EBP
    stack_top -= 4; *((unsigned int *)stack_top) = 0; // EBX
    stack_top -= 4; *((unsigned int *)stack_top) = 0; // ESI
    stack_top -= 4; *((unsigned int *)stack_top) = 0; // EDI

    // Yığının son durumunu göreve kaydet ve aktif et
    tasks[task_count].esp = stack_top;
    tasks[task_count].active = 1;
    task_count++;
}

// "Benim işim bitti, sıradaki görev çalışsın" komutu (Round-Robin Scheduling)
void schedule() {
    if (task_count <= 1) return;

    int old_task = current_task;
    
    // Bir sonraki göreve geç (Sona gelirse başa dön)
    current_task++;
    if (current_task >= task_count) current_task = 0;

    // Yığını (Stack) fiziksel olarak takas et!
    switch_task(&tasks[old_task].esp, tasks[current_task].esp);
}