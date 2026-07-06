#include "ardaos.h"

__attribute__((section(".text.entry")))
void _start(char* args) {  
    // YENİ: IPC - Paylaşılan belleğe bağlan (yilan.bin ile aynı adrese bakar)
    volatile char* shared_mem = (volatile char*)sys_shm_get();
    
    // Sadece skor değiştiğinde ekrana yazmak için eski skoru hafızada tutuyoruz
    char last_score[4] = "XX"; 
    
    sys_print("[ SKORBORD ] Yilan oyunu skoru canli dinleniyor...");
    sys_print("[ SKORBORD ] Cikmak icin terminale odaklanip 'q' tusuna basin.");

    while(1) {
        // Eğer skorda bir değişiklik varsa (Yılan yem yemişse)
        if (shared_mem[0] != '\0' && (shared_mem[0] != last_score[0] || shared_mem[1] != last_score[1])) {
            
            // Yeni skoru hafızaya al
            last_score[0] = shared_mem[0];
            last_score[1] = shared_mem[1];
            last_score[2] = '\0';
            
            // Ekrana basılacak metni (String) birleştir
            char msg[64] = "--> Yilan Yem Yedi! Guncel Skor: ";
            int i = 0; while(msg[i] != '\0') i++;
            int j = 0; while(last_score[j] != '\0') msg[i++] = last_score[j++];
            msg[i] = '\0';
            
            // Terminale yazdır
            sys_print(msg);
        }
        
        // İşlemciyi yormamak ve diğer uygulamalara vakit tanımak için biraz bekle
        sys_yield();
        
        // q tuşuna basılırsa döngüyü kır ve uygulamayı kapat
        if (sys_poll_key() == 'q') break;
    }
    
    sys_print("[ SKORBORD ] Dinleme sonlandirildi.");
    sys_exit();
    while(1);
}