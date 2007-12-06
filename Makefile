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
# gtk-1.2.10 is the apparent default
# however gtk-2.0 is on my system

# This will get you gtk-1.2.10
#CFLAGS = `gtk-config --cflags`
#GTKLIBS = `gtk-config --libs`

# The following will get you gtk-2.10.12
# -g switch gets debugging information.

CFLAGS = $(COPTS) `pkg-config --cflags gtk+-2.0`
GTKLIBS = `pkg-config --libs gtk+-2.0`

OBJS = gtopo.o maplet.o archive.o tpq_io.o settings.o places.o terra.o xml.o http.o utils.o

#COPTS = -g
#TARGET = gtopo

# -m32 lets you build a 32 bit version on a 64 bit system
COPTS = -g -m32
TARGET = gtopo-32

all:	$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)

hinstall:
	scp $(TARGET) hacksaw:/mmt/bin

install:
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
	cc -o gtopo $(OBJS) $(CFLAGS) $(GTKLIBS)

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
