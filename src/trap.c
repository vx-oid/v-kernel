#include "hal.h"
#include "timer.h"
#include "syscall.h"
#include "trap.h"

static void decode_mcause(unsigned int mcause);

void c_trap_handler(unsigned int *saved) {
    v_print("\n================================\n");
    v_print("  >>> CPU TRAP INTERCEPTED <<<  \n");

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
    decode_mcause(mcause);
    v_print("\nmtval: ");
    v_print_hex(mtval);
    v_print("\nmstatus: ");
    v_print_hex(mstatus);
    v_print("\nmtvec: ");
    v_print_hex(mtvec);
    v_print("\n================================\n");

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

    unsigned int is_interrupt = (mcause >> 31) & 1u;
    unsigned int cause = mcause & 0x7fffffffu;

    if (is_interrupt && cause == 7) {
        systicks++;
        clear_timer_interrupt();
        return;
    }

    if (is_interrupt && cause == 11) {
        systicks++;
        clear_timer_interrupt();
        return;
    }

    if (!is_interrupt && (cause == 8 || cause == 9 || cause == 11)) {
        if (cause == 11) {
            if (saved) {
                unsigned int syscall_num = saved[11];
                unsigned int args[7];
                for (int i = 0; i < 7; i++) args[i] = saved[4 + i];
                unsigned int ret = syscall_dispatch(syscall_num, args);
                saved[4] = ret;
            }
            mepc += 4;
            asm volatile ("csrw mepc, %0" : : "r" (mepc));
            return;
        }

        mepc += 4;
        asm volatile ("csrw mepc, %0" : : "r" (mepc));
        return;
    }

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
