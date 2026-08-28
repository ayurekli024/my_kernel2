#include "ardaos.h"
#include "libc.h"

char current_path[128] = ""; 
char dir_content[2048];
char files[15][32]; 
int file_count = 0;

void load_directory() {
    sys_clear_gui(); 
    
    // 1. ÜST ARAÇ ÇUBUĞU (ID'ler sıfırdan başlar: 0, 1, 2)
    sys_create_button(10, 10, 60, 25, "<- GERI");      // ID: 0
    sys_create_button(80, 10, 60, 25, "YENILE");       // ID: 1
    sys_create_button(150, 10, 120, 25, "+ YENI KLASOR");// ID: 2

    // 2. YOL GÖSTERGESİ (Metni aşağı iterek butonlarla çakışmasını engelliyoruz)
    char window_text[2100] = "\n\n\n\n  MEVCUT KONUM: C:/";
    strcat(window_text, current_path);
    strcat(window_text, "\n  --------------------------------------\n");
    
    file_count = 0;
    int res = sys_list_dir(current_path, dir_content);
    
    if (res < 0) {
        strcat(window_text, "  [HATA] Klasor okunamadi veya bos!\n");
        sys_set_window_text(window_text);
        return;
    }

    // 3. İÇERİK LİSTESİ (Dosyalar ID 3'ten itibaren başlar)
    int i = 0;
    while (dir_content[i] != '\0' && file_count < 15) {
        int j = 0;
        while (dir_content[i] != '\n' && dir_content[i] != '\0') {
            files[file_count][j++] = dir_content[i++];
        }
        files[file_count][j] = '\0';
        if (dir_content[i] == '\n') i++;
        
        // Butonları daha aşağıdan (y = 100) ve düzenli çiz
        sys_create_button(20, 100 + (file_count * 28), 220, 22, files[file_count]);
        file_count++;
    }
    sys_set_window_text(window_text);
}

void _start(char* args) {
    sys_create_window("ArdaOS Gorsel Gezgin", 320, 560);
    load_directory();

    while(1) {
        int ev_id;
        int ev_type = sys_poll_event(&ev_id);
        
        if (ev_type == 1) { // Tıklama Yakalandı
            if (ev_id == 0) { // <- GERI
                int len = 0; while(current_path[len]) len++;
                while(len > 0 && current_path[len-1] != '/') len--; 
                if (len > 0) current_path[len-1] = '\0'; 
                else current_path[0] = '\0'; 
                load_directory();
            }
            else if (ev_id == 1) { // YENILE
                load_directory();
            }
            else if (ev_id == 2) { // + YENI KLASOR
                char new_dir_path[128]; new_dir_path[0] = '\0';
                if (current_path[0] != '\0') {
                    strcat(new_dir_path, current_path);
                    strcat(new_dir_path, "/");
                }
                strcat(new_dir_path, "YENIDIZN"); // FAT16 8-karakter sınırı
                sys_create_dir(new_dir_path);
                load_directory(); // Anında ekrana yansıt
            }
            else if (ev_id >= 3 && ev_id < file_count + 3) { // DOSYA VEYA KLASÖR SEÇİLDİ
                int file_idx = ev_id - 3;
                char* clicked = files[file_idx];
                
                if (clicked[0] == '<' && clicked[1] == 'D' && clicked[2] == '>') { // Klasör ise içine gir
                    if (current_path[0] != '\0') strcat(current_path, "/");
                    strcat(current_path, &clicked[4]); 
                    load_directory();
                }
                else { // YENİ: DOSYA İSE ÇALIŞTIR!
                    char target_file[128]; target_file[0] = '\0';
                    // Eğer alt klasördeysek yolu birleştir
                    if (current_path[0] != '\0') {
                        strcat(target_file, current_path);
                        strcat(target_file, "/");
                    }
                    strcat(target_file, &clicked[4]); // Başındaki hizalama boşluklarını atla
                    
                    // ELF dosyalarını ve uygulamaları çekirdeğe yolla
                    sys_exec(target_file, "");
                }
            }
        }
        sys_yield();
    }
}