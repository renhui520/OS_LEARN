#ifndef __AWA_PIC_H
#define __AWA_PIC_H
// TODO: PIC

static inline void
pic_disable()
{
    // ref: https://wiki.osdev.org/8259_PIC
    asm volatile ("movb $0xff, %al\n"
        "outb %al, $0xa1\n"
        "outb %al, $0x21\n");
}

#endif
