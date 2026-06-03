.section .text
.global irq0_stub
.global irq1_stub
.global idt_load

irq0_stub:
    pusha
    call timer_irq
    popa
    iret

irq1_stub:
    pusha
    call keyboard_irq
    popa
    iret

idt_load:
    mov 4(%esp), %eax
    lidt (%eax)
    ret

