# OneFileForth

	A single file implementation of a non-standard Forth written in the FIG style
	This project is hosted on GitHub, and can be cloned as follows:

		git clone http://github.com/paraplegic/OneFileForth .

## Building:

### HOSTED:

	This project should make out of the box on most HOSTED systems, and particularly
	on Linux and/or BSD:

		sudo make install
		[sudo] password for rob: 
		OSTYPE is Linux
		Building for Linux

		gcc -g -O2 -o ../bin/off -D NOCHECK -ldl  OneFileForth.c 
		size ../bin/off
		   text	   data	    bss	    dec	    hex	filename
		  46618	 138104	  35384	 220106	  35bca	../bin/off

		gcc -g -O2 -o ../bin/offorth -ldl  OneFileForth.c
		size ../bin/offorth
		   text	   data	    bss	    dec	    hex	filename
		  49627	 138104	  35384	 223115	  3678b	../bin/offorth

		cp ../bin/off ../bin/offorth /usr/local/bin
		rob@debian9:~/mystuff/OneFileForth/src$ off
		-- OneFileForth-Hosted alpha Version: 00.01.56F (en_US.UTF-8)
		-- www.ControlQ.com
		
		ok bye

		rob@debian9:~/mystuff/OneFileForth/src$ offorth
		-- OneFileForth-Hosted alpha Version: 00.01.56D (en_US.UTF-8)
		-- www.ControlQ.com
		
		ok bye

### TESTING:
	
	Testing on native platforms is accomplished simply:
		
		make test

	Check stdout and the logs for error messages.


### NATIVE:

	OneFileForth will build using the arm-eabi-none or arm-eabi-linux toolchain, and will 
	build for the ARM versatilepb under QEMU.  Similar in most respects to the hosted version,
	but obviously the C native interface does not work, and you cannot access things like clk and 
	bye, but the native version will continue to evolve until it is useful on real cards (BBB, Pi and 
	similar).  Assuming you have an appropriate X compilation tool, and QEMU, you can test the native
	version by typing:

		make qemu

		arm-linux-gnueabi-as --warn --fatal-warnings -march=armv5t qemu_versatile_start.s -o qemu_versatile_start.o
		arm-linux-gnueabi-gcc -c -Wall -O2 -nostdlib -nostartfiles -ffreestanding -march=armv5t -DNATIVE=native -D NOCHECK ../../../src/OneFileForth.c -o OneFileForth.o
		## arm-linux-gnueabi-gcc -c -Wall -O2 -nostdlib -nostartfiles -ffreestanding -march=armv5t -DNATIVE=native ../../../src/OneFileForth.c -o OneFileForth.o
		arm-linux-gnueabi-gcc qemu_versatile_start.o OneFileForth.o -nostartfiles -L /usr/arm-linux-gnueabi/lib -T memmap -o OneFileForth.elf -Wl,--build-id=none
		arm-linux-gnueabi-objdump -D OneFileForth.elf > OneFileForth.list
		arm-linux-gnueabi-objcopy OneFileForth.elf -O binary ../../../bin/OneFileForth-native-arm.bin
		qemu-system-arm -M versatilepb -m 256M -nographic -kernel ../../../bin/OneFileForth-native-arm.bin 
		pulseaudio: set_sink_input_volume() failed
		pulseaudio: Reason: Invalid argument
		pulseaudio: set_sink_input_mute() failed
		pulseaudio: Reason: Invalid argument
		-- OneFileForth-Native alpha Version: 00.01.56F (EMBEDDED)
		-- www.ControlQ.com

		ok words
		quit banner + - * ^ / % abs .s . u. bye words rdepth depth dup ?dup rot nip tuck drop over 
		swap pick >r r> <eof> cells cellsize @ ! r@ r! cr@ cr! h@ h! c@ c! << >> cmove word ascii 
		?key key emit type cr dp strings flashsize flash here freespace , (literal) : ; execute call 
		(colon) ' >name >code >body decimal hex base trace sigval errvar errval errstr warm cold see 
		(variable) allot create lambda does> constant variable normal immediate [ ] unresolved >mark 
		>resolve <mark <resolve ?branch branch begin again while repeat until leave if else then < > 
		>= <= == != & and or xor not buf scratch pad ( \ .( " ." count save unsave infile filename 
		outfile closeout native clks ++ -- utime ops noops do (do) i loop (loop) +loop (+loop) 
		forget <# # #s hold sign #> utf8 accept find version code data align fill ok 


	To exit the QEMU emulator, hit a cntrl-A followed by an x to exit.

	Note that the banner will indicate whether the version of forth running is a HOSTED or
	NATIVE version, the revision number, and whether stack checking is enabled (D)ebug vs a
	non-stack checking (F)ast version is running.  Not sure why the pulseaudio messages are 
	being emitted from QEMU, and I'll review those some day ...

	The Makefile will support both the BSD make and the Linux GNU style make
	(or gmake on BSD).  The OneFileForth.c should compile on cygwin with minor changes
	but as I don't run DOS/Windows, I have no need for Cygwin.  Sorry, you're on your
	own.

## Running
		*TBD*

### Tested

####	Hosted:
	  PC-BSD/TrueOS 
	  FreeBSD 10.x, 11.x
	  Debian  Jesse, Stretch
	  RaspberyPi Zero W running Raspbian Jesse
	  OSX  10.x

####	Native:
	  Qemu ARM VersatilePB

####	Coming soon:  RiscV (native/bsd/linux)
