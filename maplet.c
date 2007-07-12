/* maplet.c -- part of gtopo
 * Tom Trebisky  MMT Observatory, Tucson, Arizona
 */
#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "gtopo.h"

extern struct position cur_pos;

/* maplet cache */
struct maplet *maplets[N_SERIES];;

/* XXX - 
 * I have had this run up to 2500 or so without any trouble,
 * but someday may want to monitor this and recycle entries
 * with an old timestamp.
 * Also, it may be possible to use a tree rather than a
 * linked list to speed the lookup.
 * Also, keep separate series in their own "hash" to
 * help speed this up.
 */
int maplet_count = 0;

void
maplet_init ( void )
{
	int i;

	for ( i=0; i<N_SERIES; i++ )
	    maplets[i] = (struct maplet *) NULL;
}

struct maplet *
maplet_new ( void )
{
	struct maplet *mp;

	mp = (struct maplet *) malloc ( sizeof(struct maplet) );
	if ( ! mp )
	    error ("load maplet_nbr, out of mem\n", "" );

	mp->time = maplet_count++;
	return mp;
}

struct maplet *
maplet_lookup ( int maplet_index_lat, int maplet_index_long )
{
	struct maplet *cp;

	for ( cp = cur_pos.maplet_cache; cp; cp = cp->next ) {
	    if ( cp->maplet_index_lat == maplet_index_lat &&
	    	cp->maplet_index_long == maplet_index_long ) {
		    return cp;
	    }
	}
	return ( struct maplet *) NULL;
}

/* New sheet (and not in cache)
 */
struct maplet *
load_maplet_quad ( struct position *pos, int maplet_lat, int maplet_long )
{
    	struct maplet *mp;

	mp = maplet_new ();

	/* Try to find it in the archive
	 * This will set tpq_path as well as
	 * x_maplet and y_maplet in the maplet structure.
	 */
	if ( ! lookup_quad_nbr ( pos, mp, maplet_lat, maplet_long ) ) {
	    free ( (char *) mp );
	    return NULL;
	}

	mp->pixbuf = load_tpq_maplet ( mp->tpq_path, mp->y_maplet * pos->long_count + mp->x_maplet );

	/* get the maplet size */
	mp->xdim = gdk_pixbuf_get_width ( mp->pixbuf );
	mp->ydim = gdk_pixbuf_get_height ( mp->pixbuf );

	mp->maplet_index_lat = maplet_lat;
	mp->maplet_index_long = maplet_long;

	mp->next = pos->maplet_cache;
	pos->maplet_cache = mp;

	return mp;
}

/* Given a center maplet, load a neighbor
 * x and y are offsets in maplet units, with signs
 * correct for indexing a maplet within a quad.
 */
struct maplet *
load_maplet_nbr ( struct position *pos, int x, int y )
{
    	struct maplet *cmp;
    	struct maplet *cp;
    	struct maplet *mp;
	int maplet_index_lat;
	int maplet_index_long;
	int x_maplet;
	int y_maplet;

	/* pointer to center maplet */
	cmp = pos->maplet;

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
	if ( x_maplet < 0 || x_maplet >= pos->long_count ) {
	    printf ( "maplet nbr off sheet: (%d %d) %d %d\n", x, y, maplet_index_lat, maplet_index_long );
	    return load_maplet_quad ( pos, maplet_index_lat, maplet_index_long );
	}

	y_maplet = cmp->y_maplet + y;
	if ( y_maplet < 0 || y_maplet >= pos->lat_count ) {
	    printf ( "maplet nbr off sheet: (%d %d) %d %d\n", x, y, maplet_index_lat, maplet_index_long );
	    return load_maplet_quad ( pos, maplet_index_lat, maplet_index_long );
	}

	mp = maplet_new ();

	/* XXX - do we need to copy more stuff from cmp ??
	 *   remember, this may be found in cache later
	 *   and become cmp.
	 */
	mp->x_maplet = x_maplet;
	mp->y_maplet = y_maplet;

	printf ( "x,y maplet nbr(%d) = %d %d -- %d %d\n", maplet_count, x_maplet, y_maplet,
	    maplet_index_lat, maplet_index_long );

	/* Each maplet from the same sheet has links to the same
	 * path string, which is just fine.
	 */
	mp->tpq_path = cmp->tpq_path;

	mp->pixbuf = load_tpq_maplet ( cmp->tpq_path, y_maplet * pos->long_count + x_maplet );

	/* get the maplet size */
	mp->xdim = gdk_pixbuf_get_width ( mp->pixbuf );
	mp->ydim = gdk_pixbuf_get_height ( mp->pixbuf );

	mp->maplet_index_lat = maplet_index_lat;
	mp->maplet_index_long = maplet_index_long;

	mp->next = pos->maplet_cache;
	pos->maplet_cache = mp;

	return mp;
}

struct maplet *
load_maplet ( struct position *pos )
{
    	struct maplet *mp;
    	struct maplet *cp;
	int x_maplet, y_maplet;
	double maplet_lat, maplet_long;
	int maplet_index_lat;
	int maplet_index_long;

	printf ( "Load maplet for position %.4f %.4f\n", pos->lat_deg, pos->long_deg );

	/* Convert from degrees to "maplet units"
	 * (keep these as floating point).
	 */
	maplet_lat = pos->lat_deg / pos->maplet_lat_deg;
	maplet_long = pos->long_deg / pos->maplet_long_deg;

	/* Truncate these to integers unique to a maplet
	 */
	maplet_index_lat = maplet_lat;
	maplet_index_long = maplet_long;

	pos->fy = 1.0 - (maplet_lat - maplet_index_lat);
	pos->fx = 1.0 - (maplet_long - maplet_index_long);

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
	if ( ! lookup_quad ( pos, mp ) ) {
	    free ( (char *) mp );
	    return NULL;
	}

	x_maplet = mp->x_maplet;
	y_maplet = mp->y_maplet;

	printf ( "x,y maplet(%d) = %d %d -- %d %d\n", maplet_count, x_maplet, y_maplet,
	    maplet_index_lat, maplet_index_long );

	mp->pixbuf = load_tpq_maplet ( mp->tpq_path, y_maplet * pos->long_count + x_maplet );

	/* get the maplet size */
	mp->xdim = gdk_pixbuf_get_width ( mp->pixbuf );
	mp->ydim = gdk_pixbuf_get_height ( mp->pixbuf );

	mp->maplet_index_lat = maplet_index_lat;
	mp->maplet_index_long = maplet_index_long;

	mp->next = pos->maplet_cache;
	pos->maplet_cache = mp;

	return mp;
}

/* THE END */
