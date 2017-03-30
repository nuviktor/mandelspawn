#
# Makefile for bms, the batch-mode MandelSpawn client program
#
# This doesn't use Imake because it isn't an X program.
#
# You may need to add one or more of the following to DEFINES:
#
#   -DNEED_SYS_SELECT_H   if you need to include <sys/select.h>
#		            to define getdtablesize() or the fd_set type
#   -DNO_BCOPY 		  on System V
#

DEFINES= 

# CC=gcc
# DEBUG= -g

CFLAGS=$(DEBUG) $(DEFINES) -I../lib

LD=$(CC)
LDFLAGS=$(DEBUG)

OBJS=bms.o
MSLIB=../lib/libms.a

bms: $(OBJS) $(MSLIB)
	$(LD) $(LDFLAGS) $(OBJS) $(MSLIB) -lm -o bms

bms.o: bms.c \
   ../lib/work.h ../lib/io.h ../lib/mspawn.h ../lib/ms_job.h \
   ../lib/ms_real.h ../lib/version.h ../lib/color.h

clean:
	rm -f core *.o *~

spotless: clean
	rm -f bms

lint:
	lint $(DEFINES) *.c
