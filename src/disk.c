#include "disk.h"
#include "io.h"
#include "string.h"

// Dinamik Disk Sınırları
fat16_bpb_t bpb;
unsigned int root_dir_start_lba;
unsigned int data_start_lba;

// ==========================================
// YENİ: DİSKİ TANIMA VE FAT16 MATEMATİĞİ
// ==========================================
void init_disk() {
    unsigned short boot_sector[256]; // 512 Bayt
    
    // Diskin 0. Sektörünü (Boot Sector) Oku
    ata_lba_read(0, 1, boot_sector);
    
    // Byte-byte kopyalayarak BPB yapımızı doldur
    unsigned char* byte_ptr = (unsigned char*)boot_sector;
    for(int i = 0; i < sizeof(fat16_bpb_t); i++) {
        ((unsigned char*)&bpb)[i] = byte_ptr[i];
    }

    // Haritayı Çıkarıyoruz!
    unsigned int fat_start = bpb.reserved_sectors;
    unsigned int fat_size = bpb.fat_count * bpb.sectors_per_fat;
    
    // Kök Dizin Nerede Başlıyor? (Eskiden 19 diyorduk, artık sistem kendisi buluyor!)
    root_dir_start_lba = fat_start + fat_size;
    
    // Veriler (Dosya İçerikleri) Nerede Başlıyor?
    unsigned int root_dir_sectors = (bpb.dir_entries * 32) / 512;
    data_start_lba = root_dir_start_lba + root_dir_sectors;
}

// Diskten Belirli Bir Sektörü Okuma (Donanım Katmanı)
void ata_lba_read(unsigned int lba, unsigned char sector_count, unsigned short* target) {
    while ((inb(0x1F7) & 0x80)) {}
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, sector_count);
    outb(0x1F3, (unsigned char)(lba & 0xFF));
    outb(0x1F4, (unsigned char)((lba >> 8) & 0xFF));
    outb(0x1F5, (unsigned char)((lba >> 16) & 0xFF));
    outb(0x1F7, 0x20); 
    
    for (int j = 0; j < sector_count; j++) {
        while (!(inb(0x1F7) & 0x08)) {}
        for (int i = 0; i < 256; i++) {
            target[i] = inw(0x1F0);
        }
        target += 256;
    }
}

void ata_lba_write(unsigned int lba, unsigned char sector_count, unsigned short* source) {
    while ((inb(0x1F7) & 0x80)) {}
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, sector_count);
    outb(0x1F3, (unsigned char)(lba & 0xFF));
    outb(0x1F4, (unsigned char)((lba >> 8) & 0xFF));
    outb(0x1F5, (unsigned char)((lba >> 16) & 0xFF));
    outb(0x1F7, 0x30); 
    
    for (int j = 0; j < sector_count; j++) {
        while (!(inb(0x1F7) & 0x08)) {}
        for (int i = 0; i < 256; i++) {
            outw(0x1F0, source[i]); 
        }
        source += 256;
    }
}

// Dosyayı FAT16 Kök Dizininden Bul ve Oku
int ardaos_read_file(const char* filename, const char* ext, unsigned char* target_buffer) {
    directory_entry_t root_dir[16]; 
    
    // YENİ: Artık sabit 19'dan değil, hesaplanan Kök Dizin adresinden okuyoruz
    ata_lba_read(root_dir_start_lba, 1, (unsigned short*)root_dir);
    
    for (int i = 0; i < 16; i++) {
        if (root_dir[i].name[0] == 0 || root_dir[i].name[0] == (char)0xE5) continue; // Boş veya Silinmiş
        
        if (strncmp(root_dir[i].name, filename, 8) == 0 && strncmp(root_dir[i].ext, ext, 3) == 0) {
            
            // YENİ: FAT16 Küme (Cluster) Matematiği!
            // Verinin fiziksel sektörünü = Veri Başlangıcı + ((Küme - 2) * Küme Başına Sektör)
            unsigned int actual_lba = data_start_lba + ((root_dir[i].cluster - 2) * bpb.sectors_per_cluster);
            unsigned int sectors_to_read = (root_dir[i].size + 511) / 512;
            
            ata_lba_read(actual_lba, sectors_to_read, (unsigned short*)target_buffer);
            return root_dir[i].size; 
        }
    }
    return -1; 
}

void ardaos_list_files(char* output_buffer) {
    directory_entry_t root_dir[16];
    ata_lba_read(root_dir_start_lba, 1, (unsigned short*)root_dir); // Dinamik adres
    
    strcpy(output_buffer, "=== FAT16 DISK ICERIGI ===\n");
    int found = 0;
    for (int i = 0; i < 16; i++) {
        if (root_dir[i].name[0] != 0 && root_dir[i].name[0] != (char)0xE5) {
            
            // Gizli (Hidden) veya Birim Etiketi (Volume Label) dosyalarını atla
            if (root_dir[i].attr == 0x0F || (root_dir[i].attr & 0x08)) continue;

            found++;
            char temp_name[9]; char temp_ext[4];
            for(int j=0; j<8; j++) temp_name[j] = root_dir[i].name[j];
            for(int j=0; j<3; j++) temp_ext[j] = root_dir[i].ext[j];
            temp_name[8] = '\0'; temp_ext[3] = '\0';
            
            char size_str[16]; itoa(root_dir[i].size, size_str);
            strcat(output_buffer, "- "); strcat(output_buffer, temp_name);
            strcat(output_buffer, "."); strcat(output_buffer, temp_ext);
            strcat(output_buffer, "   ("); strcat(output_buffer, size_str); strcat(output_buffer, " Bayt)\n");
        }
    }
    if (found == 0) strcat(output_buffer, "Disk tamamen bos.");
}