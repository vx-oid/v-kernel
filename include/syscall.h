#ifndef SYSCALL_H
#define SYSCALL_H

// Syscall numbers
#define SYS_PRINT 1
#define SYS_TIME  2
#define SYS_SLEEP_MS 3

// Dispatcher called from trap handler. `args` points to saved a0..a6.
unsigned int syscall_dispatch(unsigned int num, unsigned int *args);

#endif // SYSCALL_H
