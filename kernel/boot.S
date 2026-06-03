.set ALIGN,    1<<0
.set MEMINFO,  1<<1
.set VIDEO,    1<<2
.set FLAGS,    ALIGN | MEMINFO | VIDEO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot, "a"
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM
.long 0
.long 640
.long 400
.long 32

.section .bss
.align 16
stack_bottom:
.skip 65536
stack_top:

.section .text
.global _start
.type _start, @function
_start:
    mov $stack_top, %esp
    push %ebx
    push %eax
    call kernel_main
1:
    cli
    hlt
    jmp 1b
