global task_switch

; C'deki tanımı: void task_switch(unsigned int *prev_esp, unsigned int next_esp);
task_switch:
    ; 1. ESKİ GÖREVİN DURUMUNU KAYDET (Callee-saved registers)
    push ebp
    push ebx
    push esi
    push edi

    ; 2. ESKİ GÖREVİN YIĞIN ADRESİNİ (ESP) BELLEĞE YAZ
    mov eax, [esp + 20] ; arg1: prev_esp (Geçerli görevin esp değişkeninin RAM adresi)
    mov [eax], esp      

    ; 3. YENİ GÖREVİN YIĞIN ADRESİNİ İŞLEMCİYE (ESP) YÜKLE
    mov esp, [esp + 24] ; arg2: next_esp (Yeni görevin yığın adresi)

    ; 4. YENİ GÖREVİN DURUMUNU GERİ YÜKLE
    pop edi
    pop esi
    pop ebx
    pop ebp

    ret ; İşlemci bu noktada eski göreve değil, yeni görevin kodlarına sıçrar!