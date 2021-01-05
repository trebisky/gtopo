#
#  GTopo
#
#  Copyright (C) 2007, Thomas J. Trebisky
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
#
# Makefile for gtopo
# Tom Trebisky  6-26-2007

# Note that on fedora you will need the gtk2-devel package.

# This will get you gtk-1.2.10
# gtk-1.2.10 is/was the apparent default
#CFLAGS = `gtk-config --cflags`
#GTKLIBS = `gtk-config --libs`

# The following will get you gtk-2.x.y
# Now building with 2.12.8
# -g switch gets debugging information.

GTK_CONFIG = pkg-config gtk+-2.0

#CFLAGS = $(COPTS) `pkg-config --cflags gtk+-2.0`
#GTKLIBS = `pkg-config --libs gtk+-2.0`

CFLAGS = $(COPTS) `$(GTK_CONFIG) --cflags`
GTKLIBS = `$(GTK_CONFIG) --libs`

# Added 1-4-2021 -- the gtk2 headers are using
#  deprecated variables, and that isn't my problem.
# Not yet anyway.
CFLAGS += -Wno-deprecated-declarations

OBJS = gtopo.o maplet.o archive.o tpq_io.o settings.o places.o terra.o xml.o http.o utils.o overlay.o gpx.o

#COPTS = -g
COPTS = -g -Wreturn-type
#COPTS = -g -Wall
TARGET = gtopo

# -m32 lets you build a 32 bit version on a 64 bit system
#  comment out the two lines above, and uncomment these two lines to do so.
#  (and be sure and do a make clean first).
#  I needed to yum install cairo-devel.i386 at some point for this to work.
#  and it has been a long time since I have done this, since all my systems are 64 bit now.
#COPTS = -g -m32
#TARGET = gtopo-32

# To build on OSX, you need Xcode, and gtk2 from darwinports, the usual symptom
# of not having the latter is that you won't have pkg-config and you will get a million
# compile errors, the first of which complain about pkg-config missing, and then gtk.h
# cannot be found.  (This is also the symptom of the gtk2-devel package missing on linux
# for that matter).

all:	$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)

install:	$(TARGET)
	cp $(TARGET) /home/tom/bin

.c.o:	
	cc -c -g $< $(CFLAGS)

gtopo.o:	gtopo.c gtopo.h
maplet.o:	maplet.c gtopo.h
archive.o:	archive.c gtopo.h
tpq_io.o:	tpq_io.c gtopo.h

# only works with gnu make
.PHONY: version.o

# only works with gnu make
#version.o:      VERSION
#	rm -f version.c
#	mkversion -imount_version -v$(shell cat $<) >version.c
#	$(ACC) version.c
#	rm version.c

gtopo:	$(OBJS)
	cc -o gtopo $(OBJS) $(CFLAGS) $(GTKLIBS) -lm

# same as above, different name
gtopo-32:	$(OBJS)
	cc -o gtopo-32 $(OBJS) $(CFLAGS) $(GTKLIBS)

# initial development with 2.10.8 and 2.10.12
# my home machine (32 bit trona) has 2.8.15
# now with Fedora core 7 this is 2.10.14
gtkversion:
	pkg-config --modversion gtk+-2.0

states:
	./gtopo -i /u1/topo/ca_d01/ca1_map1/ca1_map1.tpq
	./gtopo -i /u1/topo/AZ_D05/AZ1_MAP1/AZ1_MAP1.TPQ


# THE END
