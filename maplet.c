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

static struct maplet *
maplet_new ( void )
{
	struct maplet *mp;

	mp = (struct maplet *) malloc ( sizeof(struct maplet) );
	if ( ! mp )
	    error ("maplet_new, out of mem\n");

	return mp;
}

struct maplet *
maplet_lookup ( int index_lat, int index_long )
{
	struct maplet *cp;

	for ( cp = info.series->cache; cp; cp = cp->next ) {
	    if ( cp->world_index_lat == index_lat &&
	    	cp->world_index_long == index_long ) {
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
	if ( info.verbose > 1 )
	    printf ( "maplet scale: %d %d --> %d %d\n", mp->xdim, mp->ydim, pixel_norm, mp->ydim );

	if ( mp->xdim < pixel_norm - 8 || mp->xdim > pixel_norm + 8 ) {
	    if ( info.verbose > 1 )
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
	 * tpq_index in the maplet structure.
	 */
	if ( ! lookup_series ( mp, long_maplet, lat_maplet ) ) {
	    free ( (char *) mp );
	    return NULL;
	}

	mp->world_index_long = long_maplet;
	mp->world_index_lat = lat_maplet;

	if ( info.verbose > 1 )
	    printf ( "Read maplet(%d) = %d %d -- %d\n", sp->cache_count,
		    long_maplet, lat_maplet, index );

	if ( ! load_maplet_scale ( mp ) ) {
	    free ( (char *) mp );
	    return NULL;
	}

	mp->next = sp->cache;
	sp->cache = mp;
	mp->time = sp->cache_count++;

	return mp;
}

typedef void (*mfptr) ( struct maplet * );

/* States are weird, what we do is use this as an iterator to
 * grind through all the maplets and call our callback for each.
 * For states there is one TPQ file with one giant maplet per state.
 */
void
state_maplets ( mfptr handler )
{
    	struct maplet *mp;
	struct series *sp;
	struct method *xp;
	struct tpq_info *tp;
	int long_maplet, lat_maplet;

	sp = info.series;

	/* loop through all the state methods, and that
	 * is all we expect at this level ....
	 */
	for ( xp = sp->methods; xp; xp = xp->next ) {
	    if ( xp->type != M_STATE )
		continue;
	    tp = xp->tpq;

	    /* Use these as indices */
	    long_maplet = - tp->w_long;
	    lat_maplet = tp->s_lat;
	    if ( info.verbose > 1 )
		printf ( "state maplets long/lat: %d %d\n", long_maplet, lat_maplet );

	    /* First try the cache */
	    mp = maplet_lookup ( lat_maplet, long_maplet );
	    if ( mp ) {
		if ( info.verbose > 1 )
		    printf ( "state maplets cache hit\n" );
		(*handler) (mp);
	    }

	    mp = maplet_new ();

	    mp->world_index_long = long_maplet;
	    mp->world_index_lat = lat_maplet;
	    mp->sheet_index_long = 0;
	    mp->sheet_index_lat = 0;

	    if ( info.verbose > 1 )
		printf ( "State maplets read (%d) = %s\n", sp->cache_count, tp->path );

	    mp->tpq_path = tp->path;
	    mp->tpq_index = 0;

	    if ( ! load_maplet_scale ( mp ) ) {
		free ( (char *) mp );
		continue;
	    }

	    mp->next = sp->cache;
	    sp->cache = mp;
	    mp->time = sp->cache_count++;

	    (*handler) (mp);
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
