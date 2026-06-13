#include "string.h"

// İki metni karşılaştırır (Eşitse 0 döndürür)


// Metnin karakter uzunluğunu bulur
unsigned int strlen(const char *s) {
    unsigned int len = 0;
    while (s[len]) len++;
    return len;
}

// "1000" gibi bir metni matematiksel 1000 sayısına dönüştürür (Argümanlar için şart)
int atoi(const char *str) {
    int res = 0;
    // Sadece rakam olan karakterleri al
    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');
        str++;
    }
    return res;
}
void strcpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

// İki metni karşılaştırır (Aynıysa 0 döndürür)
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}
// İki metni birbirine ucuca ekler
void strcat(char* dest, const char* src) {
    while (*dest) dest++;
    while (*src) *dest++ = *src++;
    *dest = '\0';
}

// Tam sayıları (Integer) karakter dizisine (String) çevirir
void itoa(int n, char s[]) {
    int i = 0, sign;
    if ((sign = n) < 0) n = -n; // Eksi sayılar için
    do { 
        s[i++] = n % 10 + '0'; 
    } while ((n /= 10) > 0);
    if (sign < 0) s[i++] = '-';
    s[i] = '\0';
    
    // Metni ters çevir
    int j, k; 
    char c;
    for (j = 0, k = i - 1; j < k; j++, k--) {
        c = s[j]; s[j] = s[k]; s[k] = c;
    }
}