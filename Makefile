CC = riscv64-unknown-elf-gcc
CFLAGS = -march=rv32imc_zicsr -mabi=ilp32 -nostdlib -T linker.ld

all: v-kernel.bin

v-kernel.elf: boot.S main.c
	$(CC) $(CFLAGS) boot.S main.c -o v-kernel.elf

v-kernel.bin: v-kernel.elf
	esptool.py --chip esp32c3 elf2image --flash_mode dio v-kernel.elf

clean:
	rm -f v-kernel.elf v-kernel.bin