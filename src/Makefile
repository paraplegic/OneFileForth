##
## Trivial makefile for the OneFileForth project
## By default creates a "safe" interpreter (forth)
## which checks stack depth on all (?) primitives.
## In addition the off executable is generated with
## -D NOCHEK set on the compile line ... for time
## sensitive applications ...
##

##
## This makefile should work for either BSD Make, or
## gnu Make (gmake on BSD), and compile/link accordingly.
##

OUT =../bin
FRT =../code
SRC =OneFileForth.c
OBJ =$(OUT)/off $(OUT)/offorth
FAST=-D NOCHECK
CCOPT=-g -O2
OSTYPE != uname -s

include $(OSTYPE).mk

all:	$(OBJ)

$(OUT):
	mkdir -p $(OUT)

$(OUT)/off:	$(OUT) $(SRC)
	@echo "OSTYPE is $(OSTYPE)"
	@echo "Building for $(OSTYPE)"
	$(CC) $(CCOPT) -o $@ $(FAST) $(LDOPTS) $(SRC) 
	size $@

$(OUT)/offorth:	$(SRC)
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
	rm -rf cscope.out
	rm -rf load.map

realclean:	clean
	rm -rf $(OUT)/* 

map:	$(SRC)
	@echo "Building loadmap for $(OSTYPE)"
	$(CC) $(CCOPT) -o ./tmp.$$ $(FAST) $(LDOPTS) $(SRC) $(MAP) > load.map
	rm -rf ./tmp.$$

edit:	$(SRC)
	cscope -b $(SRC)

test:	$(OBJ) $(FRT)/test_00.rf
	$(OUT)/off -i $(FRT)/test_00.rf 
	$(OUT)/off -i $(FRT)/test_01.rf
	$(OUT)/offorth -i $(FRT)/test_00.rf 
	$(OUT)/offorth -i $(FRT)/test_01.rf

status:	realclean
	git status

install: $(OBJ)
	cp $(OBJ) /usr/local/bin
