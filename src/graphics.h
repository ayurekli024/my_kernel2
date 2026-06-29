#ifndef GRAPHICS_H
#define GRAPHICS_H

// Grafik motorunu başlatır
void init_graphics(unsigned int* fb, int width, int height);

// Ekrana tek bir nokta (piksel) çizer
void put_pixel(int x, int y, unsigned int color);

// İçi dolu bir dikdörtgen (veya pencere) çizer
void draw_rect(int x, int y, int width, int height, unsigned int color);
// ... (önceki kodların altı)
void draw_char(int x, int y, char c, unsigned int fg_color, unsigned int bg_color);
void draw_string(int x, int y, const char* str, unsigned int fg_color, unsigned int bg_color);
// ... (önceki kodların altı)
void swap_buffers(void);
void draw_cursor(int x, int y);
void draw_string_wrapped(int x, int y, int max_w, const char* str, unsigned int fg_color, unsigned int bg_color);

extern int dirty_min_x, dirty_min_y, dirty_max_x, dirty_max_y;
void set_clipping_rect(int x, int y, int w, int h);
void reset_clipping_rect(void);
void add_dirty_rect(int x, int y, int w, int h);
void mark_screen_dirty(void);
#endif