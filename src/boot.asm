MBOOT_PAGE_ALIGN    equ 1 << 0
MBOOT_MEM_INFO      equ 1 << 1
MBOOT_VIDEO_MODE    equ 1 << 2   
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
    
    ; VBE (Video BIOS Extension) Grafik Modu İstekleri
    dd 0        ; 0 = Lineer Grafik Modu (Piksellere sırayla erişim)
    dd 1024     ; Genişlik
    dd 768      ; Yükseklik
    dd 32       ; Renk Derinliği (32-bit True Color: ARGB)

section .bss
align 16
stack_bottom:
resb 16384
stack_top:

section .text
global _start
global keyboard_handler      
extern kernel_main
extern keyboard_handler_main 
extern update_tss_esp0       ; YENİ: TSS güncelleyici C fonksiyonumuz

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

keyboard_handler:
    cli
    pusha
    call keyboard_handler_main
    
    ; Anakarttaki PIC'e "İşlem Bitti" (EOI) onayını gönder
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

; --- DONANIM SAATİ (PIT - IRQ0) KESMESİ VE ZAMANLAYICI ---
global timer_handler
extern timer_handler_main
extern current_task  

timer_handler:
    pusha  
    
    ; Anakarttaki PIC'e "Saati okudum" onayı (EOI) gönder
    mov al, 0x20
    out 0x20, al
    
    ; C'deki timer_ticks sayacını arttır
    call timer_handler_main
    
    ; ====================================================
    ; PREEMPTIVE MULTITASKING: UYUYANLARI ATLAYAN SCHEDULER
    ; ====================================================
    mov eax, [current_task]
    cmp eax, 0          
    je .no_timer_switch
    
    ; 1. Eski görevin yığınını (ESP) kaydet
    mov [eax], esp      

.timer_find_next_task:
    ; 2. Sıradaki göreve geç: current_task = current_task->next
    mov ebx, [eax + 16] 
    mov [current_task], ebx
    
    ; 3. Sıradaki görevin State (Durum) değerini kontrol et (state = +20)
    mov ecx, [ebx + 20]
    cmp ecx, 1              
    je .timer_skip_sleeping 
    
    ; YENİ KORUMA: Sıradaki görev uyanıksa, Çekirdek Yığınını (ESP0) TSS'ye bildir!
    pusha
    call update_tss_esp0
    popa

    ; 4. Görev uyanıksa (0), yeni görevin yığınını (ESP) işlemciye yükle
    mov esp, [ebx]      
    jmp .no_timer_switch

.timer_skip_sleeping:
    mov eax, ebx            
    jmp .timer_find_next_task

.no_timer_switch:
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
    push 0              
    push 255            
    jmp isr_common

global isr0
isr0:
    cli
    push 0              
    push 0              
    jmp isr_common

global isr8
isr8:
    cli
    push 8              
    jmp isr_common

global isr13
isr13:
    cli
    push 13             
    jmp isr_common

global isr14
isr14:
    cli
    push 14             
    jmp isr_common

; --- MERKEZİ MAVİ EKRAN YÖNLENDİRİCİSİ (SSE KORUMALI) ---
isr_common:
    ; Yığını (Stack) GCC'nin çökmeyeceği şekilde 16-bayta hizala
    push ebp
    mov ebp, esp
    and esp, 0xFFFFFFF0 
    
    sub esp, 8           
    
    ; C Fonksiyonu için argümanları it (Sağdan Sola)
    push dword [ebp+8]   ; err_code
    push dword [ebp+4]   ; int_no
    
    call fault_handler   ; Mavi ekrana git!
    
    ; Buradan sonrası çalışmaz
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
    out 0x20, al        
    out 0xA0, al        
    popa
    iretd

; --- FARE (PS/2 MOUSE) KESME SARMALAYICISI ---
global mouse_handler
extern mouse_handler_main

mouse_handler:
    pusha
    call mouse_handler_main
    
    ; Master ve Slave PIC'e EOI sinyali gönder
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

; =================================================================
; YENİ: TSS (GÖREV DURUM SEGMENTİ) YÜKLEYİCİSİ
; =================================================================
global tss_flush
tss_flush:
    ; GDT'deki 5. kapımız TSS kapısıdır. (Index 5 * 8 bayt = 40 = 0x28)
    ; Ring 3 yetkisiyle (RPL 3) çağrılacağı için: 0x28 | 3 = 0x2B
    mov ax, 0x2B 
    ltr ax       ; Load Task Register (TSS'yi işlemciye yükle)
    ret

; =================================================================
; 3. SİSTEM ÇAĞRILARI (SYSCALLS - INT 0x80)
; =================================================================
global syscall_handler
extern syscall_handler_main

syscall_handler:
    cli
    pusha               ; Uygulamanın o anki tüm CPU yazmaçlarını yığına kaydet

    ; CDECL Standardı: C fonksiyonuna parametreleri sağdan sola itiyoruz!
    push edi            ; arg5
    push esi            ; arg4
    push edx            ; arg3
    push ecx            ; arg2
    push ebx            ; arg1
    push eax            ; sys_num (Çağrılmak istenen API numarası)
    
    call syscall_handler_main
    add esp, 24         ; Yığını temizle
    
    ; C fonksiyonundan dönen sonucu eski EAX'in üzerine yazıyoruz
    mov [esp + 28], eax 
    
    popa                
    iretd               

; ====================================================
; MANUEL GÖREV DEĞİŞTİRİCİ VE SCHEDULER ZEKA KÜPÜ
; ====================================================
global yield_handler

yield_handler:
    pusha               
    
    mov eax, [current_task]
    cmp eax, 0          
    je .no_yield_switch
    
    ; 1. Eski görevin ESP'sini kaydet
    mov [eax], esp      

.find_next_task:
    ; 2. Sıradaki göreve geç (next = +16)
    mov ebx, [eax + 16] 
    mov [current_task], ebx
    
    ; 3. Sıradaki görevin State (Durum) değerini kontrol et (state = +20)
    mov ecx, [ebx + 20]
    cmp ecx, 1          
    je .skip_sleeping   
    
    ; YENİ KORUMA: Sıradaki görev uyanıksa, Çekirdek Yığınını (ESP0) TSS'ye bildir!
    pusha
    call update_tss_esp0
    popa

    ; 4. Görev uyanıksa (0), ESP'yi yükle ve çalıştır
    mov esp, [ebx]      
    jmp .no_yield_switch

.skip_sleeping:
    mov eax, ebx        
    jmp .find_next_task

.no_yield_switch:
    popa
    iretd