#ifndef __AWA_SYSLOG_H
#define __AWA_SYSLOG_H

#include <stdarg.h>

#define _LEVEL_INFO  "0"
#define _LEVEL_WARN  "1"
#define _LEVEL_ERROR "2"

#define KINFO    "\x1b" _LEVEL_INFO
#define KWARN    "\x1b" _LEVEL_WARN
#define KERROR   "\x1b" _LEVEL_ERROR

//static使函数局部可见，当调用这个宏的时候会生成一个kprintf函数，且还会由于static避免与其他文件的kprintf名称冲突
#define LOG_MODULE(module)                  \
    static void kprintf(const char* fmt, ...) {    \
        va_list args;                       \
        va_start(args, fmt);                \
        __kprintf(module, fmt, args);       \
        va_end(args);                       \
    }

void
__kprintf(const char* component, const char* fmt, va_list args);


void
kprint_panic(const char* fmt, ...);

#endif
