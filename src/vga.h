#ifndef VGA_H
#define VGA_H

void clear_screen(void);
void put_char(char c);
void print_string(const char *str);
void print_number(unsigned int num);
void update_hardware_cursor(int x, int y);

#endif