OUTPUT_FORMAT("elf32-i386")
ENTRY(_start) 

SECTIONS
{
    . = 0x2000000; /* Yeniden 32. MB (Sanal Adres) - Kernel'den çok uzakta! */
    
    .text : {
        *(.text*)
        *(.rodata*)
    }
    
    .data : {
        *(.data*)
    }
    
    .bss : {
        *(.bss*)
        *(COMMON)
    }
}