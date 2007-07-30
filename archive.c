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

void
add_file_method ( struct series *sp, struct tpq_info *tp )
{
	struct method *xp;

	xp = (struct method *) malloc ( sizeof(struct method) );
	if ( ! xp )
	    error ("file method - out of memory\n", "" );

	xp->type = M_FILE;
	xp->tpq = tp;
	xp->next = sp->methods;

	sp->methods = xp;
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
series_init ( struct series *sp )
{
	sp->cache = (struct maplet *) NULL;
	sp->cache_count = 0;
	sp->pixels = NULL;
	sp->content = 0;
	sp->methods = NULL;
}

/* This is called when we are initializing to view just
 * a single TPQ file.
 */
int
file_init ( char *path )
{
	struct tpq_info *tp;
	struct series *sp;
	struct method *xp;

	tp = tpq_lookup ( path );
	if ( ! tp )
	    return 0;

	sp = &series_info[S_FILE];
	    series_init ( sp );
	    sp->series = S_FILE;

	    /* XXX - pretty bogus */
	    sp->xdim = 400;
	    sp->ydim = 400;

	    sp->lat_count = tp->lat_count;
	    sp->long_count = tp->long_count;
	    sp->map_lat_deg = tp->n_lat - tp->s_lat;
	    sp->map_long_deg = tp->e_long - tp->w_long;

	    sp->maplet_lat_deg = sp->map_lat_deg / sp->lat_count;
	    sp->maplet_long_deg = sp->map_long_deg / sp->long_count;

	    /* XXX - bogus if not in a section list */
	    sp->lat_count_d = 1.0 / sp->map_lat_deg;
	    sp->long_count_d = 1.0 / sp->map_long_deg;
	    sp->quad_lat_count = 1;
	    sp->quad_long_count = 1;

	add_file_method ( &series_info[S_FILE], tp );

	info.series = sp;

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
	int nar = 0;
	struct series *sp;

	/* 7.5 minute quadrangle files
	 * each in a single .tpq file
	 * 64 of these in a square degree
	 */
	sp = &series_info[S_24K];
	    sp->series = S_24K;
	    series_init ( sp );

	    /* Correct south of Tucson */
	    sp->xdim = 435;
	    sp->ydim = 256;

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
	    series_init ( sp );

	    /* Correct near Tucson */
	    sp->xdim = 333;
	    sp->ydim = 393;

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

	/* This varies from state to state, and the only
	 * thing that seems constant is that the maplets
	 * are 0.5 by 0.5 degrees on a side.
	 */
	sp = &series_info[S_500K];
	    sp->series = S_500K;
	    series_init ( sp );

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
	    sp->map_lat_deg = 1.0;
	    sp->map_long_deg = 1.0;
	    sp->maplet_lat_deg = 0.5;
	    sp->maplet_long_deg = 0.5;
	    sp->quad_lat_count = 8;
	    sp->quad_long_count = 8;

	/* XXX */
	sp = &series_info[S_ATLAS];
	    sp->series = S_ATLAS;
	    series_init ( sp );

	    /* XXX */
	    sp->xdim = 400;
	    sp->ydim = 400;

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
	    series_init ( sp );

	    /* XXX */
	    sp->xdim = 400;
	    sp->ydim = 400;

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

	add_section_method ( &series_info[S_24K], section_head );
	add_section_method ( &series_info[S_100K], section_head );
	add_section_method ( &series_info[S_500K], section_head );

	return nar;
}

void
toggle_series ( void )
{
	if ( info.series->series == S_FILE )
	    return;

	if ( info.series->series == S_24K )
	    set_series ( S_100K );
	else if ( info.series->series == S_100K )
	    set_series ( S_500K );
	else
	    set_series ( S_24K );
}

void
set_series ( enum s_type s )
{
	info.series = &series_info[s];
	synch_position ();
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

	if ( stat ( path_buf, &stat_buf ) >=  0 )
	    if ( S_ISREG(stat_buf.st_mode) )
		return strhide ( path_buf );

	/* Try upper case */
	lat_q  = toupper(lat_q);
	series_letter = toupper(series_letter);

	sprintf ( path_buf, "%s/%c%2d%03d%c%c.TPQ", ep->path, series_letter, lat_section, long_section, lat_q, long_q );
	if ( info.verbose )
	    printf ( "Trying %d %d -- %s\n", lat_quad, long_quad, path_buf );

	if ( stat ( path_buf, &stat_buf ) >=  0 )
	    if ( S_ISREG(stat_buf.st_mode) )
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
find_quad ( int lat_section, int long_section, int lat_quad, int long_quad )
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
	    rv = quad_path ( ep, lat_section, long_section, lat_quad, long_quad );
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
	int sheet_lat, sheet_long;
	struct series *sp;
	int m_lat, m_long;
	int x_index, y_index;

	sp = info.series;

	/* Figure out the corner indices of our map */
	sheet_lat = xp->tpq->s_lat / sp->maplet_lat_deg;
	sheet_long = - xp->tpq->e_long / sp->maplet_long_deg;
	printf ( "LQF sheet long, lat: %d %d\n", sheet_long, sheet_lat );

	/* Now figure which maplet within the sheet we need.
	 * XXX - notice the north american flip on longitude.
	 */
	m_long = maplet_long - sheet_long;
	m_lat = maplet_lat - sheet_lat;

	printf ( "LQF point : %d %d\n", maplet_long, maplet_lat );
	printf ( "LQF index: %d %d\n", m_long, m_lat );

	if ( m_long < 0 || m_long >= sp->long_count )
	    return 0;
	if ( m_lat < 0 || m_lat >= sp->lat_count )
	    return 0;

	mp->tpq_path = xp->tpq->path;

	/* flip the count to origin from the NW corner */
	x_index = sp->long_count - m_long - 1;
	y_index = sp->lat_count - m_lat - 1;

	mp->index = y_index * sp->long_count + x_index;

	return 1;	
}

static int
method_section ( struct maplet *mp, struct method *xp,
		    int maplet_long, int maplet_lat )
{
	struct series *sp;
	int lat_section, long_section;
	int lat_quad, long_quad;
	int m_long, m_lat;
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
	mp->tpq_path = find_quad ( lat_section, long_section, lat_quad, long_quad );
	if ( ! mp->tpq_path )
	    return 0;

	/* Now figure which maplet within the sheet we need.
	 */
	m_long = maplet_long - long_quad * sp->long_count - long_section * sp->long_count * sp->long_count_d;
	m_lat = maplet_lat - lat_quad * sp->lat_count - lat_section * sp->lat_count * sp->lat_count_d;

	if ( info.verbose )
	    printf ( "lat/long quad, lat/long maplet: %d %d  %d %d\n", lat_quad, long_quad, maplet_lat, maplet_long );

	/* flip the count to origin from the NW corner */
	x_index = sp->long_count - m_long - 1;
	y_index = sp->lat_count - m_lat - 1;

	mp->index = y_index * sp->long_count + x_index;

	return 1;
}

/* This is the basic call to look for a maplet, when we
 * know it is not in the cache.
 */
int
lookup_series ( struct maplet *mp, int maplet_long, int maplet_lat )
{
	struct series *sp;
	struct method *xp;
	int done;

	sp = info.series;

	done = 0;
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

int
add_full_usa ( char *disk, char *section )
{
	char si_path[100];

	sprintf ( si_path, "%s/%s", disk, section );

	printf ( "Found level 123 for full USA (cool!) at %s\n", si_path );
}

/* THE END */
