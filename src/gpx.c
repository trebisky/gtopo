/*
 *  GTopo - gpx.c
 *
 *  Copyright (C) 2020, Thomas J. Trebisky
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

#include "gpx.h"

#define skip_sp(x)	while ( *x == ' ' ) x++

/* This handles the loading of information from gpx files.
 * The actual display of the information is handled in overlay.c
 *
 * Tom Trebisky 6-21-2020
 */

void read_gpx ( FILE *, int );
void do_wpt ( FILE *, char *, int );
void do_trk ( FILE *, char *, int );

void new_waypoint ( float, float );
void new_track ( float track[][2], int );

static void
gpx_error ( char *msg )
{
    fprintf ( stderr, "%s\n", msg );
    exit ( 1 );
}

static void
gpx_error2 ( char *msg, char *extra )
{
    fprintf ( stderr, "%s %s\n", msg, extra );
    exit ( 1 );
}


/* The idea here is to search for the gpx file in various places.
 * If I hard path (beginning with "dot" or a slash is given,
 * we try it verbatim.  Otherwise, we look first in the
 * current directory, then in the ~/.gtopo directory.
 */
#define PATH_SIZE	128

static char config_dir[] = "/.gtopo/";

static FILE *
find_file ( char *gpx_file )
{
	FILE *rv;
	static char gpx_path[PATH_SIZE];

	if ( gpx_file[0] == '.' || gpx_file[0] == '/' ) {
	    return fopen ( gpx_file, "r" );
	}
	rv = fopen ( gpx_file, "r" );
	if ( rv )
	    return rv;
	strncpy ( gpx_path, getenv("HOME"), PATH_SIZE );
	strncat ( gpx_path, config_dir, PATH_SIZE-1 );
	strncat ( gpx_path, gpx_file, PATH_SIZE-1 );
	// printf ( "%s\n", gpx_path );
	return fopen ( gpx_path, "r" );
}

static void gpx_add ( char *gpx_file, int is_way )
{
	FILE *gfile;

	gfile = find_file ( gpx_file );
	if ( ! gfile ) {
	    gpx_error2 ("Cannot open:", gpx_file );
	}

	// printf ( "Read gpx file: %s\n", gpx_file );

	read_gpx ( gfile, is_way );
	fclose ( gfile );
}

void gpx_waypoints_add ( char *gpx_file )
{
	// printf ( "Load waypoints from: %s\n", gpx_file );
	gpx_add ( gpx_file, 1 );
}

void gpx_tracks_add ( char *gpx_file )
{
	// printf ( "Load tracks from: %s\n", gpx_file );
	gpx_add ( gpx_file, 0 );
}

/* The second line in the header is huge (376 bytes) */
#define MAX_LINE	512

enum GPX_mode { START, START2, READY };

// #define MAX_GPX	1024
// For the AZT (219265 points)
#define MAX_GPX	250000

static float gpx_points[MAX_GPX][2];

void
read_gpx ( FILE *file, int is_way )
{
    char buf[MAX_LINE];
    enum GPX_mode mode = START;
    char *p;

    /* fgets returns the newline */
    while ( fgets ( buf, MAX_LINE, file ) ) {
	p = buf;
	skip_sp ( p );

	if ( mode == START ) {
	    if ( strncmp ( p, "<?xml ", 6 ) != 0 )
		gpx_error ( "Not a GPX file" );
	    mode = START2;
	    continue;
	}
	if ( mode == START2 ) {
	    if ( strncmp ( p, "<gpx ", 5 ) != 0 )
		gpx_error ( "Malformed GPX file" );
	    mode = READY;
	    continue;
	}
	if ( mode == READY ) {
	    if ( strncmp ( p, "<wpt ", 5 ) == 0 ) {
		// printf ( "read_gpx -- waypoint\n" );
		do_wpt ( file, p, ! is_way );
	    }
	    else if ( strncmp ( p, "<trk>", 5 ) == 0 ) {
		do_trk ( file, p, is_way );
		// printf ( "read_gpx -- track\n" );
	    }
	    else if ( strncmp ( p, "</gpx>", 6 ) == 0 ) {
		/* This ends the file, just skip it */
	    }
	    else
		gpx_error2 ( "Unexpected: ", p );

	}
    }
}

/* Get the two values out of a line like this:
 * <wpt lat="31.71343172" lon="-110.873604761">
 */
void
get_ll ( char *vals[], char *line )
{
    char *p = line;
    int index = 0;
    char c;
    int skip = 1;
    char *vp;

    // vp = vals[0];
    // printf ( "%016x\n", vp );
    // vp = vals[index];
    // printf ( "%016x\n", vp );

    for ( p = line; c = *p; p++ ) {
	if ( c == '"' ) {
	    if ( skip ) {
		skip = 0;
		vp = vals[index++];
	    } else {
		skip = 1;
		*vp = '\0';
	    }
	} else {
	    if ( ! skip ) {
		//printf ( "Add : %c\n", c );
		*vp++ = c;
	    } else {
		//printf ( "Skip : %c\n", c );
	    }
	}
    }
}

#define MAX_VAL	64

void do_wpt ( FILE *file, char *line, int skip )
{
    char buf[MAX_LINE];
    char *ll[2];
    char lat[MAX_VAL];
    char lon[MAX_VAL];
    // double flat, flon;
    char *p;

    // printf ( "WPT !!\n" );
    // printf ( line );

    ll[0] = lat;
    ll[1] = lon;
    get_ll ( ll, line );

    // printf ( "Lat = %s\n", ll[0] );
    // printf ( "Lon = %s\n", ll[1] );
    // flat = atof ( lat );
    // flon = atof ( lon );
    if ( ! skip ) {
	// printf ( "Waypoint: %s, %s\n", lat, lon );
	new_waypoint ( atof(lat), atof(lon) );
    }

    while ( fgets ( buf, MAX_LINE, file ) ) {
	// printf ( buf );
	p = buf;
	skip_sp ( p );

	if ( strncmp ( p, "</wpt>", 6 ) == 0 )
	    break;
    }
}

void
get_thing ( char *buf, char *thing )
{
    char *p;
    int skip = 1;
    char c;

    for ( p = buf; c = *p; p++ ) {
	if ( skip ) {
	    if ( c == '>' )
		skip = 0;
	} else {
	    if ( c == '<' )
		break;
	    *thing++ = c;
	}
    }
    *thing = '\0';
}

void do_trk ( FILE *file, char *line, int skip )
{
    char buf[MAX_LINE];
    char *p;
    char thing_buf[MAX_VAL];
    char *ll[2];
    char lat[MAX_VAL];
    char lon[MAX_VAL];
    int count = 0;
    static int first_warning = 0;

    ll[0] = lat;
    ll[1] = lon;

    //printf ( "\n" );
    // printf ( "TRK !!\n" );
    // printf ( line );
    while ( fgets ( buf, MAX_LINE, file ) ) {
	// printf ( buf );
	p = buf;
	skip_sp ( p );

	if ( strncmp ( buf, "</trk>", 6 ) == 0 ) {
	    break;
	} else if ( strncmp ( p, "<name", 5 ) == 0 ) {
	    get_thing ( p, thing_buf );
	    // printf ( "Name: %s\n", thing_buf );
	} else if ( strncmp ( p, "<trkpt", 6 ) == 0 ) {
	    get_ll ( ll, p );
	    if ( ! skip ) {
		// printf ( "Track point: %s, %s\n", lat, lon );
		if ( count > MAX_GPX ) {
		    if ( ! first_warning ) {
			first_warning = 1;
			printf ( "Too many points in GPS file, limit %d (truncating)\n", MAX_GPX );
		    }
		} else {
		    gpx_points[count][0] = atof ( lat );
		    gpx_points[count][1] = atof ( lon );
		    count++;
		}
	    }
	} else {
	    ;
	    // printf ( p );
	}

    }
    if ( count > 0 )
	new_track ( gpx_points, count );
}

/* =========================================== */

struct waypoint *way_head;
struct track *track_head;

void
gpx_init ( void )
{
	way_head = NULL;
	track_head = NULL;
}

void
new_waypoint ( float wlat, float wlon )
{
    struct waypoint *wp;

    // printf ( "New waypoint: %.6f, %.6f\n", wlat, wlon );

    wp = (struct waypoint *) gmalloc ( sizeof(struct waypoint) );
    wp->way_lat = wlat;
    wp->way_long = wlon;

    wp->next = way_head;
    way_head = wp;
}

void
new_track ( float track[][2], int num )
{
    struct track *tp;
    int size;
    int i;

    // printf ( "New track: %d points\n", num );

    tp = (struct track *) gmalloc ( sizeof(struct track) );
    tp->count = num;
    
    tp->lat_min = track[0][0];
    tp->lat_max = track[0][0];
    tp->long_min = track[0][1];
    tp->long_max = track[0][1];

    for ( i=0; i<num; i++ ) {
	if ( track[i][0] < tp->lat_min ) tp->lat_min = track[i][0];
	if ( track[i][0] > tp->lat_max ) tp->lat_max = track[i][0];
	if ( track[i][1] < tp->long_min ) tp->long_min = track[i][1];
	if ( track[i][1] > tp->long_max ) tp->long_max = track[i][1];
    }

    size = 2 * num * sizeof(float);
    tp->data = (float *) gmalloc ( size );
    memcpy ( tp->data, track, size );

    tp->next = track_head;
    track_head = tp;
}

/* THE END */
