; src/switch.asm
global switch_task

; void switch_task(unsigned int *old_esp, unsigned int new_esp);
switch_task:
    ; 1. Mevcut görevin (çağıranın) yazmaçlarını yığına (stack) iterek kurtar
    push ebp
    push ebx
    push esi
    push edi

    ; 2. Eski ESP'yi kaydet
    mov eax, [esp + 20]  ; eax = old_esp pointer'ının adresi
    mov [eax], esp       ; O anki yığın adresini old_esp'nin içine yaz

    ; 3. YENİ GÖREVE GEÇİŞ!
    mov esp, [esp + 24]  ; Yığın işaretçisini new_esp adresiyle değiştir

    ; 4. Yeni görevin daha önce kaydedilmiş yazmaçlarını geri yükle
    pop edi
    pop esi
    pop ebx
    pop ebp

    ret ; C koduna geri dön (Ama artık yeni görevin C koduna dönecek!)