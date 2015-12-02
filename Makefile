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
LD=-ldl
PLTFM=Linux
CC=gcc

#ifeq( $(OS), "FreeBSD" )
LD:=
PLTFM:=FreeBSD
CC=clang
#endif

all:	$(OBJ)

mff:	$(SRC)
	@echo "Building for $(PLTFM)"
	$(CC) -o $@ $(FAST) $(LD) $(SRC)

forth:	$(SRC)
	$(CC) -o $@ $(LD) $(SRC)

clean:
	rm -rf $(OBJ)
