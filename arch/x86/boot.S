#define __ASM__ 1
#include <arch/x86/boot/multiboot.h>    // 为".section multiboot"提供参数，还有结构体

#define MB_FLAGS    MULTIBOOT_MEMORY_INFO | MULTIBOOT_PAGE_ALIGN    // 设置Multiboot flags
#define KPG_SIZE    24*1024 // 设置内核 页目录 和 页表 总大小 为24KiB


/*

    补充： 1. 大多情况下 .section定义的段都是 “虚拟段”!!
    例如： .text, .data, .bss等 段是虚拟段，在链接的时候会被编译器自动分配一个地址，

*/


//设置 multiboot信息，让grub2能够识别我们提供的信息，并引导内核，似乎这一步是必要的！
// 仅需这一步则可被识别
.section .multiboot
    .long MULTIBOOT_MAGIC
    .long MB_FLAGS
    .long CHECKSUM(MB_FLAGS)

.section .bss
    .global mb_info
    .align 16
    /* 为Multiboot info struct 预留空间 */
    mb_info:
        .skip 4096  //预留4096字节空间 用于存储Multiboot信息(结构体)
    /* 根据System V ABI，栈地址必须16字节对齐 */
    /* 这里只是一个临时栈，在_hhk_init里面我们会初始化内核专用栈 */
    stack_bottom:
        .skip 16318 * 2, 0      // 16*1024*2预留32K
    stack_top:  //会在链接时被 编译器设置为正确的地址


/* 
    1 page directory, 
    5 page tables:
        1. Mapping reserved area and hhk_init
        2-5. Remapping the kernels
*/

//存放 页表目录 和 页表数据
.section .kpg
    .global _k_ptd  // 页表目录(页目录)
    _k_ptd:
        .skip KPG_SIZE, 0   //预留内核页大小

// 初始 代码段  最先运行
.section .hhk_init
    .global start_
    .type start_, @function     /* Optional, this just give the 
                                 * linker more knowledge about the label 
                                 */
    start_: 
        cld     // 清除所有标志位
        # 确保屏蔽所有外中断，直到我们准备好PIC为止
        cli     // 关闭中断
        movl $stack_top, %esp   // 将编译器所设置的 栈顶地址 存储在 栈顶(%esp)中，这样就可以得知栈区的大小

        subl $16, %esp  // 为Multiboot信息结构体预留空间

        /* 
            将咱们的 multiboot_info 挪个地儿，就是上述预留的空间里
            而后在_hhk_init里，我们会对所有的高半核初始化代码（arch/x86下的所有）进行Identity map
            这样，我们能够保证当分页与虚拟地址开启后，我们的内核能够访问到multiboot info table
         */

        //为 _save_multiboot_info 函数提供mb_info地址 方便函数往这个地址内写入信息
        // 地址(%esp - 4)的值 变为 $mb_info的值
        movl $mb_info, 4(%esp)  // 将mb_info所指的地址压入栈顶-4的位置，即 ESP-4 的位置(因为ESP地址是向下增长的，即地址从高到低)
        
        //为 _save_multiboot_info 函数提供 Multiboot信息结构体 
        // 在启动过程中，引导加载程序将Multiboot信息结构体的地址放置在EBX寄存器中
        movl %ebx, (%esp)   // 将%ebx中的值压入栈中，即multiboot_info_t结构体的地址，为multiboot_info_t*提供地址
        call _save_multiboot_info   //保存Multiboot info结构体  应该是将栈中的内容写道mb_info中，然后在生成内核的时候被成功识别吧...

        /*
            _hhk_init用来初始化我们高半核：
                1. 初始化最简单的PD与PT（重新映射我们的内核至3GiB处，以及对相应的地方进行Identity Map）
        */

//--------------------------------------------------
//                  分页

/*因为C语言函数参数传递默认为从右往左 依次入栈，所以4(%esp)将KPG_SIZE压入更低处，(%esp)压入更高处*/
/*      栈顶
        KPG_SIZE        (%esp)
        ptd_t* ptd     4(%esp)
*/
        movl $(KPG_SIZE), 4(%esp)	# 为hhk.c中的_hhk_init(ptd_t* ptd, uint32_t kpg_size)提供kpg_size参数
        // .kpg段是虚拟段，所以_k_ptd也是属于虚拟地址！！！所以才需要( - 0xC0000000 )
        // 根据linker.ld里的配置 . += 0xC0000000 ，从而使.kpg段的地址 >=0xC0000000 看linker.ld最后的内容就知道了
        // 因为前面 += 0xC0000000，所以要 - 0xC0000000
        movl $(_k_ptd - 0xC0000000), (%esp)    /* PTD物理地址 */   # 为hhk.c中的_hhk_init(ptd_t* ptd, uint32_t kpg_size)提供ptd参数
        call _hhk_init	# 调用hhk.c中的_hhk_init函数

        /*
            基本的映射定义好了，我们可以放心的打开分页了
            我们只需要把PTD的基地址加载进CR3就好了。
        */

        /* 加载PTD基地址（物理地址） */

        // CR3 主要用于分页机制
/*
 * CR3 是一个 32 位的寄存器。
 * 其中高 20 位用来存放页目录表的物理地址。
 * 低 12 位是保留位，必须为 0
*/
        //这里提供的ptd已经由_hhk_init中的_init_page映射好了
        movl (%esp), %eax   //这里是用的movl $(_k_ptd - 0xC0000000), (%esp)中的esp(ptd)??????
        andl $0xfffff000, %eax      # 保留高位20位，清除低位12位。这一步是为了确保只保留页目录的基地址部分
        movl %eax, %cr3 /*%cr3是用于存储页目录基地址的控制寄存器，这里设置了页表目录的基地址*/

        /* 开启分页与地址转换 (CR0.PG=1) */
        //%cr0寄存器包含了处理器的控制和状态信息
/*
 * CR0 是一个 32 位的寄存器，用于控制处理器的操作模式和一些关键特性
 * 它包含了多个控制位，用于启用或禁用特定的功能，如分页、保护模式等
 *
 * 控制位:
 * PE (Protection Enable):  位 0，当 "置位" 时，进入保护模式。
 * MP (Monitor Coprocessor):位 1，当 "置位" 时，启用 Coprocessor 监视器。不清楚对不对
 * EM (Emulation):          位 2
 * TS (Task Switched):      位 3    //以下内容的补充除了 位 和 PG 以外，其他都不确定
 * ET (Extended Type):      位 4
 * NE (Numeric Error):      位 5
 * WP (Write Protect):      位 16，当 "置位" 时，不允许在实模式下修改 GDT 和 LDT。
 * AM (Address Mask):       位 18，当 "置位" 时，用于地址掩码。
 * NW (Not Write-through):  位 29，当 "置位" 时，禁用写穿透缓存行为。
 * CD (Cache Disable):      位 30，当 "置位" 时，禁用处理器的一级数据缓存。
 * PG (Paging Enable):      位 31，当 "置位" 时，启用分页机制。
 */
        movl %cr0, %eax         //保存到eax寄存器，方便 位或操作
        orl $0x80000000, %eax   //将%eax中的第31位(PG) 设为1，即设置 PG标志位(分页) 为1，开启分页
        //开启分页
        //由于我们使用multiboot和grub，所以不需要手动设置cr0的PE位，grub会 "自动" 帮我们设置 "PE位"，并进入32位保护模式
        movl %eax, %cr0 //将%eax中的新值写回到%cr0寄存器中，更新控制寄存器以启用分页功能

        addl $16, %esp //将栈指针%esp增加16个字节，清理栈空间，为后续操作做准备

        /* 进入高半核！ */
        // 压入hhk_entry_地址
        pushl $hhk_entry_	# hhk_entry_位于prologue.S中
        ret // 将pushl进来的hhk_entry地址弹出，直接实现跳转  跳转到hhk_entry_
