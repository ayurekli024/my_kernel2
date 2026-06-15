#include "disk.h"
#include "io.h"
#include "string.h"

// Diskten Belirli Bir Sektörü (512 Bayt) RAM'e Okur
void ata_lba_read(unsigned int lba, unsigned char sector_count, unsigned short* target) {
    // Diskin meşguliyetinin (BSY) bitmesini bekle
    while ((inb(0x1F7) & 0x80)) {}
    
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F)); // Master Drive seç ve LBA'nın en üst 4 bitini gönder
    outb(0x1F2, sector_count);                // Kaç sektör okunacak?
    outb(0x1F3, (unsigned char)(lba & 0xFF)); // LBA Düşük 8 Bit
    outb(0x1F4, (unsigned char)((lba >> 8) & 0xFF)); // LBA Orta 8 Bit
    outb(0x1F5, (unsigned char)((lba >> 16) & 0xFF)); // LBA Yüksek 8 Bit
    outb(0x1F7, 0x20); // KOMUT: 0x20 = Read Sectors (Okuma İşlemi)
    
    for (int j = 0; j < sector_count; j++) {
        // Diskin veriyi hazırlamasını (DRQ biti) bekle
        while (!(inb(0x1F7) & 0x08)) {}
        
        // 1 Sektör = 512 Bayt. Biz 16-bit (2 Bayt) okuduğumuz için 256 kere döneceğiz.
        for (int i = 0; i < 256; i++) {
            target[i] = inw(0x1F0); // Veri portundan oku ve RAM'e (hedefe) at
        }
        target += 256;
    }
}

// RAM'deki Bir Veriyi Diske (Kalıcı Hafızaya) Yazar
void ata_lba_write(unsigned int lba, unsigned char sector_count, unsigned short* source) {
    while ((inb(0x1F7) & 0x80)) {}
    
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, sector_count);
    outb(0x1F3, (unsigned char)(lba & 0xFF));
    outb(0x1F4, (unsigned char)((lba >> 8) & 0xFF));
    outb(0x1F5, (unsigned char)((lba >> 16) & 0xFF));
    outb(0x1F7, 0x30); // KOMUT: 0x30 = Write Sectors (Yazma İşlemi)
    
    for (int j = 0; j < sector_count; j++) {
        while (!(inb(0x1F7) & 0x08)) {}
        for (int i = 0; i < 256; i++) {
            outw(0x1F0, source[i]); // RAM'deki veriyi porta (Diske) gönder
        }
        source += 256;
    }
}
// Diskin kök dizininde ismi arar, bulursa hedefe (target) yükler
int ardaos_read_file(const char* filename, const char* ext, unsigned char* target_buffer) {
    directory_entry_t root_dir[16]; // 1 sektör = 512 bayt / 32 bayt = 16 dosya girişi sığar
    
    // Basitlik adına diskin 19. sektörünü KÖK DİZİN (Root Directory) kabul ediyoruz.
    // İleride buraya gerçek bir FAT tablosu bağlayacağız.
    ata_lba_read(19, 1, (unsigned short*)root_dir);
    
    // Kök dizindeki 16 yuvayı tek tek tara
    for (int i = 0; i < 16; i++) {
        if (root_dir[i].name[0] == 0) continue; // Boş yuva, geç
        
        // İsim ve uzantı eşleşiyor mu kontrol et
        if (strncmp(root_dir[i].name, filename, 8) == 0 && strncmp(root_dir[i].ext, ext, 3) == 0) {
            
            // Dosya bulundu! Başlangıç sektörünü ve kaç sektör kapladığını hesapla
            unsigned int start_sector = root_dir[i].cluster;
            // Boyutuna göre kaç sektör okumamız gerektiğini buluyoruz (Her sektör 512 bayt)
            unsigned int sectors_to_read = (root_dir[i].size + 511) / 512;
            
            // Dosyanın verilerini diskten oku ve RAM'deki hedef tampona yükle!
            ata_lba_read(start_sector, sectors_to_read, (unsigned short*)target_buffer);
            return root_dir[i].size; // Başarılıysa dosya boyutunu döndür
        }
    }
    
    return -1; // Dosya bulunamadı hatası
}
// Diske yeni bir dosya kaydeder ve kök dizine yazar
int ardaos_write_file(const char* filename, const char* ext, unsigned int start_sector, unsigned int size, unsigned char* source_buffer) {
    directory_entry_t root_dir[16];
    ata_lba_read(19, 1, (unsigned short*)root_dir); // Mevcut kök dizini oku
    
    int free_slot = -1;
    for (int i = 0; i < 16; i++) {
        if (root_dir[i].name[0] == 0) { // İlk boş yuvayı bul
            free_slot = i;
            break;
        }
    }
    
    if (free_slot == -1) return -1; // Kök dizin dolu!
    
    // Yeni dosyanın üst bilgilerini (Metadata) doldur
    strcpy(root_dir[free_slot].name, filename);
    strcpy(root_dir[free_slot].ext, ext);
    root_dir[free_slot].cluster = start_sector;
    root_dir[free_slot].size = size;
    root_dir[free_slot].attr = 0x00; // Normal dosya
    
    // Önce dosyanın gerçek içeriğini veri alanına yaz
    unsigned int sectors_to_write = (size + 511) / 512;
    ata_lba_write(start_sector, sectors_to_write, (unsigned short*)source_buffer);
    
    // Sonra güncellenmiş Kök Dizin tablosunu geri diske kilitle!
    ata_lba_write(19, 1, (unsigned short*)root_dir);
    return 0; // Başarılı
}