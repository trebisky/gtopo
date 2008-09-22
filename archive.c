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
#include "protos.h"

extern struct topo_info info;
extern struct settings settings;

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
 * For Arizona, level 3 (500K) is 4 TPQ files, each
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
 *  contains 4 maplets.
 *
 * In summary, the three states I have are all quite
 *  different for level 3, although the maplets are
 *  all the same size (0.5 by 0.5 degrees).
 *
 * Nevada has one file per section with 4 maplets in it.
 * Arizona has four big 5x5 degree files with 100 maplets,
 * California has one monster file for the entire state.
 *
 */

/* This subsystem keeps a linked list of "sections"
 * the idea being you can give a latitude and longitude
 * to the nearest degree and be able to get a path to
 * the directory holding the stuff for that 1x1 degree
 * chunk of the world.
 */
struct section {
	struct section *next;
    	struct section_dir *dir_head;
	int	latlong;
	int	dir_count;
};

struct section_dir {
    	struct section_dir *next;
	char *path;
	int tpq_code[N_SERIES];
	int tpq_count[N_SERIES];
};

/* Used to build the comprehensive level 3,4,5 section list */
static struct section *temp_section_head;

/* Prototypes ... */
static int add_new_archive ( char * );
static int add_disk ( char *, char * );
static void add_full_usa ( char *, char * );
static int add_dir ( char *, char * );

static struct section *lookup_section ( struct section *, int );

/* Each level has a list of methods that need to be run through
 * in an attempt to find the desired maplet.
 *
 * The file method is just a single file with known lat/long limits.
 * The state method is a special variant of the file method with
 *   the file containing one giant maplet.
 *   the difference is in how this is displayed in gtopo.c
 * The section method is a list of section structures representing
 *   1x1 degree land areas with various files in it.
 *
 * Back when I had just the Arizona and California sets,
 *  the scheme was as follows:
 *
 * level 1 - state method for each state.
 * level 2 - file method for each state.
 * level 3 - file method for arizona, section for california
 * level 4 - section method
 * level 4 - section method
 *
 * When the Nevada set came along and added the nicely organized SI_D01
 *  the scheme became:
 * level 1 - file method (one file for entire US)
 * level 2 - file method (one file for entire US)
 * level 3 - section method
 * level 4 - section method
 * level 5 - section method
 *
 * notice that here, level 3 has sections covering the entire country, but only
 * with level 3 maps.  Arizona sections add maps for levels 4 and 5.
 * We can ignore the Arizona level 3 file method stuff if we have SI_D01
 * California sections add maps for levels 3, 4, and 5
 * (the level 3 maps added are redundant with the SI_D01 stuff)
 * Nevada seems to be just like California, the sections add maps for
 * levels 3, 4, and 5 and the level 3 stuff is redundant.
 *
 * So, should we have separate section lists for levels 3, 4, and 5 ?
 * I argue not, even though the level 3 section list is quite big.
 * (The program reports 1359 sections)
 * If we want to know for a given lat and long what level coverage we have
 * (and we do, to avoid changing scale to a white screen), we want to
 * have all the section stuff in one data structure.
 */

static int
add_file_method ( struct series *sp, char *path )
{
	struct method *xp;
	struct tpq_info *tp;

	xp = (struct method *) gmalloc ( sizeof(struct method) );
	if ( ! xp )
	    error ("file method - out of memory\n");

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
add_section_method ( struct series *sp, struct section *head )
{
	struct method *xp;

	xp = (struct method *) gmalloc ( sizeof(struct method) );
	if ( ! xp )
	    error ("section method - out of memory\n");

	xp->type = M_SECTION;
	xp->sections = head;
	xp->next = sp->methods;

	sp->methods = xp;
}

static void
series_init_one ( struct series *sp, enum s_type series )
{
	sp->cache = (struct maplet *) NULL;
	sp->cache_count = 0;
	sp->pixels = NULL;
	sp->content = 0;
	sp->methods = NULL;
	sp->series = series;
	sp->lat_offset = 0.0;
	sp->long_offset = 0.0;
}

char *
wonk_series ( enum s_type series )
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
file_info ( char *path, int extra )
{
	struct maplet *mp;
	struct tpq_info *tp;
	struct series *sp;
	double lat_scale;
	double long_scale;
	double long_scale_raw;
	int nn;
	int i;

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
	series_init_one ( sp, tp->series );
	info.series = sp;

	lat_scale = mp->tpq->maplet_lat_deg / mp->ydim;
	long_scale = mp->tpq->maplet_long_deg * cos ( tp->mid_lat * DEGTORAD ) / mp->xdim;
	long_scale_raw = mp->tpq->maplet_long_deg / mp->xdim;

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
	printf ( " long scale: %.8f  (%.8f) at lat %.5f\n", long_scale, long_scale_raw, tp->mid_lat );
	printf ( " series: %s\n", wonk_series ( tp->series ) );
	printf ( " index size: %d\n", tp->index_size );

	if ( ! extra )
	    return;

	for ( i=0; i<tp->index_size; i++ )
	    printf ( "%4d) %10d %10d\n", i+1, tp->index[i].offset, tp->index[i].size );
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
	series_init_one ( sp, tp->series );
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

void
series_init ( void )
{
	struct series *sp;

	/* 7.5 minute quadrangle files
	 * each in a single .tpq file
	 * 64 of these in a square degree
	 */
	sp = &info.series_info[S_24K];
	    series_init_one ( sp, S_24K );

	    /* Correct south of Tucson */
	    sp->xdim = 435;
	    sp->ydim = 256;
	    sp->terra = 0;

	    sp->lat_count = 10;
	    sp->long_count = 5;
	    sp->lat_count_d = 8;
	    sp->long_count_d = 8;
	    sp->maplet_lat_deg = 0.0125;
	    sp->maplet_long_deg = 0.0250;
	    sp->quad_lat_count = 1;
	    sp->quad_long_count = 1;
	    sp->x_pixel_scale = sp->maplet_long_deg / (double) sp->xdim;
	    sp->y_pixel_scale = sp->maplet_lat_deg / (double) sp->ydim;

	/* 2 of these in a square degree
	 * one of top of the other a1 and e1
	 */
	sp = &info.series_info[S_100K];
	    series_init_one ( sp, S_100K );

	    /* Correct near Tucson */
	    sp->xdim = 333;
	    sp->ydim = 393;
	    sp->terra = 0;

	    sp->lat_count = 8;
	    sp->long_count = 16;
	    sp->lat_count_d = 2;
	    sp->long_count_d = 1;
	    sp->maplet_lat_deg = 0.0625;
	    sp->maplet_long_deg = 0.0625;
	    sp->quad_lat_count = 4;
	    sp->quad_long_count = 8;
	    sp->x_pixel_scale = sp->maplet_long_deg / (double) sp->xdim;
	    sp->y_pixel_scale = sp->maplet_lat_deg / (double) sp->ydim;

	/* This varies from state to state, and the only
	 * thing that seems constant is that the maplets
	 * are 0.5 by 0.5 degrees on a side.
	 */
	sp = &info.series_info[S_500K];
	    series_init_one ( sp, S_500K );

	    /* Correct in Southern Nevada */
	    sp->xdim = 380;
	    sp->ydim = 480;
	    sp->terra = 0;

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
	    sp->x_pixel_scale = sp->maplet_long_deg / (double) sp->xdim;
	    sp->y_pixel_scale = sp->maplet_lat_deg / (double) sp->ydim;

	/* XXX */
	sp = &info.series_info[S_ATLAS];
	    series_init_one ( sp, S_ATLAS );

	    /* true for full USA */
	    sp->maplet_lat_deg = 1.0;
	    sp->maplet_long_deg = 1.0;

	    sp->xdim = 290;
	    sp->ydim = 364;
	    sp->terra = 0;

	    sp->lat_count = 1;
	    sp->long_count = 1;
	    sp->lat_count_d = 1;
	    sp->long_count_d = 1;
	    sp->quad_lat_count = 1;
	    sp->quad_long_count = 1;
	    sp->x_pixel_scale = sp->maplet_long_deg / (double) sp->xdim;
	    sp->y_pixel_scale = sp->maplet_lat_deg / (double) sp->ydim;

	/* XXX - The entire state */
	sp = &info.series_info[S_STATE];
	    series_init_one ( sp, S_STATE );

	    /* XXX - complete BS for this series */
	    sp->lat_count = 1;
	    sp->long_count = 1;
	    sp->lat_count_d = 1;
	    sp->long_count_d = 1;
	    sp->quad_lat_count = 1;
	    sp->quad_long_count = 1;

	    sp->terra = 0;

#ifdef notdef
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
#endif
	    /* true for full USA */
	    sp->maplet_lat_deg = 3.250;
	    sp->maplet_long_deg = 4.9167;

	    /* XXX - yecch, this should get calculated from
	     * the file when we read the header.
	     * (as the above)
	     */
	    sp->lat_offset = 1.25;
	    sp->long_offset = -2.079;

	    sp->xdim = 309;
	    sp->ydim = 256;
	    sp->x_pixel_scale = sp->maplet_long_deg / (double) sp->xdim;
	    sp->y_pixel_scale = sp->maplet_lat_deg / (double) sp->ydim;

#ifdef TERRA
	/* Terraserver 2m (like 1:24k) */
	sp = &info.series_info[S_TOPO_2M];
	    series_init_one ( sp, S_TOPO_2M );

	    sp->xdim = 200;
	    sp->ydim = 200;
	    sp->terra = 1;
	    sp->x_pixel_scale = 2.0;
	    sp->y_pixel_scale = 2.0;
	    sp->scale_name = "Scale2m";

	/* Terraserver 8m (like 1:100) */
	sp = &info.series_info[S_TOPO_8M];
	    series_init_one ( sp, S_TOPO_8M );

	    sp->xdim = 200;
	    sp->ydim = 200;
	    sp->terra = 1;
	    sp->x_pixel_scale = 8.0;
	    sp->y_pixel_scale = 8.0;
	    sp->scale_name = "Scale8m";

	/* Terraserver 32m */
	sp = &info.series_info[S_TOPO_32M];
	    series_init_one ( sp, S_TOPO_32M );

	    sp->xdim = 200;
	    sp->ydim = 200;
	    sp->terra = 1;
	    sp->x_pixel_scale = 32.0;
	    sp->y_pixel_scale = 32.0;
	    sp->scale_name = "Scale32m";
#endif
}

/* Added this list of archives 6-5-2008, to allow this
 * to by dynamically handled via the config file.
 * We now check for the existence of the directories
 * before we add them to the list, so the list may be
 * empty by the time we try to initialize (which is a
 * good thing, since we can declare an error then).
 *
 * Notice also that this scheme allows gtopo to handle
 * an archive that is split into any number of directories,
 * which I find handy on a laptop with space on two different
 * disk partitions.
 */
struct archive_entry {
	struct archive_entry *next;
	char *path;
};

static struct archive_entry *archive_head = NULL;

void
archive_clear ( void )
{
	archive_head = NULL;
}

void
archive_add ( char *path )
{
	struct archive_entry *ap;
	struct archive_entry *lp;

	if ( ! is_directory ( path ) )
	    return;

	ap = (struct archive_entry *) gmalloc ( sizeof(struct archive_entry) );
	ap->next = NULL;
	ap->path = strhide ( path );

	/* keep list in order (if that matters, doesn't hurt) */
	if ( ! archive_head )
	    archive_head = ap;
	else {
	    lp = archive_head;
	    while ( lp->next )
	    	lp = lp->next;
	    lp->next = ap;
	}
}

/* This is the usual initialization when we want to setup to
 * view a whole collection of potentially multiple states.
 */
int
archive_init ( void )
{
	struct archive_entry *ap;
	int nar;

	if ( ! archive_head )
	    return 0;

	series_init ();

	nar = 0;

	info.n_sections = 0;

	/* Look for the SI_D01 thing first off */
	for ( ap = archive_head; ap; ap = ap->next ) {
	    if ( add_usa ( ap->path, 1 ) )
		nar++;
	}

	info.have_usa = nar;

	temp_section_head = NULL;

	for ( ap = archive_head; ap; ap = ap->next ) {
	    if ( add_new_archive ( ap->path ) )
		nar++;
	}

	/* XXX - These all use the same section list */
	add_section_method ( &info.series_info[S_24K], temp_section_head );
	add_section_method ( &info.series_info[S_100K], temp_section_head );

	/* Won't need this if we have the full USA set */
	if ( ! info.have_usa )
	    add_section_method ( &info.series_info[S_500K], temp_section_head );

	return nar;
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

static void
show_methods ( struct series *sp )
{
	struct method *xp;

	for ( xp = sp->methods; xp; xp = xp->next ) {
	    printf ( "%s method", wonk_method(xp->type) );
	    if ( xp->type != M_SECTION )
		printf ( ": %s", xp->tpq->path );
	    printf ( "\n" );
	}
}

void
show_statistics ( void )
{
    	int s;

	printf ( "Total sections: %d\n", info.n_sections );

	for ( s=0; s<N_SERIES; s++ ) {
	    printf ( "Map series %d (%s)\n", s+1, wonk_series(s) );
	    show_methods ( &info.series_info[s] );
	}
}

static int
try_series ( int new_series )
{
	int rv = 0;

	/* Give it a whirl, see if we can load a maplet
	 * for this series at the center position.
	 * Trying to load the maplet avoids changing to
	 * a white screen in areas where we have no coverage.
	 */
	info.series = &info.series_info[new_series];
	synch_position ();

	/* No harm done in loading the maplet, this gets it
	 * into the cache, and we will soon be fetching it
	 * to display anyway.
	 */
	if ( load_maplet ( info.maplet_x, info.maplet_y ) )
	    rv = 1;

	if ( settings.verbose & V_BASIC )
	    printf ( "try series returns %d for %d\n", rv, new_series );

	return rv;
}

/* Move to a less detailed series */
void
up_series ( void )
{
	int series = info.series->series;

	if ( settings.verbose & V_BASIC )
	    printf ( "up series called on series %d\n", series );

	if ( series == 0 )
	    return;

	if ( try_series ( series - 1 ) ) {
	    redraw_series ();
	    if ( settings.verbose & V_BASIC )
		printf ( "up series moved to series %d\n", series-1 );
	    return;
	}

	/* cannot switch, restore original series */
	info.series = &info.series_info[series];
	synch_position ();
}

/* Move to a more detailed series */
void
down_series ( void )
{
	int series = info.series->series;

	if ( settings.verbose & V_BASIC )
	    printf ( "down series called on series %d\n", series );

	if ( series == N_SERIES - 1 )
	    return;

	if ( try_series ( series + 1 ) ) {
	    redraw_series ();
	    if ( settings.verbose & V_BASIC )
		printf ( "down series moved to series %d\n", series+1 );
	    return;
	}

	/* cannot switch, restore original series */
	info.series = &info.series_info[series];
	synch_position ();
}

/* Like set_series, but doesn't try to synch position */
void
initial_series ( enum s_type s )
{
    	if ( s < 0 || s >= N_SERIES )
	    error ( "initial_series, impossible value: %d\n", s );

	info.series = &info.series_info[s];
}

void
set_series ( enum s_type s )
{
    	if ( s < 0 || s >= N_SERIES )
	    error ( "set_series, impossible value: %d\n", s );

	info.series = &info.series_info[s];

	synch_position ();
	if ( settings.verbose & V_BASIC ) {
	    printf ( "Switch to series %d (%s)\n", s+1, wonk_series ( s ) );
	    show_methods ( info.series );
	}
}

/* Try both upper and lower case path names for the tpq file.
 * It turns out we have to allow the file extension to be
 * lower case, while the name may be upper case, i.e. we
 * must try:
 * 	c41120A1.tpq
 * 	C41120A1.TPQ
 * 	C41120A1.tpq
 * 	c41120A1.TPQ
 */
static char *
section_map_path ( struct section_dir *dp, int lat_section, int long_section, int lat_quad, int long_quad )
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

	series_letter = dp->tpq_code[info.series->series];

	/* 1 - try all lower case */
	sprintf ( path_buf, "%s/%c%2d%03d%c%c.tpq", dp->path, series_letter, lat_section, long_section, lat_q, long_q );
	if ( settings.verbose & V_ARCHIVE )
	    printf ( "Trying %d %d -- %s\n", lat_quad, long_quad, path_buf );

	if ( is_file(path_buf) )
	    return strhide ( path_buf );

	/* 2 - try all upper case */
	lat_q  = toupper(lat_q);
	series_letter = toupper(series_letter);

	sprintf ( path_buf, "%s/%c%2d%03d%c%c.TPQ", dp->path, series_letter, lat_section, long_section, lat_q, long_q );
	if ( settings.verbose & V_ARCHIVE )
	    printf ( "Trying %d %d -- %s\n", lat_quad, long_quad, path_buf );

	if ( is_file(path_buf) )
	    return strhide ( path_buf );

	/* 3 - try upper case name, with lower case .tpq */
	sprintf ( path_buf, "%s/%c%2d%03d%c%c.tpq", dp->path, series_letter, lat_section, long_section, lat_q, long_q );
	if ( settings.verbose & V_ARCHIVE )
	    printf ( "Trying %d %d -- %s\n", lat_quad, long_quad, path_buf );

	if ( is_file(path_buf) )
	    return strhide ( path_buf );

	/* 4 - unlikely, but try lower case name, with upper case .TPQ */
	lat_q  = tolower(lat_q);
	series_letter = tolower(series_letter);

	sprintf ( path_buf, "%s/%c%2d%03d%c%c.TPQ", dp->path, series_letter, lat_section, long_section, lat_q, long_q );
	if ( settings.verbose & V_ARCHIVE )
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
section_find_map ( struct section *head, int lat_section, int long_section, int lat_quad, int long_quad )
{
	struct section *ep;
	struct section_dir *dp;
	char *rv;

	ep = lookup_section ( head, lat_section * 1000 + long_section );
	if ( ! ep )
	    return 0;

	/* Handle the case where multiple section directories
	 * cover the same area (such as along state boundaries
	 * where one section directory comes from each state).
	 */
	for ( dp=ep->dir_head; dp; dp = dp->next ) {
	    rv = section_map_path ( dp, lat_section, long_section, lat_quad, long_quad );
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
		int maplet_x, int maplet_y )
{
	int x_index, y_index;

	/* Now figure which maplet within the sheet we need.
	 */
	mp->sheet_x = maplet_x - xp->tpq->sheet_long;
	mp->sheet_y = maplet_y - xp->tpq->sheet_lat;

	if ( settings.verbose & V_ARCHIVE ) {
	    printf ( "MF sheet long, lat: %d %d\n", xp->tpq->sheet_long, xp->tpq->sheet_lat );
	    printf ( "MF maplet indices (x/y) : %d %d\n", maplet_x, maplet_y );
	    printf ( "MF sheet indices (x/y): %d %d\n", mp->sheet_x, mp->sheet_y );
	}

	if ( mp->sheet_x < 0 || mp->sheet_x >= xp->tpq->long_count )
	    return 0;
	if ( mp->sheet_y < 0 || mp->sheet_y >= xp->tpq->lat_count )
	    return 0;

	mp->tpq_path = xp->tpq->path;

	/* flip the count to origin from the NW corner */
	x_index = xp->tpq->long_count - mp->sheet_x - 1;
	y_index = xp->tpq->lat_count - mp->sheet_y - 1;

	mp->tpq_index = y_index * xp->tpq->long_count + x_index;

	return 1;	
}

static int
method_section ( struct maplet *mp, struct method *xp,
		    int maplet_x, int maplet_y )
{
	struct series *sp;
	int lat_section, long_section;
	int lat_quad, long_quad;
	int x_index, y_index;

	sp = info.series;

	lat_section = maplet_y / (sp->lat_count_d * sp->lat_count);
	long_section = maplet_x / (sp->long_count_d * sp->long_count);

	if ( settings.verbose & V_ARCHIVE )
	    printf ( "lookup_quad, section: %d %d\n", lat_section, long_section );

	lat_quad = maplet_y / sp->lat_count - lat_section * sp->lat_count_d;
	long_quad = maplet_x / sp->long_count - long_section * sp->long_count_d;

	/* See if the map sheet is available.
	 */
	mp->tpq_path = section_find_map ( xp->sections, lat_section, long_section, lat_quad, long_quad );
	if ( ! mp->tpq_path )
	    return 0;

	/* Now figure which maplet within the sheet we need.
	 */
	mp->sheet_x = maplet_x - long_quad * sp->long_count - long_section * sp->long_count * sp->long_count_d;
	mp->sheet_y = maplet_y - lat_quad * sp->lat_count - lat_section * sp->lat_count * sp->lat_count_d;

	if ( settings.verbose & V_ARCHIVE )
	    printf ( "long/lag quad, x/y maplet: %d %d  %d %d\n", long_quad, lat_quad, maplet_x, maplet_y );

	/* flip the count to origin from the NW corner */
	x_index = sp->long_count - mp->sheet_x - 1;
	y_index = sp->lat_count - mp->sheet_y - 1;

	mp->tpq_index = y_index * sp->long_count + x_index;

	return 1;
}

/* This is the basic call to look for a maplet, when we
 * know it is not in the cache.
 * It returns with the following set:
 *	tpq_path
 *	tpq_index
 *	sheet_x
 *	sheet_y
 */
int
lookup_series ( struct maplet *mp, int maplet_x, int maplet_y )
{
	struct series *sp;
	struct method *xp;
	int done;

	sp = info.series;

	done = 0;
	/* We skip STATE_METHOD in this loop */
	for ( xp = sp->methods; xp; xp = xp->next ) {
	    if ( xp->type == M_SECTION )
	    	done = method_section ( mp, xp, maplet_x, maplet_y );
	    if ( xp->type == M_FILE )
	    	done = method_file ( mp, xp, maplet_x, maplet_y );
	    if ( done )
	    	return 1;
	}

	return 0;
}

/* Look for the SI_D01 directory (either case)
 * Notice recursion limited to one level.
 */
int
add_usa ( char *archive, int depth )
{
	DIR *dd;
	struct dirent *dp;

	if ( ! is_directory ( archive ) )
	    return 0;

	if ( ! (dd = opendir ( archive )) )
	    return 0;

	/* Loop through this possible archive, looking
	 * only for SI_D01
	 */
	for ( ;; ) {
	    if ( ! (dp = readdir ( dd )) )
	    	break;
	    if ( strlen(dp->d_name) != 6 )
	    	continue;
	    if ( dp->d_name[2] != '_' )
	    	continue;

	    if ( strcmp_l("si_d01", dp->d_name) == 0 ) {
	    	add_full_usa ( archive, dp->d_name );
		closedir ( dd );
		return 1;
	    }

	    if ( depth > 0 ) {
		char ar_path[100];
		sprintf ( ar_path, "%s/%s", archive, dp->d_name );
		if ( add_usa ( ar_path, depth-1 ) ) {
		    closedir ( dd );
		    return 1;
		}
	    }
	}

	closedir ( dd );
	return 0;
}

static int
add_new_archive ( char *archive )
{
	DIR *dd;
	struct dirent *dp;
	int rv = 0;

	if ( ! is_directory ( archive ) )
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

static int
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

	if ( settings.verbose & V_BASIC )
	    printf ( "Found disk: %s\n", disk_path );

	/* Loop through this disk image.
	 */
	for ( ;; ) {
	    if ( ! (dp = readdir ( dd )) )
	    	break;
	    if ( strlen(dp->d_name) == 8 )
	    	add_dir ( disk_path, dp->d_name );
	    if ( strlen(dp->d_name) != 6 )
	    	continue;

	    if ( strcmp_l("si_d01", dp->d_name) == 0 )
		continue;
	    if ( dp->d_name[0] == 'D' || dp->d_name[0] == 'd' )
	    	add_section ( disk_path, dp->d_name, &temp_section_head );
	}

	closedir ( dd );
	return 1;
}

/* We have a directory that looks like a section.
 * Open and scan it, doing a tally of the TPQ files inside
 * and what the lead letters are in this directory.
 *
 * Here is what I have seen in the sets I have so far:
 * Arizona:
 * 	Q = 7.5 minute (24K)  16 of these
 * 	K = 100K ( 2 of these)
 * California:
 * 	q = 7.5 minute (24K)  16 of these
 * 	k = 100K ( 2 of these)
 * Nevada
 * 	n = 7.5 minute (24K)  16 of these
 * 	c = 100K ( 2 of these)
 * 	g = 500K
 * Full USA (SI_D01)
 * 	G = 500K
 *
 * Note that in the nevada case, the 500K files here
 * are redundant with the capital G files in the SI directories
 * 	I have seen some 't' files that seem to be
 * 	derelict temporary files that duplicate one of
 * 	the "standard" letters which is also present.
 */

#define N_LETTERS	256
static int letter_count[N_LETTERS];

static void
scan_section ( char *path, int codes[], int count[] )
{
	DIR *dd;
	struct dirent *dp;
	int i;

	count[S_STATE] = 0;
	count[S_ATLAS] = 0;
	count[S_500K] = 0;
	count[S_100K] = 0;
	count[S_24K] = 0;

	if ( ! is_directory ( path ) )
	    return;

	if ( ! (dd = opendir ( path )) )
	    return;

	for ( i=0; i< N_LETTERS; i++ )
	    letter_count[i] = 0;

	for ( ;; ) {
	    if ( ! (dp = readdir ( dd )) )
	    	break;
	    if ( strlen(dp->d_name) != 12 )
	    	continue;
	    /* XXX - what is this about ? */
	    if ( dp->d_name[9] != 't' && dp->d_name[9] != 'T' )
	    	continue;

	    letter_count[dp->d_name[0]]++;
	}

	closedir ( dd );

	codes[S_STATE] = ' ';
	codes[S_ATLAS] = ' ';

	codes[S_500K] = ' ';
	if ( letter_count['G'] ) {
	    codes[S_500K] = 'G';
	    count[S_500K] += letter_count['G'];
	}

	if ( letter_count['g'] ) {
	    codes[S_500K] = 'g';
	    count[S_500K] += letter_count['g'];
	}

	codes[S_100K] = ' ';
	if ( letter_count['c'] ) {
	    codes[S_100K] = 'c';
	    count[S_100K] += letter_count['c'];
	}
	if ( letter_count['C'] ) {
	    codes[S_100K] = 'C';
	    count[S_100K] += letter_count['C'];
	}
	if ( letter_count['k'] ) {
	    codes[S_100K] = 'k';
	    count[S_100K] += letter_count['k'];
	}
	if ( letter_count['K'] ) {
	    codes[S_100K] = 'K';
	    count[S_100K] += letter_count['K'];
	}

	codes[S_24K] = ' ';
	if ( letter_count['n'] ) {
	    codes[S_24K] = 'n';
	    count[S_24K] += letter_count['n'];
	}
	if ( letter_count['N'] ) {
	    codes[S_24K] = 'N';
	    count[S_24K] += letter_count['N'];
	}
	if ( letter_count['q'] ) {
	    codes[S_24K] = 'q';
	    count[S_24K] += letter_count['q'];
	}
	if ( letter_count['Q'] ) {
	    codes[S_24K] = 'Q';
	    count[S_24K] += letter_count['Q'];
	}
}

/* A section is a term we use only in this program for a 1x1 degree
 * piece of land, NOT a section in the one square mile land survey
 * nomenclature.  It is a directory that actually holds TPQ files
 * which represent individual 7.5 minute maps (and more!)
 * Note that the same section can be covered by more than one path
 * if it is on the boundary of several states.
 */
int
add_section ( char *disk, char *section, struct section **head )
{
	char section_path[100];
	struct section_dir *dp;
	struct section *ep;
	int latlong;
	int count;

	sprintf ( section_path, "%s/%s", disk, section );

	dp = (struct section_dir *) gmalloc ( sizeof(struct section_dir) );
	if ( ! dp )
	    error ("Section new (dir) - out of memory\n");

	latlong = atol ( &section[1] );

	dp->path = strhide ( section_path );
	dp->next = (struct section_dir *) NULL;

	scan_section ( section_path, dp->tpq_code, dp->tpq_count );
	count = dp->tpq_count[S_500K] + dp->tpq_count[S_100K] + dp->tpq_count[S_24K];
	if ( count == 0 ) {
	    free ( (char *) dp );
	    return 0;
	}

	ep = lookup_section ( *head, latlong );

	if ( ! ep ) {
	    /* Does not yet exist on main list */
	    ep = (struct section *) gmalloc ( sizeof(struct section) );
	    if ( ! ep )
		error ("Section new - out of memory\n");

	    ep->latlong = latlong;
	    ep->dir_head = (struct section_dir *) NULL;
	    ep->dir_count = 0;

	    if ( settings.verbose & V_ARCHIVE )
		printf ( "Added section:" );

	    ep->next = *head;
	    *head = ep;
	    info.n_sections++;
	} else {
	    if ( settings.verbose & V_ARCHIVE )
		printf ( "Added section (border):" );
	}

	if ( settings.verbose & V_ARCHIVE ) {
	    printf ( " %d  %s", latlong, dp->path );
	    printf ( " %c %c %c\n",
		    dp->tpq_code[S_500K],
		    dp->tpq_code[S_100K],
		    dp->tpq_code[S_24K] );
	}

	dp->next = ep->dir_head;
	ep->dir_head = dp;
	ep->dir_count++;

	return 1;
}

static struct section *
lookup_section ( struct section *head, int latlong )
{
	struct section *ep;

	for ( ep = head; ep; ep = ep->next ) {
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

	dp = (struct dir_table *) gmalloc ( sizeof(struct dir_table) );
	if ( ! dp )
	    error ("dir_lookup - out of memory\n");

	dp->name = lower;
	dp->next = dir_head;
	dir_head = dp;
	return 0;
}

static void
add_dir_series ( int series, char *dirpath, char *name )
{
	char tpq_path[100];

	sprintf ( tpq_path, "%s/%s", dirpath, name );
	if ( settings.verbose & V_ARCHIVE )
	    printf ( "add dir series:  %s\n", tpq_path );

	if ( is_directory ( tpq_path ) )
	    return;

	/* If we have the full USA, we don't need this */
	if ( info.have_usa )
		return;

	if ( settings.verbose & V_ARCHIVE )
	    printf ( "Add file/state method: %d  %s\n", series, tpq_path );

	(void) add_file_method ( &info.series_info[series], tpq_path );
}

/* We are here when we have found an 8 character name within
 * a disk.  We are expecting something like az1_map1,
 * within which are TPQ files for levels 1 2 or 3
 * (the final "1" or whatever determines the series)
 */
static int
add_dir ( char *archive, char *dir )
{
	DIR *dd;
	struct dirent *dp;
	char dir_path[100];
	int series;

	if ( dir[7] == '1' )
	    series = S_STATE;
	else if ( dir[7] == '2' )
	    series = S_ATLAS;
	else if ( dir[7] == '3' )
	    series = S_500K;
	else
	    return 0;

	sprintf ( dir_path, "%s/%s", archive, dir );

	if ( ! is_directory ( dir_path ) )
	    return 0;

	/* See if we already got this directory
	 * from some other disk.
	 */
	if ( dir_lookup ( dir ) )
	    return 0;

	if ( settings.verbose & V_ARCHIVE )
	    printf ( "add dir: %d %s\n", series, dir_path );

	if ( ! (dd = opendir ( dir_path )) )
	    return 0;

	for ( ;; ) {
	    if ( ! (dp = readdir ( dd )) )
	    	break;
	    add_dir_series ( series, dir_path, dp->d_name );
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
static void
add_usa_file ( int series, char *path, char *name )
{
	char map_path[100];
	char *new_path;

	sprintf ( map_path, "%s/%s", path, name );
	if ( is_file ( map_path ) ) {
	    (void) add_file_method ( &info.series_info[series], map_path );
	    return;
	}

	new_path = str_lower ( map_path );
	if ( is_file ( new_path ) )
	    (void) add_file_method ( &info.series_info[series], new_path );
	free ( new_path );
}

static void
add_usa_12 ( char *path, char *name )
{
	DIR *dd;
	struct dirent *dp;
	char map_path[100];

	sprintf ( map_path, "%s/%s", path, name );

	if ( ! is_directory ( map_path ) )
	    return;

	if ( ! (dd = opendir ( map_path )) )
	    return;

	/* Loop through this directory
	 */
	for ( ;; ) {
	    if ( ! (dp = readdir ( dd )) )
	    	break;

	    if ( strcmp_l("us1_map1.tpq", dp->d_name) == 0 )
		add_usa_file ( S_STATE, map_path, dp->d_name );
	    if ( strcmp_l("us1_map2.tpq", dp->d_name) == 0 )
		add_usa_file ( S_ATLAS, map_path, dp->d_name );
	}

	closedir ( dd );
}

/* We do one level of recursion
 * the first call will see B directories
 * the second call will see D directories
 * D directories hold tpq files (like G35117a1.tpq).
 * This would chase along forever, except that it
 * only recurses when it sees a B directory.
 */
static void
add_usa_500k ( char *path, char *name )
{
	DIR *dd;
	struct dirent *dp;
	char map_path[100];

	sprintf ( map_path, "%s/%s", path, name );

	if ( ! is_directory ( map_path ) )
	    return;

	if ( ! (dd = opendir ( map_path )) )
	    return;

	/* Loop through this directory
	 * We expect to see directories
	 *  like B30115 or D30117
	 */
	for ( ;; ) {
	    if ( ! (dp = readdir ( dd )) )
	    	break;
	    if ( strlen(dp->d_name) != 6 )
	    	continue;
	    if ( dp->d_name[0] == 'b' || dp->d_name[0] == 'B' ) {
		add_usa_500k ( map_path, dp->d_name );
		continue;
	    }
	    if ( dp->d_name[0] == 'd' || dp->d_name[0] == 'D' ) {
	    	add_section ( map_path, dp->d_name, &temp_section_head );
		continue;
	    }
	}

	closedir ( dd );
}

static void
add_full_usa ( char *path, char *name )
{
	DIR *dd;
	struct dirent *dp;

	char si_path[100];
	char map_path[100];

	sprintf ( si_path, "%s/%s", path, name );

	if ( ! is_directory ( si_path ) )
	    return;

	if ( settings.verbose & V_BASIC )
	    printf ( "Found level 123 for full USA at %s\n", si_path );

#ifdef notdef
	/* believe it or not, these weird mixed case file names actually
	 * are what appear, namely US1_MAP1.tpq
	 * XXX - this would be cleaner if we would just loop
	 * through the names actually in the archive and do a
	 * str_cmp_l against target names.
	 */

	sprintf ( map_path, "%s/USMAPS", si_path );
	if ( is_directory ( map_path ) ) {
	    add_usa_file ( S_STATE, map_path, "US1_MAP1.TPQ" );
	    add_usa_file ( S_ATLAS, map_path, "US1_MAP2.TPQ" );
	    add_usa_file ( S_STATE, map_path, "US1_MAP1.tpq" );
	    add_usa_file ( S_ATLAS, map_path, "US1_MAP2.tpq" );
	}

	sprintf ( map_path, "%s/usmaps", si_path );
	if ( is_directory ( map_path ) ) {
	    add_usa_file ( S_STATE, map_path, "US1_MAP1.TPQ" );
	    add_usa_file ( S_ATLAS, map_path, "US1_MAP2.TPQ" );
	    add_usa_file ( S_STATE, map_path, "US1_MAP1.tpq" );
	    add_usa_file ( S_ATLAS, map_path, "US1_MAP2.tpq" );
	}
#endif

	if ( ! (dd = opendir ( si_path )) )
	    return;

	temp_section_head = NULL;

	/* Loop through this directory
	 */
	for ( ;; ) {
	    if ( ! (dp = readdir ( dd )) )
	    	break;

	    if ( strcmp_l("usmaps", dp->d_name) == 0 )
		add_usa_12 ( si_path, dp->d_name );

	    if ( strcmp_l("us_ne", dp->d_name) == 0 )
		add_usa_500k ( si_path, dp->d_name );
	    if ( strcmp_l("us_nw", dp->d_name) == 0 )
		add_usa_500k ( si_path, dp->d_name );
	    if ( strcmp_l("us_se", dp->d_name) == 0 )
		add_usa_500k ( si_path, dp->d_name );
	    if ( strcmp_l("us_sw", dp->d_name) == 0 )
		add_usa_500k ( si_path, dp->d_name );
	}

	closedir ( dd );

	add_section_method ( &info.series_info[S_500K], temp_section_head );
}

/* THE END */
