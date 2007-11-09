/*
 *  GTopo - places.c
 *
 *  Copyright (C) 2007, Thomas J. Trebisky
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "gtopo.h"
#include "protos.h"

#define MAX_LINE	128
#define MAX_WORDS	16

void
set_place ( char *lon, char *lat, char *name )
{
	printf ( "set place %s %s -- %s\n", lon, lat, name );
}

static void
load_places ( char *path )
{
	FILE *fp;
	char line[MAX_LINE];
	char *wp[3];
	int nw;

	fp = fopen ( path, "r" );
	if ( ! fp )
	    return;

	/* Remember fgets includes the newline */
	while ( fgets ( line, MAX_LINE, fp ) ) {
	    /* kill the newline */
	    line[strlen(line)-1] = '\0';

	    /* allow blank lines and comments */
	    if ( line[0] == '\0' || line[0] == '#' )
	    	continue;

	    nw = split_n ( line, wp, 2 );
	    printf ( "split_n: %d %s\n", nw, wp[2] );

	    if ( nw > 2 )
	    	set_place ( wp[0], wp[1], wp[2] );
	}

	fclose ( fp );
}

void
places_init ( void )
{
	char buf[128];
	char *home;

	load_places ( "/etc/gtopo/places" );

	home = find_home ();
	if ( home ) {
	    strcpy ( buf, home );
	    strcat ( buf, "/.gtopo/places" );
	    load_places ( buf );
	}
}

/* THE END */
