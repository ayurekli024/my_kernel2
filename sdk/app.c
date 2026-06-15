#include "ardaos.h"

void __attribute__((section(".entry"))) _start() {
    int snake_x[100];
    int snake_y[100];
    int length = 5;
    int dir_x = 10, dir_y = 0;

    // Yılanın başlangıç pozisyonu (Pencere içine göre uyarlandı)
    for(int i = 0; i < length; i++) {
        snake_x[i] = 300 - (i * 10);
        snake_y[i] = 200;
    }

    int apple_x = 400, apple_y = 200;

    while(1) {
        char key = sys_get_key();
        if(key == 'w' && dir_y == 0) { dir_x = 0; dir_y = -10; }
        if(key == 's' && dir_y == 0) { dir_x = 0; dir_y = 10; }
        if(key == 'a' && dir_x == 0) { dir_x = -10; dir_y = 0; }
        if(key == 'd' && dir_x == 0) { dir_x = 10; dir_y = 0; }

        for(int i = length - 1; i > 0; i--) {
            snake_x[i] = snake_x[i-1];
            snake_y[i] = snake_y[i-1];
        }
        
        snake_x[0] += dir_x;
        snake_y[0] += dir_y;

        // Pencere Sınırları (Çarpınca karşıdan çıkma)
        if(snake_x[0] >= 590) snake_x[0] = 0;
        if(snake_x[0] < 0) snake_x[0] = 580;
        if(snake_y[0] >= 410) snake_y[0] = 0;
        if(snake_y[0] < 0) snake_y[0] = 400;

        // Elma Yeme
        if(snake_x[0] == apple_x && snake_y[0] == apple_y) {
            if (length < 99) length++;
            
            apple_x = (snake_x[0] * 7 + 130) % 580;
            apple_y = (snake_y[0] * 11 + 70) % 400;
            apple_x -= (apple_x % 10);
            apple_y -= (apple_y % 10);
        }

        sys_clear_shapes();
        add_shape(apple_x, apple_y, 10, 10, 0x00FF2D55); // Elma
        
        for(int i = 0; i < length; i++) {
            add_shape(snake_x[i], snake_y[i], 10, 10, 0x0034C759); // Yılan
        }

  // 5. OYUN HIZI (Gecikme)
        // Eğer QEMU'da oyun çok hızlı gelirse bu sayıyı artır, yavaşsa azalt.
        for(volatile int d = 0; d < 2000000; d++) {}
        
        // Arka plandaki işletim sistemine (Saat, Fare) nefes aldır!
        sys_yield(); 
    }
}