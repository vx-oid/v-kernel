#include "syscall.h"
#include "timer.h"

extern void v_print(const char *s);

unsigned int syscall_dispatch(unsigned int num, unsigned int *args) {
    switch (num) {
        case SYS_PRINT: {
            const char *s = (const char *)args[0];
            if (s) v_print(s);
            return 0;
        }
        case SYS_TIME: {
            unsigned long long t = read_timg0_counter();
            return (unsigned int)(t & 0xFFFFFFFFu);
        }
        case SYS_SLEEP_MS: {
            unsigned int ms = args[0];
            unsigned long long start = read_timg0_counter();
            unsigned long long target = start + (unsigned long long)ms * 1000ull;
            while (read_timg0_counter() < target) {
                ;
            }
            return 0;
        }
        default:
            return (unsigned int)-1;
    }
}
