/* maplet.c -- part of gtopo
 * Tom Trebisky  MMT Observatory, Tucson, Arizona
 */
#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "gtopo.h"

/* maplet cache */
struct maplet *maplet_head;

int maplet_count = 0;

void
maplet_init ( void )
{
    	maplet_head = (struct maplet *) NULL;
}

/* Given a center maplet, load a neighbor
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

    	/* First try cache */
	maplet_index_lat = cmp->maplet_index_lat - y;
	maplet_index_long = cmp->maplet_index_long - x;

	for ( cp = maplet_head; cp; cp = cp->next ) {
	    if ( cp->maplet_index_lat == maplet_index_lat &&
	    	cp->maplet_index_long == maplet_index_long ) {
		    printf ( "maplet nbr cache hit: (%d %d) %d %d\n", x, y, maplet_index_lat, maplet_index_long );
		    return cp;
	    }
	}

	/* XXX - For now, we just stay on one 7.5 minute sheet
	 */
	x_maplet = cmp->x_maplet + x;
	if ( x_maplet < 0 || x_maplet > 4 ) {
	    printf ( "maplet nbr off sheet: (%d %d) %d %d\n", x, y, maplet_index_lat, maplet_index_long );
	    return NULL;
	}

	y_maplet = cmp->y_maplet + y;
	if ( y_maplet < 0 || y_maplet > 9 ) {
	    printf ( "maplet nbr off sheet: (%d %d) %d %d\n", x, y, maplet_index_lat, maplet_index_long );
	    return NULL;
	}

	mp = (struct maplet *) malloc ( sizeof(struct maplet) );
	if ( ! mp )
	    error ("load maplet_nbr, out of mem\n", "" );
	maplet_count++;

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

	mp->pixbuf = load_tpq_maplet ( cmp->tpq_path, x_maplet, y_maplet );

	/* get the maplet size */
	mp->xdim = gdk_pixbuf_get_width ( mp->pixbuf );
	mp->ydim = gdk_pixbuf_get_height ( mp->pixbuf );

	mp->maplet_index_lat = maplet_index_lat;
	mp->maplet_index_long = maplet_index_long;

	mp->next = maplet_head;
	maplet_head = mp;

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
	maplet_lat = pos->lat_deg * 8.0 * 10.0;
	maplet_long = pos->long_deg * 8.0 * 5.0;

	/* Truncate these to integers unique to a maplet
	 * assuming 7.5 minute quads and 5x10 maplets
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
	for ( cp = maplet_head; cp; cp = cp->next ) {
	    if ( cp->maplet_index_lat == maplet_index_lat &&
	    	cp->maplet_index_long == maplet_index_long ) {
		    printf ( "maplet cache hit: %d %d\n", maplet_index_lat, maplet_index_long );
		    return cp;
	    }
	}
	printf ( "maplet cache lookup fails for: %d %d\n", maplet_index_lat, maplet_index_long );

	/* Looks like we will be setting up a new entry.
	 */
	mp = (struct maplet *) malloc ( sizeof(struct maplet) );
	if ( ! mp )
	    error ("load maplet, out of mem\n", "" );
	maplet_count++;

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

	mp->pixbuf = load_tpq_maplet ( mp->tpq_path, x_maplet, y_maplet );

	/* get the maplet size */
	mp->xdim = gdk_pixbuf_get_width ( mp->pixbuf );
	mp->ydim = gdk_pixbuf_get_height ( mp->pixbuf );

	mp->maplet_index_lat = maplet_index_lat;
	mp->maplet_index_long = maplet_index_long;

	mp->next = maplet_head;
	maplet_head = mp;

	return mp;
}

/* THE END */
