#ifndef MOUSE_H
#define MOUSE_H

// Çekirdeğin her yerinden erişilebilecek global fare değişkenleri
extern int mouse_x;
extern int mouse_y;
extern int mouse_left_button; // KESİN ÇÖZÜM: Çekirdeğe bu değişkeni tanıtıyoruz!

void init_mouse(void);
void mouse_handler_main(void);

#endif