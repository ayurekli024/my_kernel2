#ifndef DISK_H
#define DISK_H

typedef struct {
    unsigned char jump[3];
    char oem[8];
    unsigned short bytes_per_sector;
    unsigned char sectors_per_cluster;
    unsigned short reserved_sectors;
    unsigned char fat_count;
    unsigned short dir_entries;
    unsigned short total_sectors;
    unsigned char media_descriptor;
    unsigned short sectors_per_fat;
    unsigned short sectors_per_track;
    unsigned short heads;
    unsigned int hidden_sectors;
    unsigned int large_sectors;
} __attribute__((packed)) fat16_bpb_t;

typedef struct {
    char name[8];               // 0-7: Dosya Adı
    char ext[3];                // 8-10: Uzantı
    unsigned char attr;         // 11: Özellikler (Dosya/Klasör)
    unsigned char reserved[10]; // 12-21: Rezerve
    unsigned short time;        // 22-23: Zaman
    unsigned short date;        // 24-25: Tarih
    unsigned short cluster;     // 26-27: Başlangıç Kümesi (Cluster)
    unsigned int size;          // 28-31: Dosya Boyutu (Bayt cinsinden)
} __attribute__((packed)) directory_entry_t;

void init_disk(void);
void ata_lba_read(unsigned int lba, unsigned char sector_count, unsigned short* target);
void ata_lba_write(unsigned int lba, unsigned char sector_count, unsigned short* source);
int ardaos_read_file(const char* filename, const char* ext, unsigned char* target_buffer);
int ardaos_write_file(const char* filename, const char* ext, unsigned int size, unsigned char* source_buffer);
void ardaos_list_files(char* output_buffer);

int ardaos_delete_file(const char* filename, const char* ext);
int ardaos_create_dir(const char* dirname);
int vfs_open(const char* filename, const char* ext);
int vfs_read(int fd, unsigned char* buffer, int count);
int vfs_write(int fd, unsigned char* buffer, int count);
void vfs_close(int fd);
int vfs_list_dir(const char* path, char* buffer);

int ardaos_delete_path(const char* full_path);
int ardaos_copy_file(const char* src_path, const char* dst_path);
#endif