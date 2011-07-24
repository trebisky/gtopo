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
#include "protos.h"

extern struct topo_info info;
extern struct settings settings;

/* I have had the maplet cache get up to 2500 entries without
 * any trouble, but someday may want to monitor the size and
 * recycle entries with an age timestamp.
 * Also, it may be possible to use a more sophisticated data
 * structure than a linear linked list to speed up the search.
 * (use the low bits as a hash index, this puts local blocks
 * into separate hash bins, something like:
 * (maplet_lat & 0xf) << 4 | (maplet_long & 0xf)
 * Also, when an entry is found, move it to the front of
 * the list.
 */

static struct maplet *
maplet_new ( void )
{
	struct maplet *mp;

	mp = (struct maplet *) gmalloc ( sizeof(struct maplet) );
	if ( ! mp )
	    error ("maplet_new, out of mem\n");

	return mp;
}

/* XXX - plain old linear search, maybe someday will do some hashing.
 */
static struct maplet *
maplet_cache_lookup ( struct maplet *head, int maplet_x, int maplet_y )
{
	struct maplet *cp;

	for ( cp = head; cp; cp = cp->next ) {
	    if ( cp->world_y == maplet_y &&
	    	cp->world_x == maplet_x ) {
		    return cp;
	    }
	}
	return ( struct maplet *) NULL;
}

static void
maplet_cache_dump ( void )
{
	struct maplet *cp;

	for ( cp = info.series->cache; cp; cp = cp->next ) {
	    printf ( "series %d, x, y = %d %d %s\n",
		info.series->series, cp->world_x, cp->world_y, cp->tpq_path );
	}
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
static int
load_maplet_scale ( struct maplet *mp )
{
	GdkPixbuf *tmp;
	double pixel_width;
	int pixel_norm;
	struct tpq_info *tp;

	if ( ! load_tpq_maplet ( mp ) )
	    return 0;

	/* get the maplet size */
	mp->xdim = gdk_pixbuf_get_width ( mp->pixbuf );
	mp->ydim = gdk_pixbuf_get_height ( mp->pixbuf );

	tp = mp->tpq;

	/* The usual situation here with a 7.5 minute quad is that the
	 * maplets are 256 tall by 512 wide, before fussing with cos(lat)
	 */
	pixel_width = mp->ydim * tp->maplet_long_deg / tp->maplet_lat_deg;
	pixel_width *= cos ( tp->mid_lat * DEGTORAD );
	pixel_norm = pixel_width;

	if ( settings.verbose & V_SCALE )
	    printf ( "maplet scale: %d %d --> %d %d\n", mp->xdim, mp->ydim, pixel_norm, mp->ydim );

	if ( mp->xdim < pixel_norm - 8 || mp->xdim > pixel_norm + 8 ) {
	    if ( settings.verbose & V_SCALE )
		printf ( "SCALING\n" );
	    tmp = mp->pixbuf;
	    mp->pixbuf = gdk_pixbuf_scale_simple ( tmp, pixel_norm, mp->ydim, GDK_INTERP_BILINEAR );
	    gdk_pixbuf_unref ( tmp );
	    mp->xdim = gdk_pixbuf_get_width ( mp->pixbuf );
	    mp->ydim = gdk_pixbuf_get_height ( mp->pixbuf );
	}

	return 1;
}

struct maplet *
load_maplet ( int maplet_x, int maplet_y )
{
    	struct series *sp;
    	struct maplet *mp;
	int rv;

	sp = info.series;

	if ( settings.verbose & V_MAPLET )
	    printf ( "Load maplet for position %d %d\n", maplet_x, maplet_y );

	mp = maplet_cache_lookup ( info.series->cache, maplet_x, maplet_y );
	if ( mp ) {
	    if ( settings.verbose & V_MAPLET )
		printf ( "maplet cache hit: %d %d\n", maplet_x, maplet_y );
	    return mp;
	} else {
	    if ( settings.verbose & V_MAPLET ) {
		printf ( "maplet cache lookup fails for: %d %d\n", maplet_x, maplet_y );
		maplet_cache_dump ();
	    }
	}

	/* Set up a new entry.
	 */
	mp = maplet_new ();

	/* This is what lookup_series uses */
	mp->world_x = maplet_x;
	mp->world_y = maplet_y;

	if ( settings.verbose & V_MAPLET )
	    printf ( "Read maplet(cache=%d) = %d %d\n", sp->cache_count,
		    maplet_x, maplet_y );

	if ( sp->terra ) {
	    rv = load_terra_maplet ( mp );
	} else {
	    /* Try to find it in the archive
	     * This will set tpq_path as well as
	     * tpq_index in the maplet structure.
	     */
	    if ( ! lookup_series ( mp ) ) {
		free ( (char *) mp );
		return NULL;
	    }
	    rv = load_maplet_scale ( mp );
	}

	if ( ! rv ) {
	    free ( (char *) mp );
	    return NULL;
	}

	mp->next = sp->cache;
	sp->cache = mp;
	mp->time = sp->cache_count++;

	return mp;
}

/* This is an iterator to crank through all the maplets in a file
 * and feed them one by one to some handler function.
 * NO LONGER USED, method_file() in archive.c does this now!
 *
 * This involves some keeping straight of coordinate systems,
 * ---
 * Long and lat should be familiar,
 *  long is positive up (to the north)
 *  lat is positive right (to the east)
 *  (don't get fooled!!)
 * My maplet count system is an integer count so that:
 *   mx increases left (to the west)
 *   my increases up (to the north)
 * Counts within a TPQ file have
 *   the upper left is the 0,0 maplet in the TPQ file,
 *   ix increases right (opposite mx) from 0 on the left.
 *   iy increases down (opposite my) from 0 at the top.
 *   the tpq index is computed from ix and iy.
 *
 * Maplet counts are of little use for the file method here.
 *  each file in a series typically has a different maplet size,
 *  so maplet counts for one file cannot be compared to maplet
 *  counts from another file in the same series.
 * (This is true for alaska versus hawaii versus US at the
 *  ATLAS and STATE levels).
 * We just keep track of maplets to locate them in the cache,
 * and so the handler we call knows what it is getting.
 */

void
file_maplets ( struct method *xp, mfptr handler )
{
    	struct maplet *mp;
	struct series *sp;
	struct tpq_info *tp;
	int maplet_x, maplet_y;
	int tpq_x, tpq_y;

	sp = info.series;

	if ( xp->type != M_FILE )
	    return;

	tp = xp->tpq;

	for ( tpq_y = 0; tpq_y < tp->lat_count; tpq_y++ ) {
	    for ( tpq_x = 0; tpq_x < tp->long_count; tpq_x++ ) {

		/*
		maplet_x = tp->long_count - tpq_x - 1;
		*/
		maplet_x = tpq_x;
		maplet_y = tp->lat_count - tpq_y - 1;

		if ( settings.verbose & V_MAPLET )
		    printf ( "file maplet mx, my: %d %d\n", maplet_x, maplet_y );

		/* First try the cache */
		mp = maplet_cache_lookup ( xp->cache, maplet_x, maplet_y );
		if ( mp ) {
		    if ( settings.verbose & V_MAPLET )
			printf ( "file maplets cache hit\n" );
		    (*handler) (mp);
		    continue;
		}

		mp = maplet_new ();

		mp->world_x = maplet_x;
		mp->world_y = maplet_y;

		mp->sheet_x = 0;	/* XXX */
		mp->sheet_y = 0;

		if ( settings.verbose & V_MAPLET )
		    printf ( "File maplets read (%d) = %s\n", sp->cache_count, tp->path );

		mp->tpq_path = tp->path;
		mp->tpq_index = tpq_y * tp->long_count + tpq_x;

		if ( ! load_maplet_scale ( mp ) ) {
		    free ( (char *) mp );
		    continue;
		}

		mp->next = xp->cache;
		xp->cache = mp;
		mp->time = sp->cache_count++;

		(*handler) (mp);

	    }
	}
}

/* This is used when we want to "sniff at" a TPQ file prior to actually loading and
 * displaying maplets from it.  The best thing to do is to load a maplet near
 * the center of the map, at least when we are in file view mode.
 */
struct maplet *
load_maplet_any ( char *path )
{
    	struct maplet *mp;
	struct series *sp;

	mp = maplet_new ();

	mp->tpq_path = strhide ( path );
	mp->tpq_index = -1;

	load_maplet_scale ( mp );

	/* Put this on the cache for that series */
	sp = &info.series_info[mp->tpq->series];
	mp->next = sp->cache;
	sp->cache = mp;
	mp->time = sp->cache_count++;

	return mp;
}

/* THE END */
