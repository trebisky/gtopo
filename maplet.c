/* maplet.c -- part of gtopo
 * Tom Trebisky  MMT Observatory, Tucson, Arizona
 */
#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>

#include "gtopo.h"

extern struct topo_info info;

static void load_maplet_scale ( struct maplet *, int );

/* XXX - 
 * I have had the maplet cache get up to 2500 entries without
 * any trouble, but someday may want to monitor the size and
 * recycle entries with an old timestamp.
 * Also, it may be possible to use a tree rather than a
 * linked list to speed the lookup.
 * Also, keep separate series in their own "hash" to
 * help speed this up.
 */

struct maplet *
maplet_new ( void )
{
	struct maplet *mp;

	mp = (struct maplet *) malloc ( sizeof(struct maplet) );
	if ( ! mp )
	    error ("load maplet_nbr, out of mem\n", "" );

	mp->time = info.series->cache_count++;
	return mp;
}

struct maplet *
maplet_lookup ( int maplet_index_lat, int maplet_index_long )
{
	struct maplet *cp;

	for ( cp = info.series->cache; cp; cp = cp->next ) {
	    if ( cp->maplet_index_lat == maplet_index_lat &&
	    	cp->maplet_index_long == maplet_index_long ) {
		    return cp;
	    }
	}
	return ( struct maplet *) NULL;
}

/* New sheet (and not in cache)
 */
static struct maplet *
load_maplet_quad ( int maplet_lat, int maplet_long )
{
    	struct maplet *mp;
	struct series *sp;

	sp = info.series;

	mp = maplet_new ();

	/* Try to find it in the archive
	 * This will set tpq_path as well as
	 * x_maplet and y_maplet in the maplet structure.
	 */
	if ( ! lookup_quad_nbr ( mp, maplet_lat, maplet_long ) ) {
	    free ( (char *) mp );
	    return NULL;
	}

	mp->maplet_index_lat = maplet_lat;
	mp->maplet_index_long = maplet_long;

	load_maplet_scale ( mp, mp->y_maplet * sp->long_count + mp->x_maplet );

	mp->next = sp->cache;
	sp->cache = mp;

	return mp;
}

/* Given a center maplet, load a neighbor
 * x and y are offsets in maplet units, with signs
 * correct for indexing a maplet within a quad.
 */
struct maplet *
load_maplet_nbr ( int x, int y )
{
	struct series *sp;
    	struct maplet *cmp;
    	struct maplet *cp;
    	struct maplet *mp;
	int maplet_index_lat;
	int maplet_index_long;
	int x_maplet;
	int y_maplet;

	sp = info.series;
	cmp = sp->center;

    	/* First try cache
	 * flip sign to increase with lat and long
	 * this gives a unique pair of numbers for the maplet.
	 */
	maplet_index_lat = cmp->maplet_index_lat - y;
	maplet_index_long = cmp->maplet_index_long - x;

	cp = maplet_lookup ( maplet_index_lat, maplet_index_long );
	if ( cp ) {
	    if ( info.verbose > 1 )
		printf ( "maplet nbr cache hit: (%d %d) %d %d\n", x, y, maplet_index_lat, maplet_index_long );
	    return cp;
	}

	/* See if this maplet is on same sheet as center.
	 */
	x_maplet = cmp->x_maplet + x;
	if ( x_maplet < 0 || x_maplet >= sp->long_count ) {
	    if ( info.verbose > 1 )
		printf ( "maplet nbr off sheet: (%d %d) %d %d\n", x, y, maplet_index_lat, maplet_index_long );
	    return load_maplet_quad ( maplet_index_lat, maplet_index_long );
	}

	y_maplet = cmp->y_maplet + y;
	if ( y_maplet < 0 || y_maplet >= sp->lat_count ) {
	    if ( info.verbose > 1 )
		printf ( "maplet nbr off sheet: (%d %d) %d %d\n", x, y, maplet_index_lat, maplet_index_long );
	    return load_maplet_quad ( maplet_index_lat, maplet_index_long );
	}

	mp = maplet_new ();

	/* XXX - do we need to copy more stuff from cmp ??
	 *   remember, this may be found in cache later
	 *   and become cmp.
	 */
	mp->x_maplet = x_maplet;
	mp->y_maplet = y_maplet;

	if ( info.verbose > 1 )
	    printf ( "x,y maplet nbr(%d) = %d %d -- %d %d\n", sp->cache_count, x_maplet, y_maplet,
		maplet_index_lat, maplet_index_long );

	/* Each maplet from the same sheet has links to the same
	 * path string, which is just fine.
	 */
	mp->tpq_path = cmp->tpq_path;

	mp->maplet_index_lat = maplet_index_lat;
	mp->maplet_index_long = maplet_index_long;

	load_maplet_scale ( mp, y_maplet * sp->long_count + x_maplet );

	mp->next = sp->cache;
	sp->cache = mp;

	return mp;
}

struct maplet *
load_maplet ( void )
{
    	struct series *sp;
    	struct maplet *mp;
    	struct maplet *cp;
	int x_maplet, y_maplet;
	double maplet_lat, maplet_long;
	int maplet_index_lat;
	int maplet_index_long;

	sp = info.series;

	if ( info.verbose > 1 )
	    printf ( "Load maplet for position %.4f %.4f\n", info.lat_deg, info.long_deg );

	/* Convert from degrees to "maplet units"
	 * (keep these as floating point).
	 */
	maplet_lat = info.lat_deg / sp->maplet_lat_deg;
	maplet_long = info.long_deg / sp->maplet_long_deg;

	/* Truncate these to integers unique to a maplet
	 */
	maplet_index_lat = maplet_lat;
	maplet_index_long = maplet_long;

	sp->fy = 1.0 - (maplet_lat - maplet_index_lat);
	sp->fx = 1.0 - (maplet_long - maplet_index_long);

	/* now search the cache for a maplet matching this.
	 * We may have:
	 *  1 - just moved position in the same maplet.
	 *  2 - moved to an already visited maplet.
	 *  3 - moved to an adjoining maplet.
	 */
	cp = maplet_lookup ( maplet_index_lat, maplet_index_long );
	if ( cp ) {
	    if ( info.verbose > 1 )
		printf ( "maplet cache hit: %d %d\n", maplet_index_lat, maplet_index_long );
	    return cp;
	}

	/* Looks like we will be setting up a new entry.
	 */
	mp = maplet_new ();

	/* Try to find it in the archive
	 * This will set tpq_path as well as
	 * x_maplet and y_maplet in the maplet structure.
	 */
	if ( ! lookup_quad ( mp ) ) {
	    free ( (char *) mp );
	    return NULL;
	}

	x_maplet = mp->x_maplet;
	y_maplet = mp->y_maplet;

	if ( info.verbose > 1 )
	    printf ( "x,y maplet(%d) = %d %d -- %d %d\n", sp->cache_count, x_maplet, y_maplet,
		maplet_index_lat, maplet_index_long );

	mp->maplet_index_lat = maplet_index_lat;
	mp->maplet_index_long = maplet_index_long;

	load_maplet_scale ( mp, y_maplet * sp->long_count + x_maplet );

	mp->next = sp->cache;
	sp->cache = mp;

	return mp;
}

/* This need to scale popped up with the Mt. Hopkins quadrangle
 * which has 330x256 maplets, and to have equal x/y pixel scales
 * ought to have 436x256 maplets or so.  Most quadrangles do have
 * maplets that give square pixels in ground distances, but this
 * one doesn't, and perhaps others don't as well.
 * XXX - just wired in the hack for this quadrangle.
 * What is needed is some kind of test against 512*cos(lat).
 * For two areas where the pixel scale is correct, we see:
 *
 * Taboose pass area (has 410x256 maplets), 512*cos(37.017) = 408.8
 * O'Neill Hills (has 436x256 maplets), 512*cos(32) = 434.2
 * Mt. Hopkins (has 330x256 maplets), 512*cos(31.69) = 435.66
 *
 * Bilinear interpolation looks flawless by the way ...
 */
void
load_maplet_scale ( struct maplet *mp, int index )
{
	GdkPixbuf *tmp;
	double lat_deg;
	double pixel_width;
	int pixel_norm;
	struct series *sp;

	sp = info.series;

	lat_deg = mp->maplet_index_lat * sp->maplet_lat_deg;

	tmp = load_tpq_maplet ( mp->tpq_path, index );

	/* get the maplet size */
	mp->xdim = gdk_pixbuf_get_width ( tmp );
	mp->ydim = gdk_pixbuf_get_height ( tmp );

	/* The usual situation here with a 7.5 minute quad is that the
	 * maplets are 256 tall by 512 wide, before fussing with cos(lat)
	 */
	pixel_width = mp->ydim * sp->maplet_long_deg / sp->maplet_lat_deg;
	pixel_width *= cos ( lat_deg * DEGTORAD );
	pixel_norm = pixel_width;
	if ( info.verbose > 1 )
	    printf ( "maplet scale: %d %d --> %d %d\n", mp->xdim, mp->ydim, pixel_norm, mp->ydim );

	if ( mp->xdim < pixel_norm - 8 || mp->xdim > pixel_norm + 8 ) {
	    if ( info.verbose > 1 )
		printf ( "SCALING\n" );
	    mp->pixbuf = gdk_pixbuf_scale_simple ( tmp, pixel_norm, mp->ydim, GDK_INTERP_BILINEAR );
	    gdk_pixbuf_unref ( tmp );
	    mp->xdim = gdk_pixbuf_get_width ( mp->pixbuf );
	    mp->ydim = gdk_pixbuf_get_height ( mp->pixbuf );
	} else {
	    if ( info.verbose > 1 )
		printf ( "NOT -- SCALING\n" );
	    mp->pixbuf = tmp;
	}
}

/* THE END */
