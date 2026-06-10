MBOOT_PAGE_ALIGN    equ 1 << 0
MBOOT_MEM_INFO      equ 1 << 1
MBOOT_VIDEO_MODE    equ 1 << 2   ; YENİ: Grafik Modu İstek Bayrağı
MBOOT_HEADER_MAGIC  equ 0x1BADB002
MBOOT_HEADER_FLAGS  equ MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO | MBOOT_VIDEO_MODE
MBOOT_CHECKSUM      equ -(MBOOT_HEADER_MAGIC + MBOOT_HEADER_FLAGS)

section .multiboot
align 4
    dd MBOOT_HEADER_MAGIC
    dd MBOOT_HEADER_FLAGS
    dd MBOOT_CHECKSUM
    
    ; Adres Alanları (ELF formatı kullandığımız için hepsini 0 bırakıyoruz)
    dd 0 
    dd 0
    dd 0
    dd 0
    dd 0
    
    ; YENİ: VBE (Video BIOS Extension) Grafik Modu İstekleri
    dd 0        ; 0 = Lineer Grafik Modu (Piksellere sırayla erişim)
    dd 1024     ; Genişlik
    dd 768      ; Yükseklik
    dd 32       ; Renk Derinliği (32-bit True Color: ARGB)

; --- BURADAN SONRASI AYNEN KALACAK (section .bss vb.) ---
section .bss
align 16
stack_bottom:
resb 16384
stack_top:

section .text
global _start
global keyboard_handler      ; Dışarıya açıyoruz ki C kodu görebilsin
extern kernel_main
extern keyboard_handler_main ; C'deki fonksiyonumuz

_start:
    ; Yığın alanını ayarla
    mov esp, stack_top
    
    ; GRUB'ın bize bıraktığı verileri C fonksiyonuna parametre olarak gönder
    push ebx  ; Multiboot bilgi yapısının bellek adresi
    push eax  ; Multiboot sihirli numarası (Magic Number)
    
    ; C koduna dallan
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang

; --- KLAVYE KESMESİ (IRQ1) ---
global keyboard_handler
extern keyboard_handler_main

keyboard_handler:
    cli
    pusha
    call keyboard_handler_main
    
    ; Master PIC'e (Port 0x20) işlemi bitirdiğimizi (EOI) söylüyoruz
    mov al, 0x20
    out 0x20, al 
    
    popa
    iretd
    ; --- SAYFALAMA (PAGING) AKTİFLEŞTİRİCİ ---
global enable_paging

enable_paging:
    push ebp
    mov ebp, esp
    
    ; C fonksiyonundan gönderilen Page Directory adresini al
    mov eax, [ebp+8]
    
    ; CR3 yazmacına Page Directory adresini yükle
    mov cr3, eax
    
    ; CR0 yazmacındaki 31. biti (PG - Paging) 1 yaparak sayfalamayı başlat
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax
    
    pop ebp
    ret
; --- SİSTEM SAATİ KESME SARMALAYICISI ---
global timer_handler
extern timer_handler_main

timer_handler:
    pusha
    call timer_handler_main
    popa
    iretd
; --- İSTİSNA (EXCEPTION) YAKALAYICILARI ---
global isr0
global isr6
global isr8
global isr11
global isr13
global isr14
extern fault_handler

; =================================================================
; 1. GÜVENLİK AĞI: BİLİNMEYEN CPU İSTİSNALARI (0-31 ARASI)
; =================================================================
global isr_default_ex
isr_default_ex:
    cli
    push 0              ; Sahte hata kodu
    push 255            ; 255'i "Bilinmeyen CPU İstisnası" olarak etiketliyoruz
    jmp isr_common

; Spesifik CPU İstisnaları
global isr0
isr0:
    cli
    push 0              ; Sahte hata kodu
    push 0              ; Kesme Numarası
    jmp isr_common

global isr8
isr8:
    cli
    push 8              ; CPU zaten hata kodunu itti, biz sadece 8'i itiyoruz
    jmp isr_common

global isr13
isr13:
    cli
    push 13             ; CPU zaten hata kodunu itti, biz sadece 13'ü itiyoruz
    jmp isr_common

global isr14
isr14:
    cli
    push 14             ; CPU zaten hata kodunu itti, biz sadece 14'ü itiyoruz
    jmp isr_common

; --- MERKEZİ MAVİ EKRAN YÖNLENDİRİCİSİ (SSE KORUMALI) ---
extern fault_handler
isr_common:
    ; Yığını (Stack) GCC'nin çökmeyeceği şekilde 16-bayta hizala
    push ebp
    mov ebp, esp
    and esp, 0xFFFFFFF0 
    
    sub esp, 8           ; Hizalamayi korumak icin 8 bayt bosluk birak
    
    ; C Fonksiyonu için argümanları it (Sağdan Sola)
    push dword [ebp+8]   ; err_code
    push dword [ebp+4]   ; int_no
    
    call fault_handler   ; Mavi ekrana git!
    
    ; Buradan sonrası çalışmaz çünkü sistem kilitlenecek ama kurallara uyalım
    mov esp, ebp
    pop ebp
    add esp, 8
    cli
.hang:
    hlt
    jmp .hang

; =================================================================
; 2. GÜVENLİK AĞI: HAYALET DONANIM KESMELERİ (32-255 ARASI)
; =================================================================
global isr_default_int
isr_default_int:
    cli
    pusha
    mov al, 0x20
    out 0x20, al        ; Master PIC'e onay gönder
    out 0xA0, al        ; Slave PIC'e onay gönder
    popa
    iretd
; --- FARE (PS/2 MOUSE) KESME SARMALAYICISI ---
global mouse_handler
extern mouse_handler_main

mouse_handler:
    pusha
    call mouse_handler_main
    
    ; Master ve Slave PIC'e (Programmable Interrupt Controller) EOI (İşlem Bitti) sinyali gönder
    mov al, 0x20
    out 0xA0, al 
    out 0x20, al 
    
    popa
    iretd
; --- GDT HARİTA YÜKLEYİCİSİ ---
global gdt_flush
gdt_flush:
    mov eax, [esp+4]    ; C kodundan gelen adresi al
    lgdt [eax]          ; Yeni GDT'yi işlemciye yükle
    
    ; Veri segmentlerini yeni GDT'ye göre güncelle (0x10)
    mov ax, 0x10      
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Kod segmentini yenilemek için "Uzak Sıçrama" (Far Jump) yap (0x08)
    jmp 0x08:.flush
.flush:
    ret