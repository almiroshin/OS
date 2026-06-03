.section .text
.global app_image_start

app_image_start:
    .long 0x31505041
    .long app_entry - app_image_start
    .long app_image_end - app_image_start
    .long 0

app_entry:
    push %ebp
    mov %esp, %ebp
    push %esi
    push %ebx

    mov 8(%ebp), %esi

    call 1f
1:
    pop %ebx

    lea msg_title - 1b(%ebx), %eax
    push %eax
    call *0(%esi)
    add $4, %esp

    lea msg_kind - 1b(%ebx), %eax
    push %eax
    call *0(%esi)
    add $4, %esp

    call *8(%esi)
    call *4(%esi)

    lea path_apps - 1b(%ebx), %eax
    push %eax
    call *16(%esi)
    add $4, %esp

    lea msg_done - 1b(%ebx), %eax
    push %eax
    call *0(%esi)
    add $4, %esp

    push $0
    call *20(%esi)
    add $4, %esp

    xor %eax, %eax
    pop %ebx
    pop %esi
    leave
    ret

msg_title:
    .asciz "HELLO FROM NATIVE.APP"
msg_kind:
    .asciz "THIS IS A RAW I386 IMAGE FILE"
path_apps:
    .asciz "C:\\APPS"
msg_done:
    .asciz "NATIVE APP RETURNING TO LOADER"

.balign 4
app_image_end:

