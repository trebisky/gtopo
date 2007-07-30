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

/* I have had the maplet cache get up to 2500 entries without
 * any trouble, but someday may want to monitor the size and
 * recycle entries with an old timestamp.
 * Also, it may be possible to use a more sophisticated data
 * structure than a linear linked list to speed up the search.
 * (use the low bits as a hash index, this puts local blocks
 * into separate hash bins, something like:
 * (maplet_lat & 0xf) << 4 | (maplet_long & 0xf)
 * Also, when an entry is found, move it to the front of
 * the list.
 */

struct maplet *
maplet_new ( void )
{
	struct maplet *mp;

	mp = (struct maplet *) malloc ( sizeof(struct maplet) );
	if ( ! mp )
	    error ("maplet_new, out of mem\n", "" );

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

/* The need to scale popped up with the Mt. Hopkins quadrangle
 * which has 330x256 maplets, and to have equal x/y pixel scales
 * ought to have 436x256 maplets or so.  Most quadrangles do have
 * maplets that give square pixels in ground distances, but this
 * one doesn't, and perhaps others don't as well.
 *
 * For two areas where the pixel scale is correct, we see:
 *
 * Taboose pass area (has 410x256 maplets), 512*cos(37.017) = 408.8
 * O'Neill Hills (has 436x256 maplets), 512*cos(32) = 434.2
 * Mt. Hopkins (has 330x256 maplets), 512*cos(31.69) = 435.66
 *
 * Bilinear interpolation looks flawless by the way ...
 */
static void
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

struct maplet *
load_maplet ( int long_maplet, int lat_maplet )
{
    	struct series *sp;
    	struct maplet *mp;

	sp = info.series;

	if ( info.verbose > 1 )
	    printf ( "Load maplet for position %d %d\n", long_maplet, lat_maplet );

	mp = maplet_lookup ( lat_maplet, long_maplet );
	if ( mp ) {
	    if ( info.verbose > 1 )
		printf ( "maplet cache hit: %d %d\n", long_maplet, lat_maplet );
	    return mp;
	}

	/* Looks like we will be setting up a new entry.
	 */
	mp = maplet_new ();

	/* Try to find it in the archive
	 * This will set tpq_path as well as
	 * "index" in the maplet structure.
	 */
	if ( ! lookup_series ( mp, long_maplet, lat_maplet ) ) {
	    free ( (char *) mp );
	    return NULL;
	}

	mp->maplet_index_long = long_maplet;
	mp->maplet_index_lat = lat_maplet;

	if ( info.verbose > 1 )
	    printf ( "Read maplet(%d) = %d %d -- %d\n", sp->cache_count,
		    long_maplet, lat_maplet, index );

	load_maplet_scale ( mp, mp->index );

	mp->next = sp->cache;
	sp->cache = mp;

	return mp;
}

/* THE END */
