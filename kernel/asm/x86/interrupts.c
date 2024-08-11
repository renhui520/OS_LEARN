#include <arch/x86/interrupts.h>
#include <libc/stdio.h>

#include <awa/tty/tty.h>

void isr0 (isr_param* param) {
	tty_clear();
	tty_set_theme(VGA_COLOR_RED, VGA_COLOR_BLACK);//将文字颜色设置为红色，突出
    printf("[PANIC] Exception (%d) CS=0x%X, EIP=0x%X", param->vector, param->cs, param->eip);
}

void 
interrupt_handler(isr_param* param) {
    switch (param->vector)
    {
        case 0:
            isr0(param);
            break;
    }
}
