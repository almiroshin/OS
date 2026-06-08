OUTPUT_FORMAT(elf32-i386)
ENTRY(app_image_start)

SECTIONS
{
    . = 0;

    .text : {
        KEEP(*(.app_header))
        *(.text*)
        *(.rodata*)
    }

    .data : {
        *(.got*)
        *(.got.plt*)
        *(.data*)
        *(.bss*)
        *(COMMON)
    }

    app_image_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
