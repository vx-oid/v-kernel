CC = riscv64-unknown-elf-gcc
CFLAGS = -march=rv32imc_zicsr -mabi=ilp32 -T linker.ld -ffreestanding -fno-builtin -nostartfiles -Wall -Wextra -O2 -g

PORT ?= /dev/ttyUSB0

all: v-kernel.bin

v-kernel.elf: boot.S main.c
	$(CC) $(CFLAGS) boot.S main.c -o v-kernel.elf

v-kernel.bin: v-kernel.elf
	esptool.py --chip esp32c3 elf2image --flash_mode dio v-kernel.elf

flash: v-kernel.bin
	# Flash the image to the device. Override PORT if needed, e.g. `make flash PORT=/dev/tty.SLAB_USBtoUART`
	esptool.py --chip esp32c3 --port $(PORT) --baud 460800 write_flash --flash_mode dio 0x0 v-kernel.bin

clean:
	rm -f v-kernel.elf v-kernel.bin

.PHONY: all flash clean