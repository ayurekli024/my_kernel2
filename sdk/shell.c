#include "ardaos.h" 
#include "libc.h" // Kendi standart kütüphaneni dahil ettik!

// --- libc.c İÇİNDE OLMAYAN YARDIMCI FONKSİYONLAR ---
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, int n) {
    while (n > 0 && *s1 && (*s1 == *s2)) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int atoi(const char* str) {
    int res = 0, sign = 1;
    if (*str == '-') { sign = -1; str++; }
    while (*str >= '0' && *str <= '9') { res = res * 10 + (*str - '0'); str++; }
    return res * sign;
}

// ==========================================
// SHELL ANA DÖNGÜSÜ
// ==========================================
void _start() {
    // printf kütüphanende olduğu için sys_print yerine onu da kullanabilirdik, 
    // ancak şimdilik mimariyi bozmadan sys_print ile devam ediyoruz.
    sys_print("===================================\n");
    sys_print("  ArdaOS Shell v1.0 (User Space)   \n");
    sys_print("===================================\n");

    char cmd[256];
    char response[1024];

    while(1) {
        if (sys_get_cmd(cmd)) {
            response[0] = '\0';
            
            char first_word[32];
            char app_args[128] = "";
            int f_idx = 0;
            while (cmd[f_idx] != ' ' && cmd[f_idx] != '\0' && f_idx < 31) {
                first_word[f_idx] = cmd[f_idx];
                f_idx++;
            }
            first_word[f_idx] = '\0';
            if (cmd[f_idx] == ' ') strcpy(app_args, &cmd[f_idx + 1]);

            int fw_len = strlen(first_word);

            // [ SISTEM KOMUTLARI ]
            if (strcmp(cmd, "info") == 0) {
                sys_print("Sistem: ArdaOS V0.5\nMimari: 32-bit x86\nOzel: Microkernel Shell\n");
            } 
            else if (strcmp(cmd, "temizle") == 0) {
                sys_clear_terminal();
            }
            else if (strcmp(cmd, "help") == 0) {
                sys_print("--- ARDAOS KOMUT LISTESI ---\n");
                sys_print("[ SISTEM ] info, help, temizle, saat, uptime, ram, memorytest\n");
                sys_print("[ GOREV  ] ps, kill <PID>, <uygulama>.elf [arg]\n");
                sys_print("[ DISK   ] ls (dir), yaz <DSY.UZT> <metin>, rm <DSY.UZT>, mkdir <AD>\n");
                sys_print("[ GRAFIK ] renk <mavi/kirmizi>, ciz dikdortgen <x y w h rnk>, ciz temizle\n");
                sys_print("[ DIGER  ] hesapla <a+b>, yanki <mesaj>, bip, melodi, ping\n");
            }
            else if (strcmp(cmd, "ps") == 0) {
                sys_get_process_list(response);
                sys_print(response);
            }
            else if (strncmp(cmd, "kill ", 5) == 0) {
                int target_pid = atoi(&cmd[5]);
                if (target_pid == 0 || target_pid == 1 || target_pid == 2) {
                    sys_print("[ HATA ] KERNEL, SYSTEM veya SHELL gorevleri oldurulemez!\n");
                } else {
                    sys_kill(target_pid);
                    sys_print("[ SISTEM ] Kill sinyali gonderildi.\n");
                }
            }
            else if (strcmp(cmd, "memorytest") == 0) {
                // Kendi libc'ndeki malloc ve free'yi çağırıyoruz!
                void* test_ptr = malloc(1024);
                if (test_ptr != 0) {
                    free(test_ptr);
                    sys_print("[ BASARILI ] 1 KB Heap bellegi Kernel'den alindi ve iade edildi.\n");
                } else {
                    sys_print("[ HATA ] Yetersiz Heap bellegi.\n");
                }
            }

            // [ DONANIM VE ÇEKİRDEK KISAYOLLARI (SYSCALL 29) ]
            else if (strcmp(cmd, "bip") == 0) {
                sys_system_action(1, response);
                sys_print("Bip sesi calindi!\n");
            }
            else if (strcmp(cmd, "melodi") == 0) {
                sys_print("8-bit Nostalji Melodisi caliniyor...\n");
                sys_system_action(2, response); 
            }
            else if (strcmp(cmd, "ping") == 0) {
                sys_system_action(3, response);
                sys_print("[ INTERNET ] Ping komutu ag kartina (RTL8139) iletildi.\n");
            }
            else if (strcmp(cmd, "renk mavi") == 0) {
                sys_system_action(4, response);
                sys_print("Masaustu rengi mavi olarak degistirildi.\n");
            }
            else if (strcmp(cmd, "renk kirmizi") == 0) {
                sys_system_action(5, response);
                sys_print("Masaustu rengi kirmizi olarak degistirildi.\n");
            }
            else if (strcmp(cmd, "saat") == 0) {
                sys_system_action(6, response); sys_print(response); sys_print("\n");
            }
            else if (strcmp(cmd, "uptime") == 0) {
                sys_system_action(7, response); sys_print(response); sys_print("\n");
            }
            else if (strcmp(cmd, "ram") == 0) {
                sys_system_action(8, response); sys_print(response); sys_print("\n");
            }

            // [ DISK VE DOSYA İŞLEMLERİ ]
            else if (strcmp(cmd, "ls") == 0 || strcmp(cmd, "dir") == 0) {
                sys_list_files(response);
                sys_print(response); sys_print("\n");
            }
            else if (strcmp(first_word, "yaz") == 0) {
                if (app_args[0] == '\0') { sys_print("Hata: Kullanim -> yaz DOSYA.TXT Icerik...\n"); } 
                else {
                    char fat_name[9] = "        "; char fat_ext[4] = "   "; char file_content[512] = {0};
                    int i = 0, k = 0;
                    while (app_args[i] != '.' && app_args[i] != ' ' && app_args[i] != '\0' && k < 8) {
                        char c = app_args[i++]; if (c >= 'a' && c <= 'z') c -= 32; fat_name[k++] = c;
                    }
                    if (app_args[i] == '.') {
                        i++; k = 0;
                        while (app_args[i] != ' ' && app_args[i] != '\0' && k < 3) {
                            char c = app_args[i++]; if (c >= 'a' && c <= 'z') c -= 32; fat_ext[k++] = c;
                        }
                    }
                    while (app_args[i] == ' ') i++;
                    int c_idx = 0;
                    while (app_args[i] != '\0' && c_idx < 511) file_content[c_idx++] = app_args[i++];
                    file_content[c_idx] = '\0';

                    if (c_idx == 0) sys_print("Hata: Icerik bos olamaz!\n");
                    else {
                        if (sys_write_file(fat_name, fat_ext, (unsigned char*)file_content) == 0) sys_print("[ BASARILI ] Dosya diske yazildi.\n");
                        else sys_print("[ HATA ] Disk hatasi.\n");
                    }
                }
            }
            else if (strcmp(first_word, "rm") == 0) {
                if (app_args[0] == '\0') sys_print("Hata: Silinecek dosyayi belirtin.\n");
                else {
                    char fat_name[9] = "        "; char fat_ext[4] = "   "; int i = 0, k = 0;
                    while (app_args[i] != '.' && app_args[i] != '\0' && k < 8) {
                        char c = app_args[i++]; if (c >= 'a' && c <= 'z') c -= 32; fat_name[k++] = c;
                    }
                    if (app_args[i] == '.') {
                        i++; k = 0;
                        while (app_args[i] != '\0' && k < 3) {
                            char c = app_args[i++]; if (c >= 'a' && c <= 'z') c -= 32; fat_ext[k++] = c;
                        }
                    }
                    if (sys_delete_file(fat_name, fat_ext) == 0) sys_print("[ BASARILI ] Dosya silindi.\n");
                    else sys_print("[ HATA ] Dosya bulunamadi.\n");
                }
            }
            else if (strcmp(first_word, "mkdir") == 0) {
                if (app_args[0] == '\0') sys_print("Hata: Klasor adini belirtin.\n");
                else {
                    char fat_name[9] = "        "; int i = 0;
                    while(app_args[i] != ' ' && app_args[i] != '\0' && i < 8) {
                        char c = app_args[i]; if (c >= 'a' && c <= 'z') c -= 32; fat_name[i++] = c;
                    }
                    if (sys_create_dir(fat_name) == 0) sys_print("[ BASARILI ] Klasor olusturuldu.\n");
                    else sys_print("[ HATA ] Klasor olusturulamadi.\n");
                }
            }

            // [ MATEMATİK VE GRAFİK ]
            else if (strncmp(cmd, "yanki ", 6) == 0) { 
                sys_print("Sen dedin ki: "); sys_print(cmd + 6); sys_print("\n"); 
            }
            else if (strncmp(cmd, "hesapla ", 8) == 0) {
                int i = 8; 
                while(cmd[i] == ' ') i++; int num1 = atoi(&cmd[i]);
                while((cmd[i] >= '0' && cmd[i] <= '9') || cmd[i] == '-') i++;
                while(cmd[i] == ' ') i++; char op = cmd[i++];
                while(cmd[i] == ' ') i++; int num2 = atoi(&cmd[i]);
                
                int result = 0, valid = 1;
                if (op == '+') result = num1 + num2; else if (op == '-') result = num1 - num2;
                else if (op == '*') result = num1 * num2;
                else if (op == '/') { if (num2 == 0) { valid = 0; sys_print("Hata: Sifira bolme!\n"); } else result = num1 / num2; }
                
                if (valid) {
                    sys_print("Sonuc: "); char res_str[16]; itoa(result, res_str); sys_print(res_str); sys_print("\n");
                }
            }
            else if (strncmp(cmd, "ciz ", 4) == 0) {
                char* args = cmd + 4; 
                if (strncmp(args, "temizle", 7) == 0) {
                    sys_clear_shapes(); sys_print("Masaustu tuvali temizlendi!\n");
                }
                else if (strncmp(args, "dikdortgen ", 11) == 0) {
                    int i = 11;
                    while(args[i] == ' ') i++; int x = atoi(&args[i]); while((args[i] >= '0' && args[i] <= '9') || args[i] == '-') i++;
                    while(args[i] == ' ') i++; int y = atoi(&args[i]); while((args[i] >= '0' && args[i] <= '9') || args[i] == '-') i++;
                    while(args[i] == ' ') i++; int w = atoi(&args[i]); while((args[i] >= '0' && args[i] <= '9') || args[i] == '-') i++;
                    while(args[i] == ' ') i++; int h = atoi(&args[i]); while((args[i] >= '0' && args[i] <= '9') || args[i] == '-') i++;
                    while(args[i] == ' ') i++;
                    
                    unsigned int c = 0x00FFFFFF;
                    if (strncmp(&args[i], "kirmizi", 7) == 0) c = 0x00FF2D55;
                    else if (strncmp(&args[i], "yesil", 5) == 0) c = 0x0034C759;
                    else if (strncmp(&args[i], "mavi", 4) == 0) c = 0x000078D7;
                    else if (strncmp(&args[i], "sari", 4) == 0) c = 0x00FFCC00;

                    add_shape(x, y, w, h, c);
                    sys_print("Masaustu Sekli Eklendi!\n");
                }
            }

            // [ HARİCİ UYGULAMA YÜKLEYİCİ ]
            else if (fw_len > 4 && (strcmp(first_word + fw_len - 4, ".elf") == 0 || strcmp(first_word + fw_len - 4, ".bin") == 0 ||
                                    strcmp(first_word + fw_len - 4, ".ELF") == 0 || strcmp(first_word + fw_len - 4, ".BIN") == 0)) {
                int pid = sys_exec(first_word, app_args); 
                if (pid > 0) {
                    sys_print("[ SISTEM ] "); sys_print(first_word); sys_print(" basariyla baslatildi.\n");
                } else {
                    sys_print("[ HATA ] Uygulama diskte bulunamadi!\n");
                }
            }
            else if (strcmp(cmd, "") != 0) {
                sys_print("Hata: Bilinmeyen komut! 'help' yazarak komutlari gorebilirsiniz.\n");
            }
        }
        
        sys_yield(); 
    }
}