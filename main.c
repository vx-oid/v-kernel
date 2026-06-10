#include "hal.h"
#include "timer.h"
#include "trap.h"
#include "syscall.h"

#define TIMG0_WDT_PROTECT (*(volatile unsigned int *)(0x6001F064))
#define TIMG0_WDT_CONFIG0 (*(volatile unsigned int *)(0x6001F048))
#define RTC_WDT_PROTECT   (*(volatile unsigned int *)(0x600080B0))
#define RTC_WDT_CONFIG0   (*(volatile unsigned int *)(0x600080A8))

extern void trap_vector();

static inline unsigned int call_sys(unsigned int num, unsigned int a0) {
    register unsigned int _a0 asm("a0") = a0;
    register unsigned int _a7 asm("a7") = num;
    asm volatile ("ecall" : "+r"(_a0) : "r"(_a7) : "memory");
    return _a0;
}

int my_global_variable = 42;

void main() {
    TIMG0_WDT_PROTECT = 0x50D83AA1;
    TIMG0_WDT_CONFIG0 = 0;
    RTC_WDT_PROTECT = 0x50D83AA1;
    RTC_WDT_CONFIG0 = 0;

    for (volatile int i = 0; i < 4000000; i++);

    v_print("\x1B[2J\x1B[H");
    v_print("================================\n");
    v_print("      V-Kernel Diagnostics      \n");
    v_print("================================\n\n");

    v_print("main() function is at: ");
    v_print_hex((unsigned int)&main);
    v_print("\n");

    v_print("my_global_variable is at: ");
    v_print_hex((unsigned int)&my_global_variable);
    v_print("\n\n");

    unsigned int mtvec_val = ((unsigned int)trap_vector) & ~0x3u;
    asm volatile ("csrw mtvec, %0" :: "r" (mtvec_val));
    unsigned int mtvec_readback;
    asm volatile ("csrr %0, mtvec" : "=r" (mtvec_readback));
    if ((mtvec_readback & 0x3u) == 0) {
        v_print("mtvec CSR configured (direct mode).\n");
    } else {
        v_print("mtvec configured but in vectored mode (continuing).\n");
    }

    timer_init_periodic(100000u);

    asm volatile ("csrrs x0, mstatus, %0" :: "r" (1u << 3));

    v_print("Pressing the RISC-V panic button (ecall)...\n");
    asm volatile ("ecall");

    v_print("\n>>> SURVIVED THE TRAP! Back in main loop. <<<\n\n");

    const char demo_msg[] = "Hello from syscall SYS_PRINT!\n";
    call_sys(SYS_PRINT, (unsigned int)demo_msg);

    unsigned int t = call_sys(SYS_TIME, 0);
    v_print("Time low32: ");
    v_print_hex(t);
    v_print("\n");

    v_print("Sleeping 100 ms via SYS_SLEEP_MS...\n");
    call_sys(SYS_SLEEP_MS, 100);
    v_print("Woke up\n");

    unsigned long long last_counter = read_timg0_counter();
    unsigned int visible_ticks = 0;

    while (1) {
        unsigned long long current_counter = read_timg0_counter();

        if (timer_delta != 0 && next_tick_alarm != 0 && current_counter >= next_tick_alarm) {
            systicks++;
            next_tick_alarm += (unsigned long long)timer_delta;
        }

        while ((current_counter - last_counter) >= 1000000ull) {
            last_counter += 1000000ull;
            visible_ticks++;
        }

        if (visible_ticks != 0) {
            v_print("System stable... ticks=");
            v_print_hex(visible_ticks);
            v_print(" counter=");
            v_print_hex((unsigned int)current_counter);
            v_print(" irq_ticks=");
            v_print_hex(systicks);
            read_and_print_timg_status();
            read_and_print_timg_regs();
            v_print("\n");
        }

        for (volatile int i = 0; i < 200000; i++);
    }
}
