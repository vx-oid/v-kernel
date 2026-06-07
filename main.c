// --- Hardware Registers ---
#define TIMG0_WDT_PROTECT (*(volatile unsigned int *)(0x6001F064))
#define TIMG0_WDT_CONFIG0 (*(volatile unsigned int *)(0x6001F048))
#define RTC_WDT_PROTECT   (*(volatile unsigned int *)(0x600080B0))
#define RTC_WDT_CONFIG0   (*(volatile unsigned int *)(0x600080A8))
#define USB_SERIAL_JTAG_TX       (*(volatile unsigned int *)(0x60043000))
#define USB_SERIAL_JTAG_EP1_CONF (*(volatile unsigned int *)(0x60043004))

extern void trap_vector();

/* Forward declarations for timer/systick symbols defined later. */
extern volatile unsigned int systicks;
void clear_timer_interrupt(void);
void timer_init(unsigned int dummy_frequency_divider);

#include "include/syscall.h"

// Syscall dispatcher implemented in src/syscall.c
unsigned int syscall_dispatch(unsigned int num, unsigned int *args);

/* Simple inline syscall wrapper for one-arg syscalls used in demos. */
static inline unsigned int call_sys(unsigned int num, unsigned int a0) {
    register unsigned int _a0 asm("a0") = a0;
    register unsigned int _a7 asm("a7") = num;
    asm volatile ("ecall" : "+r"(_a0) : "r"(_a7) : "memory");
    return _a0;
}

// --- Hardware Abstraction Layer (HAL) ---
void v_print(const char *str) {
    while (*str) {
        if (*str == '\n') {
            // Wait until Bit 1 (0x02) is 1 (meaning buffer is free)
            while ((USB_SERIAL_JTAG_EP1_CONF & 0x02) == 0);
            USB_SERIAL_JTAG_TX = '\r';
        }

        // Wait until buffer is free
        while ((USB_SERIAL_JTAG_EP1_CONF & 0x02) == 0);
        USB_SERIAL_JTAG_TX = *str;

        str++;
    }
    // Set Bit 0 (0x01) to 1 to force the hardware to flush the data to the Mac
    USB_SERIAL_JTAG_EP1_CONF |= 0x01;
}

void v_print_hex(unsigned int val) {
    v_print("0x");
    for (int i = 28; i >= 0; i -= 4) {
        int nibble = (val >> i) & 0xF;

        // Wait until buffer is free
        while ((USB_SERIAL_JTAG_EP1_CONF & 0x02) == 0);

        if (nibble < 10) {
            USB_SERIAL_JTAG_TX = '0' + nibble;
        } else {
            USB_SERIAL_JTAG_TX = 'A' + (nibble - 10);
        }
    }
    // Flush the data
    USB_SERIAL_JTAG_EP1_CONF |= 0x01;
}

// --- The C Trap Handler ---
static void decode_mcause(unsigned int mcause);

void c_trap_handler(unsigned int *saved) {
    v_print("\n================================\n");
    v_print("  >>> CPU TRAP INTERCEPTED <<<  \n");

    /* Read CSRs: mepc, mcause, mtval, mstatus, mtvec */
    unsigned int mepc, mcause, mtval, mstatus, mtvec;
    asm volatile ("csrr %0, mepc" : "=r" (mepc));
    asm volatile ("csrr %0, mcause" : "=r" (mcause));
    asm volatile ("csrr %0, mtval" : "=r" (mtval));
    asm volatile ("csrr %0, mstatus" : "=r" (mstatus));
    asm volatile ("csrr %0, mtvec" : "=r" (mtvec));

    v_print("Trapped at Address: ");
    v_print_hex(mepc);
    v_print("\nmcause: ");
    v_print_hex(mcause);
    /* Print human-readable mcause description */
    decode_mcause(mcause);
    v_print("\nmtval: ");
    v_print_hex(mtval);
    v_print("\nmstatus: ");
    v_print_hex(mstatus);
    v_print("\nmtvec: ");
    v_print_hex(mtvec);
    v_print("\n================================\n");

    /* Dump the saved caller-saved registers from the frame passed in a0 */
    if (saved != 0) {
        v_print("Saved registers (from trap frame):\n");
        v_print(" ra="); v_print_hex(saved[0]);
        v_print(" t0="); v_print_hex(saved[1]);
        v_print(" t1="); v_print_hex(saved[2]);
        v_print(" t2="); v_print_hex(saved[3]);
        v_print(" a0="); v_print_hex(saved[4]);
        v_print(" a1="); v_print_hex(saved[5]);
        v_print(" a2="); v_print_hex(saved[6]);
        v_print(" a3="); v_print_hex(saved[7]);
        v_print(" a4="); v_print_hex(saved[8]);
        v_print(" a5="); v_print_hex(saved[9]);
        v_print(" a6="); v_print_hex(saved[10]);
        v_print(" a7="); v_print_hex(saved[11]);
        v_print(" t3="); v_print_hex(saved[12]);
        v_print(" t4="); v_print_hex(saved[13]);
        v_print(" t5="); v_print_hex(saved[14]);
        v_print(" t6="); v_print_hex(saved[15]);
        v_print("\n");
    }

    /* Only advance mepc for ECALLs (U-mode=8, S-mode=9, M-mode=11). */
    unsigned int is_interrupt = (mcause >> 31) & 1u;
    unsigned int cause = mcause & 0x7fffffffu;

    /* Handle machine-timer interrupt (interrupt && cause==7) as a systick. */
    if (is_interrupt && cause == 7) {
        systicks++;
        clear_timer_interrupt();
        return; /* return to restore regs and mret */
    }

    /* Some SoC timer peripherals raise external interrupts (cause 11).
       Treat external interrupts as potential TIMG events and handle them
       here by clearing the TIMG source and updating `systicks`. */
    if (is_interrupt && cause == 11) {
        /* Clear TIMG interrupt if pending and increment systicks. */
        /* Note: clear_timer_interrupt() is idempotent for non-TIMG interrupts. */
        systicks++;
        clear_timer_interrupt();
        return;
    }

    if (!is_interrupt && (cause == 8 || cause == 9 || cause == 11)) {
        if (cause == 11) {
            /* Environment call (ECALL) from M-mode: dispatch syscall.
               The saved frame contains registers: saved[4]..saved[10] == a0..a6
               and saved[11] == a7 (syscall number). */
            if (saved) {
                unsigned int syscall_num = saved[11];
                unsigned int args[7];
                for (int i = 0; i < 7; i++) args[i] = saved[4 + i];
                unsigned int ret = syscall_dispatch(syscall_num, args);
                /* Return value in a0 (saved[4]) */
                saved[4] = ret;
            }
            mepc += 4;
            asm volatile ("csrw mepc, %0" : : "r" (mepc));
            return; /* return to assembly which will restore regs and mret */
        }

        mepc += 4;
        asm volatile ("csrw mepc, %0" : : "r" (mepc));
        return; /* return to assembly which will restore regs and mret */
    }

    /* For unexpected traps, print and halt to avoid returning into a fault. */
    v_print("Unhandled trap, halting.\n");
    while (1);
}

static void decode_mcause(unsigned int mcause) {
    unsigned int is_interrupt = (mcause >> 31) & 1u;
    unsigned int code = mcause & 0x7fffffffu;
    if (is_interrupt) {
        v_print("\nInterrupt: ");
        switch (code) {
            case 3: v_print("Machine software interrupt\n"); break;
            case 7: v_print("Machine timer interrupt\n"); break;
            case 11: v_print("Machine external interrupt\n"); break;
            default: v_print("Unknown interrupt\n"); break;
        }
    } else {
        v_print("\nException: ");
        switch (code) {
            case 0: v_print("Instruction address misaligned\n"); break;
            case 1: v_print("Instruction access fault\n"); break;
            case 2: v_print("Illegal instruction\n"); break;
            case 3: v_print("Breakpoint\n"); break;
            case 4: v_print("Load address misaligned\n"); break;
            case 5: v_print("Load access fault\n"); break;
            case 6: v_print("Store/AMO address misaligned\n"); break;
            case 7: v_print("Store/AMO access fault\n"); break;
            case 8: v_print("Environment call from U-mode\n"); break;
            case 9: v_print("Environment call from S-mode\n"); break;
            case 11: v_print("Environment call from M-mode\n"); break;
            case 12: v_print("Instruction page fault\n"); break;
            case 13: v_print("Load page fault\n"); break;
            case 15: v_print("Store/AMO page fault\n"); break;
            default: v_print("Unknown exception\n"); break;
        }
    }
}

int my_global_variable = 42;

volatile unsigned int systicks = 0;
volatile unsigned int timer_delta = 0;
volatile unsigned long long next_tick_alarm = 0;

static unsigned long long read_timg0_counter(void) {
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

static void read_and_print_timg_status(void) {
    volatile unsigned int *base = (volatile unsigned int *)0x6001F000;
    volatile unsigned int *int_ena = base + (0x0070/4);
    volatile unsigned int *int_raw = base + (0x0074/4);
    volatile unsigned int *int_st  = base + (0x0078/4);

    v_print(" TIMG: int_raw="); v_print_hex(*int_raw);
    v_print(" int_st="); v_print_hex(*int_st);
    v_print(" int_ena="); v_print_hex(*int_ena);
}

static void read_and_print_timg_regs(void) {
    volatile unsigned int *base = (volatile unsigned int *)0x6001F000;
    volatile unsigned int *t0cfg = base + (0x0000/4);
    volatile unsigned int *t0lo = base + (0x0010/4);
    volatile unsigned int *t0hi = base + (0x0014/4);
    volatile unsigned int *t0lo_read = base + (0x0004/4);
    volatile unsigned int *t0hi_read = base + (0x0008/4);

    v_print(" T0CFG="); v_print_hex(*t0cfg);
    v_print(" T0ALARMLO="); v_print_hex(*t0lo);
    v_print(" T0ALARMHI="); v_print_hex(*t0hi);
    /* Latch and read current counter */
    volatile unsigned int *t0update = base + (0x000C/4);
    *t0update = 1u;
    while ((*t0update & 1u) != 0);
    v_print(" T0LO="); v_print_hex(*t0lo_read);
    v_print(" T0HI="); v_print_hex(*t0hi_read);
}

/* Platform-specific: clear the timer interrupt source. Implement if needed. */
void clear_timer_interrupt(void) {
    /* Clear TIMG timer 0 interrupt (write-1-to-clear), then re-arm next alarm.
       This handles hardware that doesn't auto-reload the alarm. */
    volatile unsigned int *base = (volatile unsigned int *)0x6001F000;
    volatile unsigned int *timg_int_clr = base + (0x007C/4);
    volatile unsigned int *t0update = base + (0x000C/4);
    volatile unsigned int *t0lo_read = base + (0x0004/4);
    volatile unsigned int *t0hi_read = base + (0x0008/4);
    volatile unsigned int *t0lo = base + (0x0010/4);
    volatile unsigned int *t0hi = base + (0x0014/4);

    /* Clear interrupt source first */
    *timg_int_clr = 1u;

    /* Latch current counter value */
    *t0update = 1u;
    while ((*t0update & 1u) != 0);

    unsigned int lo = *t0lo_read;
    unsigned int hi = *t0hi_read;
    unsigned long long cur = ((unsigned long long)hi << 32) | (unsigned long long)lo;
    unsigned long long alarm = cur + (unsigned long long)timer_delta;

    *t0lo = (unsigned int)(alarm & 0xFFFFFFFFu);
    *t0hi = (unsigned int)(alarm >> 32);
}

/* Minimal timer init stub: user should program the timer/compare to generate
   machine-timer interrupts. This stub enables MTIE and global MIE so the
   machine-timer interrupt (if configured) will fire. */
void timer_init(unsigned int dummy_frequency_divider) {
    (void)dummy_frequency_divider;
    /* Enable TIMG0 register clock so software can access timer registers. */
    volatile unsigned int *timg_regclk = (volatile unsigned int *)(0x6001F000 + 0x00FC);
    /* TIMG_TIMER_CLK_IS_ACTIVE is bit 30 in TIMG_REGCLK_REG */
    *timg_regclk |= (1u << 30);

    /* Enable Timer0 interrupt mask in TIMG_INT_ENA_TIMERS_REG (bit0) */
    volatile unsigned int *timg_int_ena = (volatile unsigned int *)(0x6001F000 + 0x0070);
    *timg_int_ena |= 1u;

    /* Enable global interrupts (mstatus.MIE bit = 3). Avoid writing `mie`. */
    asm volatile ("csrrs zero, mstatus, %0" :: "r" (1u << 3));
}

/* Full TIMG0 T0 setup: latch current counter, program alarm = now+delta, enable autoreload and start. */
void timer_init_periodic(unsigned int delta_ticks) {
    timer_delta = delta_ticks;
    volatile unsigned int *base = (volatile unsigned int *)0x6001F000;
    volatile unsigned int *t0cfg = base + (0x0000/4);
    volatile unsigned int *t0lo = base + (0x0010/4);
    volatile unsigned int *t0hi = base + (0x0014/4);
    volatile unsigned int *t0update = base + (0x000C/4);
    volatile unsigned int *t0lo_read = base + (0x0004/4);
    volatile unsigned int *t0hi_read = base + (0x0008/4);
    volatile unsigned int *timg_regclk = base + (0x00FC/4);
    volatile unsigned int *timg_int_ena = base + (0x0070/4);

    /* Ensure TIMG0 clock is enabled */
    *timg_regclk |= (1u << 30);

    /* Disable timer while configuring */
    *t0cfg &= ~(1u << 31);

    /* Use APB clock with a prescaler of 80 so the timer advances at 1 MHz. */
    *t0cfg &= ~(((1u << 16) - 1u) << 13);
    *t0cfg |= (80u << 13);
    *t0cfg |= (1u << 12); /* reset divider */

    /* Latch current counter value */
    *t0update = 1u;
    /* Wait until hardware clears the update bit */
    while ((*t0update & 1u) != 0);

    unsigned int lo = *t0lo_read;
    unsigned int hi = *t0hi_read;
    unsigned long long cur = ((unsigned long long)hi << 32) | (unsigned long long)lo;
    unsigned long long alarm = cur + (unsigned long long)delta_ticks;

    *t0lo = (unsigned int)(alarm & 0xFFFFFFFFu);
    *t0hi = (unsigned int)(alarm >> 32);

     /* Record software next-tick alarm so polling fallback can work if IRQs
         are not delivered. */
     next_tick_alarm = alarm;

    /* Enable TIMG0 interrupt mask */
    *timg_int_ena |= 1u;

    /* Configure: set increase (bit30), autoreload (bit29), alarm_en (bit11), enable (bit31) */
    unsigned int cfg = *t0cfg;
    cfg |= (1u << 30); /* INCREASE */
    cfg |= (1u << 29); /* AUTORELOAD */
    cfg |= (1u << 11); /* ALARM_EN */
    cfg |= (1u << 31); /* EN */
    *t0cfg = cfg;
}

// --- OS Entry Point ---
void main() {
    TIMG0_WDT_PROTECT = 0x50D83AA1;
    TIMG0_WDT_CONFIG0 = 0;
    RTC_WDT_PROTECT = 0x50D83AA1;
    RTC_WDT_CONFIG0 = 0;

    // Startup delay to give terminal time to connect
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

    // Configure the Trap Vector
    /* Attempt to set mtvec in direct mode (clear low two bits). If the
       implementation/boot ROM forces vectored mode, accept it and continue
       without noisy retries. */
    unsigned int mtvec_val = ((unsigned int)trap_vector) & ~0x3u;
    asm volatile ("csrw mtvec, %0" :: "r" (mtvec_val));
    unsigned int mtvec_readback;
    asm volatile ("csrr %0, mtvec" : "=r" (mtvec_readback));
    if ((mtvec_readback & 0x3u) == 0) {
        v_print("mtvec CSR configured (direct mode).\n");
    } else {
        v_print("mtvec configured but in vectored mode (continuing).\n");
    }

    // Start periodic TIMG0 timer (delta ticks). Use smaller test delta.
    timer_init_periodic(100000u);

     /* Enable global interrupts (mstatus.MIE) safely. Avoid writing `mie` which
         may be unavailable on this core implementation (causes illegal instr). */
     asm volatile ("csrrs x0, mstatus, %0" :: "r" (1u << 3));

    // Trigger a manual Exception
    v_print("Pressing the RISC-V panic button (ecall)...\n");
    asm volatile ("ecall");

    // If mret in boot.S works, the CPU will resume execution right here!
    v_print("\n>>> SURVIVED THE TRAP! Back in main loop. <<<\n\n");

    /* Demo: syscall usage */
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

        /* Polling fallback: if interrupts are not delivered, advance systicks
           in software when the TIMG counter reaches the next scheduled alarm. */
        if (timer_delta != 0 && next_tick_alarm != 0 && current_counter >= next_tick_alarm) {
            systicks++;
            /* advance the software alarm */
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
            /* Print TIMG interrupt registers and alarm/counter for debugging */
            read_and_print_timg_status();
            read_and_print_timg_regs();
            v_print("\n");
        }

        for (volatile int i = 0; i < 200000; i++);
    }
}