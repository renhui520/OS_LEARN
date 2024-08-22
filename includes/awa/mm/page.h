#ifndef __AWA_PAGE_H
#define __AWA_PAGE_H
#include <stdint.h>
#include <awa/common.h>

#define PG_SIZE_BITS                12                      // 12位 即页大小为4KB
#define PG_SIZE                     (1 << PG_SIZE_BITS)     // 4KB : (1 << 12)= 1 * 2^12
#define PG_INDEX_BITS               10                      // 10位 页索引的比特位数

#define PG_MAX_ENTRIES              1024U                   // 1024个最大页表项
#define PG_LAST_TABLE               PG_MAX_ENTRIES - 1      // 最后一个页表项的索引
#define PG_FIRST_TABLE              0                       // 第一个页表项的索引

#define PTE_NULL                    0                       // 页表项为空 或 无效

#define P2V(paddr)          ((uintptr_t)(paddr)  +  HIGHER_HLF_BASE)    // 将 物理地址 转换为 虚拟地址
#define V2P(vaddr)          ((uintptr_t)(vaddr)  -  HIGHER_HLF_BASE)    // 将 虚拟地址 转换为 物理地址

//0xFFFFF000UL在二进制表示中为 1111111111111111111111111111111100000000
//最高20位保持不变 最低12位被清零   4KB对齐?
#define PG_ALIGN(addr)      ((uintptr_t)(addr)   & 0xFFFFF000UL)        // 将地址转换为页对齐的地址

#define L1_INDEX(vaddr)     (uint32_t)(((uintptr_t)(vaddr) & 0xFFC00000UL) >> 22)   // 获取页目录索引 pde?
#define L2_INDEX(vaddr)     (uint32_t)(((uintptr_t)(vaddr) & 0x003FF000UL) >> 12)   // 获取页表索引 pte?
#define PG_OFFSET(vaddr)    (uint32_t)((uintptr_t)(vaddr)  & 0x00000FFFUL)        // 获取页内偏移

#define GET_PT_ADDR(pde)    PG_ALIGN(pde)   // 获取页目录地址
#define GET_PG_ADDR(pte)    PG_ALIGN(pte)   // 获取页表地址

#define PG_DIRTY(pte)           ((pte & (1 << 6)) >> 6) // 获取页表是否被修改   获取修改(脏？)位
#define PG_ACCESSED(pte)        ((pte & (1 << 5)) >> 5) // 获取页表是否被访问   获取访问位

#define IS_CACHED(entry)    ((entry & 0x1)) // 获取页表是否被缓存

#define PG_PRESENT              (0x1)       // 页表 [被映射][存在]
#define PG_WRITE                (0x1 << 1)  // 页表 [可写]
#define PG_ALLOW_USER           (0x1 << 2)  // 页表 [允许用户访问]
#define PG_WRITE_THROUGHT       (1 << 3)    // 页表 [写回缓存]
#define PG_DISABLE_CACHE        (1 << 4)    // 页表 [禁用缓存]
#define PG_PDE_4MB              (1 << 7)    // 页表 [4MB]

#define NEW_L1_ENTRY(flags, pt_addr)     (PG_ALIGN(pt_addr) | ((flags) & 0xfff))    // 新建页目录项 pde
#define NEW_L2_ENTRY(flags, pg_addr)     (PG_ALIGN(pg_addr) | ((flags) & 0xfff))    // 新建页表项 pte

#define V_ADDR(pd, pt, offset)  ((pd) << 22 | (pt) << 12 | (offset)) // 获取虚拟地址
#define P_ADDR(ppn, offset)     ((ppn << 12) | (offset)) // 获取物理地址

#define PG_ENTRY_FLAGS(entry)   (entry & 0xFFFU) // 获取页表属性
#define PG_ENTRY_ADDR(entry)   (entry & ~0xFFFU) // 获取页表地址

#define HAS_FLAGS(entry, flags)             ((PG_ENTRY_FLAGS(entry) & (flags)) == flags) // 判断页表是否包含 flags中所有的属性
#define CONTAINS_FLAGS(entry, flags)        (PG_ENTRY_FLAGS(entry) & (flags)) // 判断页表是否包含 至少flags中的一个属性

#define PG_PREM_R              PG_PRESENT                               // 页表  [存在]
#define PG_PREM_RW             PG_PRESENT | PG_WRITE                    // 页表  [可写]
#define PG_PREM_UR             PG_PRESENT | PG_ALLOW_USER               // 页表  [允许用户访问]
#define PG_PREM_URW            PG_PRESENT | PG_WRITE | PG_ALLOW_USER    // 页表  [允许用户读写]

// 用于对PD进行循环映射，因为我们可能需要对PD进行频繁操作，我们在这里禁用TLB缓存
#define T_SELF_REF_PERM        PG_PREM_RW | PG_DISABLE_CACHE


// 页目录的虚拟基地址，可以用来访问到各个PDE
#define L1_BASE_VADDR                0xFFFFF000U

// 页表的虚拟基地址，可以用来访问到各个PTE
#define L2_BASE_VADDR                 0xFFC00000U

// 用来获取特定的页表的虚拟地址
#define L2_VADDR(pd_offset)           (L2_BASE_VADDR | (pd_offset << 12))

typedef unsigned long ptd_t;
typedef unsigned long pt_t;
typedef unsigned int pt_attr;

/**
 * @brief 虚拟映射属性
 * 
 */
typedef struct {
    // 物理页码（如果不存在映射，则为0）
    uint32_t pn;
    // 物理页地址（如果不存在映射，则为0）
    uintptr_t pa;
    // 映射的flags
    uint16_t flags;
} v_mapping;

typedef uint32_t x86_pte_t;
typedef struct
{
    x86_pte_t entry[PG_MAX_ENTRIES]; // 页表项
} __attribute__((packed)) x86_page_table;



#endif
