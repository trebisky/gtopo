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
#include "protos.h"

#define MINIMUM_VIEW	100
#define INITIAL_VIEW	800

/* I have tried to group some "commonly fiddled"
 * parameters here for your convenience.  Someday
 * this will be handled by a .gtopo settings file
 */

/*
 Possible series are the 5 following:

    S_STATE;
    S_ATLAS;
    S_500K;
    S_100K;
    S_24K;
*/

#define INITIAL_SERIES	S_STATE

/*
#define INITIAL_ARCHIVE	"/u1/backroads"
*/

#ifdef notdef
/* Mt. Hopkins, Arizona */
#define INITIAL_LONG	-110.88
#define INITIAL_LAT	31.69

/* In California west of Taboose Pass */
#define INITIAL_LONG	-dms2deg ( 118, 31, 0 )
#define INITIAL_LAT	dms2deg ( 37, 1, 0 )

/* Near Alturas, NE California */
#define INITIAL_LONG	-120.5
#define INITIAL_LAT	41.5

/* Near Las Vegas, Nevada */
#define INITIAL_LONG	-114.9894
#define INITIAL_LAT	36.2338
#endif

/* In California west of Taboose Pass */
#define INITIAL_LONG	-dms2deg ( 118, 31, 0 )
#define INITIAL_LAT	dms2deg ( 37, 1, 0 )

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
 * version 0.9.1 - 8/5/2007 - make series 1 work for AZ, CA
 * version 0.9.2 - 8/8/2007 - discover GdkPixbufLoader
 *
 *  TODO
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

#ifdef INITIAL_ARCHIVE
char *topo_archives[] = { INITIAL_ARCHIVE, NULL };
#else
char *topo_archives[] = { "/u1/topo", "/u2/topo", "/mmt/topo", NULL };
#endif

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

gint
destroy_handler ( GtkWidget *w, GdkEvent *event, gpointer data )
{
	gtk_main_quit ();
	show_statistics ();

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
	if ( info.verbose & V_WINDOW && expose_count < 4 )
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
	double dpp_ew, dpp_ns;
	double vpe, vpw, vps, vpn;

	tp = mp->tpq;

	if ( info.verbose & V_BASIC ) {
	    printf ( "State handler %s\n", mp->tpq->path );
	    printf ( "Position, long, lat: %.4f %.4f\n", info.long_deg, info.lat_deg );
	    printf ( "Sheet, S, N: %.4f %.4f\n", tp->s_lat, tp->n_lat );
	    printf ( "Sheet, W, E: %.4f %.4f\n", tp->w_long, tp->e_long );
	}

	/* degrees per pixel */
	dpp_ew = (tp->e_long - tp->w_long) / mp->xdim;
	dpp_ns = (tp->n_lat - tp->s_lat) / mp->ydim;

	/* viewport limits in degrees */
	vpw = info.long_deg - vp_info.vxcent * dpp_ew;
	vpe = info.long_deg + vp_info.vxcent * dpp_ew;
	vps = info.lat_deg - vp_info.vycent * dpp_ns;
	vpn = info.lat_deg + vp_info.vycent * dpp_ns;

	/* Test if this map is not in our viewport */ 
	if ( tp->e_long < vpw )
	    return;
	if ( tp->w_long > vpe )
	    return;
	if ( tp->n_lat < vps )
	    return;
	if ( tp->s_lat > vpn )
	    return;

	fx = (info.long_deg - tp->w_long ) / (tp->e_long - tp->w_long );
	fy = 1.0 - (info.lat_deg - tp->s_lat ) / (tp->n_lat - tp->s_lat );

	/* location of the center within the maplet */
	offx = fx * mp->xdim;
	offy = fy * mp->ydim;

	origx = vp_info.vxcent - offx;
	origy = vp_info.vycent - offy;

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
	int xx, yy;
	int mx, my;

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
	if ( ! info.have_usa && info.series->series == S_STATE ) {
	    state_maplets ( state_handler );
	    /* mark center */
	    if ( info.center_dot )
		gdk_draw_rectangle ( info.series->pixels, vp_info.da->style->black_gc, TRUE,
			vp_info.vxcent-1, vp_info.vycent-1, 3, 3 );
	    return;
	}

	/* A first guess, hopefull to be corrected
	 * as soon as we actually read a maplet
	 */
	mx = info.series->xdim;
	my = info.series->ydim;

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
	    info.series->xdim = mx = mp->xdim;
	    info.series->ydim = my = mp->ydim;
	    if ( info.verbose & V_DRAW )
		printf ( "Center maplet x,ydim = %d, %d\n", mx, my );
	}

	/* location of the center within the maplet */
	offx = info.fx * mx;
	offy = info.fy * my;

	origx = vp_info.vxcent - offx;
	origy = vp_info.vycent - offy;

	if ( info.verbose & V_DRAW )
	    printf ( "Maplet off, orig: %d %d -- %d %d\n", offx, offy, origx, origy );

	if ( info.center_only ) {
	    nx1 = nx2 = 0;
	    ny1 = ny2 = 0;
	} else {
	    nx1 = - (vxdim - (origx + mx) + mx - 1 ) / mx;
	    nx2 = + (origx + mx - 1 ) / mx;
	    ny1 = - (vydim - (origy + my) + my - 1 ) / my;
	    ny2 = + (origy + my - 1 ) / my;
	}

	if ( info.verbose & V_DRAW ) {
	    printf ( "redraw -- viewport: %d %d -- maplet %d %d -- offset: %d %d\n",
		vxdim, vydim, mx, my, offx, offy );
	    printf ( "redraw range: x,y = %d %d %d %d\n", nx1, nx2, ny1, ny2 );
	}

	for ( y = ny1; y <= ny2; y++ ) {
	    for ( x = nx1; x <= nx2; x++ ) {

		mp = load_maplet ( info.long_maplet + x, info.lat_maplet + y );
		if ( ! mp ) {
		    if ( info.verbose & V_DRAW2 )
			printf ( "redraw, no maplet at %d %d\n", x, y );
		    continue;
		}

		if ( info.verbose & V_DRAW2 )
		    printf ( "redraw OK for %d %d, draw at %d %d\n",
			x, y, origx + mp->xdim*x, origy + mp->ydim*y );
		draw_maplet ( mp,
			origx - mp->xdim * x,
			origy - mp->ydim * y );
	    }
	}

	if ( info.show_maplets ) {
	    for ( x = nx1+1; x <= nx2; x++ ) {
		xx = origx - mx * x,
		gdk_draw_line ( info.series->pixels, vp_info.da->style->black_gc,
		    xx, 0, xx, vp_info.vy );
	    }
	    for ( y = ny1+1; y <= ny2; y++ ) {
		yy = origy - my * y,
		gdk_draw_line ( info.series->pixels, vp_info.da->style->black_gc,
		    0, yy, vp_info.vx, yy );
	    }
	}


	/* mark center */
	if ( info.center_dot )
	    gdk_draw_rectangle ( info.series->pixels, vp_info.da->style->black_gc, TRUE,
		vp_info.vxcent-1, vp_info.vycent-1, 3, 3 );
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

	if ( info.verbose & V_WINDOW )
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

/* Take a snapshot of current pixmap.
 * A pixmap is a server side resource (which makes switching
 * them around and handling expose events very fast).
 * but to fiddle with pixel values, we need to copy them
 * to a client side Image or Pixbuf.
 */
void
snap ( void )
{
	GdkPixbuf *pixbuf;

	printf ( "Snapshot\n" );

	if ( info.series->series != S_STATE ) {
	    struct maplet *mp;
	    mp = load_maplet ( info.long_maplet, info.lat_maplet );
	    printf ( " from file: %s\n", mp->tpq->path );
	    printf ( " quad: %s (%s)\n", mp->tpq->quad, mp->tpq->state );
	}

	pixbuf = gdk_pixbuf_get_from_drawable(NULL, info.series->pixels, NULL,
		0, 0, 0, 0, vp_info.vx, vp_info.vy );

#ifdef notdef
	int n_channels;
	int width, height;
	int rowstride;
	guchar *pixels;

	/* Typically 3 channels, and on my machine, full screen is 1280x999
	 * If there was an alpha channel, there would be 4 channels.
	 * with an 800x800 display and 3 channels, rowstride is 3*800
	 * (i.e. rowstride is n_channels * width).
	 * a pixel address is: pixels + y*rowstride + x *n_channels;
	 */
	n_channels = gdk_pixbuf_get_n_channels (pixbuf);
	width = gdk_pixbuf_get_width (pixbuf);
        height = gdk_pixbuf_get_height (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);

	printf ( "pixbuf channels: %d\n", n_channels );
	printf ( "pixbuf width: %d\n", width );
	printf ( "pixbuf height: %d\n", height );
	printf ( "pixbuf stride: %d\n", rowstride );

	pixels = gdk_pixbuf_get_pixels ( pixbuf );
#endif

#ifdef notdef
	/* quality 0 is least compressed (my test gets 1.9 M) */
	gdk_pixbuf_save ( pixbuf, "gtopo0.png", "png", NULL, "compression", "0", NULL );
	/* quality 9 is most compressed (my test gets 1.9 M) */
	gdk_pixbuf_save ( pixbuf, "gtopo9.png", "png", NULL, "compression", "9", NULL );

	/* quality 100 - (my test gives 0.8 M) */
	gdk_pixbuf_save ( pixbuf, "gtopo.jpg", "jpeg", NULL, "quality", "100", NULL );

	/* quality 50 - (my test gives 0.152 M) - and looks fine */
	gdk_pixbuf_save ( pixbuf, "gtopo.jpg", "jpeg", NULL, "quality", "50", NULL );

	/* quality 10 - (my test gives 0.058 M) - readable, but bad artifacts*/
	gdk_pixbuf_save ( pixbuf, "gtopo.jpg", "jpeg", NULL, "quality", "10", NULL );

	/* quality 25 - (my test gives 0.101 M) - some artifacts*/
	gdk_pixbuf_save ( pixbuf, "gtopo.jpg", "jpeg", NULL, "quality", "25", NULL );

#endif
	gdk_pixbuf_save ( pixbuf, "gtopo.jpg", "jpeg", NULL, "quality", "50", NULL );
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

	if ( info.verbose & V_EVENT )
	    printf ( "Button event %d %.3f %.3f in (%d %d)\n",
		event->button, event->x, event->y, vp_info.vx, vp_info.vy );

	/* flip to new series, may be able to avoid redrawing the pixmap */
	if ( event->button == 3 ) {
	    toggle_series ();
	    if ( ! info.series->pixels )
		info.series->pixels = gdk_pixmap_new ( wp->window, vp_info.vx, vp_info.vy, -1 );
	    if ( ! info.series->content )
		pixmap_redraw ();
	    pixmap_expose ( 0, 0, vp_info.vx, vp_info.vy );
	    return TRUE;
	}
	if ( event->button == 2 ) {
	    snap ();
	    return TRUE;
	}

	/* viewport center */
	vxcent = vp_info.vx / 2;
	vycent = vp_info.vy / 2;

	if ( info.verbose & V_EVENT )
	    printf ( "Button: orig position (lat/long) %.4f %.4f\n",
		info.lat_deg, info.long_deg );

	x_pixel_scale = info.series->maplet_long_deg / (double) info.series->xdim;
	y_pixel_scale = info.series->maplet_lat_deg / (double) info.series->ydim;

	dlat  = (event->y - (double)vycent) * y_pixel_scale;
	dlong = (event->x - (double)vxcent) * x_pixel_scale;

	if ( info.verbose & V_EVENT )
	    printf ( "Button: delta position (lat/long) %.4f %.4f\n", dlat, dlong );

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
	if ( info.verbose & V_EVENT )
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
    	info.long_maplet = m_long;
    	info.lat_maplet = m_lat;

	if ( info.verbose & V_BASIC ) {
	    printf ( "Synch position: long/lat = %.3f %.3f\n", info.long_deg, info.lat_deg );
	    printf ( "maplet indices of position: %d %d\n",
		info.long_maplet, info.lat_maplet );
	}

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
	GtkWidget *focal;
	int view;
	int start_series;

#ifdef notdef
	if ( ! temp_init() ) {
	    printf ("Sorry, I can't find a place to put temporary files\n");
	    return 1;
	}
#endif

	/* Let gtk strip off any of its arguments first
	 */
	gtk_init ( &argc, &argv );

	argc--;
	argv++;

	info.verbose = INITIAL_VERBOSITY;

	info.center_only = 0;
	info.center_dot = 1;
	info.series_info = series_info_buf;
	info.show_maplets = 0;

	view = INITIAL_VIEW;

	start_series = INITIAL_SERIES;

	while ( argc-- ) {
	    p = *argv++;
	    if ( strcmp ( p, "-v" ) == 0 )
	    	info.verbose = 0xffff;
	    if ( strcmp ( p, "-c" ) == 0 )
	    	info.center_only = 1;
	    if ( strcmp ( p, "-d" ) == 0 )
	    	info.center_dot = 0;
	    if ( strcmp ( p, "-m" ) == 0 )
	    	info.show_maplets = 1;
	    if ( strcmp ( p, "-s" ) == 0 ) {
		if ( argc < 1 )
		    usage ();
		argc--;
		/* XXX - accept a number 1-5, pretty gross */
		start_series = atol ( *argv++ ) - 1;
	    }
	    if ( strcmp ( p, "-g" ) == 0 ) {
		/* won't the standard X geometry options work here?
		 * (if I cooperate and do not
		 * brute force resize and override ...
		 */
		if ( argc < 1 )
		    usage ();
		argc--;
		view = atol ( *argv++ );
	    }
	    if ( strcmp ( p, "-f" ) == 0 ) {
		/* show a single tpq file */
		if ( argc < 1 )
		    usage ();
		argc--;
		file_name = *argv++;
		file_opt = 1;
	    }
	    if ( strcmp ( p, "-i" ) == 0 ) {
		/* show file information */
		if ( argc < 1 )
		    usage ();
		file_info ( *argv, 0 );
		return 0;
	    }
	    if ( strcmp ( p, "-j" ) == 0 ) {
		/* show file information, include index to maplets */
		if ( argc < 1 )
		    usage ();
		file_info ( *argv, 1 );
		return 0;
	    }
	}

	if ( file_opt ) {
	    if ( ! file_init ( file_name ) ) {
		printf ( "No TOPO file: %s\n", file_name );
		return 1;
	    }
	    printf ( "Displaying single file: %s\n", file_name );
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

	/* Hook up the expose and configure signals, we could also
	 * connect to the "realize" signal, but I haven't found a need
	 * for that yet
	 */
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

	focal = vp_info.da;
	/*
	focal = vp_info.da->window;
	*/
#ifdef notyet
	/* XXX - doesn't work yet */
	gtk_signal_connect ( GTK_OBJECT(focal), "focus_event",
			GTK_SIGNAL_FUNC(focus_handler), NULL );
	gtk_widget_add_events ( GTK_WIDGET(focal), GDK_FOCUS_CHANGE );

	/* XXX - doesn't work yet */
	gtk_signal_connect ( GTK_OBJECT(focal), "key_press_event",
			GTK_SIGNAL_FUNC(keyboard_handler), NULL );
	GTK_WIDGET_SET_FLAGS ( GTK_WIDGET(focal), GTK_CAN_FOCUS );
	gtk_widget_add_events ( GTK_WIDGET(focal), GDK_KEY_PRESS );
	/*
	gtk_widget_add_events ( GTK_WIDGET(focal), GDK_KEY_RELEASE );
	*/
	gtk_grab_focus ( focal );
#endif

	syscm = gdk_colormap_get_system ();

	if ( ! file_opt ) {

	    /* not strictly needed, but set_series will access
	     * these values.
	     */
	    info.long_deg = 0.0;
	    info.lat_deg = 0.0;

	    set_series ( start_series );

	    set_position ( INITIAL_LONG, INITIAL_LAT );
	}

#ifdef notdef
	/* XXX - Someday what we would like to do is make a way so
	 * that gtopo comes up 800x800 but can be resized smaller
	 * by the user.
	 */
#endif

	gtk_drawing_area_size ( GTK_DRAWING_AREA(vp_info.da), view, view );

	/*
	gtk_drawing_area_size ( GTK_DRAWING_AREA(vp_info.da), MINIMUM_VIEW, MINIMUM_VIEW );
	gtk_widget_set_usize ( GTK_WIDGET(vp_info.da), view, view );
	gdk_window_resize ( vp_info.da->window, view, view );
	*/

	vp_info.vx = view;
	vp_info.vy = view;

	gtk_widget_show ( vp_info.da );

	gtk_main ();

	return 0;
}

/* THE END */
