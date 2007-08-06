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

#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "gtopo.h"

#define MINIMUM_VIEW	100
#define INITIAL_VIEW	800

/* gtopo.c - the main file for gtopo
 *
 * Tom Trebisky  MMT Observatory, Tucson, Arizona
 *
 * version 0.1 - really just a JPG file display gizmo, but
 *	a starting point for what is to follow.  Did not get
 *	any bits displayed until I moved the draw calls into
 *	the expose event handler  7/2/2007
 * version 0.2 - draw into a pixmap which gets copied to
 *	the drawing area on an expose event.  7/3/2007
 * version 0.3 - actually pull a single maplet out of
 *	a TPQ file and display it.  7/5/2007
 * version 0.4 - pull a 2x2 group of maplets out of
 *	a TPQ file and display them.  7/5/2007
 * version 0.5 - begin to navigate the directory structure
 * 	as shipped on the CD-roms  7/6/2007
 *	introduce archive.c
 * version 0.6 - try to get mouse events 7/6/2007
 * 	also add code to display an area given a lat and long.
 *	split out tpq_io.c
 * version 0.7 - mouse events working, always displays central
 * 	maplet on image center  7/8/2007
 *	add code to find neighboring maplets and roam around
 *	an entire 7.5 minute sheet.  It will jump to new sheets
 *	if you click on a white region, actually usable.  7/9/2007
 *	jumps from sheet to sheet cleanly 7/11/2007
 * version 0.8 - add alternate series support.
 * 	works for 100K series 7/12/2007
 *	add series structure and reorganize 7/13/2007
 * version 0.8.2 - work on series 3 support. 7/24/2007
 *	add support for Nevada (version 4.2 of TOPO!)
 *	works on series 2-5, series 1 maplet sizes vary.
 * version 0.9.0 - first release  8/3/2007, add GPL stuff
 *
 *  TODO
 *   - add a mode where this can be pointed at any TPQ file
 *	and it will view it. (Use the -f switch).
 *  - use tree rather than linear linked list for section
 *	stuff and for maplet cache.
 *   - add age field to maplet cache and expire/recycle
 *     if size grows beyond some limit.
 *   - handle maplet size discontinuity.
 *   - be able to run off of mounted CDrom
 *   - put temp file in cwd, home, then /tmp
 *     give it a .topo.tmp name.
 *   - show lat and long of current point
 *   - positioner manager window to save positions
 *   ** emit .jpg image of selected region
 *   ** print postscript on 8.5 by 11 paper
 */

/* Some notes on map series:
 *  I began work with only the California and Arizona sets
 *  to work from.  In July, 2007 I added the Nevada set,
 *  which had some significant changes.
 *  There are some unique differences in these sets on levels
 *  1 thru 3, levels 4 and 5 seem uniform, but we shall see.
 *  The nevada set has the entire USA on levels 1,2,3
 *
 * The full state maps are found in
 * /u1/topo/AZ_D01/AZ1_MAP1/AZ1_MAP1.TPQ
 *  lat 31-38  long 108-115  422x549 jpeg
 * /u1/topo/CA_D01/CA1_MAP1/CA1_MAP1.TPQ
 *  lat 32-42  long 114-125  687x789 jpeg
 *
 * And then there is series 3 (the 500K series)
 *  for California it is a single file
 *  with 22 in latitude, 20 in longitude
 *  (440 maplets).
 * - for Arizona, we get 4 tpq files:
 *   F30105.tpq, F30110, F35105, F35110
 */

struct topo_info info;

struct series series_info_buf[N_SERIES];

/* This is a list of "root directories" where images of the
 * CDROMS may be found.  It is used as a kind of search path,
 * if directories do not exist they are ignored.
 * If they do exist, they are searched for subdirectories like
 * CA_D06 and az_d02, and so forth.
 * This allows a path to be set up that will work on multiple
 * machines that keep the topos in different places.
 */
char *topo_archives[] = { "/u1/topo", "/u2/topo", "/mmt/topo", NULL };

GdkColormap *syscm;

struct viewport {
	int vx;
	int vy;
	int vxcent;
	int vycent;
	GtkWidget *da;
} vp_info;

/* Prototypes ..........
 */

void
error ( char *msg, char *arg )
{
	printf ( msg, arg );
	exit ( 1 );
}

double
dms2deg ( int deg, int min, int sec )
{
	double rv;

	rv = deg;
	rv += ((double)min/60.0);
	rv += ((double)sec/3600.0);
	return rv;
}

gint
destroy_handler ( GtkWidget *w, GdkEvent *event, gpointer data )
{
	gtk_main_quit ();
	return FALSE;
}

void
pixmap_expose ( gint x, gint y, gint nx, gint ny )
{
	gdk_draw_pixmap ( vp_info.da->window,
		vp_info.da->style->fg_gc[GTK_WIDGET_STATE(vp_info.da)],
		info.series->pixels,
		x, y, x, y, nx, ny );
}

static int expose_count = 0;

gint
expose_handler ( GtkWidget *wp, GdkEventExpose *ep, gpointer data )
{
	if ( info.verbose && expose_count < 4 )
	    printf ( "Expose event %d\n", expose_count++ );

    	pixmap_expose ( ep->area.x, ep->area.y, ep->area.width, ep->area.height );

	return FALSE;
}

/* Draw a pixbuf onto a drawable (in this case our backing pixmap)
 * The second argument is a gc (graphics context), which only needs to
 * be non-null if you expect clipping -- it sets foreground and background colors.
 * second to last two -1, -1 tell it to get h and w from the pixbuf
 * Last 3 arguments are dithering.
 */
#define SRC_X	0
#define SRC_Y	0

void
draw_maplet ( struct maplet *mp, int x, int y )
{
	gdk_draw_pixbuf ( info.series->pixels, NULL, mp->pixbuf,
		SRC_X, SRC_Y, x, y, -1, -1,
		GDK_RGB_DITHER_NONE, 0, 0 );
}

void
state_handler ( struct maplet *mp )
{
	struct tpq_info *tp;
	double fx, fy;
	int offx, offy;
	int origx, origy;

    	printf ( "State handler %s\n", mp->tpq->path );
	printf ( "Position, long, lat: %.4f %.4f\n", info.long_deg, info.lat_deg );

	tp = mp->tpq;
	printf ( "Sheet, S, N: %.4f %.4f\n", tp->s_lat, tp->n_lat );
	printf ( "Sheet, W, E: %.4f %.4f\n", tp->w_long, tp->e_long );

	if ( info.long_deg < tp->w_long || info.long_deg > tp->e_long )
	    return;
	if ( info.lat_deg < tp->s_lat || info.lat_deg > tp->n_lat )
	    return;

	fx = (info.long_deg - tp->w_long ) / (tp->e_long - tp->w_long );
	fy = 1.0 - (info.lat_deg - tp->s_lat ) / (tp->n_lat - tp->s_lat );
	printf ( " fx, fy = %.6f %.6f\n", fx, fy );

	/* location of the center within the maplet */
	offx = fx * mp->xdim;
	offy = fy * mp->ydim;

	origx = vp_info.vxcent - offx;
	origy = vp_info.vycent - offy;
	printf ( " ox, oy = %d %d\n", origx, origy );

	draw_maplet ( mp, origx, origy );
}

/* This is the guts of what goes on during a reconfigure */
void
pixmap_redraw ( void )
{
	int vxdim, vydim;
	int nx1, nx2, ny1, ny2;
	int offx, offy;
	int origx, origy;
	int x, y;
	struct maplet *mp;

	/* get the viewport size */
	vxdim = vp_info.vx;
	vydim = vp_info.vy;

	/* clear the whole pixmap to white */
	gdk_draw_rectangle ( info.series->pixels, vp_info.da->style->white_gc, TRUE, 0, 0, vxdim, vydim );
	info.series->content = 1;

	/* The state series are a special case.  In fact the usual
	 * thing here (if there is a usual thing) is that the whole
	 * state is handled with a single tpq file with one big maplet,
	 * but the maplet size varies from state to state (but the pixel
	 * scale does seem constant).
	 * California (and Nevada) are a 11 long by 10 degree lat map of 751 by 789 pixels
	 * Arizona is a 7 by 7 degree map of 484 by 549 pixels
	 */
	if ( info.series->series == S_STATE ) {
	    state_maplets ( state_handler );
	    /* mark center */
	    gdk_draw_rectangle ( info.series->pixels, vp_info.da->style->black_gc, TRUE, vp_info.vxcent-1, vp_info.vycent-1, 3, 3 );
	    return;
	}

	/* load the maplet containing the current position so
	 * we can get the maplet pixel size up front.
	 */
	mp = load_maplet ( info.long_maplet, info.lat_maplet );

	/* The above can fail if we have used the mouse to wander off
	 * the edge of the map coverage.  We use the series information
	 * in that case to allow something like reasonable mouse motion
	 * to move us back.  Note that we replace the "guessed" info
	 * about maplet size -- with a hopefully better guess.
	 */
	if ( mp ) {
	    info.series->xdim = mp->xdim;
	    info.series->ydim = mp->ydim;
	    if ( info.verbose )
		printf ( "Center maplet x,ydim = %d, %d\n", mp->xdim, mp->ydim );
	}

	/* location of the center within the maplet */
	offx = info.fx * info.series->xdim;
	offy = info.fy * info.series->ydim;

	origx = vp_info.vxcent - offx;
	origy = vp_info.vycent - offy;

	if ( info.verbose )
	    printf ( "Maplet off, orig: %d %d -- %d %d\n", offx, offy, origx, origy );

	if ( info.center_only ) {
	    nx1 = nx2 = 0;
	    ny1 = ny2 = 0;
	} else {
	    nx1 = - (vxdim - (origx + info.series->xdim) + info.series->xdim - 1 ) / info.series->xdim;
	    nx2 = + (origx + info.series->xdim - 1 ) / info.series->xdim;
	    ny1 = - (vydim - (origy + info.series->ydim) + info.series->ydim - 1 ) / info.series->ydim;
	    ny2 = + (origy + info.series->ydim - 1 ) / info.series->ydim;
	}

	if ( info.verbose ) {
	    printf ( "redraw -- viewport: %d %d -- maplet %d %d -- offset: %d %d\n",
		vxdim, vydim, info.series->xdim, info.series->ydim, offx, offy );
	    printf ( "redraw range: x,y = %d %d %d %d\n", nx1, nx2, ny1, ny2 );
	}

	for ( y = ny1; y <= ny2; y++ ) {
	    for ( x = nx1; x <= nx2; x++ ) {
		if ( info.verbose > 3 )
		    printf ( "redraw, load maplet  %d %d\n", x, y );
		mp = load_maplet ( info.long_maplet + x, info.lat_maplet + y );
		if ( ! mp ) {
		    if ( info.verbose > 3 )
			printf ( "Nope\n");
		    continue;
		}
		if ( info.verbose > 3 )
		    printf ( "OK, draw at %d %d\n", origx + mp->xdim*x, origy + mp->ydim*y );
		draw_maplet ( mp,
			origx - mp->xdim * x,
			origy - mp->ydim * y );
	    }
	}

	/* mark center */
	gdk_draw_rectangle ( info.series->pixels, vp_info.da->style->black_gc, TRUE, vp_info.vxcent-1, vp_info.vycent-1, 3, 3 );
}

static int config_count = 0;

/* This gets called when the drawing area gets created or resized.
 * (and after every one of these you get an expose event).
 */
gint
configure_handler ( GtkWidget *wp, GdkEvent *event, gpointer data )
{
	int vxdim, vydim;
	int i;
	struct series *sp;

	/* get the viewport size */
	vp_info.vx = vxdim = wp->allocation.width;
	vp_info.vy = vydim = wp->allocation.height;
	vp_info.vxcent = vp_info.vx / 2;
	vp_info.vycent = vp_info.vy / 2;

	if ( info.verbose )
	    printf ( "Configure event %d (%d, %d)\n", config_count++, vxdim, vydim );

	for ( i=0; i<N_SERIES; i++ ) {
	    sp = &info.series_info[i];
	    /* Avoid memory leak */
	    if ( sp->pixels )
		gdk_pixmap_unref ( sp->pixels );
	    sp->pixels = NULL;
	    sp->content = 0;
	}

	info.series->pixels = gdk_pixmap_new ( wp->window, vxdim, vydim, -1 );
	pixmap_redraw ();

	return TRUE;
}

gint
mouse_handler ( GtkWidget *wp, GdkEventButton *event, gpointer data )
{
	int button;
	int vxcent, vycent;
	double dlat, dlong;
	double x_pixel_scale, y_pixel_scale;
	float x, y;
	int i;

	if ( info.verbose )
	    printf ( "Button event %d %.3f %.3f in (%d %d)\n",
		event->button, event->x, event->y, vp_info.vx, vp_info.vy );

	/* flip to new series, may be able to avoid redrawing the pixmap */
	if ( event->button != 1 ) {
	    toggle_series ();
	    if ( ! info.series->pixels )
		info.series->pixels = gdk_pixmap_new ( wp->window, vp_info.vx, vp_info.vy, -1 );
	    if ( ! info.series->content )
		pixmap_redraw ();
	    pixmap_expose ( 0, 0, vp_info.vx, vp_info.vy );
	    return TRUE;
	}

	/* viewport center */
	vxcent = vp_info.vx / 2;
	vycent = vp_info.vy / 2;

	if ( info.verbose )
	    printf ( "Orig position (lat/long) %.4f %.4f\n",
		info.lat_deg, info.long_deg );

	x_pixel_scale = info.series->maplet_long_deg / (double) info.series->xdim;
	y_pixel_scale = info.series->maplet_lat_deg / (double) info.series->ydim;

	dlat  = (event->y - (double)vycent) * y_pixel_scale;
	dlong = (event->x - (double)vxcent) * x_pixel_scale;

	if ( info.verbose )
	    printf ( "Delta position (lat/long) %.4f %.4f\n", dlat, dlong );

	/* Make location of the mouse click be the current position */
	set_position ( info.long_deg + dlong, info.lat_deg - dlat );

	for ( i=0; i<N_SERIES; i++ )
	    info.series_info[i].content = 0;

	/* redraw on the new center */
	pixmap_redraw ();

	/* put the new pixmap on the screen */
	pixmap_expose ( 0, 0, vp_info.vx, vp_info.vy );

	return TRUE;
}

gint
keyboard_handler ( GtkWidget *wp, GdkEventKey *event, gpointer data )
{
	if ( event->length > 0 )
		printf ("Keyboard event string: %s\n", event->string );

	printf ( "Keyboard event %d %s)\n",
		event->keyval, gdk_keyval_name(event->keyval) );

	return TRUE;
}

/* Focus events are a funky business, but there is no way to
 * get the keyboard involved without handling them.
 */
gint
focus_handler ( GtkWidget *wp, GdkEventFocus *event, gpointer data )
{
	printf ( "Focus event %d\n", event->in );
}

void
synch_position ( void )
{
    	double m_lat, m_long;

    	m_lat = info.lat_deg / info.series->maplet_lat_deg;
    	m_long = - info.long_deg / info.series->maplet_long_deg;

	/* indices of the maplet we are in
	 */
    	info.lat_maplet = m_lat;
    	info.long_maplet = m_long;

	/* fractional offset of our position in that maplet
	 */
	info.fy = 1.0 - (m_lat - info.lat_maplet);
	info.fx = 1.0 - (m_long - info.long_maplet);
}

void
set_position ( double long_deg, double lat_deg )
{
	info.long_deg = long_deg;
	info.lat_deg = lat_deg;

	if ( info.verbose )
	    printf ("Set position: long/lat = %.3f %.3f\n", long_deg, lat_deg );

	synch_position ();
}

void
usage ( void )
{
	printf ( "Usage: gtopo [-v -f/i <file>]\n" );
	exit ( 1 );
}

static int file_opt = 0;

int
main ( int argc, char **argv )
{
	GtkWidget *main_window;
	GtkWidget *vb;
	char *p;
	char *file_name;

	if ( ! temp_init() ) {
	    printf ("Sorry, I can't find a place to put temporary files\n");
	    return 1;
	}
	/* Let gtk strip off any of its arguments first
	 */
	gtk_init ( &argc, &argv );

	argc--;
	argv++;

	info.verbose = 0;
	info.center_only = 0;
	info.series_info = series_info_buf;

	while ( argc-- ) {
	    p = *argv++;
	    if ( strcmp ( p, "-v" ) == 0 )
	    	info.verbose = 999;
	    if ( strcmp ( p, "-c" ) == 0 )
	    	info.center_only = 1;
	    if ( strcmp ( p, "-f" ) == 0 ) {
		if ( argc < 1 )
		    usage ();
		argc--;
		file_name = *argv++;
		file_opt = 1;
		printf ( "Using file option on %s\n", file_name );
	    }
	    if ( strcmp ( p, "-i" ) == 0 ) {
		if ( argc < 1 )
		    usage ();
		argc--;
		file_name = *argv++;
		printf ( "File info on %s\n", file_name );
		file_info ( file_name );
		return 0;
	    }
	}

	if ( file_opt ) {
	    if ( ! file_init ( file_name ) ) {
		printf ( "No TOPO file: %s\n", file_name );
		return 1;
	    }
	} else {
	    if ( ! archive_init ( topo_archives ) ) {
		printf ( "No topo archives found\n" );
		return 1;
	    }
	}

	main_window = gtk_window_new ( GTK_WINDOW_TOPLEVEL );

	gtk_widget_show ( main_window );

	gtk_signal_connect ( GTK_OBJECT(main_window), "delete_event",
			GTK_SIGNAL_FUNC(destroy_handler), NULL );

	vb = gtk_vbox_new ( FALSE, 0 );
	gtk_widget_show ( vb );
	gtk_container_add ( GTK_CONTAINER(main_window), vb );

	vp_info.da = gtk_drawing_area_new ();
	gtk_box_pack_start ( GTK_BOX(vb), vp_info.da, TRUE, TRUE, 0 );

	gtk_signal_connect ( GTK_OBJECT(vp_info.da), "expose_event",
			GTK_SIGNAL_FUNC(expose_handler), NULL );
	gtk_signal_connect ( GTK_OBJECT(vp_info.da), "configure_event",
			GTK_SIGNAL_FUNC(configure_handler), NULL );

	/* We never see the release event, unless we add the press
	 * event to the mask.
	 */
	gtk_signal_connect ( GTK_OBJECT(vp_info.da), "button_release_event",
			GTK_SIGNAL_FUNC(mouse_handler), NULL );
	gtk_widget_add_events ( GTK_WIDGET(vp_info.da), GDK_BUTTON_RELEASE_MASK );
	gtk_widget_add_events ( GTK_WIDGET(vp_info.da), GDK_BUTTON_PRESS_MASK );

#ifdef notdef
	/* XXX - doesn't work yet */
	gtk_signal_connect ( GTK_OBJECT(vp_info.da), "focus_event",
			GTK_SIGNAL_FUNC(focus_handler), NULL );
	gtk_widget_add_events ( GTK_WIDGET(vp_info.da), GDK_FOCUS_CHANGE );

	/* XXX - doesn't work yet */
	gtk_signal_connect ( GTK_OBJECT(vp_info.da), "key_press_event",
			GTK_SIGNAL_FUNC(keyboard_handler), NULL );
	GTK_WIDGET_SET_FLAGS ( GTK_WIDGET(vp_info.da), GTK_CAN_FOCUS );
	gtk_widget_add_events ( GTK_WIDGET(vp_info.da), GDK_KEY_PRESS );
	/*
	gtk_widget_add_events ( GTK_WIDGET(vp_info.da), GDK_KEY_RELEASE );
	*/
#endif

	syscm = gdk_colormap_get_system ();

	if ( ! file_opt ) {

	    /* not strictly needed, but set_series will access
	     * these values.
	     */
	    info.long_deg = 0.0;
	    info.lat_deg = 0.0;

	    /*
	    set_series ( S_STATE );
	    set_series ( S_ATLAS );
	    set_series ( S_500K );
	    set_series ( S_100K );
	    set_series ( S_24K );
	    */

	    set_series ( S_ATLAS );

#ifdef notdef
	    /* Nevada */
	    set_position ( -114.9894, 36.2338 );

	    /* Mt. Hopkins, Arizona */
	    set_position ( -110.88, 31.69 );
#endif

	    /* In California west of Taboose Pass */
	    set_position ( -dms2deg ( 118, 31, 0 ), dms2deg ( 37, 1, 0 ) );

	}

#ifdef notdef
	/* XXX - Someday what we would like to do is resize ourself
	 * to INITIAL_VIEW (the idea being that if we start off
	 * at 800x800, we cannot be made smaller, which is bad.
	 */
	vp_info.vx = MINIMUM_VIEW;
	vp_info.vy = MINIMUM_VIEW;
#endif

	vp_info.vx = INITIAL_VIEW;
	vp_info.vy = INITIAL_VIEW;

	gtk_drawing_area_size ( GTK_DRAWING_AREA(vp_info.da), vp_info.vx, vp_info.vy );

	gtk_widget_show ( vp_info.da );

	gtk_main ();

	return 0;
}

/* THE END */
