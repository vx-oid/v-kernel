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
void c_trap_handler() {
    v_print("\n================================\n");
    v_print("  >>> CPU TRAP INTERCEPTED <<<  \n");
    
    // Read the Machine Exception Program Counter (where the trap happened)
    unsigned int mepc;
    asm volatile ("csrr %0, mepc" : "=r" (mepc));
    
    v_print("Trapped at Address: ");
    v_print_hex(mepc);
    v_print("\n================================\n");

    // Move the return address forward by 4 bytes to skip the 'ecall' instruction
    // Otherwise, we will return directly to ecall and infinite loop
    mepc += 4;
    
    // Write the modified address back to the CPU
    asm volatile ("csrw mepc, %0" : : "r" (mepc));
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
    asm volatile ("csrw mtvec, %0" : : "r" (trap_vector));
    v_print("mtvec CSR configured.\n");

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