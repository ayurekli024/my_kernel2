#ifndef IO_H
#define IO_H

// Porttan 8-bit veri okur
static inline unsigned char inb(unsigned short port) {
    unsigned char result;
    __asm__ __volatile__("inb %1, %0" : "=a" (result) : "Nd" (port));
    return result;
}

// Porta 8-bit veri yazar
static inline void outb(unsigned short port, unsigned char data) {
    __asm__ __volatile__("outb %0, %1" : : "a"(data), "Nd"(port));
}

// YENİ VE KRİTİK DÜZELTME: static inline ile çoklu tanım hatasını engelliyoruz!
static inline unsigned short inw(unsigned short port) {
    unsigned short result;
    __asm__ __volatile__("inw %1, %0" : "=a" (result) : "Nd" (port));
    return result;
}

static inline void outw(unsigned short port, unsigned short data) {
    __asm__ __volatile__("outw %0, %1" : : "a"(data), "Nd"(port));
}

#endif