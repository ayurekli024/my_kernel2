#include "io.h"

// SB16 Port Adresleri
#define DSP_RESET 0x226
#define DSP_READ  0x22A
#define DSP_WRITE 0x22C
#define DSP_STATUS 0x22E

extern void terminal_print(const char* text);
extern void itoa(int, char*);
extern void strcat(char*, const char*);

// DSP'ye komut göndermek için bekleme motoru
void dsp_write(unsigned char value) {
    while (inb(DSP_WRITE) & 0x80); // Meşgul bayrağı (Bit 7) inene kadar bekle
    outb(DSP_WRITE, value);
}

// DSP'den veri okumak için bekleme motoru
unsigned char dsp_read() {
    while (!(inb(DSP_STATUS) & 0x80)); // Veri hazır bayrağı (Bit 7) kalkana kadar bekle
    return inb(DSP_READ);
}

// Kartı uyandırma (Reset) Ritüeli
void init_sb16() {
    outb(DSP_RESET, 1);
    
    // Yaklaşık 3 mikrosaniye bekle (I/O gecikmesi)
    for(int i = 0; i < 1000; i++) inb(0x80); 
    
    outb(DSP_RESET, 0);
    
    // Kart başarıyla uyandıysa 0xAA (170) yanıtını döner
    unsigned char status = dsp_read();
    if (status == 0xAA) {
        terminal_print("[ DONANIM ] Sound Blaster 16 Ses Karti basariyla baslatildi!");
        
        // DSP Sürümünü Oku (Komut: 0xE1)
        dsp_write(0xE1);
        unsigned char major = dsp_read();
        unsigned char minor = dsp_read();
        
        char msg[64] = "SB16 DSP Surumu: ";
        char maj_str[4], min_str[4];
        itoa(major, maj_str); itoa(minor, min_str);
        strcat(msg, maj_str); strcat(msg, "."); strcat(msg, min_str);
        terminal_print(msg);
        
        // Hoparlörü Aç (Komut: 0xD1)
        dsp_write(0xD1);
    } else {
        terminal_print("[ HATA ] Sound Blaster 16 bulunamadi!");
    }
}

// Kart müzik çalmayı bitirdiğinde bu kesme (Interrupt) tetiklenir
void sb16_handler_main() {
    // 8-Bit sesler için kesmeyi onayla (Acknowledge)
    inb(0x22F); 
    
    // PIC donanımına "Kesme bitti" mesajını yolla
    outb(0x20, 0x20);
}
// ==========================================================
// DMA (DOĞRUDAN BELLEK ERİŞİMİ) VE SES OYNATMA MOTORU
// ==========================================================
#define DMA_MASK_PORT  0x0A
#define DMA_MODE_PORT  0x0B
#define DMA_FLIP_FLOP  0x0C
#define DMA_ADDR_PORT  0x02
#define DMA_COUNT_PORT 0x03
#define DMA_PAGE_PORT  0x83

// 32 KB olan sınırı 64 KB'a (Maksimum ISA DMA Sınırı) çıkardık!
unsigned char audio_buffer[65536] __attribute__((aligned(65536)));

void sb16_play_file() {
    extern int ardaos_read_file(const char*, const char*, unsigned char*);
    int size = ardaos_read_file("SES", "WAV", audio_buffer);
    
    if (size <= 44) {
        terminal_print("[ SB16 ] HATA: Diskte SES.WAV bulunamadi veya desteklenmiyor!");
        return;
    }
    
    // ZIRH: Eğer dosya 64 KB'tan büyükse, Kernel'i ezmemesi için boyutu kırp!
    if (size > 65536) size = 65536; 
    
    unsigned int phys_addr = (unsigned int)audio_buffer + 44;
    unsigned int len = size - 44 - 1;

    outb(DMA_MASK_PORT, 0x05); 
    outb(DMA_FLIP_FLOP, 0x00);
    
    outb(DMA_ADDR_PORT, (unsigned char)(phys_addr & 0xFF));
    outb(DMA_ADDR_PORT, (unsigned char)((phys_addr >> 8) & 0xFF));
    outb(DMA_PAGE_PORT, (unsigned char)((phys_addr >> 16) & 0xFF));
    
    outb(DMA_FLIP_FLOP, 0x00);
    outb(DMA_COUNT_PORT, (unsigned char)(len & 0xFF));
    outb(DMA_COUNT_PORT, (unsigned char)((len >> 8) & 0xFF));
    
    outb(DMA_MODE_PORT, 0x49);
    outb(DMA_MASK_PORT, 0x01); 
    
    dsp_write(0x40); 
    dsp_write(165); 
    
    dsp_write(0x14); 
    dsp_write((unsigned char)(len & 0xFF));
    dsp_write((unsigned char)((len >> 8) & 0xFF));
    
    terminal_print("[ SB16 ] ArdaOS Acilis Sesi caliniyor!");
}