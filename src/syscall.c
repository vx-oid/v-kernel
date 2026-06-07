#include <stdint.h>
#include "../include/syscall.h"

extern void v_print(const char *s);

static unsigned long long read_timg0_counter64(void) {
    volatile unsigned int *base = (volatile unsigned int *)0x6001F000;
    volatile unsigned int *t0update = base + (0x000C / 4);
    volatile unsigned int *t0lo_read = base + (0x0004 / 4);
    volatile unsigned int *t0hi_read = base + (0x0008 / 4);

    *t0update = 1u;
    while ((*t0update & 1u) != 0);

    unsigned long long lo = (unsigned long long)(*t0lo_read);
    unsigned long long hi = (unsigned long long)(*t0hi_read);
    return (hi << 32) | lo;
}

unsigned int syscall_dispatch(unsigned int num, unsigned int *args) {
    switch (num) {
        case SYS_PRINT: {
            const char *s = (const char *)args[0];
            if (s) v_print(s);
            return 0;
        }
        case SYS_TIME: {
            unsigned long long t = read_timg0_counter64();
            return (unsigned int)(t & 0xFFFFFFFFu);
        }
        case SYS_SLEEP_MS: {
            unsigned int ms = args[0];
            unsigned long long start = read_timg0_counter64();
            unsigned long long target = start + (unsigned long long)ms * 1000ull; // timer at 1MHz
            while (read_timg0_counter64() < target) {
                ;
            }
            return 0;
        }
        default:
            return (unsigned int)-1;
    }
}
