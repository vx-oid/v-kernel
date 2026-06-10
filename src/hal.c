#include "hal.h"

#define USB_SERIAL_JTAG_TX       (*(volatile unsigned int *)(0x60043000))
#define USB_SERIAL_JTAG_EP1_CONF (*(volatile unsigned int *)(0x60043004))

void v_print(const char *str) {
    while (*str) {
        if (*str == '\n') {
            while ((USB_SERIAL_JTAG_EP1_CONF & 0x02) == 0);
            USB_SERIAL_JTAG_TX = '\r';
        }

        while ((USB_SERIAL_JTAG_EP1_CONF & 0x02) == 0);
        USB_SERIAL_JTAG_TX = *str;

        str++;
    }
    USB_SERIAL_JTAG_EP1_CONF |= 0x01;
}

void v_print_hex(unsigned int val) {
    v_print("0x");
    for (int i = 28; i >= 0; i -= 4) {
        int nibble = (val >> i) & 0xF;

        while ((USB_SERIAL_JTAG_EP1_CONF & 0x02) == 0);

        if (nibble < 10) {
            USB_SERIAL_JTAG_TX = '0' + nibble;
        } else {
            USB_SERIAL_JTAG_TX = 'A' + (nibble - 10);
        }
    }
    USB_SERIAL_JTAG_EP1_CONF |= 0x01;
}
