#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "gtopo.h"

/* Tom Trebisky  MMT Observatory, Tucson, Arizona
 * part of gtopo.c as of version 0.5.
 * 7/6/2007
 */

/* This subsystem keeps a linked list of "sections"
 * the idea being you can give a latitude and longitude
 * to the nearest degree and be able to get a path to
 * the directory holding the stuff for that 1x1 degree
 * chunk of the world.
 */
struct _section {
	struct _section *next;
	int	latlong;
	char	*path;
};

struct _section *section_head;

static int verbose = 0;

/* Prototypes ... */
int add_archive ( char * );
int add_disk ( char *, char * );
char *lookup_section ( int, int );

char *
strhide ( char *data )
{
	int n = strlen(data);
	char *rv;

	rv = malloc ( n + 1 );
	strcpy ( rv, data );
	return rv;
}

int
archive_init ( char *archives[], int verbose_arg )
{
	char **p;
	int nar = 0;

	section_head = (struct _section *) NULL;
	verbose = verbose_arg;

	for ( p=archives; *p; p++ ) {
	    if ( add_archive ( *p ) )
		nar++;
	    else if ( verbose )
	    	printf ( "Not a topo archive: %s\n", *p );
	}

	return nar;
}

void
set_series ( struct position *pos, enum series s )
{
	pos->series = s;

	/* 7.5 minute quadrangle */
	if ( s == S_24K ) {
	    pos->lat_count = 10;
	    pos->long_count = 5;
	    pos->map_lat_deg = 1.0 / 8.0;
	    pos->map_long_deg = 1.0 / 8.0;
	    pos->maplet_lat_deg = 1.0 / (8.0 * pos->lat_count );
	    pos->maplet_long_deg = 1.0 / (8.0 * pos->long_count );
	}

	/* The entire state */
	if ( s == S_STATE ) {
	    pos->lat_count = 1;
	    pos->long_count = 1;

	    /* XXX - true for arizona */
	    pos->map_lat_deg = 7.0;
	    pos->map_long_deg = 7.0;
	    pos->maplet_lat_deg = 7.0;
	    pos->maplet_long_deg = 7.0;

	    /* XXX - true for california */
	    pos->map_lat_deg = 10.0;
	    pos->map_long_deg = 11.0;
	    pos->maplet_lat_deg = 10.0;
	    pos->maplet_long_deg = 11.0;
	}
}

/* For some lat/long position, find the 7.5 minute quad file
 * containing it, and the indices of the maplet within that
 * file containing the position.
 * returns mp->tpq_path and mp->x/y_maplet
 *
 * Note that the map "codes" follow the way that lat and long
 * increase.  There are 64 quads in a 1x1 "section".
 * a1 is in the southeast, h8 is in the northwest.
 * i.e longitude become 1-8, latitude a-h
 */
int
lookup_quad ( struct position *pos, struct maplet *mp )
{
	struct stat stat_buf;
	int lat_int, long_int;
	int lat_index, long_index;
	int lat_q, long_q;
	double maplet_long, maplet_lat;
	double lat_deg_quad;
	double long_deg_quad;
	char *section_path;
	char path_buf[100];

	lat_int = pos->lat_deg;
	long_int = pos->long_deg;

	printf ( "lookup for %.4f, %.4f\n", pos->lat_deg, pos->long_deg );

	section_path = lookup_section ( lat_int, long_int );
	if ( ! section_path )
	    return 0;

	/* This will yield indexes from 0-7,
	 * then a-h for latitude (a at the south)
	 *  and 1-8 for longitude (1 at the east)
	 */
	lat_index = (pos->lat_deg  - (double)lat_int) / pos->map_lat_deg;
	long_index = (pos->long_deg - (double)long_int) / pos->map_long_deg;

	lat_q  = 'a' + lat_index;
	long_q = '1' + long_index;

	/* These are values in degrees that specify where on the quadrangle we are at.
	 * Origin is 0,0 at the SE corner
	 */
	lat_deg_quad = pos->lat_deg - (double)lat_int - ((double)lat_index) * pos->map_lat_deg;
	long_deg_quad = pos->long_deg - (double)long_int - ((double)long_index) * pos->map_long_deg;

	/* These count from E to W and from S to N */
	maplet_long = long_deg_quad / pos->maplet_long_deg;
	maplet_lat = lat_deg_quad / pos->maplet_lat_deg;

	/* flip the count to origin from the NW corner */
	mp->x_maplet = pos->long_count - 1 - (int) maplet_long;
	mp->y_maplet = pos->lat_count - 1 - (int) maplet_lat;

	sprintf ( path_buf, "%s/q%2d%03d%c%c.tpq", section_path, lat_int, long_int, lat_q, long_q );
	printf ( "Trying %s\n", path_buf );

	if ( stat ( path_buf, &stat_buf ) >=  0 ) {
	    if ( S_ISREG(stat_buf.st_mode) ) {
		mp->tpq_path = strhide ( path_buf );
		return 1;
	    }
	}

	/* Try upper case */
	lat_q = toupper ( lat_q );
	sprintf ( path_buf, "%s/Q%2d%03d%c%c.TPQ", section_path, lat_int, long_int, lat_q, long_q );
	printf ( "Trying %s\n", path_buf );

	if ( stat ( path_buf, &stat_buf ) >=  0 ) {
	    if ( S_ISREG(stat_buf.st_mode) ) {
		mp->tpq_path = strhide ( path_buf );
		return 1;
	    }
	}

	return 0;
}

int
add_archive ( char *archive )
{
	struct stat stat_buf;
	DIR *dd;
	struct dirent *dp;

	if ( stat ( archive, &stat_buf ) < 0 )
	    return 0;
	if ( ! S_ISDIR(stat_buf.st_mode) )
	    return 0;

	if ( ! (dd = opendir ( archive )) )
	    return 0;

	/* Loop through this possible archive, look
	 * for files with names 6 characters in length,
	 * and the pattern: CA_D06 or AZ_D05 and in
	 * any case.
	 */
	for ( ;; ) {
	    if ( ! (dp = readdir ( dd )) )
	    	break;
	    if ( strlen(dp->d_name) != 6 )
	    	continue;
	    if ( dp->d_name[2] != '_' )
	    	continue;
	    if ( dp->d_name[3] == 'D' || dp->d_name[3] == 'd' )
	    	add_disk ( archive, dp->d_name );
	}

	closedir ( dd );
	return 1;
}

int
add_disk ( char *archive, char *disk )
{
	struct stat stat_buf;
	DIR *dd;
	struct dirent *dp;
	char disk_path[100];

	sprintf ( disk_path, "%s/%s", archive, disk );

	if ( stat ( disk_path, &stat_buf ) < 0 )
	    return 0;
	if ( ! S_ISDIR(stat_buf.st_mode) )
	    return 0;

	if ( ! (dd = opendir ( disk_path )) )
	    return 0;

	if ( verbose )
	    printf ( "Found disk: %s\n", disk_path );

	/* Loop through this disk image.
	 */
	for ( ;; ) {
	    if ( ! (dp = readdir ( dd )) )
	    	break;
	    if ( strlen(dp->d_name) != 6 )
	    	continue;
	    if ( dp->d_name[0] == 'D' || dp->d_name[0] == 'd' )
	    	add_section ( disk_path, dp->d_name );
	}

	closedir ( dd );
	return 1;
}

/* A section is a term we use only in this program for a 1x1 degree
 * piece of land, NOT a section in the one square mile land survey
 * nomenclature.  It is a directory that actually holds TPQ files
 * which represent individual 7.5 minute maps (and more!)
 */
int
add_section ( char *disk, char *section )
{
	struct stat stat_buf;
	DIR *dd;
	char section_path[100];
	struct _section *sp;

	sprintf ( section_path, "%s/%s", disk, section );

	if ( stat ( section_path, &stat_buf ) < 0 )
	    return 0;
	if ( ! S_ISDIR(stat_buf.st_mode) )
	    return 0;

	/* Just verify we can open it */
	if ( ! (dd = opendir ( section_path )) )
	    return 0;
	closedir ( dd );

	sp = (struct _section *) malloc ( sizeof(struct _section) );
	if ( ! sp )
	    error ("Section new - out of memory\n", "" );

	sp->latlong = atol ( &section[1] );
	sp->path = strhide ( section_path );
	sp->next = section_head;
	section_head = sp;

	if ( verbose )
	    printf ( "Added section: %d  %s\n", sp->latlong, section_path );

	return 1;
}

char *
lookup_section ( int lat_deg, int long_deg )
{
	int latlong = lat_deg * 1000 + long_deg;
	struct _section *sp;

	for ( sp = section_head; sp; sp = sp->next ) {
	    if ( sp->latlong == latlong )
	    	return sp->path;
	}
	return NULL;
}

/* THE END */
