#include "ardaos.h"
#include "libc.h"

int win_id;
char disk_buf[2048];
int file_count = 0;
char file_names[32][16]; // Tıklanabilir dosyaların isimlerini tutacağımız RAM depomuz

void parse_files() {
    sys_list_files(disk_buf); // FAT16 disk içeriğini metin olarak çek
    file_count = 0;
    
    int i = 0, line_idx = 0, c_idx = 0;
    char current_line[128];

    while (disk_buf[i] != '\0') {
        if (disk_buf[i] == '\n') {
            current_line[c_idx] = '\0';
            
            // Eğer satır "- " ile başlıyorsa bu kesinlikle bir dosyadır
            if (line_idx > 0 && current_line[0] == '-' && current_line[1] == ' ') {
                char clean_name[16];
                int k = 0;
                
                // İsim kısmını al (Örn: "KEDI    " -> "KEDI")
                for (int j = 2; j < 10; j++) {
                    if (current_line[j] != ' ') clean_name[k++] = current_line[j];
                }
                
                // Eğer uzantı varsa onu da birleştir (Örn: "KEDI" + ".ELF")
                if (current_line[10] == '.') {
                    clean_name[k++] = '.';
                    for (int j = 11; j < 14; j++) {
                        if (current_line[j] != ' ') clean_name[k++] = current_line[j];
                    }
                }
                clean_name[k] = '\0';
                
                strcpy(file_names[file_count], clean_name);
                file_count++;
            }
            
            line_idx++;
            c_idx = 0;
        } else {
            current_line[c_idx++] = disk_buf[i];
        }
        i++;
    }
}

void _start(char* args) {
    win_id = sys_create_window("ArdaOS Explorer (C:\\)", 450, 350);
    parse_files();
    sys_set_window_text(disk_buf); // Dosyaları ekrana düz yazı olarak çizdir

    int last_btn = 0;

    while(1) {
        int mx, my, mbtn;
        sys_get_mouse(&mx, &my, &mbtn);
        
        // Sadece farenin sol tuşuna TIKLANDIĞI anı (Rising Edge) yakala
        if (mbtn == 1 && last_btn == 0) {
            int wx, wy, ww, wh;
            if (sys_get_window_pos(win_id, &wx, &wy, &ww, &wh)) {
                
                // Tıklama, pencerenin içerik alanının (Beyaz Tuval) sınırları içinde mi?
                if (mx >= wx + 10 && mx <= wx + ww - 10 && my >= wy + 40 && my <= wy + wh - 10) {
                    
                    // Farenin y eksenindeki yerinden, pencerenin üst menü boşluğunu çıkar
                    int rel_y = my - (wy + 40);
                    
                    // Çekirdeğin render motoru yazıları 16 piksel aralıklarla çiziyor!
                    int clicked_line = rel_y / 16; 
                    
                    // 0. Satır "=== FAT16 DISK ICERIGI ===" başlığı olduğu için atlıyoruz
                    int file_idx = clicked_line - 1; 
                    
                    if (file_idx >= 0 && file_idx < file_count) {
                        char* fname = file_names[file_idx];
                        int len = strlen(fname);
                        
                        // YENİ GÜCÜMÜZ: Eğer tıklanan dosya .ELF uzantılıysa arka planda başlat!
                        if (len > 4 && fname[len-3] == 'E' && fname[len-2] == 'L' && fname[len-1] == 'F') {
                            sys_exec(fname, "");
                        }
                    }
                }
            }
        }
        last_btn = mbtn;
        
        sys_yield(); // Çekirdeği yormadan tetikte bekle
    }
}