#ifndef __AWA_CONSTANTS_H
#define __AWA_CONSTANTS_H

#include <stddef.h>

#define K_STACK_SIZE            (64 << 10)                              //内核栈大小为64KB
#define K_STACK_START           ((0xFFBFFFFFU - K_STACK_SIZE) + 1)      //内核栈起始地址
#define HIGHER_HLF_BASE         0xC0000000UL                            //高半段起始地址
#define MEM_1MB                 0x100000UL                              //1MB

#define VGA_BUFFER_VADDR        0xB0000000UL    // VGA缓冲区虚拟地址
#define VGA_BUFFER_PADDR        0xB8000UL       // VGA缓冲区物理地址
#define VGA_BUFFER_SIZE         4096            // VGA缓冲区大小

#define SYS_TIMER_FREQUENCY_HZ  2048            // 系统时钟频率


// From Linux kernel v2.6.0 <kernel.h:194>
/**
 * container_of - cast a member of a structure out to the containing structure
 *
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({			            \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
        (type *)( (char *)__mptr - offsetof(type,member) );})

#endif
