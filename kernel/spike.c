#include <awa/spike.h>
#include <arch/x86/interrupts.h>
#include <klibc/stdio.h>

static char buffer[1024];

void __assert_fail(const char* expr, const char* file, unsigned int line) {
    sprintf(buffer, "%s (%s:%u)", expr, file, line);

    // Here we load the buffer's address into %edi ("D" constraint)
    //  This is a convention we made that the AWA_SYS_PANIC syscall will
    //  print the panic message passed via %edi. (see kernel/asm/x86/interrupts.c)
    asm(
        "int %0"
        ::"i"(AWA_SYS_PANIC), "D"(buffer)
    );

    spin();     // never reach
}

// 直接打印报错信息
void panick(const char* msg) {
    asm(
        "int %0"
        ::"i"(AWA_SYS_PANIC), "D"(msg)
    );
    spin();
}
