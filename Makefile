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

SRC =MiniForth.c
OBJ =mff forth
FAST=-D NOCHECK
CCOPT=-g -O3

include $(OSTYPE).mk

all:	$(OBJ)

mff:	$(SRC)
	@echo "OSTYPE is $(OSTYPE)"
	@echo "Building for $(OSTYPE)"
	$(CC) $(CCOPT) -o $@ $(FAST) $(LDOPTS) $(SRC)
	size $@

forth:	$(SRC)
	$(CC) $(CCOPT) -o $@ $(LDOPTS) $(SRC)
	size $@

clean:
	rm -rf $(OBJ)
	rm -rf test.log
	rm -rf *.out

edit:	$(SRC)
	cscope -b $(SRC)

test:	$(OBJ) test_00.rf
	./mff -i test_00.rf 
	./mff -i test_01.rf
	./forth -i test_00.rf 
	./forth -i test_01.rf

status:	clean
	git status

install: $(OBJ)
	cp $(OBJ) /usr/local/bin
