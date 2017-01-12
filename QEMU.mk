ARMGNU ?= arm-linux-gnueabi

LDPTH ?= /usr/lib/gcc/arm-linux-gnueabi/4.9/libgcc.a
AARCH = -march=armv5t
AOPS = --warn --fatal-warnings $(AARCH)
COPS = -Wall -O2 -nostdlib -nostartfiles -ffreestanding $(AARCH)

MiniForth.bin :  qemu_versatile_start.o MiniForth.o memmap
	$(ARMGNU)-ld qemu_versatile_start.o MiniForth.o $(LDPTH) $(LDPTH) -T memmap -o MiniForth.elf
	$(ARMGNU)-objdump -D MiniForth.elf > MiniForth.list
	$(ARMGNU)-objcopy MiniForth.elf -O binary MiniForth.bin

qemu_versatile_start.o	: qemu_versatile_start.s
	$(ARMGNU)-as $(AOPS) qemu_versatile_start.s -o qemu_versatile_start.o

MiniForth.o : MiniForth.c
	$(ARMGNU)-gcc -c $(COPS) MiniForth.c -o MiniForth.o

qemu:	MiniForth.bin
	qemu-system-arm -M versatilepb -m 128M -nographic -kernel MiniForth.bin
##	qemu-system-arm -M versatilepb -m 128M -kernel MiniForth.bin


clean:
	rm -rf *.o *.bin *.list *.elf
