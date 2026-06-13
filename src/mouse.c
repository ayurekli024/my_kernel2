#include "mouse.h"
#include "io.h"
// I/O Port iletişim fonksiyonları (inb, outb) klavyede kullandığın kütüphaneden gelmeli
// Eğer io.h adında bir dosyan yoksa, bu fonksiyonları assembly ile buraya tanımlamalısın.
extern unsigned char inb(unsigned short port);
extern void outb(unsigned short port, unsigned char data);

int mouse_x = 512; // Fare başlama koordinatı (Ekranın ortası)
int mouse_y = 384;
int mouse_left_button = 0;

unsigned char mouse_cycle = 0;
unsigned char mouse_byte[3];

// PS/2 portuna fare komutu gönderme yardımcısı
void mouse_write(unsigned char a_write) {
    outb(0x64, 0xD4); // PS/2 denetleyicisine "Fareye komut göndereceğim" de
    outb(0x60, a_write); // Komutu yolla
    inb(0x60); // Fareden gelen "ACK" (Onay) baytını oku ve yut
}

void init_mouse() {
    unsigned char _status;
    
    outb(0x64, 0xA8); // Fare donanımını (Auxiliary Device) aktifleştir
    
    // Fareden gelecek donanım kesmelerini (IRQ12) aç
    outb(0x64, 0x20); 
    _status = inb(0x60) | 2; // COMPAQ Status baytının 2. bitini 1 yap
    outb(0x64, 0x60);
    outb(0x60, _status);
    
    // Fareyi varsayılan ayarlara getir ve paket akışını başlat
    mouse_write(0xF6); // Varsayılan ayarlar (Defaults)
    mouse_write(0xF4); // Veri akışını başlat (Enable Packet Streaming)
}

void mouse_handler_main() {
    // PS/2 denetleyicisinden gelen durumu oku
    unsigned char status = inb(0x64);
    if (!(status & 1)) return; // Çıktı tamponu boşsa çık
    if (!(status & 0x20)) return; // Gelen veri fareden gelmiyorsa (klavyeden geliyorsa) çık
    
    // 3 baytlık fare paketini topla
    mouse_byte[mouse_cycle++] = inb(0x60);
    
    if (mouse_cycle == 3) {
        mouse_cycle = 0;
        if ((mouse_byte[0] & 0x80) || (mouse_byte[0] & 0x40)) return;
        
        // YENİ: Paketin ilk baytından sol buton durumunu filtrele (Bit 0)
        mouse_left_button = (mouse_byte[0] & 0x01);
        
        int d_x = mouse_byte[1];
        int d_y = mouse_byte[2];
        
        if (d_x && (mouse_byte[0] & 0x10)) d_x |= 0xFFFFFF00;
        if (d_y && (mouse_byte[0] & 0x20)) d_y |= 0xFFFFFF00;
        
        mouse_x += d_x;
        mouse_y -= d_y; // VBE modunda Y ekseni aşağı doğru artar
        
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x > 1023) mouse_x = 1023;
        if (mouse_y > 767) mouse_y = 767;
    }
}