/* 高半核入口点 - 0xC0000000 */

//代码段 .text
.section .text
    .global hhk_entry_      //从boot.S 跳转到这
    hhk_entry_:
        /*
            欢迎来到虚拟内存的世界！ :D
         */

        // 为栈留出16字节空间
        // 16字节对齐，优化性能 ??
        subl $16, %esp
        /* 
            最终还是决定将IDT&GDT的初始化和安装放在这里
            注意：由于已开启分页，GDTR与IDTR里面放的不是物理地址，是线性地址！ 
        */
        /* 
            加载 GDT 
            P.s. 虽然GDT在分页后已变得不重要，甚至可以忽略不作。但为了保持完整性，还是选择加载他
                    这主要是为了保险起见，让GDTR有一个合法的值，否则多咱的粗心大意，容易出#GP
        */
        call _init_gdt
        //lgdt需要6字节的值，所以需要将GDTR地址入栈
        //_gdt占用4字节
        movl $_gdt, 2(%esp)     // 将数组入栈 _gdt本身是一个数组，数组自身就是地址，所以无须放入ax寄存器然后再取地址

        //_gdt_limit占用2字节   所以需要2(%esp)移动到esp下2字节的位置存放_gdt
        movw _gdt_limit, %ax    //_gdt_limit不是数组，所以需要放入ax寄存器，取地址

        movw %ax, (%esp)        // 将 _gdt_limit 入栈
        lgdt (%esp)

        /* 更新段寄存器 */
        movw $0x10, %cx //0x10 对应于 SEG_R0_DATA 描述符
/*
 * 代表gdt中的索引 “2”  十进制为16 根据公式(n代表索引) n * 8 + rpl
 *
 * 因为 0x10(十六进制) = 16(十进制) = 2 * 8 + 0(rpl)
 * 所以 0x10 对应于 SEG_R0_DATA 描述符
 *
 * rpl为低3位，代表RPL( ring privilege level )，也就是权限级别。
 * 0 代表内核权限，也就是最高权限
 * 即 000 为内核权限 011(十进制为3) 为用户权限
 *
 * 如 索引1 权限0 十六进制数则等于 1 * 8 + 0 = 8 = 0x08
 *    索引1 权限1 十六进制数则等于 1 * 8 + 1 = 9 = 0x09
 *    索引2 权限2 十六进制数则等于 2 * 8 + 2 = 18 = 0x12
 *    索引3 权限3 十六进制数则等于 3 * 8 + 3 = 27 = 0x1B
*/


        /* 更新段寄存器 */
        movw %cx, %es
        movw %cx, %ds
        movw %cx, %fs
        movw %cx, %gs
        movw %cx, %ss

        /* 更新 CS:EIP 指针*/
        pushw $0x08 //0x08 对应于 SEG_R0_CODE 描述符
/*
 * 0x08还做为CS 寄存器的值，因此它代表内核代码段
 * 0x08 对应于 SEG_R0_CODE 描述符
 * 因为 0x08(十六进制) = 8(十进制) = 2 * 8 + 0(rpl)
 * 所以 0x08 对应于 SEG_R0_CODE 描述符
 *
 * rpl为低3位，代表RPL( ring privilege level )，也就是权限级别。
 * 0 代表内核权限，也就是最高权限
 * 即 000 为内核权限 011(十进制为3) 为用户权限
 *
 * 如 索引1 权限0 十六进制数则等于 1 * 8 + 0 = 8 = 0x08
 *    索引1 权限1 十六进制数则等于 1 * 8 + 1 = 9 = 0x09
 *    索引2 权限2 十六进制数则等于 2 * 8 + 2 = 18 = 0x12
 *    索引3 权限3 十六进制数则等于 3 * 8 + 3 = 27 = 0x1B
*/

        pushl $_after_gdt //将 _after_gdt 标签的地址压入堆栈，这将成为新的 EIP 值 将会被retf弹出
        retf //处理器从堆栈中弹出 _after_gdt 标签地址并将其赋值给 EIP 寄存器，然后从堆栈中弹出 0x08 并将其赋值给 CS 寄存器

/*

retf(远返回)指令用于从当前的 CS:EIP 指针跳转到另一个 CS:EIP 指针
这里没有指定索引值，因此默认从堆栈中弹出两个值：先是 EIP，然后是 CS
这意味着处理器将从堆栈中弹出 _after_gdt 标签的地址作为新的 EIP 值，并弹出 0x08 作为新的 CS 值。处理器将使用新的 CS:EIP 继续执行指令

retf(远返回) 弹出堆栈中的两个值，第一个默认被设置为EIP，第二个默认被设置为CS

*/


    _after_gdt:

        movl $mb_info, _k_init_mb_info

        call _kernel_pre_init

        subl $6, %esp

        # 加载 IDT
        movl $_idt, 2(%esp)
        movw _idt_limit, %ax
        movw %ax, (%esp)
        lidt (%esp)

        addl $6, %esp

        call _kernel_init

        /* 
            加载新的栈指针，位于0xffbfffff，但因为16字节对齐的需求，低四位清零。
            为什么不是0xffffffff？ 因为0xffc00000 - 0xffffffff 这4MiB的空间用于
            对页表与页目录的循环映射。
        */
        mov $0xffbffff0, %esp

        call _kernel_post_init

        /* 进入内核 */  
        call _kernel_main

        cli
    j_:
        hlt
        jmp j_
