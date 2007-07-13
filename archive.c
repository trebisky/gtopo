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

extern struct position cur_pos;
extern struct maplet *maplets[];

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
toggle_series ( void )
{
	if ( cur_pos.series == S_24K )
	    set_series ( S_100K );
	else
	    set_series ( S_24K );
}

void
set_series ( enum series s )
{
	struct position *pos;

	pos = &cur_pos;

	pos->series = s;
	pos->maplet_cache = maplets[s];

	/* 7.5 minute quadrangle files
	 * 64 of these in a square degree
	 */
	if ( s == S_24K ) {
	    pos->lat_count = 10;
	    pos->long_count = 5;
	    pos->lat_count_d = 8;
	    pos->long_count_d = 8;
	    pos->map_lat_deg = 1.0 / 8.0;
	    pos->map_long_deg = 1.0 / 8.0;
	    pos->maplet_lat_deg = pos->map_lat_deg / pos->lat_count;
	    pos->maplet_long_deg = pos->map_long_deg / pos->long_count;
	    pos->quad_lat_count = 1;
	    pos->quad_long_count = 1;
	    pos->q_code = 'q';
	}

	/* 2 of these in a square degree
	 * one of top of the other a1 and e1
	 */
	if ( s == S_100K ) {
	    pos->lat_count = 8;
	    pos->long_count = 16;
	    pos->lat_count_d = 2;
	    pos->long_count_d = 1;
	    pos->map_lat_deg = 1.0 / 2.0;
	    pos->map_long_deg = 1.0;
	    pos->maplet_lat_deg = pos->map_lat_deg / pos->lat_count;
	    pos->maplet_long_deg = pos->map_long_deg / pos->long_count;
	    pos->quad_lat_count = 4;
	    pos->quad_long_count = 8;
	    pos->q_code = 'k';
	}

	if ( s == S_500K ) {
	    pos->lat_count = 10;
	    pos->long_count = 5;
	    pos->lat_count_d = 1;
	    pos->long_count_d = 1;
	    pos->map_lat_deg = 1.0 / 8.0;
	    pos->map_long_deg = 1.0 / 8.0;
	    pos->maplet_lat_deg = pos->map_lat_deg / pos->lat_count;
	    pos->maplet_long_deg = pos->map_long_deg / pos->long_count;
	    pos->q_code = 'X';
	    pos->quad_lat_count = 1;
	    pos->quad_long_count = 1;
	}

	if ( s == S_ATLAS ) {
	    pos->lat_count = 10;
	    pos->long_count = 5;
	    pos->lat_count_d = 1;
	    pos->long_count_d = 1;
	    pos->map_lat_deg = 1.0 / 8.0;
	    pos->map_long_deg = 1.0 / 8.0;
	    pos->maplet_lat_deg = pos->map_lat_deg / pos->lat_count;
	    pos->maplet_long_deg = pos->map_long_deg / pos->long_count;
	    pos->q_code = 'X';
	    pos->quad_lat_count = 1;
	    pos->quad_long_count = 1;
	}

	/* The entire state */
	if ( s == S_STATE ) {
	    pos->lat_count = 1;
	    pos->long_count = 1;
	    pos->lat_count_d = 1;
	    pos->long_count_d = 1;
	    pos->q_code = 'X';
	    pos->quad_lat_count = 1;
	    pos->quad_long_count = 1;

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

/* Try both upper and lower case path names for the tpq file
 */
char *
quad_path ( char *section_path, int lat_section, int long_section, int lat_quad, int long_quad )
{
	char path_buf[100];
	struct stat stat_buf;
	int lat_q, long_q;
	int series_q;

	/* give a-h for latitude (a at the south)
	 *  and 1-8 for longitude (1 at the east)
	 * These run through the full range for 7.5 minute quads.
	 * For the 100K series, within a section, we only get 
	 *  k37118a1 and k37118e1
	 */
	lat_q  = 'a' + lat_quad * cur_pos.quad_lat_count;
	long_q = '1' + long_quad * cur_pos.quad_long_count;

	series_q = cur_pos.q_code;

	sprintf ( path_buf, "%s/%c%2d%03d%c%c.tpq", section_path, series_q, lat_section, long_section, lat_q, long_q );
	printf ( "Trying %d %d -- %s\n", lat_quad, long_quad, path_buf );

	if ( stat ( path_buf, &stat_buf ) >=  0 )
	    if ( S_ISREG(stat_buf.st_mode) )
		return strhide ( path_buf );

	/* Try upper case */
	lat_q  = 'A' + lat_quad;
	series_q = toupper(series_q);

	sprintf ( path_buf, "%s/%c%2d%03d%c%c.TPQ", section_path, series_q, lat_section, long_section, lat_q, long_q );
	printf ( "Trying %d %d -- %s\n", lat_quad, long_quad, path_buf );

	if ( stat ( path_buf, &stat_buf ) >=  0 )
	    if ( S_ISREG(stat_buf.st_mode) )
		return strhide ( path_buf );

	return NULL;
}

/* This gets called when we are looking for a neighboring
 * maplet, have not found it in the cache, and it is not
 * on the same sheet as the center maplet.
 */
int
lookup_quad_nbr ( struct position *pos, struct maplet *mp, int maplet_lat, int maplet_long )
{
	int lat_section, long_section;
	int lat_quad, long_quad;
	int m_long, m_lat;
	char *section_path;

	lat_section = maplet_lat / (pos->lat_count_d * pos->lat_count);
	long_section = maplet_long / (pos->long_count_d * pos->long_count);

	printf ( "lookup_quad_nbr: %d %d\n", lat_section, long_section );

	section_path = lookup_section ( lat_section, long_section );
	if ( ! section_path )
	    return 0;

	printf ("lookup_quad_nbr, found section: %s\n", section_path );

	lat_quad = maplet_lat / pos->lat_count - lat_section * pos->lat_count_d;
	long_quad = maplet_long / pos->long_count - long_section * pos->long_count_d;

	/* See if the map sheet is available.
	 */
	mp->tpq_path = quad_path ( section_path, lat_section, long_section, lat_quad, long_quad );
	if ( ! mp->tpq_path )
	    return 0;

	/* Now figure which maplet within the sheet we need.
	 */
	m_long = maplet_long - long_quad * pos->long_count - long_section * pos->long_count * pos->long_count_d;
	m_lat = maplet_lat - lat_quad * pos->lat_count - lat_section * pos->lat_count * pos->lat_count_d;

	printf ( "lat/long quad, lat/long maplet: %d %d  %d %d\n", lat_quad, long_quad, maplet_lat, maplet_long );

	/* flip the count to origin from the NW corner */
	mp->x_maplet = pos->long_count - m_long - 1;
	mp->y_maplet = pos->lat_count - m_lat - 1;

	return 1;
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
	int lat_section, long_section;
	int lat_quad, long_quad;
	double maplet_long;
	double maplet_lat;
	double lat_deg_quad;
	double long_deg_quad;
	char *section_path;

	/* section indices tell us which 1x1 degree chunk
	 * of the world we are dealing with
	 */
	lat_section = pos->lat_deg;
	long_section = pos->long_deg;

	printf ( "lookup for %.4f, %.4f\n", pos->lat_deg, pos->long_deg );

	section_path = lookup_section ( lat_section, long_section );
	if ( ! section_path )
	    return 0;

	/* This gives a unique index for the map.
	 * (0-7 for 7.5 minute quads).
	 */
	lat_quad = (pos->lat_deg  - (double)lat_section) / pos->map_lat_deg;
	long_quad = (pos->long_deg - (double)long_section) / pos->map_long_deg;

	/* See if the map sheet is available.
	 */
	mp->tpq_path = quad_path ( section_path, lat_section, long_section, lat_quad, long_quad );
	if ( ! mp->tpq_path )
	    return 0;

	/* Now figure which maplet within the sheet we need.
	 */
	lat_deg_quad = pos->lat_deg - (double)lat_section - ((double)lat_quad) * pos->map_lat_deg;
	long_deg_quad = pos->long_deg - (double)long_section - ((double)long_quad) * pos->map_long_deg;

	/* These count from E to W and from S to N */
	maplet_long = long_deg_quad / pos->maplet_long_deg;
	maplet_lat = lat_deg_quad / pos->maplet_lat_deg;

	/* flip the count to origin from the NW corner */
	mp->x_maplet = pos->long_count - (int) maplet_long - 1;
	mp->y_maplet = pos->lat_count - (int) maplet_lat - 1;

	return 1;
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
lookup_section ( int lat_section, int long_section )
{
	int latlong = lat_section * 1000 + long_section;
	struct _section *sp;

	for ( sp = section_head; sp; sp = sp->next ) {
	    if ( sp->latlong == latlong )
	    	return sp->path;
	}
	return NULL;
}

/* THE END */
