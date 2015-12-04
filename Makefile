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

SRC=MiniForth.c
OBJ=mff forth
FAST=-D NOCHECK
OS=(shell uname -s)
LDOPTS=-ldl
PLATFORM=Linux
CC=gcc

#ifeq( $(OS), "FreeBSD" )
LDOPTS:=
PLATFORM:=FreeBSD
CC=clang
#endif

all:	$(OBJ)

mff:	$(SRC)
	@echo "Building for $(PLATFORM)"
	$(CC) -o $@ $(FAST) $(LDOPTS) $(SRC)

forth:	$(SRC)
	$(CC) -o $@ $(LDOPTS) $(SRC)

clean:	$(OBJ)
	rm -rf $(OBJ)
	rm -rf test.log

test:	test.rf $(OBJ)
	./mff -i test.rf 
	./forth -i test.rf 

status:	clean
	git status
