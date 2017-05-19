LDOPTS:=-ldl 
MAP:=-Wl,--print-map
CC:=gcc

ARMGNU ?= arm-linux-gnueabi
## Debian Jessie ... crosstools
## LDPTH ?= /usr/lib/gcc/arm-linux-gnueabi/4.9/libgcc.a
## Debian Stretch ... crosstools
LDPTH ?= -L /usr/arm-linux-gnueabi/lib
