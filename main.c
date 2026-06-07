#define TIMG0_WDT_PROTECT (*(volatile unsigned int *)(0x6001F064))
#define TIMG0_WDT_CONFIG0 (*(volatile unsigned int *)(0x6001F048))

#define RTC_WDT_PROTECT (*(volatile unsigned int *)(0x600080B0))
#define RTC_WDT_CONFIG0 (*(volatile unsigned int *)(0x600080A8))

#define USB_SERIAL_JTAG_TX (*(volatile unsigned int *)(0x60043000))

void main() {
  TIMG0_WDT_PROTECT = 0x50D83AA1;
  TIMG0_WDT_CONFIG0 = 0; // Turn it off

  RTC_WDT_PROTECT = 0x50D83AA1;
  RTC_WDT_CONFIG0 = 0; // Turn it off

  while (1) {
    USB_SERIAL_JTAG_TX = 'V';
    USB_SERIAL_JTAG_TX = '\r';
    USB_SERIAL_JTAG_TX = '\n';

    for (volatile int i = 0; i < 500000; i++)
      ;
  }
}
