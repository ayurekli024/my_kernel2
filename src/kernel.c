// GRUB'ın bize donanım bilgilerini ilettiği veri yapısı
struct multiboot_info {
    unsigned int flags;
    unsigned int mem_lower; // KB cinsinden alt bellek (Geleneksel ilk 1 MB)
    unsigned int mem_upper; // KB cinsinden üst bellek (1 MB'ın üzerindeki kısım)
} __attribute__((packed));

// --- EKSİK OLAN FONKSİYON PROTOTİPLERİ BURAYA GELMELİ ---
// --- FONKSİYON PROTOTİPLERİ ---
void put_char(char c);
void print_string(const char *str);
void clear_screen(void);
void update_hardware_cursor(int x, int y);
void* pmm_alloc_block();
void pmm_free_block(void* physical_address);
// ------------------------------
// --------------------------------------------------------

// Tam sayıları ekrana yazdırmak için yardımcı fonksiyon
void print_number(unsigned int num) {
    if (num == 0) {
        put_char('0');
        return;
    }
    
    char buf[16];
    int i = 0;
    
    // Sayıyı basamaklarına ayır (Ters sırada oluşur)
    while (num > 0) {
        buf[i++] = (num % 10) + '0';
        num /= 10;
    }
    
    // Ters sıradaki rakamları düzeltip ekrana bas
    while (i > 0) {
        put_char(buf[--i]);
    }
}
// ============================================================================
// FİZİKSEL BELLEK YÖNETİCİSİ (PMM - BITMAP)
// ============================================================================
#define BLOCK_SIZE 4096       // Her RAM bloğu 4 KB
#define BLOCKS_PER_BYTE 8
#define TOTAL_BLOCKS 32768    // 128 MB RAM için maksimum blok sayısı
#define BITMAP_SIZE (TOTAL_BLOCKS / 32) // 32 bitlik int dizisinin boyutu

// Haritamız (1 = Dolu/Kullanımda, 0 = Boş)
// Başlangıçta tüm haritayı 0 (boş) olarak tanımlıyoruz
unsigned int memory_bitmap[BITMAP_SIZE] = {0}; 

// Bir bloğu "Dolu" (1) olarak işaretle
void bitmap_set(int bit) {
    // bit / 32 ile dizideki indeksi, bit % 32 ile o int içindeki kaydırmayı buluruz
    memory_bitmap[bit / 32] |= (1 << (bit % 32));
}

// Bir bloğu "Boş" (0) olarak işaretle
void bitmap_clear(int bit) {
    memory_bitmap[bit / 32] &= ~(1 << (bit % 32));
}

// Bir bloğun dolu mu boş mu olduğunu test et (Doluysa 1, boşsa 0 döner)
int bitmap_test(int bit) {
    return memory_bitmap[bit / 32] & (1 << (bit % 32));
}
// ============================================================================
// PMM (PHYSICAL MEMORY MANAGER) ALLOCATOR FONKSİYONLARI
// ============================================================================

// Bitmap içinde değeri 0 (boş) olan ilk bloğun indeksini bulur
int pmm_find_first_free_block() {
    for (int i = 0; i < BITMAP_SIZE; i++) {
        // Eğer dizi elemanı 0xFFFFFFFF (tüm bitleri 1) değilse, içinde boş bir yer var demektir
        if (memory_bitmap[i] != 0xFFFFFFFF) {
            // O 32 bitin hangisinin 0 olduğunu bulmak için teker teker test et
            for (int bit = 0; bit < 32; bit++) {
                int test_bit = 1 << bit;
                if (!(memory_bitmap[i] & test_bit)) {
                    return (i * 32) + bit; // Boş bloğun gerçek indeksini döndür
                }
            }
        }
    }
    return -1; // Bellek tamamen doluysa -1 döner (Out of Memory - OOM)
}

// Sistemden 1 adet boş RAM bloğu (4 KB) talep eder ve adresini döndürür
void* pmm_alloc_block() {
    int free_block = pmm_find_first_free_block();
    
    if (free_block == -1) {
        print_string("PANIK: Fiziksel Bellek Tukendi (Out of Memory)!\n");
        return 0; // Hata durumunda NULL (0) döner
    }
    
    bitmap_set(free_block); // Bulunan bloğu haritada 'dolu' olarak işaretle
    
    // Bloğun indeks numarasını gerçek fiziksel RAM adresine çevir
    // Örnek: 256. blok * 4096 bayt = 0x100000 (1 MB adresi)
    unsigned int physical_address = free_block * BLOCK_SIZE;
    
    return (void*)physical_address;
}

// İşi biten bir RAM bloğunu (4 KB) haritada tekrar 'boş' olarak işaretler
void pmm_free_block(void* physical_address) {
    unsigned int addr = (unsigned int)physical_address;
    int block_index = addr / BLOCK_SIZE;
    bitmap_clear(block_index);
}


// ============================================================================
// 1. DONANIM PORT İLETİŞİMİ (I/O)
// ============================================================================

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ __volatile__ ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ __volatile__ ("outb %0, %1" : : "a"(val), "Nd"(port));
}

// ============================================================================
// 2. VGA EKRAN VE İMLEÇ (CURSOR) SÜRÜCÜSÜ
// ============================================================================
int cursor_x = 0;
int cursor_y = 0;
const int SCREEN_WIDTH = 80;
const int SCREEN_HEIGHT = 25;

// Yanıp sönen donanım imlecini ekran koordinatlarına göre günceller
void update_hardware_cursor(int x, int y) {
    unsigned short position = (y * SCREEN_WIDTH) + x;

    // VGA denetleyicisine imleç konumunun alt ve üst baytlarını gönderiyoruz
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(position & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((position >> 8) & 0xFF));
}

// Tüm ekranı temizler ve imleci sol üste (0,0) alır
void clear_screen(void) {
    char *video_memory = (char*) 0xB8000;
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        video_memory[i * 2] = ' ';     // Boşluk karakteri
        video_memory[i * 2 + 1] = 0x07; // Standart siyah üzerine gri renk
    }
    cursor_x = 0;
    cursor_y = 0;
    update_hardware_cursor(cursor_x, cursor_y);
}

// Ekrana tek bir karakter basar, satır sonu ve yeni satır (\n) kontrolü yapar
void put_char(char c) {
    char *video_memory = (char*) 0xB8000;

    // --- BACKSPACE (SİLME) KONTROLÜ ---
    if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--; // Aynı satırda bir geri git
        } else if (cursor_y > 0) {
            // Satır başındaysak bir üst satırın en sağına geç
            cursor_y--;
            cursor_x = SCREEN_WIDTH - 1;
        }
        
        // İmlecin yeni konumundaki harfi "boşluk" karakteri ile ezerek ekrandan sil
        int offset = (cursor_y * SCREEN_WIDTH + cursor_x) * 2;
        video_memory[offset] = ' ';
        video_memory[offset + 1] = 0x0A; // Renk kodunu koru
        
        // İmleci güncelle ve fonksiyonu burada bitir
        update_hardware_cursor(cursor_x, cursor_y);
        return; 
    } 
    // --- YENİ SATIR KONTROLÜ ---
    else if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } 
    // --- NORMAL KARAKTER YAZDIRMA ---
    else {
        int offset = (cursor_y * SCREEN_WIDTH + cursor_x) * 2;
        video_memory[offset] = c;
        video_memory[offset + 1] = 0x0A;
        cursor_x++;
    }

    // Satır sonu dolduysa alt satıra geç
    if (cursor_x >= SCREEN_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }

    // Ekran sınırı aşma kontrolü (Taşmayı önle)
    if (cursor_y >= SCREEN_HEIGHT) {
        cursor_y = 0; 
        cursor_x = 0;
    }

    // İmlecin son konumunu donanıma bildir
    update_hardware_cursor(cursor_x, cursor_y);
}

// Karakter dizilerini (String) ekrana yazdırmak için yardımcı fonksiyon
void print_string(const char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        put_char(str[i]);
    }
}
// --- SHELL (KOMUT YORUMLAYICI) DEĞİŞKENLERİ VE FONKSİYONLARI ---
char command_buffer[256]; // Kullanıcının yazdığı satırı tutacak bellek
int buffer_index = 0;     // Bellekteki mevcut konumumuz

// Standart C kütüphanesindeki strcmp fonksiyonunun kendi üretimimiz olan versiyonu
int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// Enter'a basıldığında buffer'daki yazıyı komut olarak değerlendiren fonksiyon
void execute_command(void) {
    if (buffer_index == 0) return; // Boş basıldıysa hiçbir şey yapma
    
    command_buffer[buffer_index] = '\0'; // String dizisini kurallara uygun sonlandır

    // Komutları kontrol et
    if (strcmp(command_buffer, "help") == 0) {
        print_string("Kullanilabilir komutlar: help, clear, merhaba\n");
    } 
    else if (strcmp(command_buffer, "clear") == 0) {
        clear_screen();
    } 
    else if (strcmp(command_buffer, "merhaba") == 0) {
        print_string("Sisteme hos geldin! Ilk komutun basariyla calisti.\n");
    } 
    else {
        print_string("Unknown Command: ");
        print_string(command_buffer);
        print_string("\n");
    }

    // Komut işlendikten sonra buffer'ı sıfırla
    buffer_index = 0; 
}
// ============================================================================
// 3. KLAVYE SÜRÜCÜSÜ VE TARAMA TABLOSU (SCANCODE MAP)
// ============================================================================
// Standart US Klavye düzeni (İndeks numaraları donanımdan dönen scancode'ları temsil eder)
unsigned char keyboard_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	'9', '0', '-', '=', '\b',	
  '\t', 'q', 'w', 'e', 'r',	't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0, '\\', 
  'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0, '*',   0, ' ',	
    0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  '-',   0,   0,   0, '+',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};
// Shift tuşuna basılıyken kullanılacak alternatif tarama tablosu
unsigned char keyboard_map_shifted[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',   
  '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',   
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',   0, '|', 
  'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?',   0, '*',   0, ' ', 
    0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  '-',   0,   0,   0, '+',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

// Shift tuşunun basılı olup olmadığını takip eden küresel durum değişkeni
int shift_pressed = 0;

// Klavyeden bir tuşa basıldığında tetiklenen asıl fonksiyon
void keyboard_handler_main(void) {
    unsigned char status = inb(0x64);
    unsigned char keycode;

    if (status & 0x01) {
        keycode = inb(0x60);
        
        if (keycode == 0x2A || keycode == 0x36) {
            shift_pressed = 1;
        } else if (keycode == 0xAA || keycode == 0xB6) {
            shift_pressed = 0;
        } else if (!(keycode & 0x80)) {
            char c = shift_pressed ? keyboard_map_shifted[keycode] : keyboard_map[keycode];
            
            if (c != 0) {
                // EĞER ENTER'A BASILDIYSA: Komutu çalıştır ve yeni satıra geç
                if (c == '\n') {
                    put_char('\n');
                    execute_command();
                    print_string("> "); // Yeni komut için prompt yazdır
                } 
                // EĞER SİLME (BACKSPACE) TUŞUNA BASILDIYSA: Sadece buffer'da harf varsa sil
                else if (c == '\b') {
                    if (buffer_index > 0) {
                        buffer_index--; // Bellekten sil
                        put_char('\b'); // Ekrandan sil
                    }
                } 
                // NORMAL BİR HARF YAZILDIYSA: Belleğe ekle ve ekrana yaz
                else {
                    if (buffer_index < 255) { // Bellek sınırını (Buffer Overflow) koru
                        command_buffer[buffer_index++] = c;
                        put_char(c);
                    }
                }
            }
        }
    }
    outb(0x20, 0x20); // EOI
}

// ============================================================================
// 4. IDT (KESME TANIMLAYICI TABLOSU) YAPILANDIRMASI
// ============================================================================
struct IDT_entry {
    unsigned short offset_lowerbits;
    unsigned short selector;
    unsigned char zero;
    unsigned char type_attr;
    unsigned short offset_higherbits;
} __attribute__((packed));

struct IDT_pointer {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

struct IDT_entry idt[256];
struct IDT_pointer idt_ptr;

void idt_set_gate(unsigned char num, unsigned long base, unsigned short sel, unsigned char flags) {
    idt[num].offset_lowerbits = base & 0xFFFF;
    idt[num].offset_higherbits = (base >> 16) & 0xFFFF;
    idt[num].selector = sel;
    idt[num].zero = 0;
    idt[num].type_attr = flags;
}

// Donanım kesmelerinin (IRQ) x86 içsel kesmeleriyle çakışmaması için PIC'i yeniden haritalandırıyoruz
// Donanım kesmelerini x86 standartlarından ayırıp yeniden haritalandırıyoruz
void pic_remap(void) {
    outb(0x20, 0x11); 
    outb(0xA0, 0x11);
    
    outb(0x21, 0x20); // Master PIC'i 32. indise taşı
    outb(0xA1, 0x28); // Slave PIC'i 40. indise taşı
    
    outb(0x21, 0x04); 
    outb(0xA1, 0x02);
    outb(0x21, 0x01); 
    outb(0xA1, 0x01);
    
    // ÇÖZÜM BURADA: MASK (Filtreleme)
    // 0xFD (Binary: 1111 1101) -> Sadece 1. bit (IRQ1 - Klavye) 0'dır (Açık). 
    // Diğer tüm donanım kesmeleri 1 (Kapalı/Maskeli) durumdadır.
    outb(0x21, 0xFD); 
    outb(0xA1, 0xFF); // İkinci PIC'teki her şeyi tamamen kapat
}

extern void keyboard_handler(void); // boot.asm'deki Assembly sarmalayıcısı

void init_idt(void) {
    idt_ptr.limit = (sizeof(struct IDT_entry) * 256) - 1;
    idt_ptr.base = (unsigned int)&idt;
    
    // Klavye donanımı IRQ1'dir, PIC haritalamasından sonra 33. kesme kapısına bağlanır (32 + 1)
    idt_set_gate(33, (unsigned long)keyboard_handler, 0x08, 0x8E);
    
    __asm__ __volatile__ ("lidt %0" : : "m" (idt_ptr));
}

// ============================================================================
// 5. ANA SİSTEM GİRİŞ NOKTASI (KERNEL ENTRY)
// ============================================================================
// Artık dışarıdan parametreleri alıyoruz
void kernel_main(unsigned int magic, struct multiboot_info* mb_info) {
    clear_screen();  
    pic_remap();     
    init_idt();      
    __asm__ __volatile__ ("sti"); 
    
    print_string("ArdaOS surum 0.0.1 basariyla yuklendi!\n");

    // GRUB Sihirli Numarası Kontrolü (0x2BADB002)
    if (magic != 0x2BADB002) {
        print_string("HATA: Sistem Multiboot uyumlu bir bootloader ile baslatilmadi!\n");
        return;
    }

    // Multiboot flags'in 0. biti 1 ise, mem_lower ve mem_upper değerleri geçerlidir
    if (mb_info->flags & 0x01) {
        // mem_upper 1 MB'tan sonraki belleği KB cinsinden verir.
        // Toplam RAM (MB) = (mem_upper / 1024) + 1 MB (Temel bellek)
        unsigned int total_memory_mb = (mb_info->mem_upper / 1024) + 1;
        
        print_string("Sistemde Tespit Edilen Toplam RAM: ");
        print_number(total_memory_mb);
        print_string(" MB\n");
        // Toplam blok sayısını bul
        unsigned int total_blocks = (total_memory_mb * 1024 * 1024) / BLOCK_SIZE;
        print_string("Toplam RAM Blok Sayisi: ");
        print_number(total_blocks);
        print_string("\n");

        // --- ÇEKİRDEK KORUMASI ---
        // RAM'in ilk 1 MB'lık kısmını (0x0 - 0x100000 arası) dolu olarak işaretle.
        // Çünkü burada BIOS donanımı, VGA ekran belleği ve bizim ArdaOS'un kodları duruyor.
        // 1 MB = 256 adet 4 KB'lık blok demektir.
        for (int i = 0; i < 256; i++) {
            bitmap_set(i);
        }
        print_string("Ilk 1 MB'lik sistem hucresi (256 blok) korumaya alindi.\n");
    } else {
        print_string("HATA: Bootloader bellek miktarini iletemedi.\n");
    }

    print_string("> "); 

    while(1) {
        __asm__ __volatile__ ("hlt"); 
    }
}