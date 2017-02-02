##
## Trivial makefile for the OneFileForth project
## By default creates a "safe" interpreter (forth)
## which checks stack depth on all (?) primitives.
## In addition the mff executable is generated with
## -D NOCHEK set on the compile line ... for time
## sensitive applications ...
##

##
## This makefile should work for either BSD Make, or
## gnu Make (gmake on BSD), and compile/link accordingly.
##

OUT =../bin
FRT =../code
SRC =MiniForth.c
OBJ =$(OUT)/mff $(OUT)/forth
FAST=-D NOCHECK
CCOPT=-g -O0
OSTYPE != uname -s

include $(OSTYPE).mk

all:	$(OBJ)

$(OUT)/mff:	$(SRC)
	@echo "OSTYPE is $(OSTYPE)"
	@echo "Building for $(OSTYPE)"
	$(CC) $(CCOPT) -o $@ $(FAST) $(LDOPTS) $(SRC) 
	size $@

$(OUT)/forth:	$(SRC)
	$(CC) $(CCOPT) -o $@ $(LDOPTS) $(SRC)
	size $@

clean:
	rm -rf $(OBJ)
	rm -rf test.log
	rm -rf *.out
	rm -rf *.o
	rm -rf *.elf
	rm -rf *.bin
	rm -rf *.list

realclean:	clean
	rm -rf $(OUT)/*

edit:	$(SRC)
	cscope -b $(SRC)

test:	$(OBJ) $(FRT)/test_00.rf
	$(OUT)/mff -i $(FRT)/test_00.rf 
	$(OUT)/mff -i $(FRT)/test_01.rf
	$(OUT)/forth -i $(FRT)/test_00.rf 
	$(OUT)/forth -i $(FRT)/test_01.rf

status:	realclean
	git status

install: $(OBJ)
	cp $(OBJ) /usr/local/bin
