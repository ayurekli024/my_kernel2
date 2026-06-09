#ifndef GRAPHICS_H
#define GRAPHICS_H

// Grafik motorunu başlatır
void init_graphics(unsigned int* fb, int width, int height);

// Ekrana tek bir nokta (piksel) çizer
void put_pixel(int x, int y, unsigned int color);

// İçi dolu bir dikdörtgen (veya pencere) çizer
void draw_rect(int x, int y, int width, int height, unsigned int color);

#endif