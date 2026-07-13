#include "disk.h"
#include "io.h"
#include "string.h"
#include "graphics.h"
#include "task.h"
fat16_bpb_t bpb;
unsigned int root_dir_start_lba;
unsigned int data_start_lba;

void init_disk() {
    unsigned short boot_sector[256] = {0}; 
    int timeout = 100000;

    do {
        ata_lba_read(0, 1, boot_sector);
        timeout--;
    } while (boot_sector[255] != 0xAA55 && timeout > 0);

    unsigned char* bpb_bytes = (unsigned char*)boot_sector;
    bpb.bytes_per_sector    = bpb_bytes[11] | (bpb_bytes[12] << 8);
    bpb.sectors_per_cluster = bpb_bytes[13];
    bpb.reserved_sectors    = bpb_bytes[14] | (bpb_bytes[15] << 8);
    bpb.fat_count           = bpb_bytes[16];
    bpb.dir_entries         = bpb_bytes[17] | (bpb_bytes[18] << 8);
    bpb.sectors_per_fat     = bpb_bytes[22] | (bpb_bytes[23] << 8);

    if (bpb.bytes_per_sector != 512 || bpb.sectors_per_cluster == 0 || bpb.dir_entries == 0 || bpb.sectors_per_fat == 0) {
        bpb.bytes_per_sector = 512;
        bpb.sectors_per_cluster = 1;  // TEST: 16'dan 1'e düştü
        bpb.reserved_sectors = 1;
        bpb.fat_count = 2;
        bpb.dir_entries = 512;
        bpb.sectors_per_fat = 32;
    }

    unsigned int fat_start = bpb.reserved_sectors;
    unsigned int fat_size = bpb.fat_count * bpb.sectors_per_fat;
    root_dir_start_lba = fat_start + fat_size;
    
    unsigned int root_dir_sectors = (bpb.dir_entries * 32) / 512;
    data_start_lba = root_dir_start_lba + root_dir_sectors;
}

void ata_lba_read(unsigned int lba, unsigned char sector_count, unsigned short* target) {
    if (sector_count == 0) sector_count = 1; // DONANIM KİLİDİ KORUMASI
    
    while ((inb(0x1F7) & 0x80)) {}
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, sector_count);
    outb(0x1F3, (unsigned char)(lba & 0xFF));
    outb(0x1F4, (unsigned char)((lba >> 8) & 0xFF));
    outb(0x1F5, (unsigned char)((lba >> 16) & 0xFF));
    outb(0x1F7, 0x20); 
    for (int j = 0; j < sector_count; j++) {
        while (!(inb(0x1F7) & 0x08)) {}
        for (int i = 0; i < 256; i++) { target[i] = inw(0x1F0); }
        target += 256;
    }
}

void ata_lba_write(unsigned int lba, unsigned char sector_count, unsigned short* source) {
    if (sector_count == 0) sector_count = 1; // DONANIM KİLİDİ KORUMASI
    
    while ((inb(0x1F7) & 0x80)) {}
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, sector_count);
    outb(0x1F3, (unsigned char)(lba & 0xFF));
    outb(0x1F4, (unsigned char)((lba >> 8) & 0xFF));
    outb(0x1F5, (unsigned char)((lba >> 16) & 0xFF));
    outb(0x1F7, 0x30); 
    for (int j = 0; j < sector_count; j++) {
        while (!(inb(0x1F7) & 0x08)) {}
        for (int i = 0; i < 256; i++) { outw(0x1F0, source[i]); }
        source += 256;
    }
}

// Basit kernel-side debug yardimcisi: terminale yazdir
extern void api_print(const char*);
static void dbg_print_term(const char* prefix, int value) {
    char buf[64];
    char num[16];
    itoa(value, num);
    strcpy(buf, prefix);
    strcat(buf, num);
    api_print(buf);
}

int ardaos_read_file(const char* filename, const char* ext, unsigned char* target_buffer) {
    directory_entry_t root_dir[16]; 
    ata_lba_read(root_dir_start_lba, 1, (unsigned short*)root_dir);
    for (int i = 0; i < 16; i++) {
        if (root_dir[i].name[0] == 0 || root_dir[i].name[0] == (char)0xE5) continue; 
        if (strncmp(root_dir[i].name, filename, 8) == 0 && strncmp(root_dir[i].ext, ext, 3) == 0) {
            unsigned int actual_lba = data_start_lba + ((root_dir[i].cluster - 2) * bpb.sectors_per_cluster);
            if (actual_lba < data_start_lba) return -1; 
            
            // Eğer dosya bir şekilde 0 bayt kalmışsa bile, okuma esnasında kilidi engelle!
            unsigned int sectors_to_read = (root_dir[i].size == 0) ? 1 : ((root_dir[i].size + 511) / 512);
            ata_lba_read(actual_lba, sectors_to_read, (unsigned short*)target_buffer);
            return root_dir[i].size; 
        }
    }
    return -1; 
}

int ardaos_write_file(const char* filename, const char* ext, unsigned int size, unsigned char* source_buffer) {
    // DONANIM ZIRHI 3: Herhangi bir hata sonucu boyut 0 gelirse, 0 Baytlık dosya oluşmasını engelle!
    if (size == 0 || size > 10240) { 
        size = 512; 
    }
    
    directory_entry_t root_dir[16];
    ata_lba_read(root_dir_start_lba, 1, (unsigned short*)root_dir);
    
    int target_slot = -1;
    int target_cluster = -1;
    
    for (int i = 0; i < 16; i++) {
        if (strncmp(root_dir[i].name, filename, 8) == 0 && strncmp(root_dir[i].ext, ext, 3) == 0) {
            target_slot = i;
            target_cluster = root_dir[i].cluster;
            if (target_cluster < 2) { target_cluster = -1; }
            break;
        }
    }
    dbg_print_term("Found slot:", target_slot);
    dbg_print_term("Found cluster:", target_cluster);
    
    if (target_slot == -1) {
        for (int i = 0; i < 16; i++) {
            if (root_dir[i].name[0] == 0x00 || root_dir[i].name[0] == (char)0xE5) {
                target_slot = i; break;
            }
        }
        if (target_slot == -1) return -1; 
    }

    if (target_cluster == -1) {
        unsigned short fat_table[256];
        unsigned int fat_lba = bpb.reserved_sectors;
        ata_lba_read(fat_lba, 1, fat_table);
        for (int i = 2; i < 256; i++) { 
            if (fat_table[i] == 0x0000) {
                target_cluster = i;
                fat_table[i] = 0xFFFF; 
                break;
            }
        }
        if (target_cluster == -1) return -1; 
        ata_lba_write(fat_lba, 1, fat_table); 
        if (bpb.fat_count > 1) ata_lba_write(fat_lba + bpb.sectors_per_fat, 1, fat_table);
        
        for(int i=0; i<8; i++) root_dir[target_slot].name[i] = filename[i];
        for(int i=0; i<3; i++) root_dir[target_slot].ext[i] = ext[i];
        root_dir[target_slot].attr = 0x00;
        root_dir[target_slot].cluster = target_cluster;
        root_dir[target_slot].size = 0;  // Yeni dosya başlangıçta 0 bayt
        root_dir[target_slot].time = 0;
        root_dir[target_slot].date = 0;
        dbg_print_term("Allocated cluster:", target_cluster);
    }
    
    // Veri yazılacak LBA'yı hesapla (SADECE cluster valid olduktan sonra)
    unsigned int actual_lba = data_start_lba + ((target_cluster - 2) * bpb.sectors_per_cluster);
    if (actual_lba <= root_dir_start_lba || target_cluster < 2) { 
        return -1; 
    }
    
    // 1. ADIM: Veri hemen diske yaz
    unsigned int sectors_to_write = (size + 511) / 512;
    if (sectors_to_write == 0) sectors_to_write = 1; // En az 1 sektör yaz
    dbg_print_term("Writing LBA:", actual_lba);
    dbg_print_term("Sectors:", sectors_to_write);
    ata_lba_write(actual_lba, sectors_to_write, (unsigned short*)source_buffer);
    api_print("ata_lba_write called");

    // Veri yazma sonrası hemen okuma ile doğrulama
    unsigned short verify_sector[256];
    ata_lba_read(actual_lba, 1, verify_sector);
    int nonzero = 0;
    for (int vi = 0; vi < 256; vi++) {
        if (verify_sector[vi] != 0) { nonzero = 1; break; }
    }
    if (nonzero) {
        api_print("DATA VERIFY: nonzero");
    } else {
        api_print("DATA VERIFY: all zero");
    }
    
    // 2. ADIM: Veri yazıldıktan sonra directory size'ını ayarla ve disk'e yaz
    root_dir[target_slot].size = size;
    ata_lba_write(root_dir_start_lba, 1, (unsigned short*)root_dir);

    // Doğrulama: directory'yi tekrar oku ve yazılan size'ı kontrol et
    directory_entry_t verify_dir[16];
    ata_lba_read(root_dir_start_lba, 1, (unsigned short*)verify_dir);
    if (verify_dir[target_slot].size != size) {
        char msg[64];
        itoa(verify_dir[target_slot].size, msg);
        api_print("VERIFY SIZE:");
        api_print(msg);
    } else {
        api_print("VERIFY OK: size matches");
    }
    
    return 0; 
}

void ardaos_list_files(char* output_buffer) {
    directory_entry_t root_dir[16];
    ata_lba_read(root_dir_start_lba, 1, (unsigned short*)root_dir); 
    strcpy(output_buffer, "=== FAT16 DISK ICERIGI ===\n");
    int found = 0;
    for (int i = 0; i < 16; i++) {
        if (root_dir[i].name[0] != 0 && root_dir[i].name[0] != (char)0xE5) {
            if (root_dir[i].attr == 0x0F || (root_dir[i].attr == 0x08)) continue;
            found++;
            char temp_name[9]; char temp_ext[4];
            for(int j=0; j<8; j++) temp_name[j] = root_dir[i].name[j];
            for(int j=0; j<3; j++) temp_ext[j] = root_dir[i].ext[j];
            temp_name[8] = '\0'; temp_ext[3] = '\0';
            
            strcat(output_buffer, "- "); strcat(output_buffer, temp_name);
            
            // YENİ: Dizin ise [KLASOR] yazdır, değilse boyutunu yazdır
            if (root_dir[i].attr & 0x10) {
                strcat(output_buffer, "   [KLASOR]\n");
            } else {
                strcat(output_buffer, "."); strcat(output_buffer, temp_ext);
                char size_str[16]; itoa(root_dir[i].size, size_str);
                strcat(output_buffer, "   ("); strcat(output_buffer, size_str); strcat(output_buffer, " Bayt)\n");
            }
        }
    }
    if (found == 0) strcat(output_buffer, "Disk tamamen bos.");
}
// ==========================================================
// FAT16 DOSYA SİLME MOTORU (0xE5 Sihri ve Zincir Kırma)
// ==========================================================
int ardaos_delete_file(const char* filename, const char* ext) {
    directory_entry_t root_dir[16];
    ata_lba_read(root_dir_start_lba, 1, (unsigned short*)root_dir);

    int target_slot = -1;
    unsigned short target_cluster = 0;

    // 1. Dosyayı Root Directory'de Bul
    for (int i = 0; i < 16; i++) {
        if (root_dir[i].name[0] == 0x00 || root_dir[i].name[0] == (char)0xE5) continue;
        if (strncmp(root_dir[i].name, filename, 8) == 0 && strncmp(root_dir[i].ext, ext, 3) == 0) {
            target_slot = i;
            target_cluster = root_dir[i].cluster;
            break;
        }
    }

    if (target_slot == -1) return -1; // Dosya bulunamadı

    // 2. Dosyanın isminin ilk harfini 0xE5 (Silinmiş) olarak işaretle
    root_dir[target_slot].name[0] = (char)0xE5;
    ata_lba_write(root_dir_start_lba, 1, (unsigned short*)root_dir);

    // 3. FAT Tablosundaki Alanları Serbest Bırak (Zinciri Kır)
    if (target_cluster >= 2) {
        unsigned short fat_table[256];
        unsigned int fat_lba = bpb.reserved_sectors;
        ata_lba_read(fat_lba, 1, fat_table);

        unsigned short current_cluster = target_cluster;
        // FAT16 dosya sonu işareti 0xFFF8'den büyüktür
        while (current_cluster >= 2 && current_cluster < 0xFFF8) {
            unsigned short next_cluster = fat_table[current_cluster];
            fat_table[current_cluster] = 0x0000; // Sektörü boşa çıkar
            current_cluster = next_cluster;
        }

        ata_lba_write(fat_lba, 1, fat_table);
        if (bpb.fat_count > 1) {
            ata_lba_write(fat_lba + bpb.sectors_per_fat, 1, fat_table);
        }
    }
    return 0; // Başarıyla silindi
}

// ==========================================================
// FAT16 KLASÖR OLUŞTURMA MOTORU (Attribute 0x10)
// ==========================================================
int ardaos_create_dir(const char* dirname) {
    directory_entry_t root_dir[16];
    ata_lba_read(root_dir_start_lba, 1, (unsigned short*)root_dir);

    int target_slot = -1;
    for (int i = 0; i < 16; i++) {
        if (root_dir[i].name[0] == 0x00 || root_dir[i].name[0] == (char)0xE5) {
            target_slot = i; break; // Boş yer bulundu
        }
    }

    if (target_slot == -1) return -1; // Dizin dolu

    // İsmi kopyala (Boşluk ile doldurulmuş)
    for(int i=0; i<8; i++) root_dir[target_slot].name[i] = dirname[i];
    for(int i=0; i<3; i++) root_dir[target_slot].ext[i] = ' '; 
    
    root_dir[target_slot].attr = 0x10; // 0x10 ATTR_DIRECTORY işaretleyicisi
    root_dir[target_slot].cluster = 0; 
    root_dir[target_slot].size = 0;
    
    ata_lba_write(root_dir_start_lba, 1, (unsigned short*)root_dir);
    return 0;
}
// ==========================================================
// SANAL DOSYA SİSTEMİ (VFS) MOTORU
// ==========================================================

// 1. sys_open: Dosyayı bul, bilet numarasını (FD) dön
int vfs_open(const char* filename, const char* ext) {
    if (current_task == 0) return -1;
    
    int free_fd = -1;
    for (int i = 0; i < MAX_FD_PER_TASK; i++) {
        if (current_task->fd_table[i].is_open == 0) {
            free_fd = i; break;
        }
    }
    if (free_fd == -1) return -1;

    // ==========================================
    // YENİ: SANAL DONANIM (DEVICE) KONTROLÜ
    // ==========================================
    // Eğer açılmak istenen dosya "DEV.KBD" (Keyboard Device) ise diske gitme!
    if (strncmp(filename, "DEV", 3) == 0 && strncmp(ext, "KBD", 3) == 0) {
        current_task->fd_table[free_fd].is_open = 1;
        current_task->fd_table[free_fd].type = 1; // 1 = Sanal Cihaz (Klavye)
        current_task->fd_table[free_fd].size = 0xFFFFFFFF; // Klavye akışının sonu yoktur!
        current_task->fd_table[free_fd].offset = 0;
        return free_fd;
    }
    // YENİ: Ağ Soketi Açma (UNIX Felsefesi)
    if (strncmp(filename, "NET", 3) == 0 && strncmp(ext, "UDP", 3) == 0) {
        current_task->fd_table[free_fd].is_open = 1;
        current_task->fd_table[free_fd].type = 2; // 2 = Ağ Soketi
        current_task->fd_table[free_fd].size = 0xFFFFFFFF; 
        current_task->fd_table[free_fd].offset = 0;
        
        // Muazzam Bir Hack: QEMU'nun dâhili DNS Sunucusuna (10.0.2.3:53) bağlanıyoruz.
        // Oraya rastgele metin yolladığımızda bize kızıp "Format Hatası" diye geri UDP yollayacak!
        current_task->fd_table[free_fd].target_ip[0] = 10;
        current_task->fd_table[free_fd].target_ip[1] = 0;
        current_task->fd_table[free_fd].target_ip[2] = 2;
        current_task->fd_table[free_fd].target_ip[3] = 3;
        current_task->fd_table[free_fd].target_port = 53; // DNS Portu
        current_task->fd_table[free_fd].local_port = 5555;
        
        return free_fd;
    }

    // Klasik FAT16 dizin arama kodu
    directory_entry_t root_dir[16]; 
    ata_lba_read(root_dir_start_lba, 1, (unsigned short*)root_dir);
    for (int i = 0; i < 16; i++) {
        if (root_dir[i].name[0] == 0 || root_dir[i].name[0] == (char)0xE5) continue; 
        if (strncmp(root_dir[i].name, filename, 8) == 0 && strncmp(root_dir[i].ext, ext, 3) == 0) {
            current_task->fd_table[free_fd].is_open = 1;
            current_task->fd_table[free_fd].type = 0; // 0 = Normal Disk Dosyası
            current_task->fd_table[free_fd].size = root_dir[i].size;
            current_task->fd_table[free_fd].offset = 0;
            current_task->fd_table[free_fd].cluster = root_dir[i].cluster;
            current_task->fd_table[free_fd].lba_start = data_start_lba + ((root_dir[i].cluster - 2) * bpb.sectors_per_cluster);
            return free_fd;
        }
    }
    return -1; 
}

// 2. sys_read: Bilet numarasına (FD) göre diskten veya cihazdan oku
int vfs_read(int fd, unsigned char* target_buffer, int count) {
    if (current_task == 0 || fd < 0 || fd >= MAX_FD_PER_TASK) return -1;
    if (current_task->fd_table[fd].is_open == 0) return -1; 

    file_obj_t* file = &current_task->fd_table[fd];
    
    // ==========================================
    // YENİ: EĞER BU BİR SANAL KLAVYE İSE
    // ==========================================
    if (file->type == 1) {
        extern int kernel_read_keyboard(unsigned char* buffer);
        return kernel_read_keyboard(target_buffer); 
    }
    // YENİ: Soketten Veri Okuma
    if (file->type == 2) {
        extern int udp_inbox_ready;
        extern int udp_inbox_size;
        extern unsigned char udp_inbox[];
        
        if (udp_inbox_ready) {
            int to_copy = count < udp_inbox_size ? count : udp_inbox_size;
            for(int i = 0; i < to_copy; i++) target_buffer[i] = udp_inbox[i];
            target_buffer[to_copy] = '\0';
            udp_inbox_ready = 0; // Okuduk, kutuyu boşalt
            return to_copy;
        }
        
        return 0;
    }
    // Eğer Disk Dosyasıysa (type == 0), eski okuma mantığına devam et:
    if (file->offset >= file->size) return 0; 
    
    int bytes_left = file->size - file->offset;
    if (count > bytes_left) count = bytes_left;
    
    unsigned int sectors_to_read = (file->size + 511) / 512;
    if (sectors_to_read == 0) sectors_to_read = 1;
    
    ata_lba_read(file->lba_start, sectors_to_read, (unsigned short*)target_buffer);
    
    file->offset += count; 
    return count; 
}

// 3. sys_close: Bileti (FD) iptal et
void vfs_close(int fd) {
    if (current_task != 0 && fd >= 0 && fd < MAX_FD_PER_TASK) {
        current_task->fd_table[fd].is_open = 0; 
    }
}
// YENİ: VFS Üzerinden Veri (veya Ağ Paketi) Yazma Motoru
int vfs_write(int fd, unsigned char* buffer, int count) {
    if (current_task == 0 || fd < 0 || fd >= MAX_FD_PER_TASK) return -1;
    if (current_task->fd_table[fd].is_open == 0) return -1; 

    file_obj_t* file = &current_task->fd_table[fd];
    
    // Eğer bu bir Ağ Soketiyse, VFS veriyi doğrudan UDP motoruna yollar!
    if (file->type == 2) {
        extern void rtl8139_send_udp(unsigned char*, unsigned short, unsigned short, unsigned char*, int);
        rtl8139_send_udp(file->target_ip, file->target_port, file->local_port, buffer, count);
        return count;
    }
    
    return -1; // Disk dosyalarına yazma (şimdilik desteklenmiyor)
}