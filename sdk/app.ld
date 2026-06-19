/* İŞTE UNUTTUĞUM O EFSANE KORUMA ZIRHI! GCC'yi saf makine kodu üretmeye zorlar! */
OUTPUT_FORMAT("binary")

SECTIONS
{
    /* Uygulamanın RAM'deki adresini 0 varsayarak derle */
    . = 0x00000000;
    
    .text : ALIGN(4) {
        *(.text.entry) /* Entry point ZORUNLU olarak 0. baytta kalır */
        *(.text*)
        *(.rodata*)    /* C dilindeki sabit metinler ("ZAFER!" vb.) koda gömülür */
    }
    
    .data : ALIGN(4) {
        *(.data*)
    }
    
    .bss : ALIGN(4) {
        *(.bss*)
        *(COMMON)
    }
}