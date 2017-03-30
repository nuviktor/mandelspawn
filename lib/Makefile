#
# Makefile for MandelSpawn library
#
# This doesn't use Imake because it isn't an X program.
#
# You may have to remove one or more of the following from DEFINES:
#
#  -DHAVE_SOCKETS	  if you don't have sockets
#  -DHAVE_SELECT	  if you don't have select()
#
# If your machine lacks strdup(), add -DNO_STRDUP to DEFINES.
#

DEFINES=-DHAVE_SOCKETS -DHAVE_SELECT 

CFLAGS=$(DEFINES)

LIB=libms.a
OBJS=cmap.o parse.o work.o mspawn.o io.o strdup.o

$(LIB): $(OBJS)
	ar rvl $(LIB) $(OBJS)
	-ranlib $(LIB)

cmap.o: cmap.c color.h
parse.o: parse.c colors.c color.h
work.o: work.c work.h datarep.h ms_ipc.h ms_real.h io.h inet.h
mspawn.o: mspawn.c mspawn.h datarep.h ms_ipc.h inet.h ms_job.h
io.o: io.c io.h

clean:
	rm -f *.o *~

spotless: clean
	rm -f $(LIB)
