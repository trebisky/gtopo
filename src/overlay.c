/*
 *  GTopo -- overlay.c
 *
 *  Copyright (C) 2007, Thomas J. Trebisky
 *  Copyright (C) 2020, Thomas J. Trebisky
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

#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "gtopo.h"
#include "protos.h"

#include "xml.h"
#include "remote.h"

extern struct remote remote_info;

/* This is overlay.c
 *
 * This was added to gtopo in June of 2020 with the aim of being able
 * to plot "markers" (like waypoints or places) as well as tracks
 * extracted from GPX files.
 *
 * Up to this time only gtopo.c contained any actual gtk2 specific code,
 *  but this file is now a second file with gtk2 specifics.
 *
 * I began using old calls like gdk_draw_rectangle(), but learned that
 *  they are deprecated -- so I decided to bite the bullet and
 *  do all of this with Cairo, as recommended.
 *
 * The deprecation messages looked like this:
 *  gdk_draw_rectangle has been deprecated since version 2.22 and should not be used in newly-written code.
 *  Use cairo_rectangle() and cairo_fill() or cairo_stroke() instead.
 *   For stroking, the same caveats for converting code apply as for gdk_draw_line().
 */

#include "gpx.h"


extern struct topo_info info;
extern struct settings settings;
extern struct places_info p_info;

extern struct viewport vp_info;

extern struct waypoint *way_head;
extern struct track *track_head;

/* Baldy Saddle, Santa Rita Mountains */
/* A test coordinate to try to mark */
static double x_long = -110.84633;
static double x_lat = 31.70057;

/* for debug */
void
show_first_track ( float ll [][2] )
{
	printf ( " lat, long = %.7f, %.7f\n", ll[0][0], ll[0][1] );
}

/* for debug */
static void
check_gpx ( void )
{
	struct waypoint *wp;
	struct track *tp;
	int n;

	n = 0;
	wp = way_head;
	while ( wp ) {
	    n++;
	    wp = wp->next;
	}
	printf ( "%d waypoints\n", n );

	n = 0;
	tp = track_head;
	while ( tp ) {
	    n++;
	    printf ( "Track %d, %d points\n", n, tp->count );
	    show_first_track ( (float (*)[2]) tp->data );
	    tp = tp->next;
	}

}

void
overlay_init ( void )
{
	// check_gpx ();
}

#define TRACK_MARKER_SIZE	3
#define WAYPOINT_MARKER_SIZE	6

static void
draw_marker_x ( cairo_t *cr, int mx, int my, int size )
{
	mx -= size/2;
	my -= size/2;

	/* Blue */
	cairo_set_source_rgb (cr, 0, 0, 65535 );
	cairo_rectangle(cr, mx, my, size, size );
	cairo_fill (cr);
}

/* shared by test_mark() and rem_mark() */
static void
make_mark ( cairo_t *cr, double a_long, double a_lat )
{
	double long1, long2;
	double lat1, lat2;
	int x1, y1;
	int visible;

	// printf ( "Center: %.4f, %.4f\n", info.long_deg, info.lat_deg );
	// printf ( "Scale: %.5f, %.5f\n",
	//     info.series->x_pixel_scale, info.series->y_pixel_scale );

	long1 = info.long_deg - vp_info.vxcent * info.series->x_pixel_scale;
	long2 = info.long_deg + vp_info.vxcent * info.series->x_pixel_scale;
	lat1 = info.lat_deg - vp_info.vycent * info.series->y_pixel_scale;
	lat2 = info.lat_deg + vp_info.vycent * info.series->y_pixel_scale;
	// printf ( "Long: %.4f, %.4f\n", long1, long2 );
	// printf ( "Lat : %.4f, %.4f\n", lat1, lat2 );

	// mark ( cr, a_long, a_lat );
	visible = 1;
	if ( a_long < long1 || a_long > long2 ) visible = 0;
	if ( a_lat < lat1 || a_lat > lat2 ) visible = 0;

	if ( visible ) {
	    x1 = ( a_long - long1 ) / info.series->x_pixel_scale;
	    y1 = ( lat2 - a_lat ) / info.series->y_pixel_scale;
	    draw_marker_x ( cr, x1, y1, WAYPOINT_MARKER_SIZE );
	}
}

/* For test only, draw the mark at Baldy Saddle */
static void
test_mark ( cairo_t *cr )
{
#ifdef notdef
	double long1, long2;
	double lat1, lat2;
	int x1, y1;
	int visible;

	// printf ( "Center: %.4f, %.4f\n", info.long_deg, info.lat_deg );
	// printf ( "Scale: %.5f, %.5f\n",
	//     info.series->x_pixel_scale, info.series->y_pixel_scale );

	long1 = info.long_deg - vp_info.vxcent * info.series->x_pixel_scale;
	long2 = info.long_deg + vp_info.vxcent * info.series->x_pixel_scale;
	lat1 = info.lat_deg - vp_info.vycent * info.series->y_pixel_scale;
	lat2 = info.lat_deg + vp_info.vycent * info.series->y_pixel_scale;
	// printf ( "Long: %.4f, %.4f\n", long1, long2 );
	// printf ( "Lat : %.4f, %.4f\n", lat1, lat2 );

	// mark ( cr, x_long, x_lat );
	visible = 1;
	if ( x_long < long1 || x_long > long2 ) visible = 0;
	if ( x_lat < lat1 || x_lat > lat2 ) visible = 0;

	if ( visible ) {
	    x1 = ( x_long - long1 ) / info.series->x_pixel_scale;
	    y1 = ( lat2 - x_lat ) / info.series->y_pixel_scale;
	    draw_marker_x ( cr, x1, y1, WAYPOINT_MARKER_SIZE );
	}
#endif

	make_mark ( cr, x_long, x_lat );
}

static void
rem_mark ( cairo_t *cr )
{
	make_mark ( cr, remote_info.r_long, remote_info.r_lat );
}

static void
draw_path ( cairo_t *cr, float path[][2], int count )
{
	double long1, long2;
	double lat1, lat2;
	float x_long, x_lat;
	int x1, y1;
	int visible;
	int i;

	/* Get limits of visible region in lat/long */
	long1 = info.long_deg - vp_info.vxcent * info.series->x_pixel_scale;
	long2 = info.long_deg + vp_info.vxcent * info.series->x_pixel_scale;
	lat1 = info.lat_deg - vp_info.vycent * info.series->y_pixel_scale;
	lat2 = info.lat_deg + vp_info.vycent * info.series->y_pixel_scale;

	for ( i=0; i<count; i++ ) {

	    /* XXX */
	    x_lat = path[i][0];
	    x_long = path[i][1];

	    visible = 1;
	    if ( x_long < long1 || x_long > long2 ) visible = 0;
	    if ( x_lat < lat1 || x_lat > lat2 ) visible = 0;

	    if ( visible ) {
		x1 = ( x_long - long1 ) / info.series->x_pixel_scale;
		y1 = ( lat2 - x_lat ) / info.series->y_pixel_scale;
		draw_marker_x ( cr, x1, y1, TRACK_MARKER_SIZE );
	    }
	}
}

static void
rem_path ( cairo_t *cr )
{
	draw_path ( cr, remote_info.data, remote_info.npath );
	// (float (*)[2]) tp->data, tp->count );
}

static void
draw_tracks ( cairo_t *cr )
{
	double long1, long2;
	double lat1, lat2;
	int visible;
	struct track *tp;

	/* Get limits of visible region in lat/long */
	long1 = info.long_deg - vp_info.vxcent * info.series->x_pixel_scale;
	long2 = info.long_deg + vp_info.vxcent * info.series->x_pixel_scale;
	lat1 = info.lat_deg - vp_info.vycent * info.series->y_pixel_scale;
	lat2 = info.lat_deg + vp_info.vycent * info.series->y_pixel_scale;

	tp = track_head;
	while ( tp ) {

	    visible = 1;
	    if ( tp->long_min > long2 ) visible = 0;
	    if ( tp->long_max < long1 ) visible = 0;
	    if ( tp->lat_min > lat2 ) visible = 0;
	    if ( tp->lat_max < lat1 ) visible = 0;

	    if ( visible )
		draw_path ( cr, (float (*)[2]) tp->data, tp->count );

	    tp = tp->next;
	}
}

static void
draw_waypoints ( cairo_t *cr )
{
	double long1, long2;
	double lat1, lat2;
	int x1, y1;
	int visible;
	float x_long, x_lat;
	struct waypoint *wp;

	/* Get limits of visible region in lat/long */
	long1 = info.long_deg - vp_info.vxcent * info.series->x_pixel_scale;
	long2 = info.long_deg + vp_info.vxcent * info.series->x_pixel_scale;
	lat1 = info.lat_deg - vp_info.vycent * info.series->y_pixel_scale;
	lat2 = info.lat_deg + vp_info.vycent * info.series->y_pixel_scale;
	// printf ( "Long: %.4f, %.4f\n", long1, long2 );
	// printf ( "Lat : %.4f, %.4f\n", lat1, lat2 );

	wp = way_head;
	while ( wp ) {

	    /* XXX */
	    x_lat = wp->way_lat;
	    x_long = wp->way_long;

	    visible = 1;
	    if ( x_long < long1 || x_long > long2 ) visible = 0;
	    if ( x_lat < lat1 || x_lat > lat2 ) visible = 0;

	    if ( visible ) {
		x1 = ( x_long - long1 ) / info.series->x_pixel_scale;
		y1 = ( lat2 - x_lat ) / info.series->y_pixel_scale;
		draw_marker_x ( cr, x1, y1, WAYPOINT_MARKER_SIZE );
	    }

	    wp = wp->next;
	}
}

void
overlay_redraw ( void )
{
	cairo_t *cr;

	int vxdim, vydim;
	int x1, y1;
	double long1, long2;
	double lat1, lat2;
	int visible;

	cr = gdk_cairo_create (vp_info.da->window);
	// printf ( "Overlay redraw, DA =  %08x\n", vp_info.da );
	// void *zz;
	// zz = vp_info.da->window;
	// printf ( "Overlay redraw, DA.window =  %08x\n", zz );

	/* get the viewport size.
	 * This gets updated elsewhere as it should and is
	 *  always correct.
	 */
        vxdim = vp_info.vx;
        vydim = vp_info.vy;

#ifdef notdef
	x1 = vxdim / 2 - 50;
	y1 = vydim / 2 - 50;
	// printf ( "Overlay rectangle: %d, %d\n", x1, y1 );

	/* Blue */
	cairo_set_source_rgb (cr, 0, 0, 65535);
	cairo_rectangle(cr, x1, y1, 100, 100);
	cairo_fill (cr);
#endif

#ifdef notdef
	x1 = vxdim / 2;
	y1 = vydim / 2;

	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_move_to ( cr, 0, y1);
        cairo_line_to ( cr, vxdim, y1);
	cairo_move_to ( cr, x1, 0);
        cairo_line_to ( cr, x1, vydim);
	cairo_stroke ( cr );
#endif

#ifdef notdef
	/* We keep vxcent and vycent as handy variables */
	// x1 = vxdim / 2;
	// y1 = vydim / 2;
	x1 = vp_info.vxcent;
	y1 = vp_info.vycent;

	draw_marker_x ( cr, x1, y1, WAYPOINT_MARKER_SIZE );
#endif

	// Mark at baldy saddle
	// test_mark ( cr );

	if ( remote_info.active ) {
	    rem_mark ( cr );
	}

	if ( remote_info.path ) {
	    rem_path ( cr );
	}

	draw_waypoints ( cr );
	draw_tracks ( cr );

#ifdef notdef
	// printf ( "Center: %.4f, %.4f\n", info.long_deg, info.lat_deg );
	// printf ( "Scale: %.5f, %.5f\n",
	//     info.series->x_pixel_scale, info.series->y_pixel_scale );

	long1 = info.long_deg - vp_info.vxcent * info.series->x_pixel_scale;
	long2 = info.long_deg + vp_info.vxcent * info.series->x_pixel_scale;
	lat1 = info.lat_deg - vp_info.vycent * info.series->y_pixel_scale;
	lat2 = info.lat_deg + vp_info.vycent * info.series->y_pixel_scale;
	// printf ( "Long: %.4f, %.4f\n", long1, long2 );
	// printf ( "Lat : %.4f, %.4f\n", lat1, lat2 );

	// mark ( cr, x_long, x_lat );
	visible = 1;
	if ( x_long < long1 || x_long > long2 ) visible = 0;
	if ( x_lat < lat1 || x_lat > lat2 ) visible = 0;

	if ( visible ) {
	    x1 = ( x_long - long1 ) / info.series->x_pixel_scale;
	    y1 = ( lat2 - x_lat ) / info.series->y_pixel_scale;
	    draw_marker_x ( cr, x1, y1, WAYPOINT_MARKER_SIZE );
	}
#endif

	// gdk_draw_line ( info.series->pixels, vp_info.da->style->black_gc, xx, 0, xx, vp_info.vy );

	// clear screen
	// gdk_draw_rectangle ( info.series->pixels, vp_info.da->style->white_gc, TRUE, 0, 0, vxdim, vydim );
	// gdk_draw_rectangle ( info.series->pixels, vp_info.da->style->red_gc, TRUE, x1, y1, xw, yw );
}

/* just doing a overlay_redraw adds a new marker and keeps the old as well.
 * we have to do a full redraw to clear the slate.
 */
void
remote_redraw ( void )
{
	// overlay_redraw ();
	full_redraw ();
}

/* THE END */
