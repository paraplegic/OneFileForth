# OneFileForth

	A single file implementation of a non-standard Forth written in the FIG style
	This project is hosted on GitHub, and can be cloned as follows:

		git clone http://github.com/paraplegic/OneFileForth .

## Building:

### HOSTED:

	This project should make out of the box on most HOSTED systems, and particularly
	on Linux and/or BSD:

		make test
		./off

		-- OneFileForth-Hosted alpha Version: 00.01.48F
		-- www.ControlQ.com
		ok 

### NATIVE:

	OneFileForth will build using the arm-eabi-none or arm-eabi-linux toolchain, and will 
	build for the ARM versatilepb under QEMU.  Similar in most respects to the hosted version,
	but obviously the C native interface does not work, and you cannot access things like clk and 
	bye, but the native version will continue to evolve until it is useful on real cards (BBB, Pi and 
	similar).  Assuming you have an appropriate X compilation tool, and QEMU, you can test the native
	version by typing:

		make qemu

		qemu-system-arm -M versatilepb -m 1024M -nographic -kernel OneFileForth.bin 
		pulseaudio: set_sink_input_volume() failed
		pulseaudio: Reason: Invalid argument
		pulseaudio: set_sink_input_mute() failed
		pulseaudio: Reason: Invalid argument
		-- OneFileForth-Native alpha Version: 00.01.31F
		-- www.ControlQ.com
		ok

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
	  FreeBSD 10.2
	  Debian  Jesse
	  RaspberyPi Zero W running Raspbian Jesse
	  OSX  10.x

####	Native:
	  Qemu ARM VersatilePB

####	Coming soon:  RiscV (native/bsd/linux)
