/*
 *  GTopo - settings.c
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

extern struct settings settings;

/* PC keyboard Page-Up and Page-Down keys */
#define KV_PAGE_UP	65365
#define KV_PAGE_DOWN	65366

/* You could hack this file and recompile, but
 * a more sensible thing to do is place a settings
 * file in /etc/gtopo/config or HOME/.gtopo/config
 */
static void
settings_default ( void )
{
	settings.verbose = 0;

	/* 640 x 800 works pretty well for printing onto
	 * 8.5 x 11 inch paper.
	 * Actually 640 x 828 has the same aspect ratio.
	 */
	settings.x_view = 640;
	settings.y_view = 800;

	settings.starting_series = S_STATE;

#ifdef notdef
	/* Mt. Hopkins, Arizona */
	settings.starting_long = -110.88;
	settings.starting_lat = 31.69;

	/* In California west of Taboose Pass */
	settings.starting_long = -dms2deg ( 118, 31, 0.0 );
	settings.starting_lat = dms2deg ( 37, 1, 0.0 );

	/* Near Alturas, NE California */
	settings.starting_long = -120.5;
	settings.starting_lat = 41.5;

	/* Near Las Vegas, Nevada */
	settings.starting_long = -114.9894;
	settings.starting_lat = 36.2338;
#endif

	/* Flagstaff, Arizona */
	settings.starting_long = -111.6722;
	settings.starting_lat = 35.18;

	/* Marker size is "half" the marker from center to end */
	settings.center_marker = 1;
	settings.marker_size = 2;

	settings.show_maplets = 0;

	settings.m1_action = M1_GRAB;
	settings.m3_action = M1_CENTER;

	/* Keyboard key to zoom in/out (go up/down series) */
	settings.up_key = KV_PAGE_UP;
	settings.down_key = KV_PAGE_DOWN;
}

struct wtable {
	char *word;
	int index;
};

static void
gronk_word ( int *val, char *arg, struct wtable *wp )
{
	for ( ; wp->word; wp++ ) {
	    if ( strcmp ( arg, wp->word ) == 0 )
	    	*val = wp->index;
	}
}

struct wtable m1_words[] = { "center", M1_CENTER, "grab", M1_GRAB, NULL, 0 };
struct wtable m3_words[] = { "center", M3_CENTER, "zoom", M3_ZOOM, NULL, 0 };
struct wtable onoff_words[] = { "off", 0, "on", 1, NULL, 0 };

struct wtable series_words[] = {
    "state", S_STATE,
    "atlas", S_ATLAS,
    "500k", S_500K,
    "500K", S_500K,
    "100k", S_100K,
    "100K", S_100K,
    "24k", S_24K,
    "24K", S_24K,
#ifdef TERRA
    "2m", S_TOPO_2M,
    "8m", S_TOPO_8M,
    "32m", S_TOPO_32M,
#endif
    NULL, 0 
};

/* XXX - Should move to utils.c
 * since this is now used also by places.c
 */
void
gronk_series ( int *rv, char *val )
{
	gronk_word ( rv, val, series_words );
}

/* This is cheesy, but the expectation is that the user will
 * do something like "up_key u"
 * and this will extract the ascii for 'u'
 */
void
gronk_key ( int *rv, char *val )
{
	*rv = *val;
}

static void
set_one ( char *name )
{
	if ( strcmp ( name, "clear_archive" ) == 0 )
	    archive_clear ();
}

static void
set_two ( char *name, char *val )
{
	if ( strcmp ( name, "verbose" ) == 0 )
	    settings.verbose = strtol ( val, NULL, 16 );
	else if ( strcmp ( name, "x_view" ) == 0 )
	    settings.x_view = atol ( val );
	else if ( strcmp ( name, "y_view" ) == 0 )
	    settings.y_view = atol ( val );
	else if ( strcmp ( name, "center_marker" ) == 0 )
	    gronk_word ( (int *) &settings.center_marker, val, onoff_words );
	else if ( strcmp ( name, "marker_size" ) == 0 )
	    settings.marker_size = atol ( val );
	else if ( strcmp ( name, "show_maplets" ) == 0 )
	    gronk_word ( (int *) &settings.show_maplets, val, onoff_words );
	else if ( strcmp ( name, "starting_long" ) == 0 )
	    settings.starting_long = parse_dms ( val );
	else if ( strcmp ( name, "starting_lat" ) == 0 )
	    settings.starting_lat = parse_dms ( val );
	else if ( strcmp ( name, "starting_series" ) == 0 ) {
	    /*
	    gronk_word ( (int *) &settings.starting_series, val, series_words );
	    */
	    gronk_series ( (int *) &settings.starting_series, val );
	}
	else if ( strcmp ( name, "m1_action" ) == 0 )
	    gronk_word ( (int *) &settings.m1_action, val, m1_words );
	else if ( strcmp ( name, "m3_action" ) == 0 )
	    gronk_word ( (int *) &settings.m3_action, val, m3_words );
	else if ( strcmp ( name, "up_key" ) == 0 )
	    gronk_key ( (int *) &settings.up_key, val );
	else if ( strcmp ( name, "down_key" ) == 0 )
	    gronk_key ( (int *) &settings.down_key, val );
	else if ( strcmp ( name, "add_archive" ) == 0 )
	    archive_add ( val );
}

#define MAX_LINE	128
#define MAX_WORDS	8

static void
load_settings ( char *path )
{
	FILE *fp;
	char line[MAX_LINE];
	char *wp[MAX_WORDS];
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

	    nw = split_q ( line, wp, MAX_WORDS );
	    /* printf ( "%d  %s  %s\n", nw, wp[0], wp[1] ); */

	    if ( nw == 1 )
	    	set_one ( wp[0] );

	    if ( nw == 2 )
	    	set_two ( wp[0], wp[1] );
	}

	fclose ( fp );
}

void
settings_init ( void )
{
	char buf[128];
	char *home;

	settings_default ();

	load_settings ( "/etc/gtopo/config" );

	home = find_home ();
	if ( home ) {
	    strcpy ( buf, home );
	    strcat ( buf, "/.gtopo/config" );
	    load_settings ( buf );
	}
}

/* THE END */
