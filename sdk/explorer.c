#include "ardaos.h"
#include "libc.h"

char current_path[128] = "";
char dir_content[2048];
char files[15][32];
int file_count = 0;
char selected_file[32] = "";
char clipboard_file[128] = "";
int clipboard_is_cut = 0;
char status_msg[64] = "Hazir";

void load_directory() {
    sys_clear_gui();

    // 1. ÜST MAVİ BAŞLIK BARI BUTONLARI (ID: 0, 1, 2, 3)
    sys_create_title_button("Kopyala");   // ID: 0
    sys_create_title_button("Kes");       // ID: 1
    sys_create_title_button("Yapistir");  // ID: 2
    sys_create_title_button("Sil");       // ID: 3

    // 2. GEZİNTİ ARAÇ ÇUBUĞU (ID: 4, 5, 6)
    sys_create_button(10, 8, 70, 24, "<- Geri");       // ID: 4
    sys_create_button(88, 8, 70, 24, "Yenile");        // ID: 5
    sys_create_button(166, 8, 95, 24, "+ Klasor");     // ID: 6

    // 3. KONUM VE DURUM BİLGİSİ
    char window_text[2100] = "\n\n\n  KONUM: C:/";
    strcat(window_text, current_path);
    strcat(window_text, "\n  DURUM: ");
    strcat(window_text, status_msg);
    strcat(window_text, "\n  ------------------------------------------------\n");

    file_count = 0;
    int res = sys_list_dir(current_path, dir_content);

    if (res < 0) {
        strcat(window_text, "  [HATA] Klasor okunamadi veya bos!\n");
        sys_set_window_text(window_text);
        return;
    }

    // 4. DOSYA VE KLASÖR LİSTESİ (ID: 7'den itibaren)
    int i = 0;
    while (dir_content[i] != '\0' && file_count < 15) {
        int j = 0;
        while (dir_content[i] != '\n' && dir_content[i] != '\0') {
            files[file_count][j++] = dir_content[i++];
        }
        files[file_count][j] = '\0';
        if (dir_content[i] == '\n') i++;

        sys_create_button(15, 92 + (file_count * 26), 240, 22, files[file_count]);
        file_count++;
    }
    sys_set_window_text(window_text);
}

void _start(char* args) {
    (void)args;
    sys_create_window("Gezgin", 450, 520);
    load_directory();

    while (1) {
        int ev_id;
        int ev_type = sys_poll_event(&ev_id);

        if (ev_type == 1) {
            if (ev_id == 0) { // KOPYALA
                if (selected_file[0] != '\0') {
                    clipboard_file[0] = '\0';
                    if (current_path[0] != '\0') {
                        strcat(clipboard_file, current_path);
                        strcat(clipboard_file, "/");
                    }
                    strcat(clipboard_file, selected_file);
                    clipboard_is_cut = 0;
                    strcpy(status_msg, "Kopyalandi: ");
                    strcat(status_msg, selected_file);
                } else {
                    strcpy(status_msg, "Once bir dosya secin");
                }
                load_directory();
            }
            else if (ev_id == 1) { // KES
                if (selected_file[0] != '\0') {
                    clipboard_file[0] = '\0';
                    if (current_path[0] != '\0') {
                        strcat(clipboard_file, current_path);
                        strcat(clipboard_file, "/");
                    }
                    strcat(clipboard_file, selected_file);
                    clipboard_is_cut = 1;
                    strcpy(status_msg, "Kesildi: ");
                    strcat(status_msg, selected_file);
                } else {
                    strcpy(status_msg, "Once bir dosya secin");
                }
                load_directory();
            }
            else if (ev_id == 2) { // YAPIŞTIR
                if (clipboard_file[0] != '\0') {
                    strcpy(status_msg, "Yapistiriliyor: ");
                    strcat(status_msg, clipboard_file);
                } else {
                    strcpy(status_msg, "Pano bos");
                }
                load_directory();
            }
            else if (ev_id == 3) { // SİL
                if (selected_file[0] != '\0') {
                    strcpy(status_msg, "Silinecek: ");
                    strcat(status_msg, selected_file);
                } else {
                    strcpy(status_msg, "Once silinecek dosyayi secin");
                }
                load_directory();
            }
            else if (ev_id == 4) { // <- GERİ
                int len = 0; while (current_path[len]) len++;
                while (len > 0 && current_path[len - 1] != '/') len--;
                if (len > 0) current_path[len - 1] = '\0';
                else current_path[0] = '\0';
                selected_file[0] = '\0';
                load_directory();
            }
            else if (ev_id == 5) { // YENİLE
                load_directory();
            }
            else if (ev_id == 6) { // + KLASÖR
                char new_dir_path[128]; new_dir_path[0] = '\0';
                if (current_path[0] != '\0') {
                    strcat(new_dir_path, current_path);
                    strcat(new_dir_path, "/");
                }
                strcat(new_dir_path, "YENIDIZN");
                sys_create_dir(new_dir_path);
                load_directory();
            }
            else if (ev_id >= 7 && ev_id < file_count + 7) { // DOSYA / KLASÖR SEÇİMİ
                int file_idx = ev_id - 7;
                char* clicked = files[file_idx];

                if (clicked[0] == '<' && clicked[1] == 'D' && clicked[2] == '>') {
                    if (current_path[0] != '\0') strcat(current_path, "/");
                    strcat(current_path, &clicked[4]);
                    selected_file[0] = '\0';
                    load_directory();
                } else {
                    int start_idx = 0;
                    while (clicked[start_idx] == ' ') start_idx++;
                    strcpy(selected_file, &clicked[start_idx]);
                    strcpy(status_msg, "Secili: ");
                    strcat(status_msg, selected_file);
                    load_directory();
                }
            }
        }
        sys_yield();
    }
}