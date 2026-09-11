#include "ardaos.h" 
#include "libc.h"

// --- YARDIMCI METİN VE DÖNÜŞTÜRME FONKSİYONLARI ---
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

void _start() {
    sys_print("===================================\n");
    sys_print("  ArdaOS Shell v1.2 (User Space)   \n");
    sys_print("===================================\n");

    char cmd[256];
    char response[1024];

    while(1) {
        if (sys_get_cmd(cmd)) {
            response[0] = '\0';
            
            // Komut ve parametre ayıklama
            char first_word[32];
            char app_args[128] = "";
            int f_idx = 0;
            while (cmd[f_idx] != ' ' && cmd[f_idx] != '\0' && f_idx < 31) {
                first_word[f_idx] = cmd[f_idx];
                f_idx++;
            }
            first_word[f_idx] = '\0';
            
            if (cmd[f_idx] == ' ') {
                int a_idx = f_idx;
                while (cmd[a_idx] == ' ') a_idx++; // Boşlukları atla
                strcpy(app_args, &cmd[a_idx]);
            }

            int fw_len = strlen(first_word);

            // [ SİSTEM BİLGİSİ VE YÖNETİM ]
            if (strcmp(cmd, "info") == 0) {
                sys_print("Sistem: ArdaOS V1.2\nMimari: 32-bit x86\nFS: FAT16 (VFS Hiyerarsik)\n");
            } 
            else if (strcmp(cmd, "temizle") == 0 || strcmp(cmd, "clear") == 0) {
                sys_clear_terminal();
            }
            else if (strcmp(cmd, "help") == 0) {
                sys_print("--- ARDAOS GENISLETILMIS KOMUT LISTESI ---\n");
                sys_print("[ DOSYA  ] ls [dizin], cat <dosya>, yaz <d.u> <metin>, touch <d.u>, rm <d.u>, mkdir <ad>\n");
                sys_print("[ GOREV  ] ps, top, kill <PID>, <uygulama>.elf [arg]\n");
                sys_print("[ SISTEM ] info, help, clear (temizle), saat, uptime, ram, memorytest, pano\n");
                sys_print("[ DONANIM] bip, melodi, ping, renk <mavi/kirmizi>\n");
                sys_print("[ DIGER  ] echo (yanki) <mesaj>, hesapla <a+b>\n");
            }
            else if (strcmp(cmd, "ps") == 0) {
                sys_get_process_list(response);
                sys_print(response);
            }
            else if (strcmp(cmd, "top") == 0) {
                sys_print("=== ARDAOS SISTEM VE GOREV DURUMU ===\n");
                sys_system_action(7, response); sys_print(response); sys_print("\n");
                sys_system_action(8, response); sys_print(response); sys_print("\n");
                sys_get_process_list(response);
                sys_print(response);
            }
            else if (strcmp(cmd, "pano") == 0) {
                char clipboard_buf[4096];
                sys_get_clipboard(clipboard_buf);
                if (clipboard_buf[0] == '\0') {
                    sys_print("[ PANO ] Pano su an bos.\n");
                } else {
                    sys_print("[ PANO ICERIGI ]\n");
                    sys_print(clipboard_buf);
                    sys_print("\n");
                }
            }
            else if (strncmp(cmd, "kill ", 5) == 0) {
                int target_pid = atoi(&cmd[5]);
                if (target_pid <= 2) {
                    sys_print("[ HATA ] KERNEL, SYSTEM veya SHELL gorevleri sonlandirilamaz!\n");
                } else {
                    sys_kill(target_pid);
                    sys_print("[ SISTEM ] Sonlandirma sinyali iletildi.\n");
                }
            }
            else if (strcmp(cmd, "memorytest") == 0) {
                void* test_ptr = malloc(1024);
                if (test_ptr != 0) {
                    free(test_ptr);
                    sys_print("[ BASARILI ] 1 KB Heap bellegi tahsis edildi ve serbest birakildi.\n");
                } else {
                    sys_print("[ HATA ] Yetersiz Heap bellegi.\n");
                }
            }

            // [ VFS VE DOSYA İŞLEMLERİ ]
            else if (strcmp(first_word, "ls") == 0 || strcmp(first_word, "dir") == 0) {
                if (app_args[0] != '\0') {
                    int res = sys_list_dir(app_args, response);
                    if (res >= 0) {
                        sys_print(response);
                    } else {
                        sys_print("[ HATA ] Dizin bulunamadi: ");
                        sys_print(app_args);
                        sys_print("\n");
                    }
                } else {
                    sys_list_files(response);
                    sys_print(response);
                    sys_print("\n");
                }
            }
            else if (strcmp(first_word, "cat") == 0) {
                if (app_args[0] == '\0') {
                    sys_print("Kullanim: cat <dosya_adi>\n");
                } else {
                    int len = strlen(app_args);
                    if (len > 4 && (strcmp(&app_args[len-4], ".WAV") == 0 || strcmp(&app_args[len-4], ".wav") == 0 ||
                                    strcmp(&app_args[len-4], ".ELF") == 0 || strcmp(&app_args[len-4], ".elf") == 0 ||
                                    strcmp(&app_args[len-4], ".BIN") == 0 || strcmp(&app_args[len-4], ".bin") == 0)) {
                        sys_print("[ UYARI ] Ikili (binary) dosya! Metin olarak goruntulenemez.\n");
                    } else {
                        int fd = sys_open(app_args, "");
                        if (fd < 0) {
                            sys_print("[ HATA ] Dosya acilamadi: ");
                            sys_print(app_args);
                            sys_print("\n");
                        } else {
                            char read_buf[256];
                            int bytes_read;
                            while ((bytes_read = sys_read(fd, (unsigned char*)read_buf, 255)) > 0) {
                                read_buf[bytes_read] = '\0';
                                for (int b = 0; b < bytes_read; b++) {
                                    if ((unsigned char)read_buf[b] < 32 && read_buf[b] != '\n' && read_buf[b] != '\t' && read_buf[b] != '\r') {
                                        read_buf[b] = '.';
                                    }
                                }
                                sys_print(read_buf);
                            }
                            sys_print("\n");
                            sys_close(fd);
                        }
                    }
                }
            }
            else if (strcmp(first_word, "touch") == 0) {
                if (app_args[0] == '\0') {
                    sys_print("Kullanim: touch <DOSYA.UZT>\n");
                } else {
                    char fat_name[9] = "        "; char fat_ext[4] = "   ";
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
                    unsigned char empty_buf[1] = {' '};
                    if (sys_write_file(fat_name, fat_ext, empty_buf) == 0) {
                        sys_print("[ BASARILI ] Dosya olusturuldu.\n");
                    } else {
                        sys_print("[ HATA ] Dosya olusturulamadi.\n");
                    }
                }
            }
            else if (strcmp(first_word, "yaz") == 0) {
                if (app_args[0] == '\0') { 
                    sys_print("Kullanim: yaz DOSYA.TXT Icerik...\n"); 
                } else {
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

                    if (c_idx == 0) {
                        sys_print("Hata: Icerik bos olamaz!\n");
                    } else {
                        if (sys_write_file(fat_name, fat_ext, (unsigned char*)file_content) == 0) 
                            sys_print("[ BASARILI ] Dosya diske yazildi.\n");
                        else 
                            sys_print("[ HATA ] Disk yazma hatasi.\n");
                    }
                }
            }
            else if (strcmp(first_word, "rm") == 0) {
                if (app_args[0] == '\0') {
                    sys_print("Kullanim: rm <DOSYA.UZT>\n");
                } else {
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
                if (app_args[0] == '\0') {
                    sys_print("Kullanim: mkdir <KLASOR>\n");
                } else {
                    char fat_name[9] = "        "; int i = 0;
                    while(app_args[i] != ' ' && app_args[i] != '\0' && i < 8) {
                        char c = app_args[i]; if (c >= 'a' && c <= 'z') c -= 32; fat_name[i++] = c;
                    }
                    if (sys_create_dir(fat_name) == 0) sys_print("[ BASARILI ] Klasor olusturuldu.\n");
                    else sys_print("[ HATA ] Klasor olusturulamadi.\n");
                }
            }

            // [ DONANIM VE ÇEKİRDEK KISAYOLLARI ]
            else if (strcmp(cmd, "bip") == 0) {
                sys_system_action(1, response);
                sys_print("Bip sesi calindi!\n");
            }
            else if (strcmp(cmd, "melodi") == 0) {
                sys_print("8-bit Melodi caliniyor...\n");
                sys_system_action(2, response); 
            }
            else if (strcmp(cmd, "ping") == 0) {
                sys_system_action(3, response);
                sys_print("[ AG ] Ping komutu RTL8139 ag kartina iletildi.\n");
            }
            else if (strcmp(cmd, "renk mavi") == 0) {
                sys_system_action(4, response);
                sys_print("Masaustu rengi guncellendi.\n");
            }
            else if (strcmp(cmd, "renk kirmizi") == 0) {
                sys_system_action(5, response);
                sys_print("Masaustu rengi guncellendi.\n");
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

            // [ METİN, HESAPLAMA VE ÇİZİM ]
            else if (strcmp(first_word, "echo") == 0 || strcmp(first_word, "yanki") == 0) { 
                sys_print(app_args); sys_print("\n"); 
            }
            else if (strncmp(cmd, "hesapla ", 8) == 0) {
                int i = 8; 
                while(cmd[i] == ' ') i++; int num1 = atoi(&cmd[i]);
                while((cmd[i] >= '0' && cmd[i] <= '9') || cmd[i] == '-') i++;
                while(cmd[i] == ' ') i++; char op = cmd[i++];
                while(cmd[i] == ' ') i++; int num2 = atoi(&cmd[i]);
                
                int result = 0, valid = 1;
                if (op == '+') result = num1 + num2; 
                else if (op == '-') result = num1 - num2;
                else if (op == '*') result = num1 * num2;
                else if (op == '/') { 
                    if (num2 == 0) { valid = 0; sys_print("Hata: Sifira bolme!\n"); } 
                    else result = num1 / num2; 
                }
                
                if (valid) {
                    sys_print("Sonuc: "); char res_str[16]; itoa(result, res_str); sys_print(res_str); sys_print("\n");
                }
            }

            // [ ÇALIŞTIRILABİLİR DOSYA BAŞLATICI ]
            else if (fw_len > 4 && (strcmp(first_word + fw_len - 4, ".elf") == 0 || strcmp(first_word + fw_len - 4, ".bin") == 0 ||
                                    strcmp(first_word + fw_len - 4, ".ELF") == 0 || strcmp(first_word + fw_len - 4, ".BIN") == 0)) {
                int pid = sys_exec(first_word, app_args); 
                if (pid > 0) {
                    sys_print("[ SISTEM ] "); sys_print(first_word); sys_print(" baslatildi.\n");
                } else {
                    sys_print("[ HATA ] Uygulama bulunamadi!\n");
                }
            }
            else if (strcmp(cmd, "") != 0) {
                sys_print("Hata: Komut anlasilamadi! 'help' ile komutlari listeleyin.\n");
            }
        }
        
        sys_sleep(10);
    }
}