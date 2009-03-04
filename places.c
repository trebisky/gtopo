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

extern struct places_info p_info;

#ifdef notdef
static struct place *place_head;
static int next_id;

struct place {
	struct place *next;
	char *p_name;
	double p_long;
	double p_lat;
	int p_id;
};
#endif

void
new_place ( char *s_series, char *lon, char *lat, char *name )
{
	GtkTreeIter iter;
	int series;

	gronk_series ( &series, s_series );

	gtk_list_store_append ( p_info.store, &iter );

	gtk_list_store_set ( p_info.store, &iter,
		NAME_COLUMN, name,
		LONG_COLUMN, lon,
		LAT_COLUMN, lat,
		SERIES_COLUMN, series,
		-1 );

#ifdef notdef
	struct place *pp;
	struct place *lp;

	pp = (struct place *) gmalloc ( sizeof(struct place) );
	if ( ! pp )
	    error ("new place - out of memory\n");

	pp->next = NULL;
        pp->p_name = strhide ( name );
	pp->p_id = next_id++;

        /* keep list in order */
        if ( ! place_head )
            place_head = pp;
        else {
            lp = place_head;
            while ( lp->next )
                lp = lp->next;
            lp->next = pp;
        }
#endif
}

static void
load_places ( char *path )
{
	FILE *fp;
	char line[MAX_LINE];
	char *wp[5];
	int nw;

	fp = fopen ( path, "r" );
	if ( ! fp )
	    return;

	printf ( "Loading places from %s\n", path );

	/* Remember fgets includes the newline */
	while ( fgets ( line, MAX_LINE, fp ) ) {
	    /* kill the newline */
	    line[strlen(line)-1] = '\0';

	    /* allow blank lines and comments */
	    if ( line[0] == '\0' || line[0] == '#' )
	    	continue;

	    nw = split_n ( line, wp, 2 );
	    /* printf ( "split_n: %d %s\n", nw, wp[2] ); */

	    if ( nw == 2 )
	    	new_place ( "24K", wp[0], wp[1], "--" );

	    if ( nw > 2 )
	    	new_place ( "24K", wp[0], wp[1], wp[2] );

	    /* Not so easy, want to collect all words in name into
	     * the final string */
	    /*
	    if ( nw > 3 )
	    	new_place ( wp[0], wp[1], wp[2], wp[3] );
		*/
	}

	fclose ( fp );
}

void
places_init ( void )
{
	char buf[128];
	char *home;

	p_info.store = gtk_list_store_new ( N_COLUMNS,
	    G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT );

	gtk_list_store_clear ( p_info.store );

	/*
	place_head = (struct place *) NULL;
	next_id = 1;
	*/

	load_places ( "/etc/gtopo/places" );

	home = find_home ();
	if ( home ) {
	    strcpy ( buf, home );
	    strcat ( buf, "/.gtopo/places" );
	    load_places ( buf );
	}
}

/*
void
show_places ( void )
{
	struct place *pp;

	for ( pp = place_head; pp; pp = pp->next ) {
	    printf ( "Place %d: %s\n", pp->p_id, pp->p_name );
	}
}
*/

/* THE END */
