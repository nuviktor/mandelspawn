#
# Top-level Makefile for MandelSpawn
#
# Some of the subdirectory Makefiles may need editing. 
# Read the INSTALL file for instructions.
#

all:
	(cd lib; $(MAKE))
	(cd mslaved; $(MAKE))
	(cd bms; $(MAKE))
	(cd xms; xmkmf; $(MAKE) depend; $(MAKE))
