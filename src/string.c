#include "string.h"

// İki metni karşılaştırır (Eşitse 0 döndürür)
int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

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