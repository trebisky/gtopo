/* Tom Trebisky  MMT Observatory, Tucson, Arizona
 * archive.c -- navigate the directory structure
 *  to find individual TPQ files.
 * part of gtopo  7/6/2007
 */
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

extern struct topo_info info;

static struct series series_info[N_SERIES];

/* There are 5 map series,
 * level 1 is the state as a whole on the screen
 * level 2 "national atlas"
 * level 3 500K
 * level 4 100K
 * level 5 24K 7.5 minute quads in each TPQ file
 *
 * For California, level 3 is a single TPQ file
 * with 440 maplets.  22 wide and 20 tall.
 * Each maplet is 0.5 degrees square and is
 * 384 pixels wide by 480 pixels tall.
 * The pixel scale is true at latitude 36.87 degrees north
 * Level 3 covers 114W to 125W and 32N to 42N.
 *
 * For Arizona, level 3 is 4 TPQ files, each
 * covering a 5x5 degree area:
 *	F30105 F30110 F35105 F35110
 * Each tpq file holds 100 maplets (10x10),
 * The lower two have 382x480 pixel maplets
 * The upper two have 406x480 pixel maplets
 *  each maplet is 0.5 degrees square.
 * Level 3 covers 108W to 115W and 31N to 38N
 *
 * The Arizona set also has a directory that collects
 * all the level 4 maps (AZ1_MAP4), which is unlike
 * the California set.
 */

/* This subsystem keeps a linked list of "sections"
 * the idea being you can give a latitude and longitude
 * to the nearest degree and be able to get a path to
 * the directory holding the stuff for that 1x1 degree
 * chunk of the world.
 * XXX - when we are on a section on the edge of two
 * states we may need to search two (or more) of these
 * directories to find all the maps along the border.
 */
struct section {
	struct section *next;
	int	latlong;
	char	*path;
	int	q_code;
};

struct section *section_head;

/* Prototypes ... */
int add_archive ( char * );
int add_disk ( char *, char * );
static struct section *lookup_section ( int, int );

char *
strhide ( char *data )
{
	int n = strlen(data);
	char *rv;

	rv = malloc ( n + 1 );
	strcpy ( rv, data );
	return rv;
}

/* What we want here are a couple of interators.
 * This is kinda goofy, but I like having the series
 * structure local to this file only ...
 */
void
invalidate_pixels ( void )
{
	int i;

	for ( i=0; i<N_SERIES; i++ ) {
	    free_pixels ( &series_info[i] );
	    series_info[i].content = 0;
	}
}

void
invalidate_pixel_content ( void )
{
	int i;

	for ( i=0; i<N_SERIES; i++ )
	    series_info[i].content = 0;
}

int
archive_init ( char *archives[] )
{
	char **p;
	int nar = 0;
	struct series *sp;

	section_head = (struct section *) NULL;

	/* 7.5 minute quadrangle files
	 * 64 of these in a square degree
	 */
	sp = &series_info[S_24K];
	    sp->series = S_24K;
	    sp->cache = (struct maplet *) NULL;
	    sp->cache_count = 0;
	    sp->pixels = NULL;
	    sp->content = 0;

	    sp->lat_count = 10;
	    sp->long_count = 5;
	    sp->lat_count_d = 8;
	    sp->long_count_d = 8;
	    sp->map_lat_deg = 1.0 / 8.0;
	    sp->map_long_deg = 1.0 / 8.0;
	    sp->maplet_lat_deg = sp->map_lat_deg / sp->lat_count;
	    sp->maplet_long_deg = sp->map_long_deg / sp->long_count;
	    sp->quad_lat_count = 1;
	    sp->quad_long_count = 1;

	/* 2 of these in a square degree
	 * one of top of the other a1 and e1
	 */
	sp = &series_info[S_100K];
	    sp->series = S_100K;
	    sp->cache = (struct maplet *) NULL;
	    sp->cache_count = 0;
	    sp->pixels = NULL;
	    sp->content = 0;

	    sp->lat_count = 8;
	    sp->long_count = 16;
	    sp->lat_count_d = 2;
	    sp->long_count_d = 1;
	    sp->map_lat_deg = 1.0 / 2.0;
	    sp->map_long_deg = 1.0;
	    sp->maplet_lat_deg = sp->map_lat_deg / sp->lat_count;
	    sp->maplet_long_deg = sp->map_long_deg / sp->long_count;
	    sp->quad_lat_count = 4;
	    sp->quad_long_count = 8;

	/* XXX */
	sp = &series_info[S_500K];
	    sp->series = S_500K;
	    sp->cache = (struct maplet *) NULL;
	    sp->cache_count = 0;
	    sp->pixels = NULL;
	    sp->content = 0;

	    sp->lat_count = 10;
	    sp->long_count = 5;
	    sp->lat_count_d = 1;
	    sp->long_count_d = 1;
	    sp->map_lat_deg = 1.0 / 8.0;
	    sp->map_long_deg = 1.0 / 8.0;
	    sp->maplet_lat_deg = sp->map_lat_deg / sp->lat_count;
	    sp->maplet_long_deg = sp->map_long_deg / sp->long_count;
	    sp->quad_lat_count = 1;
	    sp->quad_long_count = 1;

	/* XXX */
	sp = &series_info[S_ATLAS];
	    sp->series = S_ATLAS;
	    sp->cache = (struct maplet *) NULL;
	    sp->cache_count = 0;
	    sp->pixels = NULL;
	    sp->content = 0;

	    sp->lat_count = 10;
	    sp->long_count = 5;
	    sp->lat_count_d = 1;
	    sp->long_count_d = 1;
	    sp->map_lat_deg = 1.0 / 8.0;
	    sp->map_long_deg = 1.0 / 8.0;
	    sp->maplet_lat_deg = sp->map_lat_deg / sp->lat_count;
	    sp->maplet_long_deg = sp->map_long_deg / sp->long_count;
	    sp->quad_lat_count = 1;
	    sp->quad_long_count = 1;

	/* XXX - The entire state */
	sp = &series_info[S_STATE];
	    sp->series = S_STATE;
	    sp->cache = (struct maplet *) NULL;
	    sp->cache_count = 0;
	    sp->pixels = NULL;
	    sp->content = 0;

	    sp->lat_count = 1;
	    sp->long_count = 1;
	    sp->lat_count_d = 1;
	    sp->long_count_d = 1;
	    sp->quad_lat_count = 1;
	    sp->quad_long_count = 1;

	    /* XXX - true for arizona */
	    sp->map_lat_deg = 7.0;
	    sp->map_long_deg = 7.0;
	    sp->maplet_lat_deg = 7.0;
	    sp->maplet_long_deg = 7.0;

	    /* XXX - true for california */
	    sp->map_lat_deg = 10.0;
	    sp->map_long_deg = 11.0;
	    sp->maplet_lat_deg = 10.0;
	    sp->maplet_long_deg = 11.0;

	for ( p=archives; *p; p++ ) {
	    if ( add_archive ( *p ) )
		nar++;
	    else if ( info.verbose )
	    	printf ( "Not a topo archive: %s\n", *p );
	}

	return nar;
}

void
toggle_series ( void )
{
	if ( info.series->series == S_24K )
	    set_series ( S_100K );
	else
	    set_series ( S_24K );
}

void
set_series ( enum s_type s )
{
	info.series = &series_info[s];
}

/* Try both upper and lower case path names for the tpq file
 */
static char *
quad_path ( struct section *ep, int lat_section, int long_section, int lat_quad, int long_quad )
{
	char path_buf[100];
	struct stat stat_buf;
	int lat_q, long_q;
	int series_letter;

	/* give a-h for latitude (a at the south)
	 *  and 1-8 for longitude (1 at the east)
	 * These run through the full range for 7.5 minute quads.
	 * For the 100K series, within a section, we only get 
	 *  k37118a1 and k37118e1
	 */
	lat_q  = 'a' + lat_quad * info.series->quad_lat_count;
	long_q = '1' + long_quad * info.series->quad_long_count;

	/* XXX */
	series_letter = ep->q_code;
	if ( info.series->series != S_24K ) {
	    if ( series_letter == 'q' )
		series_letter = 'k';
	    else
		series_letter = 'c';
	}

	sprintf ( path_buf, "%s/%c%2d%03d%c%c.tpq", ep->path, series_letter, lat_section, long_section, lat_q, long_q );
	printf ( "Trying %d %d -- %s\n", lat_quad, long_quad, path_buf );

	if ( stat ( path_buf, &stat_buf ) >=  0 )
	    if ( S_ISREG(stat_buf.st_mode) )
		return strhide ( path_buf );

	/* Try upper case */
	lat_q  = toupper(lat_q);
	series_letter = toupper(series_letter);

	sprintf ( path_buf, "%s/%c%2d%03d%c%c.TPQ", ep->path, series_letter, lat_section, long_section, lat_q, long_q );
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
lookup_quad_nbr ( struct maplet *mp, int maplet_lat, int maplet_long )
{
	struct series *sp;
	struct section *ep;
	int lat_section, long_section;
	int lat_quad, long_quad;
	int m_long, m_lat;

	sp = info.series;

	lat_section = maplet_lat / (sp->lat_count_d * sp->lat_count);
	long_section = maplet_long / (sp->long_count_d * sp->long_count);

	printf ( "lookup_quad_nbr: %d %d\n", lat_section, long_section );

	ep = lookup_section ( lat_section, long_section );
	if ( ! ep )
	    return 0;

	printf ("lookup_quad_nbr, found section: %s\n", ep->path );

	lat_quad = maplet_lat / sp->lat_count - lat_section * sp->lat_count_d;
	long_quad = maplet_long / sp->long_count - long_section * sp->long_count_d;

	/* See if the map sheet is available.
	 */
	mp->tpq_path = quad_path ( ep, lat_section, long_section, lat_quad, long_quad );
	if ( ! mp->tpq_path )
	    return 0;

	/* Now figure which maplet within the sheet we need.
	 */
	m_long = maplet_long - long_quad * sp->long_count - long_section * sp->long_count * sp->long_count_d;
	m_lat = maplet_lat - lat_quad * sp->lat_count - lat_section * sp->lat_count * sp->lat_count_d;

	printf ( "lat/long quad, lat/long maplet: %d %d  %d %d\n", lat_quad, long_quad, maplet_lat, maplet_long );

	/* flip the count to origin from the NW corner */
	mp->x_maplet = sp->long_count - m_long - 1;
	mp->y_maplet = sp->lat_count - m_lat - 1;

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
lookup_quad ( struct maplet *mp )
{
	struct series *sp;
	struct section *ep;
	int lat_section, long_section;
	int lat_quad, long_quad;
	double maplet_long;
	double maplet_lat;
	double lat_deg_quad;
	double long_deg_quad;

	sp = info.series;

	/* section indices tell us which 1x1 degree chunk
	 * of the world we are dealing with
	 */
	lat_section = info.lat_deg;
	long_section = info.long_deg;

	printf ( "lookup for %.4f, %.4f\n", info.lat_deg, info.long_deg );

	ep = lookup_section ( lat_section, long_section );
	if ( ! ep )
	    return 0;

	/* This gives a unique index for the map.
	 * (0-7 for 7.5 minute quads).
	 */
	lat_quad = (info.lat_deg  - (double)lat_section) / sp->map_lat_deg;
	long_quad = (info.long_deg - (double)long_section) / sp->map_long_deg;

	/* See if the map sheet is available.
	 */
	mp->tpq_path = quad_path ( ep, lat_section, long_section, lat_quad, long_quad );
	if ( ! mp->tpq_path )
	    return 0;

	/* Now figure which maplet within the sheet we need.
	 */
	lat_deg_quad = info.lat_deg - (double)lat_section - ((double)lat_quad) * sp->map_lat_deg;
	long_deg_quad = info.long_deg - (double)long_section - ((double)long_quad) * sp->map_long_deg;

	/* These count from E to W and from S to N */
	maplet_long = long_deg_quad / sp->maplet_long_deg;
	maplet_lat = lat_deg_quad / sp->maplet_lat_deg;

	/* flip the count to origin from the NW corner */
	mp->x_maplet = sp->long_count - (int) maplet_long - 1;
	mp->y_maplet = sp->lat_count - (int) maplet_lat - 1;

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

	if ( info.verbose )
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

#define N_LETTERS	26
static int letter_count[N_LETTERS];

/* We have a directory that looks like a section.
 * Open and scan it, doing a tally of the TPQ files inside
 * and what the lead letters are in this directory.
 * For old 2.7 version TOPO directories the 7.5 minute
 * quads have the lead letter 'q', but in version 4.2
 * this changed to 'n'
 * XXX - a lot could be changed and improved here
 */
int
scan_section ( char *path )
{
	struct stat stat_buf;
	DIR *dd;
	struct dirent *dp;
	int i;
	int ch;
	int rv = 'q';

	if ( stat ( path, &stat_buf ) < 0 )
	    return 0;
	if ( ! S_ISDIR(stat_buf.st_mode) )
	    return 0;

	if ( ! (dd = opendir ( path )) )
	    return 0;

	for ( i=0; i< N_LETTERS; i++ )
	    letter_count[i] = 0;

	for ( ;; ) {
	    if ( ! (dp = readdir ( dd )) )
	    	break;
	    if ( strlen(dp->d_name) != 12 )
	    	continue;
	    if ( dp->d_name[9] != 't' && dp->d_name[9] != 'T' )
	    	continue;
	    ch = dp->d_name[0];
	    if ( ch >= 'a' && ch <= 'z' )
	    	letter_count[ch-'a']++;
	    if ( ch >= 'A' && ch <= 'Z' )
	    	letter_count[ch-'A']++;
	}

	closedir ( dd );

	if ( letter_count['n'-'a'] > 0 )
	    rv = 'n';

	return rv;
}

/* A section is a term we use only in this program for a 1x1 degree
 * piece of land, NOT a section in the one square mile land survey
 * nomenclature.  It is a directory that actually holds TPQ files
 * which represent individual 7.5 minute maps (and more!)
 */
int
add_section ( char *disk, char *section )
{
	char section_path[100];
	struct section *ep;
	int quad_code;

	sprintf ( section_path, "%s/%s", disk, section );

	quad_code = scan_section ( section_path );
	if ( ! quad_code )
	    return 0;

	ep = (struct section *) malloc ( sizeof(struct section) );
	if ( ! ep )
	    error ("Section new - out of memory\n", "" );

	ep->latlong = atol ( &section[1] );
	ep->path = strhide ( section_path );
	ep->next = section_head;
	ep->q_code = quad_code;

	section_head = ep;

	if ( info.verbose )
	    printf ( "Added section: %d  %s  %c\n", ep->latlong, ep->path, ep->q_code );

	return 1;
}

static struct section *
lookup_section ( int lat_section, int long_section )
{
	int latlong = lat_section * 1000 + long_section;
	struct section *ep;

	for ( ep = section_head; ep; ep = ep->next ) {
	    if ( ep->latlong == latlong )
	    	return ep;
	}
	return NULL;
}

/* THE END */
