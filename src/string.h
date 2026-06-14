#ifndef STRING_H
#define STRING_H

void strcpy(char* dest, const char* src);
void strcat(char* dest, const char* src);
void itoa(int n, char s[]);

// DÜZELTME VE İMZALAR:
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, int n);
int atoi(const char* str);
int strlen(const char* str); // int olarak kalıyor

#endif