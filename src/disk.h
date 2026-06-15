#ifndef DISK_H
#define DISK_H

// YENİ: FAT Mimarisine Uygun Dosya Giriş Yapısı (32 Bayt)
typedef struct {
    char name[8];          // Dosya adı (Örn: "SNAKE   ")
    char ext[3];           // Uzantısı (Örn: "BIN")
    unsigned char attr;    // Dosya niteliği (Klasör mü, gizli mi?)
    unsigned char reserved[10];
    unsigned short time;   // Oluşturulma saati
    unsigned short date;   // Oluşturulma tarihi
    unsigned short cluster;// Dosyanın başladığı ilk sektör/küme numarası
    unsigned int size;     // Bayt cinsinden dosya boyutu
} __attribute__((packed)) directory_entry_t;

// Temel Sektör Okuma/Yazma Fonksiyonları
void ata_lba_read(unsigned int lba, unsigned char sector_count, unsigned short* target);
void ata_lba_write(unsigned int lba, unsigned char sector_count, unsigned short* source);

// Gelişmiş Dosya Okuma/Yazma Fonksiyonları
int ardaos_read_file(const char* filename, const char* ext, unsigned char* target_buffer);
int ardaos_write_file(const char* filename, const char* ext, unsigned int start_sector, unsigned int size, unsigned char* source_buffer);

#endif