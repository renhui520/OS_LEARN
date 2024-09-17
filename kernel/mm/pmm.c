#include <awa/mm/page.h>
#include <awa/mm/pmm.h>

//ppn("Physical Page Number") 即 物理页号
//标记 单个物理页(page)
#define MARK_PG_AUX_VAR(ppn)                                                   \
    uint32_t group = ppn / 8;                                                  \
    uint32_t msk = (0x80U >> (ppn % 8));                                       \
//0x80 ==> 1000 0000
//当ppn % 8 == 0时，即不需要位移，刚好在第八位
//直接使用即可

//ppn("Physical Page Number") 即 物理页号
//连续标记 多个物理页(chunk)
#define MARK_CHUNK_AUX_VAR(start_ppn, page_count)                              \
    uint32_t group = start_ppn / 8;                                            \
    uint32_t offset = start_ppn % 8;                                           \
    uint32_t group_count = (page_count + offset) / 8;                          \
    uint32_t remainder = (page_count + offset) % 8;                            \
    uint32_t leading_shifts =                                                  \
      (page_count + offset) < 8 ? page_count : 8 - offset;

/*
 * 位图: 用于标记 物理页 的 "状态"
 * 状态: 1. 空闲---------------------\____通常情况下只会用到这两种状态
 *      2. 已占用--------------------/
 *      3. 预留(高级情况下可能会用到)
*/

// 位图数组，用于记录 物理页 的 状态
static uint8_t pm_bitmap[PM_BMP_MAX_SIZE];

// 最大的物理页编号
static uintptr_t max_pg;

//  ... |xxxx xxxx |
//  ... |-->|


//标记 页 为 空闲
void
pmm_mark_page_free(uintptr_t ppn)
{
    MARK_PG_AUX_VAR(ppn)
    pm_bitmap[group] = pm_bitmap[group] & ~msk; // 标记为 空闲
}

//标记 页 为 已占用
void
pmm_mark_page_occupied(uintptr_t ppn)
{
    MARK_PG_AUX_VAR(ppn)
    pm_bitmap[group] = pm_bitmap[group] | msk;  // 标记为 已占用
}

//标记 块(多个页) 为 空闲
//ppn("Physical Page Number") 即 物理页号
void
pmm_mark_chunk_free(uintptr_t start_ppn, size_t page_count)
{
    MARK_CHUNK_AUX_VAR(start_ppn, page_count)

    // nasty bit level hacks but it reduce # of iterations.

    pm_bitmap[group] &= ~(((1U << leading_shifts) - 1) << (8 - offset - leading_shifts));

    group++;

    // prevent unsigned overflow
    for (uint32_t i = 0; group_count !=0 && i < group_count - 1; i++, group++) {
        pm_bitmap[group] = 0;
    }

    pm_bitmap[group] &=
      ~(((1U << (page_count > 8 ? remainder : 0)) - 1) << (8 - remainder));
}

//标记 块(多个页) 为 已占用
void
pmm_mark_chunk_occupied(uint32_t start_ppn, size_t page_count)
{
    MARK_CHUNK_AUX_VAR(start_ppn, page_count)

    pm_bitmap[group] |= (((1U << leading_shifts) - 1) << (8 - offset - leading_shifts));

    group++;

    // prevent unsigned overflow
    for (uint32_t i = 0; group_count !=0 && i < group_count - 1; i++, group++) {
        pm_bitmap[group] = 0xFFU;
    }

    pm_bitmap[group] |=
      (((1U << (page_count > 8 ? remainder : 0)) - 1) << (8 - remainder));
}

// 我们跳过位于0x0的页。我们不希望空指针是指向一个有效的内存空间。
#define LOOKUP_START 1

//页 指针 指向
size_t pg_lookup_ptr;

void
pmm_init(uintptr_t mem_upper_lim)
{
    max_pg = (PG_ALIGN(mem_upper_lim) >> 12);//对齐并将 字节 除以 2 ^ 12得到最大物理页编号

    pg_lookup_ptr = LOOKUP_START;

    // 标记所有物理页为 [已占用]
    for (size_t i = 0; i < PM_BMP_MAX_SIZE; i++) {
        pm_bitmap[i] = 0xFFU;//0xFF = 1111 1111
    }
}

void*
pmm_alloc_page()
{
    // Next fit approach. Maximize the throughput!
    uintptr_t good_page_found = (uintptr_t)NULL;
    size_t old_pg_ptr = pg_lookup_ptr;
    size_t upper_lim = max_pg;
    uint8_t chunk = 0;
    while (!good_page_found && pg_lookup_ptr < upper_lim) {
        chunk = pm_bitmap[pg_lookup_ptr >> 3];

        // skip the fully occupied chunk, reduce # of iterations
        if (chunk != 0xFFU) {
            for (size_t i = pg_lookup_ptr % 8; i < 8; i++, pg_lookup_ptr++) {
                if (!(chunk & (0x80U >> i))) {
                    pmm_mark_page_occupied(pg_lookup_ptr);
                    good_page_found = pg_lookup_ptr << 12;
                    break;
                }
            }
        } else {
            pg_lookup_ptr += 8;

            // We've searched the interval [old_pg_ptr, max_pg) but failed
            //   may be chances in [1, old_pg_ptr) ?
            // Let's find out!
            if (pg_lookup_ptr >= upper_lim && old_pg_ptr != LOOKUP_START) {
                upper_lim = old_pg_ptr;
                pg_lookup_ptr = LOOKUP_START;
                old_pg_ptr = LOOKUP_START;
            }
        }
    }
    return (void*)good_page_found;
}

int
pmm_free_page(void* page)
{
    // TODO: Add kernel reserved memory page check
    uint32_t pg = (uintptr_t)page >> 12;
    if (pg && pg < max_pg)
    {
        pmm_mark_page_free(pg);
        return 1;
    }
    return 0;
}
