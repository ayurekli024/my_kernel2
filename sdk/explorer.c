#include "ardaos.h"
#include "libc.h"

#define MAX_FILES 64
#define ITEMS_PER_PAGE 10

char current_path[128] = "";
char dir_content[2048];
char files[MAX_FILES][32];
int file_count = 0;
int current_page = 0;

char selected_item[32] = "";
int selected_is_dir = 0;
char status_msg[64] = "Hazir";

static char target_file[128];
static char target_path[128];
static char cut_src_path[128];

int str_starts_with(const char* str, const char* prefix) {
    int i = 0;
    while (prefix[i] != '\0') {
        if (str[i] != prefix[i]) return 0;
        i++;
    }
    return 1;
}

int str_eq(const char* a, const char* b) {
    int i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == '\0' && b[i] == '\0';
}

void int_to_str(int val, char* out) {
    if (val == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }
    char tmp[16];
    int i = 0;
    while (val > 0) {
        tmp[i++] = '0' + (val % 10);
        val /= 10;
    }
    int j = 0;
    while (i > 0) out[j++] = tmp[--i];
    out[j] = '\0';
}

int is_elf_file(const char* name) {
    int len = strlen(name);
    if (len < 4) return 0;
    const char* ext = &name[len - 4];
    return (ext[0] == '.' &&
           (ext[1] == 'E' || ext[1] == 'e') &&
           (ext[2] == 'L' || ext[2] == 'l') &&
           (ext[3] == 'F' || ext[3] == 'f'));
}

void build_full_path(char* out, const char* dir, const char* item) {
    out[0] = '\0';
    if (dir[0] != '\0') {
        strcat(out, dir);
        strcat(out, "/");
    }
    strcat(out, item);
}

const char* extract_filename(const char* path) {
    int last_slash = -1;
    for (int i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/' || path[i] == '\\') last_slash = i;
    }
    return &path[last_slash + 1];
}

void make_copy_name(char* out, const char* orig_name) {
    strcpy(out, "KOPYA");
    int dot = -1;
    for (int i = 0; orig_name[i] != '\0'; i++) {
        if (orig_name[i] == '.') { dot = i; break; }
    }
    if (dot != -1) {
        strcat(out, &orig_name[dot]);
    }
}

void open_selected_item() {
    if (selected_item[0] == '\0') {
        strcpy(status_msg, "Once bir dosya secin");
        return;
    }

    if (selected_is_dir) {
        if (current_path[0] != '\0') strcat(current_path, "/");
        strcat(current_path, selected_item);
        selected_item[0] = '\0';
        selected_is_dir = 0;
        current_page = 0;
        strcpy(status_msg, "Klasor acildi");
        return;
    }

    build_full_path(target_file, current_path, selected_item);

    if (is_elf_file(selected_item)) {
        sys_exec(target_file, "");
        strcpy(status_msg, "Calistirildi: ");
        strcat(status_msg, selected_item);
    } else {
        sys_exec("EDIT.ELF", target_file);
        strcpy(status_msg, "ArdaPad ile acildi: ");
        strcat(status_msg, selected_item);
    }
}

void load_directory() {
    sys_clear_gui();

    // 1. ÜST MAVİ BAŞLIK BARI BUTONLARI (ID: 0..4)
    sys_create_title_button("Ac");        // ID: 0
    sys_create_title_button("Kopyala");   // ID: 1
    sys_create_title_button("Kes");       // ID: 2
    sys_create_title_button("Yapistir");  // ID: 3
    sys_create_title_button("Sil");       // ID: 4

    // 2. ALT ARAÇ ÇUBUĞU VE SAYFALAMA (ID: 5..9)
    sys_create_button(10,  8, 68, 24, "<- Geri");   // ID: 5
    sys_create_button(84,  8, 64, 24, "Yenile");    // ID: 6
    sys_create_button(154, 8, 78, 24, "+ Klasor");  // ID: 7
    sys_create_button(244, 8, 76, 24, "< Onceki");  // ID: 8
    sys_create_button(326, 8, 84, 24, "Sonraki >"); // ID: 9

    file_count = 0;
    int res = sys_list_dir(current_path, dir_content);

    if (res >= 0) {
        int i = 0;
        while (dir_content[i] != '\0' && file_count < MAX_FILES) {
            int j = 0;
            while (dir_content[i] != '\n' && dir_content[i] != '\0' && j < 31) {
                files[file_count][j++] = dir_content[i++];
            }
            files[file_count][j] = '\0';
            while (dir_content[i] != '\n' && dir_content[i] != '\0') i++;
            if (dir_content[i] == '\n') i++;
            file_count++;
        }
    }

    int total_pages = (file_count == 0) ? 1 : ((file_count + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE);
    if (current_page >= total_pages) current_page = total_pages - 1;
    if (current_page < 0) current_page = 0;

    char page_cur_str[8], page_tot_str[8];
    int_to_str(current_page + 1, page_cur_str);
    int_to_str(total_pages, page_tot_str);

    // 3. KONUM, SAYFA VE DURUM BİLGİSİ
    // 3. KONUM, SAYFA VE DURUM BİLGİSİ
    char window_text[2100];
    strcpy(window_text, "\n\n\n  KONUM: C:/");
    strcat(window_text, current_path);
    strcat(window_text, "  [Sayfa ");
    strcat(window_text, page_cur_str);
    strcat(window_text, "/");
    strcat(window_text, page_tot_str);
    strcat(window_text, "]");
    strcat(window_text, "\n  SECILI: ");
    strcat(window_text, (selected_item[0] != '\0') ? selected_item : "(Yok)");
    strcat(window_text, " | ");
    strcat(window_text, status_msg);
    strcat(window_text, "\n  --------------------------------------------------\n");

    if (res < 0) {
        strcat(window_text, "  [HATA] Klasor okunamadi!\n");
        sys_set_window_text(window_text);
        return;
    }

    // 4. AKTİF SAYFADAKİ DOSYA VE KLASÖRLER (ID: 10..19)
    int start_idx = current_page * ITEMS_PER_PAGE;
    int end_idx = start_idx + ITEMS_PER_PAGE;
    if (end_idx > file_count) end_idx = file_count;

    int row = 0;
    for (int idx = start_idx; idx < end_idx; idx++) {
        sys_create_button(15, 92 + (row * 28), 280, 24, files[idx]);
        row++;
    }

    sys_set_window_text(window_text);
}

void _start(char* args) {
    (void)args;
    sys_create_window("Gezgin", 460, 520);
    load_directory();

    while (1) {
        int ev_id;
        int ev_type = sys_poll_event(&ev_id);

        if (ev_type == 1) {
            if (ev_id == 0) { // AÇ
                open_selected_item();
                load_directory();
            }
            else if (ev_id == 1) { // KOPYALA
                if (selected_item[0] == '\0' || selected_is_dir) {
                    strcpy(status_msg, "Kopyalamak icin dosya secin");
                } else {
                    char full_src[128];
                    build_full_path(full_src, current_path, selected_item);
                    char clip_payload[160];
                    strcpy(clip_payload, "FILE_COPY:");
                    strcat(clip_payload, full_src);
                    sys_set_clipboard(clip_payload);
                    strcpy(status_msg, "Kopyalandi: ");
                    strcat(status_msg, selected_item);
                }
                load_directory();
            }
            else if (ev_id == 2) { // KES
                if (selected_item[0] == '\0' || selected_is_dir) {
                    strcpy(status_msg, "Kesmek icin dosya secin");
                } else {
                    char full_src[128];
                    build_full_path(full_src, current_path, selected_item);
                    char clip_payload[160];
                    strcpy(clip_payload, "FILE_CUT:");
                    strcat(clip_payload, full_src);
                    sys_set_clipboard(clip_payload);
                    strcpy(status_msg, "Kesildi: ");
                    strcat(status_msg, selected_item);
                }
                load_directory();
            }
            else if (ev_id == 3) { // YAPIŞTIR
                char clip_buf[256];
                clip_buf[0] = '\0';
                sys_get_clipboard(clip_buf);

                int is_copy = str_starts_with(clip_buf, "FILE_COPY:");
                int is_cut  = str_starts_with(clip_buf, "FILE_CUT:");

                if (!is_copy && !is_cut) {
                    strcpy(status_msg, "Panoda dosya yok");
                } else {
                    const char* src_ptr = is_copy ? &clip_buf[10] : &clip_buf[9];
                    strcpy(cut_src_path, src_ptr);
                    const char* base_name = extract_filename(cut_src_path);

                    char dst_path[128];
                    build_full_path(dst_path, current_path, base_name);

                    if (is_copy && str_eq(cut_src_path, dst_path)) {
                        char copy_name[32];
                        make_copy_name(copy_name, base_name);
                        build_full_path(dst_path, current_path, copy_name);
                    }

                    if (is_cut && str_eq(cut_src_path, dst_path)) {
                        strcpy(status_msg, "Dosya zaten bu klasorde");
                        sys_set_clipboard("");
                    } else {
                        int res = sys_copy_file(cut_src_path, dst_path);
                        if (res == 0) {
                            if (is_cut) {
                                int del_res = sys_delete_path(cut_src_path);
                                sys_set_clipboard("");
                                if (del_res == 0) {
                                    strcpy(status_msg, "Tasindi: ");
                                } else {
                                    strcpy(status_msg, "Kopyalandi, kaynak silinemedi: ");
                                }
                            } else {
                                strcpy(status_msg, "Yapistirildi: ");
                            }
                            strcat(status_msg, extract_filename(dst_path));
                            selected_item[0] = '\0';
                            selected_is_dir = 0;
                        } else {
                            strcpy(status_msg, "Yapistirma basarisiz!");
                        }
                    }
                }
                load_directory();
            }
            else if (ev_id == 4) { // SİL
                if (selected_item[0] == '\0') {
                    strcpy(status_msg, "Silinecek ogeyi secin");
                } else {
                    build_full_path(target_path, current_path, selected_item);
                    if (sys_delete_path(target_path) == 0) {
                        strcpy(status_msg, "Silindi: ");
                        strcat(status_msg, selected_item);
                        selected_item[0] = '\0';
                        selected_is_dir = 0;
                    } else {
                        strcpy(status_msg, "Silme hatasi!");
                    }
                }
                load_directory();
            }
            else if (ev_id == 5) { // <- GERİ
                int len = strlen(current_path);
                while (len > 0 && current_path[len - 1] != '/') len--;
                if (len > 0) current_path[len - 1] = '\0';
                else current_path[0] = '\0';
                selected_item[0] = '\0';
                selected_is_dir = 0;
                current_page = 0;
                strcpy(status_msg, "Ust klasore cikildi");
                load_directory();
            }
            else if (ev_id == 6) { // YENİLE
                strcpy(status_msg, "Yenilendi");
                load_directory();
            }
            else if (ev_id == 7) { // + KLASÖR
                char new_dir_path[128];
                build_full_path(new_dir_path, current_path, "YENIDIZN");
                if (sys_create_dir(new_dir_path) == 0) {
                    strcpy(status_msg, "YENIDIZN olusturuldu");
                } else {
                    strcpy(status_msg, "Klasor olusturulamadi");
                }
                load_directory();
            }
            else if (ev_id == 8) { // < ÖNCEKİ SAYFA
                if (current_page > 0) {
                    current_page--;
                    strcpy(status_msg, "Onceki sayfa");
                } else {
                    strcpy(status_msg, "Ilk sayfadasiniz");
                }
                load_directory();
            }
            else if (ev_id == 9) { // SONRAKİ SAYFA >
                int total_pages = (file_count == 0) ? 1 : ((file_count + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE);
                if (current_page + 1 < total_pages) {
                    current_page++;
                    strcpy(status_msg, "Sonraki sayfa");
                } else {
                    strcpy(status_msg, "Son sayfadasiniz");
                }
                load_directory();
            }
            else if (ev_id >= 10 && ev_id < 10 + ITEMS_PER_PAGE) { // LİSTEDEN ÖĞE SEÇİMİ
                int file_idx = (current_page * ITEMS_PER_PAGE) + (ev_id - 10);
                if (file_idx < file_count) {
                    char* clicked = files[file_idx];

                    if (clicked[0] == '<' && clicked[1] == 'D' && clicked[2] == '>') {
                        strcpy(selected_item, &clicked[4]);
                        selected_is_dir = 1;
                        strcpy(status_msg, "Klasor secildi (Ac ile gir)");
                    } else {
                        int s = 0;
                        while (clicked[s] == ' ') s++;
                        strcpy(selected_item, &clicked[s]);
                        selected_is_dir = 0;
                        strcpy(status_msg, "Dosya secildi");
                    }
                    load_directory();
                }
            }
        }
        sys_yield();
    }
}