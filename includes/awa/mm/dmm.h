#ifndef __AWA_DMM_H
#define __AWA_DMM_H
// Dynamic Memory (i.e., heap) Manager
// 动态内存管理

#include <stddef.h>

#define M_ALLOCATED 0x1     //当前内存块已分配
#define M_PREV_FREE 0x2     //前一个内存块未分配

#define M_NOT_ALLOCATED 0x0 //当前内存块未分配
#define M_PREV_ALLOCATED 0x0//前一个内存块已分配

#define CHUNK_S(header) ((header) & ~0x3)           //提取内存块的大小
#define CHUNK_PF(header) ((header)&M_PREV_FREE)     //判断 前一个内存块 是否 已分配
#define CHUNK_A(header) ((header)&M_ALLOCATED)      //判断 当前内存块 是否 已分配

#define PACK(size, flags) (((size) & ~0x3) | (flags))   //打包 内存块大小 和 分配标志 (也就是往内存低位写入flags)

#define SW(p, w) (*((uint32_t*)(p)) = w)            //将w写入p
#define LW(p) (*((uint32_t*)(p)))                   //从地址p中读取一个uint32_t(32位整数)

#define HPTR(bp) ((uint32_t*)(bp)-1)                //获取(返回) 内存块bp的header指针
#define BPTR(bp) ((uint8_t*)(bp) + WSIZE)           //获取(返回) 内存块bp的 起始 指针
#define FPTR(hp, size) ((uint32_t*)(hp + size - WSIZE))//获取(返回) 内存块bp的footer指针
#define NEXT_CHK(hp) ((uint8_t*)(hp) + CHUNK_S(LW(hp)))//获取(返回) 内存块bp的下一个内存块的 起始位置

#define BOUNDARY 4  //内存块边界
#define WSIZE 4     //内存块大小(单位: 字节)

#define HEAP_INIT_SIZE 4096 //堆初始大小

//存储 堆 的相关信息
typedef struct 
{
    void* start;    // 堆 的起始地址
    void* brk;      // 当前 堆 的末端地址
    void* max_addr; // 堆 的最大可使用地址
} heap_context_t;


int
dmm_init(heap_context_t* heap); //初始化 堆

int
lxsbrk(heap_context_t* heap, void* addr);   //扩展 堆 的末端地址
void*
lxbrk(heap_context_t* heap, size_t size);   //分配内存并更新 堆 的末端地址

void*
lx_malloc_internal(heap_context_t* heap, size_t size); //内部分配内存

void
lx_free_internal(void* ptr);    //释放内存

#endif
