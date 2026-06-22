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
    char name[8];          
    char ext[3];           
    unsigned char attr;    
    unsigned char reserved[10];
    unsigned short time;   
    unsigned short date;   
    unsigned short cluster; 
    unsigned int size;     
} __attribute__((packed)) directory_entry_t;

void init_disk(void); 
void ata_lba_read(unsigned int lba, unsigned char sector_count, unsigned short* target);
void ata_lba_write(unsigned int lba, unsigned char sector_count, unsigned short* source);
int ardaos_read_file(const char* filename, const char* ext, unsigned char* target_buffer);
int ardaos_write_file(const char* filename, const char* ext, unsigned int size, unsigned char* source_buffer);
void ardaos_list_files(char* output_buffer);

int ardaos_delete_file(const char* filename, const char* ext);
int ardaos_create_dir(const char* dirname);

#endif