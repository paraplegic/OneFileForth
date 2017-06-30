
SRC = ../../../src
OUT = ../../../bin

ARCH = arm
OS = native
OSTYPE != uname -s

include $(SRC)/$(OSTYPE).mk

AARCH = -march=armv5t
AOPS = --warn --fatal-warnings $(AARCH)
COPS = -Wall -O2 -nostdlib -nostartfiles -ffreestanding $(AARCH) -DNATIVE=$(OS)

all:	$(OUT)/OneFileForth-$(OS)-$(ARCH).bin

$(OUT)/OneFileForth-$(OS)-$(ARCH).bin :  qemu_versatile_start.o OneFileForth.o memmap
	$(ARMGNU)-gcc qemu_versatile_start.o OneFileForth.o -nostartfiles $(LDPTH) -T memmap -o OneFileForth.elf -Wl,--build-id=none
	$(ARMGNU)-objdump -D OneFileForth.elf > OneFileForth.list
	$(ARMGNU)-objcopy OneFileForth.elf -O binary $(OUT)/OneFileForth-$(OS)-$(ARCH).bin

qemu_versatile_start.o	: qemu_versatile_start.s
	$(ARMGNU)-as $(AOPS) qemu_versatile_start.s -o qemu_versatile_start.o

OneFileForth.o : $(SRC)/OneFileForth.c
	$(ARMGNU)-gcc -c $(COPS) -D NOCHECK $(SRC)/OneFileForth.c -o OneFileForth.o
	## $(ARMGNU)-gcc -c $(COPS) $(SRC)/OneFileForth.c -o OneFileForth.o

qemu:	$(OUT)/OneFileForth-$(OS)-$(ARCH).bin
	qemu-system-arm -M versatilepb -m 256M -nographic -kernel $(OUT)/OneFileForth-$(OS)-$(ARCH).bin 

map:
	$(ARMGNU)-gcc qemu_versatile_start.o OneFileForth.o -nostartfiles $(LDPTH) -T memmap -o OneFileForth.elf -Wl,--build-id=none -Wl,--print-map > load.map

clean:
	rm -rf *.o *.bin *.list *.elf *.map

realclean:	clean
	rm -rf $(OUT)/OneFileForth-$(OS)-$(ARCH).bin
