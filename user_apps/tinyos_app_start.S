#include "tinyos_app.h"

.section .app_header, "a"
.global app_image_start
.extern app_image_end

app_image_start:
    .long TINYOS_APP_MAGIC
    .long app_entry - app_image_start
    .long app_image_end - app_image_start
    .long 0

.section .text
.extern tinyos_main

app_entry:
    push %ebp
    mov %esp, %ebp
    push %esi
    push %ebx

    mov 8(%ebp), %esi
    push %esi
    call tinyos_main
    add $4, %esp

    push %eax
    call *20(%esi)
    add $4, %esp

    xor %eax, %eax
    pop %ebx
    pop %esi
    leave
    ret
