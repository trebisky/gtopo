/*
 *  GTopo
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

/* Tom Trebisky  MMT Observatory, Tucson, Arizona
 *  archive.c -- navigate the directory structure
 *  to find individual TPQ files.
 *  7/6/2007
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

#include <math.h>

#include "gtopo.h"

extern struct topo_info info;

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
 *
 * My really new Nevada set is different yet.
 *  for level 3, there is a g41117a1 file that
 *  contains 4 maplets.  The maplets for all of
 *  the 3 states I have are 0.5 by 0.5 degrees.
 *  For nevada there is one file per section,
 *  with 4 maplets in it.  Arizona has the big
 *  5x5 degree files with 100 maplets,
 *  California has the one monster file for
 *  the entire state.
 */

/* This subsystem keeps a linked list of "sections"
 * the idea being you can give a latitude and longitude
 * to the nearest degree and be able to get a path to
 * the directory holding the stuff for that 1x1 degree
 * chunk of the world.
 */
struct section {
	struct section *next;
	struct section *next_ll;
	int	latlong;
	char	*path;
	int	q_code;
};

struct section *section_head = NULL;

/* Prototypes ... */
int add_archive ( char * );
int add_disk ( char *, char * );
static struct section *lookup_section ( int );

char *
strhide ( char *data )
{
	int n = strlen(data);
	char *rv;

	rv = malloc ( n + 1 );
	strcpy ( rv, data );
	return rv;
}

char *
str_lower ( char *data )
{
	char *rv, *p;

	p = rv = strhide ( data );
	for ( p = rv; *p; p++ )
	    *p = tolower ( *p );
	return rv;
}

int
is_directory ( char *path )
{
	struct stat stat_buf;

	if ( stat ( path, &stat_buf ) < 0 )
	    return 0;
	if ( S_ISDIR(stat_buf.st_mode) )
	    return 1;
	return 0;
}

int
is_file ( char *path )
{
	struct stat stat_buf;

	if ( stat ( path, &stat_buf ) < 0 )
	    return 0;
	if ( S_ISREG(stat_buf.st_mode) )
	    return 1;
	return 0;
}


static int
add_file_method ( struct series *sp, char *path )
{
	struct method *xp;
	struct tpq_info *tp;

	xp = (struct method *) malloc ( sizeof(struct method) );
	if ( ! xp )
	    error ("file method - out of memory\n", "" );

	/* This reads the TPQ header, but NOT any maplet(s) */
	tp = tpq_lookup ( path );
	if ( ! tp )
	    return 0;

	if ( tp->series == S_STATE && tp->lat_count == 1 && tp->long_count == 1 )
	    xp->type = M_STATE;
	else
	    xp->type = M_FILE;
	xp->tpq = tp;
	xp->next = sp->methods;

	sp->methods = xp;
	return 1;
}

void
add_section_method ( struct series *sp, struct section *ep )
{
	struct method *xp;

	xp = (struct method *) malloc ( sizeof(struct method) );
	if ( ! xp )
	    error ("section method - out of memory\n", "" );

	xp->type = M_SECTION;
	xp->sections = ep;
	xp->next = sp->methods;

	sp->methods = xp;
}

static void
series_init ( struct series *sp, enum s_type series )
{
	sp->cache = (struct maplet *) NULL;
	sp->cache_count = 0;
	sp->pixels = NULL;
	sp->content = 0;
	sp->methods = NULL;
	sp->series = series;
}

char *wonk_series ( enum s_type series )
{
	if ( series == S_24K )
	    return "24K";
	if ( series == S_100K )
	    return "100K";
	if ( series == S_500K )
	    return "500K";
	if ( series == S_ATLAS )
	    return "ATLAS";
	if ( series == S_STATE )
	    return "STATE";
}

/* Print info about one TPQ file and exit */
void
file_info ( char *path )
{
	struct maplet *mp;
	struct tpq_info *tp;
	struct series *sp;
	double lat_scale, long_scale;
	int nn;

	if ( ! is_file(path) ) {
	    printf ( "No such file: %s\n", path );
	    return;
	}

	mp = load_maplet_any ( path );
	if ( ! mp ) {
	    printf ( "Cannot grog file: %s\n", path );
	    return;
	}

	printf ( "File info on: %s\n", path );

	tp = mp->tpq;

	sp = &info.series_info[tp->series];
	series_init ( sp, tp->series );
	info.series = sp;

	lat_scale = mp->tpq->maplet_lat_deg / mp->ydim;
	long_scale = mp->tpq->maplet_long_deg * cos ( mp->lat_deg * DEGTORAD ) / mp->xdim;

	printf ( "File: %s\n", path );
	printf ( " state: %s", tp->state );
	if ( strlen(tp->quad) > 1 )
	    printf ( " ( %s )", tp->quad );
	printf ( "\n" );
	printf ( " nlong x nlat = %d %d ", tp->long_count, tp->lat_count );
	nn = tp->long_count * tp->lat_count;
	if ( nn < 2 )
	    printf ( "(%d maplet)\n", nn );
	else
	    printf ( "(%d maplets)\n", nn );
	printf ( " longitude range = %.4f to %.4f\n", tp->w_long, tp->e_long );
	printf ( " latitude range = %.4f to %.4f\n", tp->s_lat, tp->n_lat );
	printf ( " maplet size (long, lat) = %.4f to %.4f\n", tp->maplet_long_deg, tp->maplet_lat_deg );
	printf ( " maplet pixels (x, y) = %d by %d\n", mp->xdim, mp->ydim );
	printf ( " lat scale: %.8f\n", lat_scale );
	printf ( " long scale: %.8f\n", long_scale );
	printf ( " series: %s\n", wonk_series ( tp->series ) );
}

/* This is called when we are initializing to view just
 * a single TPQ file.
 */
int
file_init ( char *path )
{
	struct maplet *mp;
	struct series *sp;
	struct tpq_info *tp;

	if ( ! is_file(path) )
	    return 0;

	mp = load_maplet_any ( path );
	if ( ! mp )
	    return 0;

	tp = mp->tpq;

	sp = &info.series_info[tp->series];
	series_init ( sp, tp->series );
	info.series = sp;

	if ( ! add_file_method ( sp, path ) )
	    return 0;

	/* XXX */
	sp->lat_count = tp->lat_count;
	sp->long_count = tp->long_count;

	sp->maplet_lat_deg = tp->maplet_lat_deg;
	sp->maplet_long_deg = tp->maplet_long_deg;
	synch_position ();

	set_position ( (tp->w_long + tp->e_long)/2.0, (tp->s_lat + tp->n_lat)/2.0 );

	return 1;
}

/* This is the usual initialization when we want to setup to
 * view a whole collection of potentially multiple states.
 */
int
archive_init ( char *archives[] )
{
	char **p;
	int nar;
	struct series *sp;

	/* 7.5 minute quadrangle files
	 * each in a single .tpq file
	 * 64 of these in a square degree
	 */
	sp = &info.series_info[S_24K];
	    series_init ( sp, S_24K );

	    /* Correct south of Tucson */
	    sp->xdim = 435;
	    sp->ydim = 256;

	    sp->lat_count = 10;
	    sp->long_count = 5;
	    sp->lat_count_d = 8;
	    sp->long_count_d = 8;
	    sp->maplet_lat_deg = 0.0125;
	    sp->maplet_long_deg = 0.0250;
	    sp->quad_lat_count = 1;
	    sp->quad_long_count = 1;

	/* 2 of these in a square degree
	 * one of top of the other a1 and e1
	 */
	sp = &info.series_info[S_100K];
	    series_init ( sp, S_100K );

	    /* Correct near Tucson */
	    sp->xdim = 333;
	    sp->ydim = 393;

	    sp->lat_count = 8;
	    sp->long_count = 16;
	    sp->lat_count_d = 2;
	    sp->long_count_d = 1;
	    sp->maplet_lat_deg = 0.0625;
	    sp->maplet_long_deg = 0.0625;
	    sp->quad_lat_count = 4;
	    sp->quad_long_count = 8;

	/* This varies from state to state, and the only
	 * thing that seems constant is that the maplets
	 * are 0.5 by 0.5 degrees on a side.
	 */
	sp = &info.series_info[S_500K];
	    series_init ( sp, S_500K );

	    /* Correct in Southern Nevada */
	    sp->xdim = 380;
	    sp->ydim = 480;

	    /* The following is only correct for Nevada
	     * for nevada these are g-files
	     */
	    sp->lat_count = 2;
	    sp->long_count = 2;
	    sp->lat_count_d = 1;
	    sp->long_count_d = 1;
	    sp->maplet_lat_deg = 0.5;
	    sp->maplet_long_deg = 0.5;
	    sp->quad_lat_count = 8;
	    sp->quad_long_count = 8;

	/* XXX */
	sp = &info.series_info[S_ATLAS];
	    series_init ( sp, S_ATLAS );

	    /* XXX */
	    sp->xdim = 400;
	    sp->ydim = 400;

	    sp->lat_count = 1;
	    sp->long_count = 1;
	    sp->lat_count_d = 1;
	    sp->long_count_d = 1;
	    sp->maplet_lat_deg = 1.0;
	    sp->maplet_long_deg = 1.0;
	    sp->quad_lat_count = 1;
	    sp->quad_long_count = 1;

	/* XXX - The entire state */
	sp = &info.series_info[S_STATE];
	    series_init ( sp, S_STATE );

	    /* XXX */
	    sp->lat_count = 1;
	    sp->long_count = 1;
	    sp->lat_count_d = 1;
	    sp->long_count_d = 1;
	    sp->quad_lat_count = 1;
	    sp->quad_long_count = 1;

	    /* XXX - true for california */
	    sp->maplet_lat_deg = 10.0;
	    sp->maplet_long_deg = 11.0;

	    /* California */
	    sp->xdim = 751;
	    sp->ydim = 789;

	    /* XXX - true for arizona */
	    sp->maplet_lat_deg = 7.0;
	    sp->maplet_long_deg = 7.0;

	    /* Arizona */
	    sp->xdim = 484;
	    sp->ydim = 549;

	nar = 0;
	for ( p=archives; *p; p++ ) {
	    if ( add_archive ( *p ) )
		nar++;
	    else if ( info.verbose )
	    	printf ( "Not a topo archive: %s\n", *p );
	}

	/* XXX */
	add_section_method ( &info.series_info[S_24K], section_head );
	add_section_method ( &info.series_info[S_100K], section_head );
	add_section_method ( &info.series_info[S_500K], section_head );

	return nar;
}

void
toggle_series ( void )
{
	int series;
	int nseries;

	series = info.series->series;

	nseries = series;
	do {
	    if ( nseries == S_24K )
	    	nseries = S_STATE;
	    else
		++nseries;

	    if ( info.series_info[nseries].methods ) {
		set_series ( nseries );
		return;
	    }
	} while ( nseries != series );
	/* fall out the end when no other series has
	 * methods (always happens in -f mode )
	 */
}

static char *
wonk_method ( int type )
{
    	if ( type == M_FILE )
	    return "File";
    	if ( type == M_STATE )
	    return "State";
	else if ( type == M_SECTION )
	    return "Section";
	else
	    return "Unknown";
}

void
show_methods ( struct series *sp )
{
	struct method *xp;

	for ( xp = sp->methods; xp; xp = xp->next ) {
	    printf ( "%s method: ", wonk_method(xp->type) );
	    if ( xp->type != M_SECTION )
		printf ( " %s", xp->tpq->path );
	    printf ( "\n" );
	}
}

void
set_series ( enum s_type s )
{
	info.series = &info.series_info[s];
	synch_position ();
	if ( info.verbose > 0 ) {
	    printf ( "Switch to series %d\n", s );
	    show_methods ( info.series );
	}
}

/* Try both upper and lower case path names for the tpq file
 */
static char *
section_map_path ( struct section *ep, int lat_section, int long_section, int lat_quad, int long_quad )
{
	char path_buf[100];
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
	if ( info.series->series == S_100K ) {
	    if ( series_letter == 'q' )
		series_letter = 'k';
	    else /* Nevada */
		series_letter = 'c';
	}
	if ( info.series->series == S_500K ) {
		/* Nevada */
		series_letter = 'g';
	}

	sprintf ( path_buf, "%s/%c%2d%03d%c%c.tpq", ep->path, series_letter, lat_section, long_section, lat_q, long_q );
	if ( info.verbose )
	    printf ( "Trying %d %d -- %s\n", lat_quad, long_quad, path_buf );

	if ( is_file(path_buf) )
	    return strhide ( path_buf );

	/* Try upper case */
	lat_q  = toupper(lat_q);
	series_letter = toupper(series_letter);

	sprintf ( path_buf, "%s/%c%2d%03d%c%c.TPQ", ep->path, series_letter, lat_section, long_section, lat_q, long_q );
	if ( info.verbose )
	    printf ( "Trying %d %d -- %s\n", lat_quad, long_quad, path_buf );

	if ( is_file(path_buf) )
	    return strhide ( path_buf );

	return NULL;
}

/* In the current scheme of things (which is handling equal sized maps
 * at each level), this find the TPQ file containing the point in question.
 * This will need to be generalized for level 3 (as well as levels 1 and 2)
 * because those levels may have collections of randomly sized TPQ files).
 * For example, Arizona has 4 files for level 3 covering 5x5 degrees.
 * California has one monster level 3 file, Nevada has 1x1 degree files
 * within the usual degree section setup.  Other states, who knows.
 */
static char *
section_find_map ( int lat_section, int long_section, int lat_quad, int long_quad )
{
	struct section *ep;
	char *rv;

	ep = lookup_section ( lat_section * 1000 + long_section );
	if ( ! ep )
	    return 0;

	/* Handle the case along state boundaries where multiple section
	 * directories cover the same area
	 */
	for ( ; ep; ep = ep->next_ll ) {
	    rv = section_map_path ( ep, lat_section, long_section, lat_quad, long_quad );
	    if ( rv )
	    	return rv;
	}
	return NULL;
}

/* We have a single file of some sort, we just need to figure
 * out if we are on the sheet, and if so, get the offsets.
 */
static int
method_file ( struct maplet *mp, struct method *xp,
		int maplet_long, int maplet_lat )
{
	int x_index, y_index;

	/* Now figure which maplet within the sheet we need.
	 */
	mp->sheet_index_long = maplet_long - xp->tpq->sheet_long;
	mp->sheet_index_lat = maplet_lat - xp->tpq->sheet_lat;

	if ( info.verbose > 2 ) {
	    printf ( "MF sheet long, lat: %d %d\n", xp->tpq->sheet_long, xp->tpq->sheet_lat );
	    printf ( "MF point : %d %d\n", maplet_long, maplet_lat );
	    printf ( "MF index: %d %d\n", mp->sheet_index_long, mp->sheet_index_lat );
	}

	if ( mp->sheet_index_long < 0 || mp->sheet_index_long >= xp->tpq->long_count )
	    return 0;
	if ( mp->sheet_index_lat < 0 || mp->sheet_index_lat >= xp->tpq->lat_count )
	    return 0;

	mp->tpq_path = xp->tpq->path;

	/* flip the count to origin from the NW corner */
	x_index = xp->tpq->long_count - mp->sheet_index_long - 1;
	y_index = xp->tpq->lat_count - mp->sheet_index_lat - 1;

	mp->tpq_index = y_index * xp->tpq->long_count + x_index;

	return 1;	
}

static int
method_section ( struct maplet *mp, struct method *xp,
		    int maplet_long, int maplet_lat )
{
	struct series *sp;
	int lat_section, long_section;
	int lat_quad, long_quad;
	int x_index, y_index;

	sp = info.series;

	lat_section = maplet_lat / (sp->lat_count_d * sp->lat_count);
	long_section = maplet_long / (sp->long_count_d * sp->long_count);

	if ( info.verbose )
	    printf ( "lookup_quad, section: %d %d\n", lat_section, long_section );

	lat_quad = maplet_lat / sp->lat_count - lat_section * sp->lat_count_d;
	long_quad = maplet_long / sp->long_count - long_section * sp->long_count_d;

	/* See if the map sheet is available.
	 */
	mp->tpq_path = section_find_map ( lat_section, long_section, lat_quad, long_quad );
	if ( ! mp->tpq_path )
	    return 0;

	/* Now figure which maplet within the sheet we need.
	 */
	mp->sheet_index_long = maplet_long - long_quad * sp->long_count - long_section * sp->long_count * sp->long_count_d;
	mp->sheet_index_lat = maplet_lat - lat_quad * sp->lat_count - lat_section * sp->lat_count * sp->lat_count_d;

	if ( info.verbose )
	    printf ( "lat/long quad, lat/long maplet: %d %d  %d %d\n", lat_quad, long_quad, maplet_lat, maplet_long );

	/* flip the count to origin from the NW corner */
	x_index = sp->long_count - mp->sheet_index_long - 1;
	y_index = sp->lat_count - mp->sheet_index_lat - 1;

	mp->tpq_index = y_index * sp->long_count + x_index;

	return 1;
}

/* This is the basic call to look for a maplet, when we
 * know it is not in the cache.
 * It returns with the following set:
 *	tpq_path
 *	tpq_index
 *	sheet_index_long
 *	sheet_index_lat
 */
int
lookup_series ( struct maplet *mp, int maplet_long, int maplet_lat )
{
	struct series *sp;
	struct method *xp;
	int done;

	sp = info.series;

	done = 0;
	/* We skip STATE_METHOD in this loop */
	for ( xp = sp->methods; xp; xp = xp->next ) {
	    if ( xp->type == M_SECTION )
	    	done = method_section ( mp, xp, maplet_long, maplet_lat );
	    if ( xp->type == M_FILE )
	    	done = method_file ( mp, xp, maplet_long, maplet_lat );
	    if ( done )
	    	return 1;
	}

	return 0;
}

int
add_archive ( char *archive )
{
	struct stat stat_buf;
	DIR *dd;
	struct dirent *dp;
	int rv = 0;

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
	    if ( dp->d_name[3] == 'D' || dp->d_name[3] == 'd' ) {
	    	add_disk ( archive, dp->d_name );
		rv = 1;
	    }
	}

	closedir ( dd );
	return rv;
}

int
add_disk ( char *archive, char *disk )
{
	DIR *dd;
	struct dirent *dp;
	char disk_path[100];

	sprintf ( disk_path, "%s/%s", archive, disk );

	if ( ! is_directory ( disk_path ) )
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
	    if ( strlen(dp->d_name) == 8 )
	    	add_dir ( disk_path, dp->d_name );
	    if ( strcmp(dp->d_name,"SI_D01") == 0 ) {
	    	add_full_usa ( disk_path, dp->d_name );
		continue;
	    }
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
	DIR *dd;
	struct dirent *dp;
	int i;
	int ch;
	int rv = 'q';

	if ( ! is_directory ( path ) )
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
 * Note that the same section can be covered by more than one path
 * if it is on the boundary of several states.
 */
int
add_section ( char *disk, char *section )
{
	char section_path[100];
	struct section *ep;
	struct section *eep;
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
	ep->next_ll = (struct section *) NULL;
	ep->next = (struct section *) NULL;
	ep->q_code = quad_code;

	eep = lookup_section ( ep->latlong );

	/* already have an entry on the main list,
	 * so add this onto that entries sublist
	 */
	if ( eep ) {
	    ep->next_ll = eep->next_ll;
	    eep->next_ll = ep;
	    if ( info.verbose )
		printf ( "Added section (on border): %d  %s  %c\n", ep->latlong, ep->path, ep->q_code );
	    return 1;
	}

	/* entirely new entry, add to main list */
	ep->next = section_head;
	section_head = ep;

	if ( info.verbose )
	    printf ( "Added section: %d  %s  %c\n", ep->latlong, ep->path, ep->q_code );

	return 1;
}

static struct section *
lookup_section ( int latlong )
{
	struct section *ep;

	for ( ep = section_head; ep; ep = ep->next ) {
	    if ( ep->latlong == latlong )
	    	return ep;
	}
	return NULL;
}

struct dir_table {
	struct dir_table *next;
	char *name;
};

struct dir_table *dir_head = NULL;

/* Search the dir name table, if we find an entry,
 * return true, if we don't, add this one and return
 * false.  Force lower case since names can be mixed in
 * case on the various disk images.
 */
int
dir_lookup ( char *name )
{
	struct dir_table *dp;
	char *lower;

	lower = str_lower ( name );

	for ( dp=dir_head; dp; dp = dp->next ) {
	    if ( strcmp ( dp->name, lower ) == 0 ) {
		free ( lower );
		return 1;
	    }
	}

	dp = (struct dir_table *) malloc ( sizeof(struct dir_table) );
	if ( ! dp )
	    error ("dir_lookup - out of memory\n", "" );

	dp->name = lower;
	dp->next = dir_head;
	dir_head = dp;
	return 0;
}

void
add_dir_level ( int level, char *dirpath, char *name )
{
	char tpq_path[100];

	sprintf ( tpq_path, "%s/%s", dirpath, name );
	if ( info.verbose )
	    printf ( "add dir level:  %s\n", tpq_path );

	if ( is_directory ( tpq_path ) )
	    return;

	if ( info.verbose )
	    printf ( "Add file/state method: %d  %s\n", level, tpq_path );

	if ( level == 1 )
	    (void) add_file_method ( &info.series_info[S_STATE], tpq_path );
	if ( level == 2 )
	    (void) add_file_method ( &info.series_info[S_ATLAS], tpq_path );
	if ( level == 3 )
	    (void) add_file_method ( &info.series_info[S_500K], tpq_path );
}

/* We are here when we have found an 8 character name within
 * a disk.  We are expecting something like az1_map1,
 * within which are TPQ files for levels 1 2 or 3
 */
int
add_dir ( char *archive, char *dir )
{
	DIR *dd;
	struct dirent *dp;
	char dir_path[100];
	int level;

	level = dir[7] - '0';
	if ( level < 1 || level > 3 )
	    return 0;

	sprintf ( dir_path, "%s/%s", archive, dir );

	if ( ! is_directory ( dir_path ) )
	    return 0;

	/* See if we already got this directory
	 * from some other disk.
	 */
	if ( dir_lookup ( dir ) )
	    return 0;

	if ( info.verbose )
	    printf ( "add dir: %d %s\n", level, dir_path );

	if ( ! (dd = opendir ( dir_path )) )
	    return 0;

	for ( ;; ) {
	    if ( ! (dp = readdir ( dd )) )
	    	break;
	    add_dir_level ( level, dir_path, dp->d_name );
	}

	closedir ( dd );
	return 1;
}

/* My Nevada set (TOPO version 4.2 circa 2006) has a much cleaner
 * setup for levels 1, 2, and 3 on the first disk.
 * First of all level 1 is handled by a single TPQ file that
 * covers the entire country (as is level 2).  It looks like
 * level 3 for the entire country is also provided with one
 * TPQ file per 1-degree section.  Here is the story:
 * The first CD has a directory SI_D01 and in fact there are
 * no state specific files on the first CD.
 * Within SI_D01 things are as follows:
 *
 *   USMAPS/US1_MAP1.TPQ is the entire US, level 1
 *   USMAPS/US1_MAP2.TPQ is the entire US, level 2
 *   US_NE, US_SE, US_NW, and US_SW hold the level 3 maps
 *   US_SW/B30115/D33118/G33118A1.tpq is a typical path.
 *    the Bxxyyy directories hold a 5x5 degree area
 *    the Dxxyyy directories hold a single 1x1 degree TPQ file.
 *
 *  There are also Hawaii and Alaska level 1 and 2 files,
 *  (and you can use the -f option to look at them)
 *  I do not see Hawaii and Alaska level 3 files.
 * For the record:
 *  Anchorage is at 149:54 West and 61:13 North
 *  Honolulu  is at 157:50 West and 21:18 North
 */
int
add_full_usa ( char *disk, char *section )
{
	char si_path[100];

	sprintf ( si_path, "%s/%s", disk, section );

	if ( info.verbose )
	    printf ( "Found level 123 for full USA (cool!) at %s\n", si_path );
}

/* THE END */
