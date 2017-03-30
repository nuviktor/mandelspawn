#
# Makefile for mslaved, the MandelSpawn computation server
#
# This doesn't use Imake because it isn't an X program.
#
#

# Use these if you have sockets
DEFINES= -DHAVE_SOCKETS
BINARIES= mslaved mslavedc

# Use these if you don't have sockets (mslavedc is needed only with sockets)
# DEFINES=
# BINARIES= mslaved

# This is the place where "make install" will copy the 
# mslaved and mslavedc binaries
DAEMONDIR=/usr/local/etc

# Non-BSD machines usually need something like -linet or -lsocket here
LIBS=

# Make sure you use gcc if you have a Vax, 680x0 or i386 and
# you don't have a high-performance floating point unit.  
# If compiled without gcc on those machines, mslaved will use floating 
# point instead of fixed point, and any old PC Mandelbrot program
# is likely to run circles around your fancy workstation network.
# For machines with fast floating point, such as many Sparc- or
# MIPS-based workstations, gcc won't make much of a difference and
# may even be slower.

# CC=gcc

CFLAGS= -O -I../lib

LD=$(CC)
LDFLAGS=

all: $(BINARIES)

mslaved: mslaved.c ../lib/ms_ipc.h ms_real.c ../lib/ms_real.h ../lib/ms_job.h
	$(CC) $(CFLAGS) $(DEFINES) mslaved.c $(LIBS) -o mslaved

mslavedc: mslavedc.c ../lib/ms_ipc.h
	$(CC) $(CFLAGS) $(DEFINES) mslavedc.c $(LIBS) -o mslavedc

# Also remember to edit /etc/inetd.conf and /etc/services as described in
# the INSTALL file.
install:
	-mkdir $(DAEMONDIR)
	cp mslaved $(DAEMONDIR)/mslaved; strip $(DAEMONDIR)/mslaved
	cp mslavedc $(DAEMONDIR)/mslavedc; strip $(DAEMONDIR)/mslavedc

clean:
	rm -f core *.o *~

spotless: clean
	rm -f mslaved mslavedc
