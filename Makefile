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
