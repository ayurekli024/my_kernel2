OUTPUT_FORMAT("binary")
ENTRY(_start)
SECTIONS
{
    . = 0x0; /* Kodlar doğrudan 0. offsetten başlayacak */
    
    .text : {
        *(.entry)   /* _start fonksiyonunu ZORLA en başa koyar */
        *(.text)
    }
    .data : { *(.data) }
    .bss  : { *(.bss)  }
}