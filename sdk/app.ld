OUTPUT_FORMAT("elf32-i386")
ENTRY(main) /* ELF başlığına eklenecek ilk giriş noktası */

SECTIONS
{
    . = 0x400000; /* Uygulamaların yeni ortak sanal evreni! */
    
    .text : ALIGN(4096) {
        *(.text*)
        *(.rodata*)
    }
    
    .data : ALIGN(4096) {
        *(.data*)
    }
    
    .bss : ALIGN(4096) {
        *(.bss*)
        *(COMMON)
    }
}