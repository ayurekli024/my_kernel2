#include "disk.h"
#include "io.h"
#include "string.h"
#include "graphics.h"
#include "task.h"
#include "rtl8139.h"
fat16_bpb_t bpb;
unsigned int root_dir_start_lba;
unsigned int data_start_lba;
// =========================================================
// YENİ: UNIX İSİMLENDİRİLMİŞ BORULARI (NAMED PIPES / FIFO)
// =========================================================
typedef struct {
    int active;
    char name[16];     
    char buffer[512];  
    int head;          
    int tail;          
    int count;         
} pipe_t;

pipe_t system_pipes[16] = {0}; // = {0} ekleyerek tüm belleği kesin olarak sıfırla!
int fat16_find_entry(const char* full_path, directory_entry_t* out_entry); // YENİ EKLENEN PROTOTİP

static unsigned int cluster_to_lba(unsigned short cluster);
static int parse_path_node(const char** path, char* name_8, char* ext_3);
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
    char full_path[64];
    int p = 0;
    int has_dot = 0;
    
    // Gelen ismin içinde zaten bir uzantı (Nokta) var mı kontrol et
    if (filename != 0) {
        for(int k = 0; k < 60 && filename[k] != ' ' && filename[k] != '\0'; k++) {
            full_path[p++] = filename[k];
            if (filename[k] == '.') has_dot = 1;
        }
    }
    
    // Eğer isimde nokta yoksa, Kernel'in gönderdiği "ELF" gibi uzantıları ekle
    if (!has_dot && ext != 0 && ext[0] != ' ' && ext[0] != '\0') {
        full_path[p++] = '.';
        for(int k = 0; k < 3 && ext[k] != ' ' && ext[k] != '\0'; k++) {
            full_path[p++] = ext[k];
        }
    }
    full_path[p] = '\0';

    directory_entry_t entry;
    
    // Yeni Hiyerarşik Arama Motorumuzu kullan!
    if (fat16_find_entry(full_path, &entry) == 0) {
        if (entry.attr & 0x10) return -1; // Klasörler çalıştırılamaz
        
        unsigned int actual_lba = cluster_to_lba(entry.cluster);
        unsigned int sectors_to_read = (entry.size == 0) ? 1 : ((entry.size + 511) / 512);
        
        unsigned int current_lba = actual_lba;
        unsigned short* dest_ptr = (unsigned short*)target_buffer;
        
        // Devasa uygulamaları (ELF) donanımı kitlemeden 128 sektörlük parçalarla (Chunking) RAM'e yükle
        while (sectors_to_read > 0) {
            unsigned char chunk = (sectors_to_read > 128) ? 128 : sectors_to_read;
            ata_lba_read(current_lba, chunk, dest_ptr);
            current_lba += chunk;
            dest_ptr += (chunk * 256);
            sectors_to_read -= chunk;
        }
        return entry.size; 
    }
    return -1; // Uygulama bulunamadı
}

// 2. ardaos_write_file FONKSİYONUNU TÜM KÖK DİZİNİ TARAYACAK ŞEKİLDE DEĞİŞTİRİN
int ardaos_write_file(const char* filename, const char* ext, unsigned int size, unsigned char* source_buffer) {
    if (size == 0 || size > 10240) size = 512; 
    
    unsigned int root_sectors = (bpb.dir_entries * 32) / 512;
    if (root_sectors == 0) root_sectors = 32;

    directory_entry_t dir[16];
    int target_sec = -1;
    int target_slot = -1;
    int target_cluster = -1;
    
    // 1. Dosya var mı kontrol et
    for (unsigned int s = 0; s < root_sectors; s++) {
        ata_lba_read(root_dir_start_lba + s, 1, (unsigned short*)dir);
        for (int i = 0; i < 16; i++) {
            if (dir[i].name[0] == 0x00) break;
            if (dir[i].name[0] == (char)0xE5) continue;
            if (strncmp(dir[i].name, filename, 8) == 0 && strncmp(dir[i].ext, ext, 3) == 0) {
                target_sec = s; target_slot = i; target_cluster = dir[i].cluster;
                if (target_cluster < 2) target_cluster = -1;
                break;
            }
        }
        if (target_slot != -1) break;
    }
    
    // 2. Yoksa boş bir yuva bul
    if (target_slot == -1) {
        for (unsigned int s = 0; s < root_sectors; s++) {
            ata_lba_read(root_dir_start_lba + s, 1, (unsigned short*)dir);
            for (int i = 0; i < 16; i++) {
                if (dir[i].name[0] == 0x00 || dir[i].name[0] == (char)0xE5) {
                    target_sec = s; target_slot = i; break;
                }
            }
            if (target_slot != -1) break;
        }
    }
    if (target_slot == -1) return -1;

    ata_lba_read(root_dir_start_lba + target_sec, 1, (unsigned short*)dir);

    // 3. Cluster tahsisi
    if (target_cluster == -1) {
        unsigned short fat_table[256];
        unsigned int fat_lba = bpb.reserved_sectors;
        unsigned int fat_sectors = bpb.sectors_per_fat;
        int found_cluster = -1;

        for (unsigned int fs = 0; fs < fat_sectors; fs++) {
            ata_lba_read(fat_lba + fs, 1, fat_table);
            int start_c = (fs == 0) ? 2 : 0;
            for (int i = start_c; i < 256; i++) {
                if (fat_table[i] == 0x0000) {
                    found_cluster = (fs * 256) + i;
                    fat_table[i] = 0xFFFF;
                    ata_lba_write(fat_lba + fs, 1, fat_table);
                    if (bpb.fat_count > 1) ata_lba_write(fat_lba + bpb.sectors_per_fat + fs, 1, fat_table);
                    break;
                }
            }
            if (found_cluster != -1) break;
        }
        if (found_cluster == -1) return -1;
        target_cluster = found_cluster;

        for(int i = 0; i < 8; i++) dir[target_slot].name[i] = filename[i];
        for(int i = 0; i < 3; i++) dir[target_slot].ext[i] = ext[i];
        dir[target_slot].attr = 0x00;
        dir[target_slot].cluster = target_cluster;
        dir[target_slot].size = 0;
        dir[target_slot].time = 0;
        dir[target_slot].date = 0;
    }

    unsigned int actual_lba = data_start_lba + ((target_cluster - 2) * bpb.sectors_per_cluster);
    if (actual_lba <= root_dir_start_lba || target_cluster < 2) return -1;

    unsigned int sectors_to_write = (size + 511) / 512;
    if (sectors_to_write == 0) sectors_to_write = 1;
    ata_lba_write(actual_lba, sectors_to_write, (unsigned short*)source_buffer);

    dir[target_slot].size = size;
    ata_lba_write(root_dir_start_lba + target_sec, 1, (unsigned short*)dir);
    return 0;
}

void ardaos_list_files(char* output_buffer) {
    unsigned int root_sectors = (bpb.dir_entries * 32) / 512;
    if (root_sectors == 0) root_sectors = 32;

    directory_entry_t dir[16];
    strcpy(output_buffer, "=== FAT16 DISK ICERIGI ===\n");
    int found = 0;
    
    for (unsigned int s = 0; s < root_sectors; s++) {
        ata_lba_read(root_dir_start_lba + s, 1, (unsigned short*)dir);
        for (int i = 0; i < 16; i++) {
            if (dir[i].name[0] == 0) return;
            if (dir[i].name[0] == (char)0xE5 || dir[i].attr == 0x0F || dir[i].attr == 0x08) continue;
            found++;
            char temp_name[9]; char temp_ext[4];
            for(int j = 0; j < 8; j++) temp_name[j] = dir[i].name[j];
            for(int j = 0; j < 3; j++) temp_ext[j] = dir[i].ext[j];
            temp_name[8] = '\0'; temp_ext[3] = '\0';
            
            strcat(output_buffer, "- "); strcat(output_buffer, temp_name);
            if (dir[i].attr & 0x10) {
                strcat(output_buffer, "   [KLASOR]\n");
            } else {
                strcat(output_buffer, "."); strcat(output_buffer, temp_ext);
                char size_str[16]; itoa(dir[i].size, size_str);
                strcat(output_buffer, "   ("); strcat(output_buffer, size_str); strcat(output_buffer, " Bayt)\n");
            }
        }
    }
    if (found == 0) strcat(output_buffer, "Disk tamamen bos.");
}
// ==========================================================
// FAT16 DOSYA SİLME MOTORU (0xE5 Sihri ve Zincir Kırma)
// ==========================================================
// 4. ardaos_delete_file FONKSİYONUNU TÜM SEKTÖRLERİ GEZECEK ŞEKİLDE GÜNCELLEYİN
int ardaos_delete_file(const char* filename, const char* ext) {
    unsigned int root_sectors = (bpb.dir_entries * 32) / 512;
    if (root_sectors == 0) root_sectors = 32;

    directory_entry_t dir[16];
    int target_sec = -1;
    int target_slot = -1;
    unsigned short target_cluster = 0;

    for (unsigned int s = 0; s < root_sectors; s++) {
        ata_lba_read(root_dir_start_lba + s, 1, (unsigned short*)dir);
        for (int i = 0; i < 16; i++) {
            if (dir[i].name[0] == 0x00) break;
            if (dir[i].name[0] == (char)0xE5) continue;
            if (strncmp(dir[i].name, filename, 8) == 0 && strncmp(dir[i].ext, ext, 3) == 0) {
                target_sec = s; target_slot = i; target_cluster = dir[i].cluster; break;
            }
        }
        if (target_slot != -1) break;
    }
    if (target_slot == -1) return -1;

    ata_lba_read(root_dir_start_lba + target_sec, 1, (unsigned short*)dir);
    dir[target_slot].name[0] = (char)0xE5;
    ata_lba_write(root_dir_start_lba + target_sec, 1, (unsigned short*)dir);

    if (target_cluster >= 2) {
        unsigned short fat_table[256];
        unsigned int fat_lba = bpb.reserved_sectors;
        ata_lba_read(fat_lba, 1, fat_table);

        unsigned short current_cluster = target_cluster;
        while (current_cluster >= 2 && current_cluster < 0xFFF8) {
            unsigned short next_cluster = fat_table[current_cluster];
            fat_table[current_cluster] = 0x0000;
            current_cluster = next_cluster;
        }
        ata_lba_write(fat_lba, 1, fat_table);
        if (bpb.fat_count > 1) ata_lba_write(fat_lba + bpb.sectors_per_fat, 1, fat_table);
    }
    return 0;
}
// ==========================================================
// EBEVEYN DİZİN ÇÖZÜCÜ (PARENT DIRECTORY RESOLVER)
// ==========================================================
// Hedef klasörün içine girebilmek için fiziksel sektör adresini (LBA) bulur
static int fat16_get_parent_dir(const char* full_path, unsigned int* out_dir_lba, unsigned int* out_dir_sectors, char* target_name, char* target_ext) {
    char name_8[8], ext_3[3];
    const char* current_path = full_path;
    const char* next_path;

    unsigned int current_dir_lba = root_dir_start_lba;
    unsigned int dir_sectors = 32; // Root dizin

    while (1) {
        next_path = current_path;
        if (!parse_path_node(&next_path, name_8, ext_3)) break;

        if (*next_path == '\0') { // Yolun sonuna geldik, Ebeveyn klasörü burası!
            *out_dir_lba = current_dir_lba;
            *out_dir_sectors = dir_sectors;
            for(int k=0; k<8; k++) target_name[k] = name_8[k];
            for(int k=0; k<3; k++) target_ext[k] = ext_3[k];
            return 0; 
        }

        int found = 0;
        directory_entry_t dir[16];
        for (unsigned int s = 0; s < dir_sectors; s++) {
            ata_lba_read(current_dir_lba + s, 1, (unsigned short*)dir);
            for (int i = 0; i < 16; i++) {
                if (dir[i].name[0] == 0) break;
                if (dir[i].name[0] == (char)0xE5) continue;
                
                int name_match = 1;
                for (int k = 0; k < 8; k++) if (dir[i].name[k] != name_8[k]) name_match = 0;
                if (name_match && (dir[i].attr & 0x10)) { // Eğer bu bir alt klasörse içine gir!
                    current_dir_lba = cluster_to_lba(dir[i].cluster);
                    dir_sectors = bpb.sectors_per_cluster;
                    found = 1; break;
                }
            }
            if (found) break;
        }
        if (!found) return -1; // Aradaki bir klasör eksik
        current_path = next_path;
    }
    return -1;
}

// ==========================================================
// FAT16 HİYERARŞİK KLASÖR OLUŞTURMA MOTORU
// ==========================================================
int ardaos_create_dir(const char* full_path) {
    unsigned int p_lba, p_sectors;
    char name[8], ext[3];
    
    // İç içe klasörlerin (Örn: SISTEM/AYARLAR) ebeveyn LBA adresini bul!
    if (fat16_get_parent_dir(full_path, &p_lba, &p_sectors, name, ext) != 0) return -1;

    directory_entry_t dir[16];
    int target_sector = -1, target_slot = -1;

    // Ebeveyn klasörün sektörlerini tarayarak ilk boş yuveyi (Slot) bul
    for (unsigned int s = 0; s < p_sectors; s++) {
        ata_lba_read(p_lba + s, 1, (unsigned short*)dir);
        for (int i = 0; i < 16; i++) {
            if (dir[i].name[0] == 0x00 || dir[i].name[0] == (char)0xE5) {
                target_sector = s; target_slot = i; break;
            }
        }
        if (target_slot != -1) break;
    }

    if (target_slot == -1) return -1; // Klasör tamamen dolu

    // Boş yuvaya yeni klasörün bilgilerini yaz ve diske kaydet
    ata_lba_read(p_lba + target_sector, 1, (unsigned short*)dir);
    for(int i=0; i<8; i++) dir[target_slot].name[i] = name[i];
    for(int i=0; i<3; i++) dir[target_slot].ext[i] = ' '; 
    
    dir[target_slot].attr = 0x10; // 0x10: Bu bir KLASÖRDÜR
    dir[target_slot].cluster = 0; // İçine dosya konulana kadar diskte yer kaplamasın
    dir[target_slot].size = 0;
    
    ata_lba_write(p_lba + target_sector, 1, (unsigned short*)dir);
    return 0; // Başarıyla yaratıldı
}
// ==========================================================
// YENİ: HİYERARŞİK DOSYA SİSTEMİ (PATH PARSER & TRAVERSAL)
// ==========================================================

// Cluster numarasını fiziksel LBA sektörüne çevirir
static unsigned int cluster_to_lba(unsigned short cluster) {
    if (cluster < 2) return root_dir_start_lba;
    return data_start_lba + ((cluster - 2) * bpb.sectors_per_cluster);
}

// Yolu (Path) "/" işaretlerinden böler ve 8.3 FAT formatına ayıklar
static int parse_path_node(const char** path, char* name_8, char* ext_3) {
    if (**path == '\0') return 0; 
    if (**path == '/' || **path == '\\') (*path)++; // Baştaki slash'ı atla
    if (**path == '\0') return 0;
    
    for (int i = 0; i < 8; i++) name_8[i] = ' ';
    for (int i = 0; i < 3; i++) ext_3[i] = ' ';
    
    int i = 0;
    while (**path && **path != '/' && **path != '\\' && **path != '.') {
        if (i < 8) {
            char c = **path;
            if (c >= 'a' && c <= 'z') c -= 32; // Otomatik Büyük Harf Dönüşümü
            name_8[i++] = c;
        }
        (*path)++;
    }
    
    if (**path == '.') {
        (*path)++;
        int j = 0;
        while (**path && **path != '/' && **path != '\\') {
            if (j < 3) {
                char c = **path;
                if (c >= 'a' && c <= 'z') c -= 32;
                ext_3[j++] = c;
            }
            (*path)++;
        }
    }
    
    if (**path == '/' || **path == '\\') (*path)++;
    return 1; // Bir klasör veya dosya düğümü (Node) başarıyla okundu
}
// İç içe geçmiş yolu takip ederek hedef dosyayı/klasörü bulur
int fat16_find_entry(const char* full_path, directory_entry_t* out_entry) {
    char name_8[8], ext_3[3];
    const char* current_path = full_path;
    
    unsigned int current_dir_lba = root_dir_start_lba;
    unsigned int dir_sectors = 32; // Root dizin varsayılan olarak 32 sektördür
    
    // Yoldaki her bir düğümü (Klasör veya Dosya) sırayla tara
    while (parse_path_node(&current_path, name_8, ext_3)) {
        int found = 0;
        directory_entry_t dir[16];
        
        for (unsigned int s = 0; s < dir_sectors; s++) {
            ata_lba_read(current_dir_lba + s, 1, (unsigned short*)dir);
            for (int i = 0; i < 16; i++) {
                if (dir[i].name[0] == 0) break; // Klasörün sonu
                if (dir[i].name[0] == (char)0xE5) continue; // Silinmiş dosya
                
                int name_match = 1, ext_match = 1;
                for (int k = 0; k < 8; k++) if (dir[i].name[k] != name_8[k]) name_match = 0;
                for (int k = 0; k < 3; k++) if (dir[i].ext[k] != ext_3[k]) ext_match = 0;
                
                if (name_match && ext_match) {
                    *out_entry = dir[i];
                    found = 1;
                    break;
                }
            }
            if (found) break;
        }
        
        if (!found) return -1; // Yolun bu kısmı kopuk (Dosya/Klasör yok)
        
        // Eğer hedef bulunmasına rağmen yol devam ediyorsa, bulduğumuz şey bir klasör olmalı!
        if (*current_path != '\0') {
            if (!(out_entry->attr & 0x10)) return -1; // Klasör değil ama kullanıcı içine girmeye çalışıyor!
            current_dir_lba = cluster_to_lba(out_entry->cluster);
            dir_sectors = bpb.sectors_per_cluster; // Alt klasörlerin boyutu Cluster kadardır
        }
    }
    return 0; // Dosya veya son klasör başarıyla bulundu
}
// ==========================================================
// SANAL DOSYA SİSTEMİ (VFS) MOTORU
// ==========================================================

// 1. sys_open: Dosyayı bul, bilet numarasını (FD) dön
int vfs_open(const char* filename, const char* ext) {
    if (current_task == 0) return -1;
    
    int free_fd = -1;
    for (int i = 0; i < MAX_FD_PER_TASK; i++) {
        if (current_task->fd_table[i].is_open == 0) { free_fd = i; break; }
    }
    if (free_fd == -1) return -1;

    if (strncmp(filename, "DEV", 3) == 0 && strncmp(ext, "KBD", 3) == 0) {
        current_task->fd_table[free_fd].is_open = 1; current_task->fd_table[free_fd].type = 1;
        current_task->fd_table[free_fd].size = 0xFFFFFFFF; current_task->fd_table[free_fd].offset = 0;
        return free_fd;
    }
    if (strncmp(filename, "NET", 3) == 0 && strncmp(ext, "UDP", 3) == 0) {
        current_task->fd_table[free_fd].is_open = 1; current_task->fd_table[free_fd].type = 2;
        current_task->fd_table[free_fd].size = 0xFFFFFFFF; current_task->fd_table[free_fd].offset = 0;
        current_task->fd_table[free_fd].target_ip[0] = 10; current_task->fd_table[free_fd].target_ip[1] = 0;
        current_task->fd_table[free_fd].target_ip[2] = 2; current_task->fd_table[free_fd].target_ip[3] = 3;
        current_task->fd_table[free_fd].target_port = 53; current_task->fd_table[free_fd].local_port = 5555;
        return free_fd;
    }
    if (strncmp(filename, "NET", 3) == 0 && strncmp(ext, "TCP", 3) == 0) {
        extern int net_socket_create(void);
        extern int net_tcp_connect(int, unsigned char*, unsigned short);
        
        int sock_id = net_socket_create();
        if (sock_id < 0) return -1; // Soket havuzu doluysa reddet

        current_task->fd_table[free_fd].is_open = 1; 
        current_task->fd_table[free_fd].type = 3;
        current_task->fd_table[free_fd].cluster = sock_id; // Soket ID'sini VFS'de sakla!
        
        unsigned char dest_ip[4] = {142, 250, 187, 46}; // Şimdilik hala Google
        net_tcp_connect(sock_id, dest_ip, 80);
        return free_fd;
    }
    // --- YENİ: FIFO Boru Yönlendiricisi ---
    if (strncmp(ext, "FIFO", 4) == 0) {
        int p_id = -1;
        // 1. Bu isimde açık bir boru var mı? (Başka bir uygulama açmış olabilir)
        for (int i = 0; i < 16; i++) {
            if (system_pipes[i].active && strncmp(system_pipes[i].name, filename, 8) == 0) {
                p_id = i; break;
            }
        }
        // 2. Yoksa yeni bir boru yarat
        if (p_id == -1) {
            for (int i = 0; i < 16; i++) {
                if (!system_pipes[i].active) {
                    system_pipes[i].active = 1;
                    int k = 0; while (filename[k] && k < 8) { system_pipes[i].name[k] = filename[k]; k++; }
                    system_pipes[i].name[k] = '\0';
                    system_pipes[i].head = 0; 
                    system_pipes[i].tail = 0; 
                    system_pipes[i].count = 0;
                    p_id = i; break;
                }
            }
        }
        if (p_id == -1) return -1; // Boru havuzu dolu

        current_task->fd_table[free_fd].is_open = 1; 
        current_task->fd_table[free_fd].type = 4; // 4 = FIFO PIPE[cite: 7]
        current_task->fd_table[free_fd].cluster = p_id; // Boru ID'sini sakla
        return free_fd;
    }
    // =========================================================
    // YENİ: HİYERARŞİK DİSK ARAMA (PATH PARSER ENTEGRASYONU)
    // =========================================================
    char full_path[64];
    int p = 0;
    int has_dot = 0;
    
    if (filename != 0) {
        for(int k = 0; k < 60 && filename[k] != ' ' && filename[k] != '\0'; k++) {
            full_path[p++] = filename[k];
            if (filename[k] == '.') has_dot = 1;
        }
    }
    
    if (!has_dot && ext != 0 && ext[0] != ' ' && ext[0] != '\0') {
        full_path[p++] = '.';
        for(int k = 0; k < 3 && ext[k] != ' ' && ext[k] != '\0'; k++) {
            full_path[p++] = ext[k];
        }
    }
    full_path[p] = '\0';

    directory_entry_t entry;
    // Yeni yazdığımız efsanevi derinlemesine arama motorunu ateşle
    if (fat16_find_entry(full_path, &entry) == 0) {
        current_task->fd_table[free_fd].is_open = 1;
        current_task->fd_table[free_fd].type = 0; // 0 = Disk Dosyası
        current_task->fd_table[free_fd].size = entry.size;
        current_task->fd_table[free_fd].offset = 0;
        current_task->fd_table[free_fd].cluster = entry.cluster;
        // cluster_to_lba ile Alt Klasör dosyalarının fiziksel adresini kusursuz bul!
        current_task->fd_table[free_fd].lba_start = cluster_to_lba(entry.cluster);
        return free_fd;
    }

    return -1; // Dosya veya Yol (Path) bulunamadı
}

// 1. vfs_read İÇİNDEKİ DİSK OKUMA BLOĞUNU DEĞİŞTİRİN
int vfs_read(int fd, unsigned char* target_buffer, int count) {
    if (current_task == 0 || fd < 0 || fd >= MAX_FD_PER_TASK) return -1;
    if (current_task->fd_table[fd].is_open == 0) return -1; 

    file_obj_t* file = &current_task->fd_table[fd];
    
    if (file->type == 1) {
        extern int kernel_read_keyboard(unsigned char* buffer);
        return kernel_read_keyboard(target_buffer); 
    }
    if (file->type == 2) {
        extern int udp_inbox_ready; extern int udp_inbox_size; extern unsigned char udp_inbox[];
        if (udp_inbox_ready) {
            int to_copy = count < udp_inbox_size ? count : udp_inbox_size;
            for(int i = 0; i < to_copy; i++) target_buffer[i] = udp_inbox[i];
            target_buffer[to_copy] = '\0'; udp_inbox_ready = 0; 
            return to_copy;
        }
        return 0;
    }
    if (file->type == 3) {
        extern tcp_socket_t tcp_sockets[];
        int sock_id = file->cluster;
        if (sock_id < 0 || sock_id >= 16 || !tcp_sockets[sock_id].active) return -1;
        
        tcp_socket_t* sock = &tcp_sockets[sock_id];
        if (sock->state != TCP_ESTABLISHED) return 0;
        
        if (sock->rx_ready) {
            int to_copy = count < sock->rx_size ? count : sock->rx_size;
            for(int i = 0; i < to_copy; i++) target_buffer[i] = sock->rx_buf[i];
            target_buffer[to_copy] = '\0'; 
            sock->rx_ready = 0; 
            sock->rx_size = 0;
            return to_copy;
        }
        return 0; 
    }
    if (file->type == 4) { 
        int p_id = file->cluster;
        if (p_id < 0 || p_id >= 16 || !system_pipes[p_id].active) return -1;
        
        pipe_t* p = &system_pipes[p_id];
        int read_bytes = 0;
        while (read_bytes < count && p->count > 0) {
            target_buffer[read_bytes++] = p->buffer[p->tail];
            p->tail = (p->tail + 1) % 512;
            p->count--;
        }
        return read_bytes;
    }
    
    // GÜVENLİ VE BOYUT SINIRLI DİSK OKUMA (Buffer Overflow Koruması)
    if (file->offset >= file->size) return 0; 
    
    int bytes_left = file->size - file->offset;
    if (count > bytes_left) count = bytes_left;
    if (count <= 0) return 0;

    int bytes_read = 0;
    static unsigned short sector_temp[256];
    unsigned char* byte_temp = (unsigned char*)sector_temp;

    while (bytes_read < count) {
        unsigned int current_offset = file->offset;
        unsigned int sector_idx = current_offset / 512;
        unsigned int offset_in_sec = current_offset % 512;
        unsigned int cur_lba = file->lba_start + sector_idx;

        ata_lba_read(cur_lba, 1, sector_temp);

        int chunk = 512 - offset_in_sec;
        if (chunk > (count - bytes_read)) chunk = count - bytes_read;

        for (int i = 0; i < chunk; i++) {
            target_buffer[bytes_read + i] = byte_temp[offset_in_sec + i];
        }

        bytes_read += chunk;
        file->offset += chunk;
    }
    return bytes_read;
}

// 3. sys_close: Bileti (FD) iptal et
void vfs_close(int fd) {
    if (current_task != 0 && fd >= 0 && fd < MAX_FD_PER_TASK) {
        if (current_task->fd_table[fd].is_open) {
            if (current_task->fd_table[fd].type == 3) {
                // VFS kapanırken Soketi de güvenle kapat (FIN yolla) ve havuzu boşalt
                extern tcp_socket_t tcp_sockets[];
                int sock_id = current_task->fd_table[fd].cluster;
                if (sock_id >= 0 && sock_id < 16 && tcp_sockets[sock_id].active) {
                    net_tcp_send(sock_id, 0x11, 0, 0); // 0x11 = FIN | ACK
                    tcp_sockets[sock_id].active = 0;
                }
            }
            current_task->fd_table[fd].is_open = 0; 
        }
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
    // TCP Soketi (Veriyi PSH | ACK bayraklarıyla yollar)
    // TCP Soketi (Veriyi PSH | ACK bayraklarıyla yollar)
    if (file->type == 3) {
        extern tcp_socket_t tcp_sockets[];
        int sock_id = file->cluster;
        
        if (sock_id >= 0 && sock_id < 16 && tcp_sockets[sock_id].active) {
            if (tcp_sockets[sock_id].state == TCP_ESTABLISHED) { 
                return net_tcp_send(sock_id, 0x18, buffer, count); // 0x18 = PSH | ACK
            }
        }
        return -1;
    }
    // FIFO (Boru) Yazma İşlemi
    if (file->type == 4) { 
        int p_id = file->cluster;
        if (p_id < 0 || p_id >= 16 || !system_pipes[p_id].active) return -1;
        
        pipe_t* p = &system_pipes[p_id];
        int written = 0;
        
        // Boruda yer oldukça (512 bayt sınırı) yaz
        while (written < count && p->count < 512) {
            p->buffer[p->head] = buffer[written++];
            p->head = (p->head + 1) % 512; // Halkayı çevir
            p->count++;
        }
        return written; // Yazılan byte sayısını dön
    }
    
    return -1; // Disk dosyalarına yazma (şimdilik desteklenmiyor)
}
// =========================================================
// YENİ: VFS ÜZERİNDEN KLASÖR LİSTELEME
// =========================================================
int vfs_list_dir(const char* path, char* buffer) {
    unsigned int dir_lba = root_dir_start_lba;
    unsigned int dir_sectors = 32;
    
    // Eğer yol belirtilmişse (Örn: SISTEM) içine gir
    if (path != 0 && path[0] != '\0') {
        directory_entry_t entry;
        if (fat16_find_entry(path, &entry) != 0 || !(entry.attr & 0x10)) return -1;
        dir_lba = cluster_to_lba(entry.cluster);
        dir_sectors = bpb.sectors_per_cluster;
    }
    
    buffer[0] = '\0';
    directory_entry_t dir[16];
    int count = 0;
    
    for (unsigned int s = 0; s < dir_sectors; s++) {
        ata_lba_read(dir_lba + s, 1, (unsigned short*)dir);
        for (int i = 0; i < 16; i++) {
            if (dir[i].name[0] == 0) return count; // Dizin tamamen bitti
            if (dir[i].name[0] == (char)0xE5 || dir[i].attr == 0x0F || dir[i].attr == 0x08) continue;
            
            // GUI'nin ayırması için başına etiket koyuyoruz
            if (dir[i].attr & 0x10) strcat(buffer, "<D> ");
            else strcat(buffer, " ");
            
            // İsmi temizle
            char temp[9]; int t=0;
            for(int k=0; k<8 && dir[i].name[k] != ' '; k++) temp[t++] = dir[i].name[k];
            temp[t] = '\0'; strcat(buffer, temp);
            
            // Uzantıyı temizle
            if (!(dir[i].attr & 0x10)) {
                char ext[4]; int e=0;
                for(int k=0; k<3 && dir[i].ext[k] != ' '; k++) ext[e++] = dir[i].ext[k];
                ext[e] = '\0';
                if (e > 0) { strcat(buffer, "."); strcat(buffer, ext); }
            }
            strcat(buffer, "\n");
            count++;
        }
    }
    return count;
}