#ifndef STRING_H
#define STRING_H

int strcmp(const char *s1, const char *s2);
unsigned int strlen(const char *s);
int atoi(const char *str); // Metni (String) tam sayıya (Integer) çevirir
void strcpy(char* dest, const char* src);
// ... eski kodların
void strcat(char* dest, const char* src);
void itoa(int n, char s[]);
#endif