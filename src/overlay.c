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

extern struct topo_info info;
extern struct settings settings;
extern struct places_info p_info;

extern struct viewport vp_info;

/* Baldy Saddle, Santa Rita Mountains */
/* A test coordinate to try to mark */
static double x_long = -110.84633;
static double x_lat = 31.70057;

void
overlay_init ( void )
{
/*
cm = pixmap.get_colormap()
red = cm.alloc_color('red')
gc = pixmap.new_gc(foreground=red)
pixmap.draw_line(gc,0,0,w,h)
*/
}

#define MARKER_SIZE	12

static void
draw_marker ( cairo_t *cr, int mx, int my )
{
	mx -= MARKER_SIZE/2;
	my -= MARKER_SIZE/2;

	/* Blue */
	cairo_set_source_rgb (cr, 0, 0, 65535);
	cairo_rectangle(cr, mx, my, MARKER_SIZE, MARKER_SIZE);
	cairo_fill (cr);
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

	draw_marker ( cr, x1, y1 );
#endif

	/* This converts mouse location into long/lat
	c_long = info.long_deg + (mouse_info.x-vp_info.vxcent) * info.series->x_pixel_scale;
        c_lat = info.lat_deg - (mouse_info.y-vp_info.vycent) * info.series->y_pixel_scale;
	*/

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
	    draw_marker ( cr, x1, y1 );
	}

	// gdk_draw_line ( info.series->pixels, vp_info.da->style->black_gc, xx, 0, xx, vp_info.vy );

	// clear screen
	// gdk_draw_rectangle ( info.series->pixels, vp_info.da->style->white_gc, TRUE, 0, 0, vxdim, vydim );
	// gdk_draw_rectangle ( info.series->pixels, vp_info.da->style->red_gc, TRUE, x1, y1, xw, yw );
}

/* THE END */
