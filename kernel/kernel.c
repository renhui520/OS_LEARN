#include <awa/tty/tty.h>
#include <awa/arch/gdt.h>
#include <awa/arch/idt.h>

void _kernel_init()
{
//TODO	
	_init_gdt();
	_init_idt();
}

void _kernel_main(void * info_table)
{
	//remove the warning
	(void)info_table;
//TODO
	tty_set_theme(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
	tty_put_str("           This is EasyOS 's kernel !'\n\n");
	tty_put_str("               Writen by RENHUI\n\n");
	tty_put_str("             Learn From lunaixOS\n\n");
	tty_put_str("               Version 1.0dev\n\n");
	tty_put_str("            type \"help\" for help\n\n");

	/*__asm__("int $0");	//引发报错，用于测试中断*/
}
