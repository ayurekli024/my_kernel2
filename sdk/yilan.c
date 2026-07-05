#include "ardaos.h"

// PRNG (Sözde Rastgele Sayı Üretici) - Artık adresi pointer ile dışarıdan alıyor!
unsigned int rand(unsigned int* seed) {
    *seed = (*seed * 1103515245 + 12345) & 0x7fffffff;
    return *seed;
}

__attribute__((section(".text.entry")))
void _start(char* args) {
    // YENİ: GLOBALDEN LOCALE (Stack'e) ALINDI! Artık çekirdeğe zarar veremez.
    unsigned int random_seed = 12345; 

    // 1. Terminalden gelen hız parametresini (Örn: yilan.bin 3) oku
    int delay_multiplier = 4; // Varsayılan orta hız
    if (args != 0 && args[0] != '\0') {
        if (args[0] >= '1' && args[0] <= '9') {
            delay_multiplier = args[0] - '0';
        }
    }

    // 2. Oyuna özel bağımsız grafik penceresi aç (320x240 piksel)
    sys_create_window("ArdaOS Yilan", 320, 240);

    // 3. Oyun Değişkenleri
    int snake_x[95]; // Kernel'in 100 şekil limitini aşmamak için maks 95 parça
    int snake_y[95];
    int snake_len = 4;
    
    for(int i = 0; i < snake_len; i++) {
        snake_x[i] = 10 - i;
        snake_y[i] = 7;
    }

    int dir_x = 1, dir_y = 0;
    int food_x = 15, food_y = 7;
    int game_over = 0;

    // 4. ANA OYUN DÖNGÜSÜ
    while (!game_over) {
        // A) Klavyeyi ASENKRON oku (Oyun asla uykuya dalmaz!)
        char key = sys_poll_key();
        if (key == 'w' && dir_y != 1)  { dir_x = 0; dir_y = -1; }
        else if (key == 's' && dir_y != -1) { dir_x = 0; dir_y = 1; }
        else if (key == 'a' && dir_x != 1)  { dir_x = -1; dir_y = 0; }
        else if (key == 'd' && dir_x != -1) { dir_x = 1; dir_y = 0; }
        else if (key == 'q') { game_over = 1; }

        // B) Yılanın kuyruğunu kaydır
        for (int i = snake_len - 1; i > 0; i--) {
            snake_x[i] = snake_x[i-1];
            snake_y[i] = snake_y[i-1];
        }

        // C) Kafayı yeni yöne ilerlet
        snake_x[0] += dir_x;
        snake_y[0] += dir_y;

        // D) Duvara Çarpma Kontrolü 
        if (snake_x[0] < 0 || snake_x[0] >= 20 || snake_y[0] < 0 || snake_y[0] >= 13) {
            game_over = 1;
        }

        // E) Kendine Çarpma Kontrolü
        for (int i = 1; i < snake_len; i++) {
            if (snake_x[0] == snake_x[i] && snake_y[0] == snake_y[i]) {
                game_over = 1;
            }
        }

        // F) Yemi Yeme Kontrolü (YENİ: Pointer kullanımı)
        if (snake_x[0] == food_x && snake_y[0] == food_y) {
            if (snake_len < 90) snake_len++; 
            
            random_seed += snake_x[0] * snake_y[0]; // Rastgeleliği oyuncu hareketiyle besle
            food_x = rand(&random_seed) % 20;       // Adresi gönder
            food_y = rand(&random_seed) % 13;       // Adresi gönder
        }

        if (game_over) break;

        // G) Çizim (Render) Aşaması
        sys_clear_shapes(); // Önceki kareyi temizle
        
        // Yemi çiz (Kırmızı - 16x16 piksel)
        add_shape(food_x * 16, food_y * 16, 16, 16, 0x00FF2D55);
        
        // Yılanı çiz (Kafa: Koyu Yeşil, Gövde: Açık Yeşil)
        add_shape(snake_x[0] * 16, snake_y[0] * 16, 16, 16, 0x001B5E20);
        for (int i = 1; i < snake_len; i++) {
            add_shape(snake_x[i] * 16, snake_y[i] * 16, 16, 16, 0x004CAF50);
        }

        for (volatile int i = 0; i < (delay_multiplier * 10); i++) {
            sys_yield();
        }
    }

    // OYUN BİTİŞ EKRANI 
    char score_num[8];
    int temp = snake_len - 4; // Yenilen yem sayısı
    int idx = 0;
    if (temp == 0) { score_num[idx++] = '0'; }
    while(temp > 0) { score_num[idx++] = (temp % 10) + '0'; temp /= 10; }
    score_num[idx] = '\0';
    for(int i=0; i<idx/2; i++) { char t = score_num[i]; score_num[i] = score_num[idx-1-i]; score_num[idx-1-i] = t; }
    
    sys_print("--- OYUN BITTI ---");
    sys_print("Toplanan Yem:");
    sys_print(score_num);

    // Kapanmadan önce oyuncunun görebilmesi için biraz bekle
    for (volatile int i = 0; i < 15000000; i++) { sys_yield(); }
    sys_exit(); 
    while(1);
}