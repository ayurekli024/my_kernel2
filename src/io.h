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
// Porttan 32-bit veri okur (Ağ kartı için)
static inline unsigned int inl(unsigned short port) {
    unsigned int result;
    __asm__ __volatile__("inl %1, %0" : "=a" (result) : "Nd" (port));
    return result;
}

// Porta 32-bit veri yazar (Ağ kartı için)
static inline void outl(unsigned short port, unsigned int data) {
    __asm__ __volatile__("outl %0, %1" : : "a"(data), "Nd"(port));
}
#endif