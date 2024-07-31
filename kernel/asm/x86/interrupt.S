.macro isr_template vector, no_error_code=1	//定义一个宏，表示isr_template接受vector和no_error_code两个参数
	.global _asm_isr\vector		 //定义一个全局函数
	.type _asm_isr\vector, @function //同上,vector会被替换为相应的中断号，例如0，则为_asm_isr0
	_asm_isr\vector:
		.if \no_error_code	//if判断
			pushl $0x0
		.endif
		pushl $\vector
		jmp interrupt_wrapper	//跳转到中断处理程序
.endm

.section .text
	isr_template 0	//将isr_template实例化,vecor设置为0，则是_asm_isr0函数

	interrupt_wrapper:	//通用中断处理函数

		movl %esp, %eax
		andl $0xfffffff0, %esp	//清零
		subl $16, %esp
		movl %eax, (%esp)
		
		call interrupt_handler
		pop %eax
		movl %eax, %esp
		addl $8, %esp

		iret

