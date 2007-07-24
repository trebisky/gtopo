# Makefile for gtopo
# Tom Trebisky  6-26-2007
# gtk-1.2.10 is the apparent default
# however gtk-2.0 is on my system

# This will get you gtk-1.2.10
#CFLAGS = `gtk-config --cflags`
#GTKLIBS = `gtk-config --libs`

# This will get you gtk-2.10.12
# -g switch gets debugging information.
CFLAGS = -g `pkg-config --cflags gtk+-2.0`
GTKLIBS = `pkg-config --libs gtk+-2.0`

OBJS = gtopo.o maplet.o archive.o tpq_io.o

all:	gtopo

clean:
	rm -f gtopo $(OBJS)

install:
	scp gtopo hacksaw:/mmt/bin
	cp gtopo /home/tom/bin

.c.o:	
	cc -c -g $< $(CFLAGS)

gtopo.o:	gtopo.c gtopo.h
maplet.o:	maplet.c gtopo.h
archive.o:	archive.c gtopo.h
tpq_io.o:	tpq_io.c gtopo.h

gtopo:	$(OBJS)
	cc -o gtopo $(OBJS) $(CFLAGS) $(GTKLIBS)

# even though I have gtk 2.10.12, this shows 2.10.8
# my home machine (trona) gives 2.8.15
gtkversion:
	pkg-config --modversion gtk+-2.0

# THE END
