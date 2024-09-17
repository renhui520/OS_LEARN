#include <awa/common.h>
#include <awa/tty/tty.h>

#include <awa/mm/page.h>
#include <awa/mm/pmm.h>
#include <awa/mm/vmm.h>
#include <awa/mm/kalloc.h>
#include <awa/spike.h>
#include <awa/syslog.h>
#include <awa/timer.h>

#include <hal/rtc.h>
#include <hal/apic.h>
#include <hal/ioapic.h>
#include <hal/acpi/acpi.h>

#include <arch/x86/boot/multiboot.h>
#include <arch/x86/idt.h>
#include <arch/x86/interrupts.h>

#include <klibc/stdio.h>

#include <stdint.h>
#include <stddef.h>


extern uint8_t __kernel_start;
extern uint8_t __kernel_end;
extern uint8_t __init_hhk_end;


// Set remotely by kernel/asm/x86/prologue.S
multiboot_info_t* _k_init_mb_info;

LOG_MODULE("INIT");//设置一个内核专用的 kprintf函数 输出内容自带 "INIT" 标签开头

void
setup_memory(multiboot_memory_map_t* map, size_t map_size);

void
setup_kernel_runtime();

void
lock_reserved_memory();

void
unlock_reserved_memory();

/*
 * 初始化7个模块(内容):
 * idt初始化设置
 * 虚拟内存管理器
 * 物理内存管理器
 * 指定tty内存地址
 * 指定tty显示的主题颜色
 * intr_routine_init 不知道
 * rtc 不知道
*/
void
_kernel_pre_init() {
    _init_idt();    //初始化 IDT 并将 IDT数据和大小限制 分别存储进 _idt数组 和 _idt_limit变量
    intr_routine_init(); //没看到这部分，今后在写!!!!!!!!!!!!!!!!!!!!!

    // (_k_init_mb_info->mem_upper << 10)将 KiB 转化为 字节
    // 1KiB = 2^10字节 1MiB = 2^20字节
    // 所以 mem_upper << 10 ----> mem_upper * 2^10
    pmm_init(MEM_1MB + (_k_init_mb_info->mem_upper << 10));// 标记所有物理页为 [已占用]
    vmm_init();//这个函数内暂时没有任何内容
    rtc_init();//没看到这部分，今后在写!!!!!!!!!!!!!!!!!!!!!

    tty_init((void*)VGA_BUFFER_PADDR);//初始化 tty指向的VGA地址 并 清空画面
    tty_set_theme(VGA_COLOR_WHITE, VGA_COLOR_BLACK);//初始化tty主题颜色(背景色和文字色)
}

/*
 * 输出：内存信息？
 *
 * 设置：
 * setup_memory
 * setup_kernel_runtime
*/
void
_kernel_init() {
    kprintf("[MM] Mem: %d KiB, Extended Mem: %d KiB\n",
           _k_init_mb_info->mem_lower,  // 系统启动时可以立即访问的内存，该内存连续且位于较低的地址范围内，通常为1MB或者640KB
           _k_init_mb_info->mem_upper); // 除了mem_lower之外的其他可用内存，这部分内存位于较高地址范围内，且为不连续的块，这部分内存随着系统的发展可能包含更多的可用的RAM

    //计算 位图 大小
    unsigned int map_size = _k_init_mb_info->mmap_length / sizeof(multiboot_memory_map_t);
    
    //按照 Memory map 标识可用的物理页
    setup_memory((multiboot_memory_map_t*)_k_init_mb_info->mmap_addr, map_size);

    setup_kernel_runtime();
}

void 
_kernel_post_init() {
    size_t hhk_init_pg_count = ((uintptr_t)(&__init_hhk_end)) >> PG_SIZE_BITS;
    kprintf(KINFO "[MM] Releaseing %d pages from 0x0.\n", hhk_init_pg_count);

    // Fuck it, I will no longer bother this little 1MiB
    // I just release 4 pages for my APIC & IOAPIC remappings
    for (size_t i = 0; i < 3; i++) {
        vmm_unmap_page((void*)(i << PG_SIZE_BITS));
    }
    
    // 锁定所有系统预留页（内存映射IO，ACPI之类的），并且进行1:1映射
    lock_reserved_memory();

    acpi_init(_k_init_mb_info);
    uintptr_t ioapic_addr = acpi_get_context()->madt.ioapic->ioapic_addr;

    pmm_mark_page_occupied(FLOOR(__APIC_BASE_PADDR, PG_SIZE_BITS));
    pmm_mark_page_occupied(FLOOR(ioapic_addr, PG_SIZE_BITS));

    vmm_set_mapping(APIC_BASE_VADDR, __APIC_BASE_PADDR, PG_PREM_RW);
    vmm_set_mapping(IOAPIC_BASE_VADDR, ioapic_addr, PG_PREM_RW);

    apic_init();
    ioapic_init();
    timer_init(SYS_TIMER_FREQUENCY_HZ);

    for (size_t i = 256; i < hhk_init_pg_count; i++) {
        vmm_unmap_page((void*)(i << PG_SIZE_BITS));
    }
}

void
lock_reserved_memory() {
    multiboot_memory_map_t* mmaps = _k_init_mb_info->mmap_addr;
    size_t map_size = _k_init_mb_info->mmap_length / sizeof(multiboot_memory_map_t);
    for (unsigned int i = 0; i < map_size; i++) {
        multiboot_memory_map_t mmap = mmaps[i];
        if (mmap.type == MULTIBOOT_MEMORY_AVAILABLE) {
            continue;
        }
        uint8_t* pa = PG_ALIGN(mmap.addr_low);
        size_t pg_num = CEIL(mmap.len_low, PG_SIZE_BITS);
        for (size_t j = 0; j < pg_num; j++)
        {
            vmm_set_mapping((pa + (j << PG_SIZE_BITS)), (pa + (j << PG_SIZE_BITS)), PG_PREM_R);
        }
    }
}

void
unlock_reserved_memory() {
    multiboot_memory_map_t* mmaps = _k_init_mb_info->mmap_addr;
    size_t map_size = _k_init_mb_info->mmap_length / sizeof(multiboot_memory_map_t);
    for (unsigned int i = 0; i < map_size; i++) {
        multiboot_memory_map_t mmap = mmaps[i];
        if (mmap.type == MULTIBOOT_MEMORY_AVAILABLE) {
            continue;
        }
        uint8_t* pa = PG_ALIGN(mmap.addr_low);
        size_t pg_num = CEIL(mmap.len_low, PG_SIZE_BITS);
        for (size_t j = 0; j < pg_num; j++)
        {
            vmm_unmap_page((pa + (j << PG_SIZE_BITS)));
        }
    }
}

// 按照 Memory map 标识可用的物理页
void
setup_memory(multiboot_memory_map_t* map, size_t map_size) {

    // First pass, to mark the physical pages
    for (unsigned int i = 0; i < map_size; i++) {
        multiboot_memory_map_t mmap = map[i];
        kprintf("[MM] Base: 0x%x, len: %u KiB, type: %u\n",
               map[i].addr_low,         //内存区域的起始地址
               map[i].len_low >> 10,    //字节 转化为 KiB
               map[i].type);            //内存区域类型 也就是分页属性:
                                        //例如 MULTIBOOT_MEMORY_AVAILABLE

        // 标记 内存 为 可用
        if (mmap.type == MULTIBOOT_MEMORY_AVAILABLE) {
            // 整数向上取整除法
            uintptr_t pg = map[i].addr_low + 0x0fffU;

            // pg >> PG_SIZE_BITS 计算起始页数
            // map[i].len_low >> PG_SIZE_BITS 计算所占页数
            // 连续标记多个页为可用
            pmm_mark_chunk_free(pg >> PG_SIZE_BITS, map[i].len_low >> PG_SIZE_BITS);
            kprintf(KINFO "[MM] Freed %u pages start from 0x%x\n",
                   map[i].len_low >> PG_SIZE_BITS,  //计算页数，因为1页=4KiB=(1<<12)字节
                   pg & ~0x0fffU);  //进行掩码操作保证地址是对齐到页面大小 (4KiB) 的
        }
    }

    // 将内核占据的页，包括前1MB，hhk_init 设为已占用
    size_t pg_count = V2P(&__kernel_end) >> PG_SIZE_BITS; //虚拟内存 ==> 物理内存 并计算占用页数
    pmm_mark_chunk_occupied(0, pg_count);   //连续标记多个页 [已占用]
    kprintf(KINFO "[MM] Allocated %d pages for kernel.\n", pg_count);   //输出 [已占用] 页数


    size_t vga_buf_pgs = VGA_BUFFER_SIZE >> PG_SIZE_BITS; //计算 VGA 缓冲区 页数
    
    // 首先，标记VGA部分为已占用
    // VGA_BUFFER_PADDR >> PG_SIZE_BITS 计算页数
    pmm_mark_chunk_occupied(VGA_BUFFER_PADDR >> PG_SIZE_BITS, vga_buf_pgs); //连续标记多个 "VGA缓冲区" 页 [已占用]
    
    // 重映射VGA文本缓冲区（以后会变成显存，i.e., framebuffer）
    for (size_t i = 0; i < vga_buf_pgs; i++)
    {
        // i << PG_SIZE_BITS 实际上应该和 VGA_BUFFER_PADDR 一致
        //因为 i = vga_buf_pgs, vga_buf_pgs = VGA_BUFFER_SIZE >> PG_SIZE_BITS
        //所以 i = VGA_BUFFER_PADDR >> PG_SIZE_BITS
        //所以 i << PG_SIZE_BITS = VGA_BUFFER_PADDR
        vmm_map_page(                                           //将 虚拟地址 与 物理地址 建立映射关系
            (void*)(VGA_BUFFER_VADDR + (i << PG_SIZE_BITS)),    //虚拟地址  为什么+i << PG_SIZE_BITS?
            (void*)(VGA_BUFFER_PADDR + (i << PG_SIZE_BITS)),    //物理地址
            PG_PREM_RW                                          //页属性 [可读写]
        );
    }
    
    // 更新VGA缓冲区位置至虚拟地址
    tty_set_buffer((void*)VGA_BUFFER_VADDR);

    kprintf(KINFO "[MM] Mapped VGA to %p.\n", VGA_BUFFER_VADDR);
    
}

void
setup_kernel_runtime() {
    // 为内核创建一个专属栈空间。
    for (size_t i = 0; i < (K_STACK_SIZE >> PG_SIZE_BITS); i++) {
        vmm_alloc_page((void*)(K_STACK_START + (i << PG_SIZE_BITS)), PG_PREM_RW);
    }
    kprintf(KINFO "[MM] Allocated %d pages for stack start at %p\n", K_STACK_SIZE>>PG_SIZE_BITS, K_STACK_START);
    assert_msg(kalloc_init(), "Fail to initialize heap");
}
