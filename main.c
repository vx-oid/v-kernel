
// Hardware registers
#define TIMG0_WDT_PROTECT (*(volatile unsigned int *)(0x6001F064))
#define TIMG0_WDT_CONFIG0 (*(volatile unsigned int *)(0x6001F048))
#define RTC_WDT_PROTECT (*(volatile unsigned int *)(0x600080B0))
#define RTC_WDT_CONFIG0 (*(volatile unsigned int *)(0x600080A8))
#define USB_SERIAL_JTAG_TX (*(volatile unsigned int *)(0x60043000))

// HAL
//basically we just loop until there are characters inside a string
//if \n then we do break line
// other than that we print the character then we move the pointer to the next character
void v_print(const char* str) {
  while (*str) {
    if (*str == '\n') {
      USB_SERIAL_JTAG_TX = '\r';
    }

    USB_SERIAL_JTAG_TX = *str;
    str++;
  }
}


void main() {
  //watchdogs
  TIMG0_WDT_PROTECT = 0x50D83AA1;
  TIMG0_WDT_CONFIG0 = 0;
  RTC_WDT_PROTECT = 0x50D83AA1;
  RTC_WDT_CONFIG0 = 0;

  //clearing the terminal
  //with ansi escape code
  v_print("\x1B[2J\x1B[H");

  v_print("=============================");
  v_print("     Welcome to V-Kernel     ");
  v_print("    Architecture:  RISC-V    ");
  v_print("=============================");

  while (1) {
    v_print("Kernel is alive...\n");

    for (volatile int i = 0; i < 2000000; i++);
  }
}
