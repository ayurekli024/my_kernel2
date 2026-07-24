#include "ardaos.h"
#include "libc.h"

char editor_buf[1024];
int cursor_pos = 0;
char target_name[9] = "YENI    ";
char target_ext[4] = "TXT";

void parse_filename(const char* args) {
    // YENİ ZIRH: Parametre boşsa "YENI.TXT" kalsın, çökme!
    if (args == 0 || args[0] == '\0') return; 
    
    int i = 0, k = 0;
    while (args[i] != '.' && args[i] != '\0' && k < 8) {
        char c = args[i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        target_name[k++] = c;
    }
    while (k < 8) target_name[k++] = ' ';
    
    if (args[i] == '.') {
        i++; k = 0;
        while (args[i] != '\0' && k < 3) {
            char c = args[i++];
            if (c >= 'a' && c <= 'z') c -= 32;
            target_ext[k++] = c;
        }
        while (k < 3) target_ext[k++] = ' ';
    }
}

void _start(char* args) {
    parse_filename(args);
    sys_create_window("ArdaOS Not Defteri", 400, 300);
    
    int f_size = sys_read_file(target_name, target_ext, (unsigned char*)editor_buf);
    if (f_size > 0) {
        // YENİ ZIRH: Sadece gerçek metni oku, boşlukları atla
        cursor_pos = 0;
        while (cursor_pos < f_size && editor_buf[cursor_pos] != '\0') {
            cursor_pos++;
        }
    } else {
        cursor_pos = 0;
    }
    editor_buf[cursor_pos] = '\0';
    sys_set_window_text(editor_buf);

    while(1) {
        char key = sys_poll_key();
        
        if (key != 0) {
            if (key == 27) { // ESC: Kaydet ve Çık
                sys_set_window_text("Dosya diske kaydediliyor...\nLutfen bekleyin.");
                sys_write_file(target_name, target_ext, (unsigned char*)editor_buf);
                sys_exit(); 
                while(1); // YENİ: Çekirdek bizi öldürene kadar don, çökme!
            }
            else if (key == '\b') { 
                if (cursor_pos > 0) {
                    cursor_pos--;
                    editor_buf[cursor_pos] = '\0';
                }
            }
            else if (cursor_pos < 1022) { 
                editor_buf[cursor_pos++] = key;
                editor_buf[cursor_pos] = '\0';
            }
            sys_set_window_text(editor_buf);
        }
        sys_yield(); 
    }
}