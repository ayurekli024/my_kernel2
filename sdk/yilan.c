#include "ardaos.h"

// PRNG (Sözde Rastgele Sayı Üretici)
unsigned int rand(unsigned int* seed) {
    *seed = (*seed * 1103515245 + 12345) & 0x7fffffff;
    return *seed;
}

__attribute__((section(".text.entry")))
void _start(char* args) {
    // YENİ: IPC - Paylaşılan belleğin anahtarını al ve adrese bağlan
    volatile char* shared_mem = (volatile char*)sys_shm_get();
    
    // Oyun başlarken skoru "00" olarak sıfırla
    shared_mem[0] = '0';
    shared_mem[1] = '0';
    shared_mem[2] = '\0';

    unsigned int random_seed = 12345; 
    int delay_multiplier = 4; 
    if (args != 0 && args[0] != '\0') {
        if (args[0] >= '1' && args[0] <= '9') {
            delay_multiplier = args[0] - '0';
        }
    }

    sys_create_window("ArdaOS Yilan", 320, 240);

    int snake_x[95]; 
    int snake_y[95];
    int snake_len = 4;
    
    for(int i = 0; i < snake_len; i++) {
        snake_x[i] = 10 - i;
        snake_y[i] = 7;
    }

    int dir_x = 1, dir_y = 0;
    int food_x = 15, food_y = 7;
    int game_over = 0;

    while (!game_over) {
        char key = sys_poll_key();
        if (key == 'w' && dir_y != 1)  { dir_x = 0; dir_y = -1; }
        else if (key == 's' && dir_y != -1) { dir_x = 0; dir_y = 1; }
        else if (key == 'a' && dir_x != 1)  { dir_x = -1; dir_y = 0; }
        else if (key == 'd' && dir_x != -1) { dir_x = 1; dir_y = 0; }
        else if (key == 'q') { game_over = 1; }

        for (int i = snake_len - 1; i > 0; i--) {
            snake_x[i] = snake_x[i-1];
            snake_y[i] = snake_y[i-1];
        }

        snake_x[0] += dir_x;
        snake_y[0] += dir_y;

        if (snake_x[0] < 0 || snake_x[0] >= 20 || snake_y[0] < 0 || snake_y[0] >= 13) {
            game_over = 1;
        }

        for (int i = 1; i < snake_len; i++) {
            if (snake_x[0] == snake_x[i] && snake_y[0] == snake_y[i]) {
                game_over = 1;
            }
        }

        // --- YEM YENİLDİĞİ AN ---
        if (snake_x[0] == food_x && snake_y[0] == food_y) {
            if (snake_len < 90) snake_len++; 
            
            random_seed += snake_x[0] * snake_y[0]; 
            food_x = rand(&random_seed) % 20;       
            food_y = rand(&random_seed) % 13;       

            // YENİ: IPC VERİ AKTARIMI
            // Skoru anında ortak belleğe (Shared Memory) yaz!
            int current_score = snake_len - 4;
            shared_mem[0] = (current_score / 10) + '0';
            shared_mem[1] = (current_score % 10) + '0';
        }

        if (game_over) break;

        sys_clear_shapes(); 
        
        add_shape(food_x * 16, food_y * 16, 16, 16, 0x00FF2D55);
        add_shape(snake_x[0] * 16, snake_y[0] * 16, 16, 16, 0x001B5E20);
        for (int i = 1; i < snake_len; i++) {
            add_shape(snake_x[i] * 16, snake_y[i] * 16, 16, 16, 0x004CAF50);
        }

        for (volatile int i = 0; i < (delay_multiplier * 10); i++) {
            sys_yield();
        }
    }

    sys_print("--- OYUN BITTI ---");
    for (volatile int i = 0; i < 15000000; i++) { sys_yield(); }
    sys_exit(); 
    while(1);
}