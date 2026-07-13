#ifndef LIBC_H
#define LIBC_H

// Standart String (Metin) Operasyonları
int strlen(const char* str);
void strcpy(char* dest, const char* src);
void strcat(char* dest, const char* src);
void memset(void* dest, int val, int len);

// Sayı-Metin Dönüştürme (itoa)
void itoa(int n, char s[]);

// Ve o Efsanevi Fonksiyon!
void printf(const char* format, ...);
// Dinamik Bellek Yönetimi
void* malloc(unsigned int size);
void free(void* ptr);
#endif