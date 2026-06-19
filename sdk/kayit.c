#include "ardaos.h"

void str_cat(char* dest, const char* src) {
    while (*dest) dest++;
    while (*src) *dest++ = *src++;
    *dest = '\0';
}

void int_to_str(int num, char* str) {
    int i = 0;
    if (num == 0) { str[i++] = '0'; str[i] = '\0'; return; }
    int start = i;
    while (num != 0) { str[i++] = (num % 10) + '0'; num /= 10; }
    str[i] = '\0';
    int end = i - 1;
    while (start < end) { char t = str[start]; str[start] = str[end]; str[end] = t; start++; end--; }
}

__attribute__((section(".text.entry")))
void _start() {
    unsigned short w_buf[256];
    unsigned short r_buf[256];
    
    unsigned char* write_ptr = (unsigned char*)w_buf;
    unsigned char* read_ptr  = (unsigned char*)r_buf;
    
    // EFSANE CÜMLE STACK ÜZERİNE GÜVENLE YAZILIYOR
    write_ptr[0]='B'; write_ptr[1]='u'; write_ptr[2]=' '; write_ptr[3]='y'; write_ptr[4]='a'; write_ptr[5]='z'; write_ptr[6]='i'; write_ptr[7]=','; write_ptr[8]=' '; 
    write_ptr[9]='A'; write_ptr[10]='r'; write_ptr[11]='d'; write_ptr[12]='a'; write_ptr[13]='O'; write_ptr[14]='S'; write_ptr[15]=' '; 
    write_ptr[16]='t'; write_ptr[17]='a'; write_ptr[18]='r'; write_ptr[19]='a'; write_ptr[20]='f'; write_ptr[21]='i'; write_ptr[22]='n'; write_ptr[23]='d'; write_ptr[24]='a'; write_ptr[25]='n'; write_ptr[26]=' '; 
    write_ptr[27]='F'; write_ptr[28]='A'; write_ptr[29]='T'; write_ptr[30]='1'; write_ptr[31]='6'; write_ptr[32]=' '; 
    write_ptr[33]='d'; write_ptr[34]='i'; write_ptr[35]='s'; write_ptr[36]='k'; write_ptr[37]='i'; write_ptr[38]='n'; write_ptr[39]='e'; write_ptr[40]=' '; 
    write_ptr[41]='K'; write_ptr[42]='A'; write_ptr[43]='Z'; write_ptr[44]='I'; write_ptr[45]='N'; write_ptr[46]='M'; write_ptr[47]='I'; write_ptr[48]='S'; write_ptr[49]='T'; write_ptr[50]='I'; write_ptr[51]='R'; write_ptr[52]='!'; write_ptr[53]='\0';
    
    for (int i = 54; i < 512; i++) { write_ptr[i] = 0; }
    
    char f_name[9]; f_name[0]='S'; f_name[1]='K'; f_name[2]='O'; f_name[3]='R'; 
    f_name[4]=' '; f_name[5]=' '; f_name[6]=' '; f_name[7]=' '; f_name[8]='\0';
    char f_ext[4]; f_ext[0]='T'; f_ext[1]='X'; f_ext[2]='T'; f_ext[3]='\0';

    int w_res = sys_write_file(f_name, f_ext, write_ptr);
    int r_res = sys_read_file(f_name, f_ext, read_ptr);
    
    char msg[128]; msg[0] = '\0';
    char s_yaz[6]; s_yaz[0]='Y'; s_yaz[1]='A'; s_yaz[2]='Z'; s_yaz[3]=':'; s_yaz[4]=' '; s_yaz[5]='\0';
    char s_oku[9]; s_oku[0]=' '; s_oku[1]='|'; s_oku[2]=' '; s_oku[3]='O'; s_oku[4]='K'; s_oku[5]='U'; s_oku[6]=':'; s_oku[7]=' '; s_oku[8]='\0';
    char s_metin[11]; s_metin[0]=' '; s_metin[1]='|'; s_metin[2]=' '; s_metin[3]='M'; s_metin[4]='E'; s_metin[5]='T'; s_metin[6]='I'; s_metin[7]='N'; s_metin[8]=':'; s_metin[9]=' '; s_metin[10]='\0';
    
    str_cat(msg, s_yaz);
    char ws[10]; int_to_str(w_res, ws); str_cat(msg, ws);
    str_cat(msg, s_oku);
    char rs[10]; int_to_str(r_res, rs); str_cat(msg, rs);
    str_cat(msg, s_metin);
    
    if (r_res > 0) {
        read_ptr[54] = '\0'; // Uzun cümle için kilit noktasını ileri aldık
        str_cat(msg, (const char*)read_ptr);
    } else {
        char s_bos[4]; s_bos[0]='B'; s_bos[1]='O'; s_bos[2]='S'; s_bos[3]='\0';
        str_cat(msg, s_bos);
    }

    sys_print(msg);
    for (volatile int j = 0; j < 8000000; j++) { sys_yield(); }
    sys_exit(); 
    while(1); 
}