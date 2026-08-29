#include "ardaos.h"
#include "libc.h"

int num1 = 0, num2 = 0;
char op = 0;
int state = 0; // 0: İlk Sayı Giriliyor, 1: İkinci Sayı Giriliyor

void update_display(int val) {
    char buf[16];
    itoa(val, buf);
    
    char text[128] = "\n    [ ";
    strcat(text, buf);
    strcat(text, " ]\n\n----------------------");
    sys_set_window_text(text);
}

void _start(char* args) {
    sys_create_window("Hesap Makinesi", 180, 250);
    update_display(0);
    
    // 1. Satır
    sys_create_button(10,  70, 35, 30, "7");
    sys_create_button(50,  70, 35, 30, "8");
    sys_create_button(90,  70, 35, 30, "9");
    sys_create_button(130, 70, 35, 30, "/");

    // 2. Satır
    sys_create_button(10,  110, 35, 30, "4");
    sys_create_button(50,  110, 35, 30, "5");
    sys_create_button(90,  110, 35, 30, "6");
    sys_create_button(130, 110, 35, 30, "*");

    // 3. Satır
    sys_create_button(10,  150, 35, 30, "1");
    sys_create_button(50,  150, 35, 30, "2");
    sys_create_button(90,  150, 35, 30, "3");
    sys_create_button(130, 150, 35, 30, "-");

    // 4. Satır
    sys_create_button(10,  190, 35, 30, "C");
    sys_create_button(50,  190, 35, 30, "0");
    sys_create_button(90,  190, 35, 30, "=");
    sys_create_button(130, 190, 35, 30, "+");

    // Olay (Event) Döngüsü
    while(1) {
        int ev_id;
        int ev_type = sys_poll_event(&ev_id);
        
        if (ev_type == 1) { // 1 = Butona Tıklandı
            // Rakamlara Tıklandıysa (0-2, 4-6, 8-10 ve 13. ID'ler)
            if ((ev_id >= 0 && ev_id <= 2) || (ev_id >= 4 && ev_id <= 6) || 
                (ev_id >= 8 && ev_id <= 10) || ev_id == 13) {
                
                int n = 0;
                if (ev_id == 0) n = 7; else if (ev_id == 1) n = 8; else if (ev_id == 2) n = 9;
                else if (ev_id == 4) n = 4; else if (ev_id == 5) n = 5; else if (ev_id == 6) n = 6;
                else if (ev_id == 8) n = 1; else if (ev_id == 9) n = 2; else if (ev_id == 10) n = 3;
                else if (ev_id == 13) n = 0;
                
                if (state == 0) { num1 = num1 * 10 + n; update_display(num1); }
                else { num2 = num2 * 10 + n; update_display(num2); }
            }
            // Operatörlere veya Özel Butonlara Tıklandıysa
            else if (ev_id == 3) { op = '/'; state = 1; num2 = 0; }
            else if (ev_id == 7) { op = '*'; state = 1; num2 = 0; }
            else if (ev_id == 11){ op = '-'; state = 1; num2 = 0; }
            else if (ev_id == 15){ op = '+'; state = 1; num2 = 0; }
            else if (ev_id == 12){ num1 = 0; num2 = 0; op = 0; state = 0; update_display(0); } // C (Clear)
            else if (ev_id == 14){ // Eşittir (=)
                int res = 0;
                if (op == '+') res = num1 + num2;
                else if (op == '-') res = num1 - num2;
                else if (op == '*') res = num1 * num2;
                else if (op == '/') { if (num2 != 0) res = num1 / num2; else res = 0; }
                
                update_display(res);
                num1 = res; // Çıkan sonucu ilk sayıya atayarak zincirleme işleme izin ver
                state = 0;
            }
        }
        sys_sleep(10); // Çekirdeği yormadan tetikte bekle
    }
}