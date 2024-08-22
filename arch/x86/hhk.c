#include <arch/x86/boot/multiboot.h>
#include <arch/x86/idt.h>
#include <awa/mm/page.h>
#include <awa/common.h>

/*
        PTD(页目录) = PD(页目录)
    +---------------------------+
    |       PDE(页目录项)        |
    |                           |

PTD包含一系列指向PDE的指针  PTD=>PDE(页目录项)=>PTE(页表项)
PDE指向PT(页表)
PTE是页表中的条目，每个PTE指向一个 物理页 或者 包含其他相关信息

*/



/**
 * @brief 计算指定的页目录中的某个页表的地址
 * 
 * @param ptd 页目录的起始地址
 * @param pt_index 页表索引，用于确定页目录中的具体页表
 * @return 返回页表的地址
 */
#define PT_ADDR(ptd, pt_index)                       ((ptd_t*)ptd + (pt_index + 1) * 1024)

/**
 * @brief 设置页目录中的某一项 为新建页目录项(NEW_L1_ENTRY) pde
 * 
 * @param ptd 页目录的起始地址
 * @param pde_index 页目录中的条目索引
 * @param pde 新的页目录条目值
 */
#define SET_PDE(ptd, pde_index, pde)                 *((ptd_t*)ptd + pde_index) = pde;

/**
 * @brief 设置页表中的某一项 为新建页表项(NEW_L2_ENTRY) pte
 * 
 * @param ptd 页目录的起始地址
 * @param pt_index 页表索引，用于确定页目录中的具体页表
 * @param pte_index 页表中的条目索引
 * @param pte 新的页表条目值
 */
#define SET_PTE(ptd, pt_index, pte_index, pte)       *(PT_ADDR(ptd, pt_index) + pte_index) = pte;

// 获得 符号(变量？) 的 地址
#define sym_val(sym)                                 (uintptr_t)(&sym)

//计算内核和hhk_init所需的页表数量
#define KERNEL_PAGE_COUNT           ((sym_val(__kernel_end) - sym_val(__kernel_start) + 0x1000 - 1) >> 12);
#define HHK_PAGE_COUNT              ((sym_val(__init_hhk_end) - 0x100000 + 0x1000 - 1) >> 12)

//-----------------------------定义页目录中的索引
// use table #1
#define PG_TABLE_IDENTITY           0

// use table #2-4
// hence the max size of kernel is 8MiB
#define PG_TABLE_KERNEL             1   //内核页表的start地址，后续会有循环递增 直到第四个

// use table #5
#define PG_TABLE_STACK              4

// Provided by linker (see linker.ld)
extern uint8_t __kernel_start;
extern uint8_t __kernel_end;
extern uint8_t __init_hhk_end;
extern uint8_t _k_stack;

void 
_init_page(ptd_t* ptd) {
    SET_PDE(ptd, 0, NEW_L1_ENTRY(PG_PRESENT, ptd + PG_MAX_ENTRIES))
    
    // 对低1MiB空间进行对等映射（Identity mapping），也包括了我们的VGA，方便内核操作。
    for (uint32_t i = 0; i < 256; i++)
    {
        SET_PTE(ptd, PG_TABLE_IDENTITY, i, NEW_L2_ENTRY(PG_PREM_RW, (i << PG_SIZE_BITS)))//PG_TABLE_IDENTITY 应该 对应着SET_PDE中的0
    }

    // 对等映射我们的hhk_init，这样一来，当分页与地址转换开启后，我们依然能够照常执行最终的 jmp 指令来跳转至
    //  内核的入口点
    for (uint32_t i = 0; i < HHK_PAGE_COUNT; i++)
    {
        SET_PTE(ptd, PG_TABLE_IDENTITY, 256 + i, NEW_L2_ENTRY(PG_PREM_RW, 0x100000 + (i << PG_SIZE_BITS)))
    }
    
    // --- 将内核重映射至高半区 ---
    
    // 这里是一些计算，主要是计算应当映射进的 页目录 与 页表 的条目索引（Entry Index）
    uint32_t kernel_pde_index = L1_INDEX(sym_val(__kernel_start));
    uint32_t kernel_pte_index = L2_INDEX(sym_val(__kernel_start));
    uint32_t kernel_pg_counts = KERNEL_PAGE_COUNT;
    
    // 将内核所需要的页表注册进页目录
    //  当然，就现在而言，我们的内核只占用不到50个页（每个页表包含1024个页）
    //  这里分配了3个页表（12MiB），未雨绸缪。
    for (uint32_t i = 0; i < PG_TABLE_STACK - PG_TABLE_KERNEL; i++)
    {
        SET_PDE(
            ptd, 
            kernel_pde_index + i,   
            NEW_L1_ENTRY(PG_PREM_RW, PT_ADDR(ptd, PG_TABLE_KERNEL + i))
        )
    }
    
    // 首先，检查内核的大小是否可以fit进我们这几个表（12MiB）
    if (kernel_pg_counts > (PG_TABLE_STACK - PG_TABLE_KERNEL) * PG_MAX_ENTRIES) {
        // ERROR: require more pages
        //  here should do something else other than head into blocking
        while (1);
    }
    
    // 计算内核.text段的物理地址
    uintptr_t kernel_pm = V2P(&__kernel_start);
    
    // 重映射内核至高半区地址（>=0xC0000000）
    for (uint32_t i = 0; i < kernel_pg_counts; i++)
    {
        SET_PTE(
            ptd, 
            PG_TABLE_KERNEL, 
            kernel_pte_index + i, 
            NEW_L2_ENTRY(PG_PREM_RW, kernel_pm + (i << PG_SIZE_BITS))
        )
    }

    // 最后一个entry用于循环映射
    SET_PDE(
        ptd,
        PG_MAX_ENTRIES - 1,
        NEW_L1_ENTRY(T_SELF_REF_PERM, ptd)
    );
}

uint32_t __save_subset(uint8_t* destination, uint8_t* base, unsigned int size) {
    unsigned int i = 0;
    for (; i < size; i++)
    {
        *(destination + i) = *(base + i);
    }
    return i;
}

void 
_save_multiboot_info(multiboot_info_t* info, uint8_t* destination) {
    uint32_t current = 0;
    uint8_t* info_b = (uint8_t*) info;
    for (; current < sizeof(multiboot_info_t); current++)
    {
        *(destination + current) = *(info_b + current);
    }

    ((multiboot_info_t*) destination)->mmap_addr = (uintptr_t)destination + current;
    current += __save_subset(destination + current, (uint8_t*)info->mmap_addr, info->mmap_length);

    if (present(info->flags, MULTIBOOT_INFO_DRIVE_INFO)) {
        ((multiboot_info_t*) destination)->drives_addr = (uintptr_t)destination + current;
        current += __save_subset(destination + current, (uint8_t*)info->drives_addr, info->drives_length);
    }
}

void 
_hhk_init(ptd_t* ptd, uint32_t kpg_size) {

    // ptd 为 页表目录 地址

    // 初始化 kpg 全为0
    //      P.s. 真没想到GRUB会在这里留下一堆垃圾！ 老子的页表全乱套了！
    uint8_t* kpg = (uint8_t*) ptd;
    for (uint32_t i = 0; i < kpg_size; i++)
    {
        *(kpg + i) = 0; // 清空ptd每一项
    }
    
    _init_page(ptd);
}
