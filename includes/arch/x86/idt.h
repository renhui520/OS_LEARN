#ifndef __AWA_IDT_H
#define __AWA_IDT_H
#define IDT_ATTR(dpl)                   ((0x70 << 5) | (dpl & 3) << 13 | 1 << 15)

void
_init_idt();

#endif
