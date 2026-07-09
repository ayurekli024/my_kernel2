#ifndef TASK_H
#define TASK_H
typedef struct {
    int is_open;          // Dosya açık mı? (1 = Evet, 0 = Hayır)
    unsigned int size;    // Dosyanın toplam boyutu
    unsigned int offset;  // Dosyanın neresinde kaldığımızı (imleç) tutar
    
    // Arka planda diske erişmek için gereken FAT16 bilgileri
    unsigned short cluster; 
    unsigned int lba_start;
} file_obj_t;

#define MAX_FD_PER_TASK 8 // Bir uygulama aynı anda en fazla 8 dosya açabilir
typedef struct task {
    unsigned int esp;      
    unsigned int id;   
    unsigned int stack_base;    
    unsigned int app_base;     
    struct task* next;  
    int state;   
    unsigned int cpu_ticks; // YENİ: Anlık saniyedeki vuruş sayısı
    unsigned int cpu_usage;
    file_obj_t fd_table[MAX_FD_PER_TASK];
} task_t;

extern task_t* current_task;
void init_tasking(void);
int create_task(void (*func)(void), unsigned int app_base, char* args);void yield(void);
void kill_task_by_id(int task_id);
void get_process_list(char* buffer); // YENI EKLENDI
void wake_task_by_id(int task_id); // YENİ EKLENDİ
extern task_t* ready_queue; // YENİ: Sistem monitörünün görevleri okuyabilmesi için
#endif