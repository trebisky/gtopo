/* maplet.c -- part of gtopo
 * Tom Trebisky  MMT Observatory, Tucson, Arizona
 */
#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "gtopo.h"

extern struct topo_info info;

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

	mp->pixbuf = load_tpq_maplet ( mp->tpq_path, mp->y_maplet * sp->long_count + mp->x_maplet );

	/* get the maplet size */
	mp->xdim = gdk_pixbuf_get_width ( mp->pixbuf );
	mp->ydim = gdk_pixbuf_get_height ( mp->pixbuf );

	mp->maplet_index_lat = maplet_lat;
	mp->maplet_index_long = maplet_long;

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
	    printf ( "maplet nbr cache hit: (%d %d) %d %d\n", x, y, maplet_index_lat, maplet_index_long );
	    return cp;
	}

	/* See if this maplet is on same sheet as center.
	 */
	x_maplet = cmp->x_maplet + x;
	if ( x_maplet < 0 || x_maplet >= sp->long_count ) {
	    printf ( "maplet nbr off sheet: (%d %d) %d %d\n", x, y, maplet_index_lat, maplet_index_long );
	    return load_maplet_quad ( maplet_index_lat, maplet_index_long );
	}

	y_maplet = cmp->y_maplet + y;
	if ( y_maplet < 0 || y_maplet >= sp->lat_count ) {
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

	printf ( "x,y maplet nbr(%d) = %d %d -- %d %d\n", sp->cache_count, x_maplet, y_maplet,
	    maplet_index_lat, maplet_index_long );

	/* Each maplet from the same sheet has links to the same
	 * path string, which is just fine.
	 */
	mp->tpq_path = cmp->tpq_path;

	mp->pixbuf = load_tpq_maplet ( cmp->tpq_path, y_maplet * sp->long_count + x_maplet );

	/* get the maplet size */
	mp->xdim = gdk_pixbuf_get_width ( mp->pixbuf );
	mp->ydim = gdk_pixbuf_get_height ( mp->pixbuf );

	mp->maplet_index_lat = maplet_index_lat;
	mp->maplet_index_long = maplet_index_long;

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

	printf ( "x,y maplet(%d) = %d %d -- %d %d\n", sp->cache_count, x_maplet, y_maplet,
	    maplet_index_lat, maplet_index_long );

	mp->pixbuf = load_tpq_maplet ( mp->tpq_path, y_maplet * sp->long_count + x_maplet );

	/* get the maplet size */
	mp->xdim = gdk_pixbuf_get_width ( mp->pixbuf );
	mp->ydim = gdk_pixbuf_get_height ( mp->pixbuf );

	mp->maplet_index_lat = maplet_index_lat;
	mp->maplet_index_long = maplet_index_long;

	mp->next = sp->cache;
	sp->cache = mp;

	return mp;
}

/* THE END */
