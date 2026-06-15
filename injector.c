#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Çekirdekteki disk yapımızın birebir aynısı
typedef struct {
    char name[8]; char ext[3]; unsigned char attr; unsigned char res[10];
    unsigned short time; unsigned short date; unsigned short cluster; unsigned int size;
} __attribute__((packed)) dir_entry_t;

int main() {
    FILE *disk = fopen("c.img", "r+b"); // Diski güncelleme modunda aç
    FILE *app = fopen("sdk/app.bin", "rb"); // Derlenmiş uygulamamızı okuma modunda aç
    
    if(!disk || !app) {
        printf("Hata: c.img veya sdk/app.bin bulunamadi!\n");
        return 1;
    }

    // 1. Uygulamanın boyutunu ölç ve RAM'e al
    fseek(app, 0, SEEK_END);
    int size = ftell(app);
    fseek(app, 0, SEEK_SET);
    unsigned char *app_data = malloc(size);
    fread(app_data, 1, size, app);

    // 2. Diskin 19. Sektöründeki (Kök Dizin) 16 slotu oku
    dir_entry_t root[16];
    fseek(disk, 19 * 512, SEEK_SET);
    fread(root, sizeof(dir_entry_t), 16, disk);

    // 3. İlk boş slotu bul
    int slot = -1;
    for(int i = 0; i < 16; i++) {
        if(root[i].name[0] == 0) { slot = i; break; }
    }

    if (slot == -1) { printf("Hata: Diskte bos yer yok!\n"); return 1; }

    // 4. Dosya bilgilerini (Metadata) slota yaz
    strncpy(root[slot].name, "TESTAPP ", 8); // Dosya adı 8 karakter olmalı
    strncpy(root[slot].ext, "BIN", 3);
    root[slot].cluster = 20; // Dosya diskin 20. sektöründen başlayacak
    root[slot].size = size;

    // 5. Güncellenmiş indeks tablosunu 19. Sektöre geri yaz
    fseek(disk, 19 * 512, SEEK_SET);
    fwrite(root, sizeof(dir_entry_t), 16, disk);

    // 6. Uygulamanın gerçek verilerini 20. Sektöre yaz
    fseek(disk, 20 * 512, SEEK_SET);
    fwrite(app_data, 1, size, disk);

    fclose(app); fclose(disk); free(app_data);
    printf("BASARILI: TESTAPP.BIN diske (Sektor 20) enjekte edildi! (Boyut: %d Bayt)\n", size);
    return 0;
}