// --- Hardware Registers ---
#define TIMG0_WDT_PROTECT (*(volatile unsigned int *)(0x6001F064))
#define TIMG0_WDT_CONFIG0 (*(volatile unsigned int *)(0x6001F048))
#define RTC_WDT_PROTECT   (*(volatile unsigned int *)(0x600080B0))
#define RTC_WDT_CONFIG0   (*(volatile unsigned int *)(0x600080A8))
#define USB_SERIAL_JTAG_TX       (*(volatile unsigned int *)(0x60043000))
#define USB_SERIAL_JTAG_EP1_CONF (*(volatile unsigned int *)(0x60043004))

extern void trap_vector();

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
    unsigned int cause = mcause & 0xFFF;
    if (cause == 8 || cause == 9 || cause == 11) {
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
    /* Force direct mode for mtvec by clearing the low two bits. */
    unsigned int mtvec_val = ((unsigned int)trap_vector) & ~0x3u;
    asm volatile ("csrw mtvec, %0" :: "r" (mtvec_val));
    v_print("mtvec CSR configured (direct mode).\n");

    // Trigger a manual Exception
    v_print("Pressing the RISC-V panic button (ecall)...\n");
    asm volatile ("ecall");

    // If mret in boot.S works, the CPU will resume execution right here!
    v_print("\n>>> SURVIVED THE TRAP! Back in main loop. <<<\n\n");

    while (1) {
        v_print("System stable...\n");
        for (volatile int i = 0; i < 2000000; i++);
    }
}