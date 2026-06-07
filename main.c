
// Hardware registers
#define TIMG0_WDT_PROTECT (*(volatile unsigned int *)(0x6001F064))
#define TIMG0_WDT_CONFIG0 (*(volatile unsigned int *)(0x6001F048))
#define RTC_WDT_PROTECT (*(volatile unsigned int *)(0x600080B0))
#define RTC_WDT_CONFIG0 (*(volatile unsigned int *)(0x600080A8))
#define USB_SERIAL_JTAG_TX (*(volatile unsigned int *)(0x60043000))
#define USB_SERIAL_JTAG_EP1_CONF (*(volatile unsigned int *)(0x60043004))

// HAL
//basically we just loop until there are characters inside a string
//if \n then we do break line
// other than that we print the character then we move the pointer to the next character
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

  // Print the memory address of our function (should be in iram: 0x4038....)
  v_print("main() function is at: ");
  v_print_hex((unsigned int)&main);
  v_print("\n");

  // Print the memory address of our variable (should be in dram: 0x3FC8....)
  v_print("my_global_variable is at: ");
  v_print_hex((unsigned int)&my_global_variable);
  v_print("\n\n");

  while (1) {
    v_print("System stable...\n");
    for (volatile int i = 0; i < 2000000; i++);
  }
}
