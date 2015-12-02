##
## Trivial makefile for the OneFileForth project
## By default creates a "safe" interpreter (forth)
## which checks stack depth on all (?) primitives.
## In addition the mff executable is generated with
## -D NOCHEK set on the compile line ... for time
## sensitive applications ...
##
SRC=MiniForth.c
FAST="-D NOCHECK"

all:	mff forth

mff:	MiniForth.c
	$(CC) -o $@ $(FAST) MiniForth.c

forth:	MiniForth.c
	$(CC) -o $@ MiniForth.c

clean:
	rm -rf ./mff
	rm -rf ./forth
