#include "multiboot.h" //之所以可以使用C语言的头文件是因为用了大写S为文件后缀，看HELP里有

.section .multiboot
	.long MB_MAGIC
	.long MB_ALIGNED_4K_MEM_MAP
	.long CHECKSUM(MB_ALIGNED_4K_MEM_MAP)

.section .bss
	.align 16
	stack_bottom:
		//.skip 32708, 0 #32字节
		.skip 16318, 0
	stack_top:


.section .text
	.global start_
	.type start_, @function
	start_:
		movl $stack_top, %esp
		/*
			To do: kernel init
				1.LOAD GDT
				1.LOAD GDT
				1.LOAD GDT	
		*/
		call _kernel_init

		subl $0x06, %esp
		movl $_gdt, 2(%esp)
		movw _gdt_limit, %ax
		movw %ax, (%esp)
		lgdt (%esp)

		movl $_idt, 2(%esp)
		movw _idt_limit, %ax
		movw %ax, (%esp)
		lidt (%esp)

		addl $0x06, %esp
	
		movw $0x10, %cx
		movw %cx, %es
		movw %cx, %ds
		movw %cx, %fs
		movw %cx, %gs
		movw %cx, %ss

		pushw $0x08
		pushl $_after_gdt	
		retf

	_after_gdt:
		pushl %ebx
		call _kernel_main

		cli
	j_:
		hlt
		jmp j_

