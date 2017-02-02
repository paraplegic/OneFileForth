
SRC = ../../../src
OUT = ../../../bin

ARCH = arm
OS = native
OSTYPE != uname -s

include $(SRC)/$(OSTYPE).mk

AARCH = -march=armv5t
AOPS = --warn --fatal-warnings $(AARCH)
COPS = -Wall -O2 -nostdlib -nostartfiles -ffreestanding $(AARCH)

all:	$(OUT)/MiniForth-$(OS)-$(ARCH).bin

$(OUT)/MiniForth-$(OS)-$(ARCH).bin :  qemu_versatile_start.o MiniForth.o memmap
	$(ARMGNU)-ld qemu_versatile_start.o MiniForth.o $(LDPTH) $(LDPTH) -T memmap -o MiniForth.elf ## --print-map
	$(ARMGNU)-objdump -D MiniForth.elf > MiniForth.list
	$(ARMGNU)-objcopy MiniForth.elf -O binary $(OUT)/MiniForth-$(OS)-$(ARCH).bin

qemu_versatile_start.o	: qemu_versatile_start.s
	$(ARMGNU)-as $(AOPS) qemu_versatile_start.s -o qemu_versatile_start.o

MiniForth.o : $(SRC)/MiniForth.c
	$(ARMGNU)-gcc -c $(COPS) -D NOCHECK $(SRC)/MiniForth.c -o MiniForth.o

qemu:	$(OUT)/MiniForth-$(OS)-$(ARCH).bin
	qemu-system-arm -M versatilepb -m 1024M -nographic -kernel $(OUT)/MiniForth-$(OS)-$(ARCH).bin 


clean:
	rm -rf *.o *.bin *.list *.elf
