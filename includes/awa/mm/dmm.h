#ifndef __AWA_DMM_H
#define __AWA_DMM_H
// Dynamic Memory (i.e., heap) Manager

#include <stddef.h>

void
lxsbrk(void* current, void* next);

void
lxmalloc(size_t size);

void
lxfree(size_t size);

#endif
