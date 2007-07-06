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

all:	gtopo

clean:
	rm -f gtopo

gtopo:	gtopo.c
	cc -o gtopo gtopo.c $(CFLAGS) $(GTKLIBS)

# even though I have gtk 2.10.12, this shows 2.10.8
# my home machine (trona) gives 2.8.15 */
gtkversion:
	pkg-config --modversion gtk+-2.0
	
# THE END
