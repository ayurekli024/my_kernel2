MBOOT_PAGE_ALIGN    equ 1 << 0
MBOOT_MEM_INFO      equ 1 << 1
MBOOT_HEADER_MAGIC  equ 0x1BADB002
MBOOT_HEADER_FLAGS  equ MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO
MBOOT_CHECKSUM      equ -(MBOOT_HEADER_MAGIC + MBOOT_HEADER_FLAGS)

section .multiboot
align 4
    dd MBOOT_HEADER_MAGIC
    dd MBOOT_HEADER_FLAGS
    dd MBOOT_CHECKSUM

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

; --- KLAVYE KESME SARMALAYICISI (INTERRUPT WRAPPER) ---
keyboard_handler:
    pusha                    ; İşlemcinin o anki tüm genel kayıtçılarını (registers) yığına güvene al
    call keyboard_handler_main ; C dilindeki asıl fonksiyonumuzu çağır
    popa                     ; Kayıtçıları eski haline geri yükle
    iretd                    ; Kesme işleminden geri dön (Interrupt Return - 32 bit)
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