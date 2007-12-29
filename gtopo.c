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

/* 640 x 800 works pretty well for printing onto
 * 8.5 x 11 inch paper.
 * Actually 640 x 828 has the same aspect ratio.
 */
#define INITIAL_VIEW_X	640
#define INITIAL_VIEW_Y	800

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
 * version 0.9.3 - 8/30/2007 - works on x86_64 and powerpc
 * version 0.9.4 - 9/1/2007 - eliminate moves to white screen
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
struct settings settings;

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
char *topo_archives[] = { "/u1/topo", "/u2/topo", "/mmt/topo", "/topo", NULL };
#endif

GdkColormap *syscm;

struct viewport {
	int vx;
	int vy;
	int vxcent;
	int vycent;
	double mo_x;
	double mo_y;
	int mo_time;
	GtkWidget *da;
} vp_info;

/* Prototypes ..........
 */
static void cursor_show ( int );

gint
destroy_handler ( GtkWidget *w, GdkEvent *event, gpointer data )
{
	gtk_main_quit ();
	if ( settings.verbose & V_BASIC )
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

	cursor_show ( 1 );
}

static int expose_count = 0;

gint
expose_handler ( GtkWidget *wp, GdkEventExpose *ep, gpointer data )
{
	if ( settings.verbose & V_WINDOW && expose_count < 4 )
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

	if ( settings.verbose & V_BASIC ) {
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
	    return;
	}

	/* A first guess, hopefuly to be corrected
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
	    if ( settings.verbose & V_DRAW )
		printf ( "Center maplet x,ydim = %d, %d\n", mx, my );
	}

	/* location of the center within the maplet */
	offx = info.fx * mx;
	offy = info.fy * my;

	origx = vp_info.vxcent - offx;
	origy = vp_info.vycent - offy;

	if ( settings.verbose & V_DRAW )
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

	if ( settings.verbose & V_DRAW ) {
	    printf ( "redraw -- viewport: %d %d -- maplet %d %d -- offset: %d %d\n",
		vxdim, vydim, mx, my, offx, offy );
	    printf ( "redraw range: x,y = %d %d %d %d\n", nx1, nx2, ny1, ny2 );
	}

	for ( y = ny1; y <= ny2; y++ ) {
	    for ( x = nx1; x <= nx2; x++ ) {

		mp = load_maplet ( info.long_maplet + x, info.lat_maplet + y );
		if ( ! mp ) {
		    if ( settings.verbose & V_DRAW2 )
			printf ( "redraw, no maplet at %d %d\n", x, y );
		    continue;
		}

		if ( settings.verbose & V_DRAW2 )
		    printf ( "redraw OK for %d %d, draw at %d %d\n",
			x, y, origx + mp->xdim*x, origy + mp->ydim*y );
		draw_maplet ( mp,
			origx - mp->xdim * x,
			origy - mp->ydim * y );
	    }
	}

	if ( settings.show_maplets ) {
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

	if ( settings.verbose & V_WINDOW )
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
 *
 * With a 640 wide by 800 tall viewport, we get a pretty good
 * fit to a single 8.5 x 11 sheet of paper.
 * The "s" key spits out gtopo.jpg
 * convert gtopo.jpg gtopo.ps ; lpr gtopo.ps
 * This works -right now- and ain't bad at all.
 *
 * We save as a .jpg file, maybe we could have the p key do
 * "print" and save a lossless format like png, or better yet
 * direct to postscript, and even run convert ...
 *
 * Whether to force precise 7.5 minute quad scaling?
 * This could be important if we were using these as base
 * sheets for geologic mapping say, 1:24,000 is 1 inch = 2000 feet.
 * We could apply a 4x factor and get 1 inch = 500 feet.
 * However for best print copy, we want to avoid pixel interpolation
 * and should just let the scale be whatever is needed for 1:1
 * pixel mapping.
 */
void
snap ( void )
{
	GdkPixbuf *pixbuf;

	if ( settings.verbose & V_WINDOW ) {
	    printf ( "Snapshot" );

	    if ( info.series->series != S_STATE ) {
		struct maplet *mp;
		mp = load_maplet ( info.long_maplet, info.lat_maplet );
		printf ( " from file: %s", mp->tpq->path );
		printf ( " quad: %s (%s)", mp->tpq->quad, mp->tpq->state );
	    }
	    printf ( "\n" );
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

void
show_pos ( void )
{
	printf ( "Current center position (lat/long) %.4f %.4f\n", info.lat_deg, info.long_deg );
}

void
move_xy ( int new_x, int new_y )
{
	int vxcent, vycent;
	double dlat, dlong;
	double x_pixel_scale, y_pixel_scale;
	int i;

	/* viewport center */
	vxcent = vp_info.vx / 2;
	vycent = vp_info.vy / 2;

	if ( settings.verbose & V_EVENT )
	    printf ( "Button: orig position (lat/long) %.4f %.4f\n",
		info.lat_deg, info.long_deg );

	x_pixel_scale = info.series->maplet_long_deg / (double) info.series->xdim;
	y_pixel_scale = info.series->maplet_lat_deg / (double) info.series->ydim;

	dlat  = (new_y - (double)vycent) * y_pixel_scale;
	dlong = (new_x - (double)vxcent) * x_pixel_scale;

	if ( settings.verbose & V_EVENT )
	    printf ( "Button: delta position (lat/long) %.4f %.4f\n", dlat, dlong );

	/* Make location of the mouse click be the current position */
	if ( ! try_position ( info.long_deg + dlong, info.lat_deg - dlat ) )
	    return;

	for ( i=0; i<N_SERIES; i++ )
	    info.series_info[i].content = 0;

	/* redraw on the new center */
	pixmap_redraw ();

	/* put the new pixmap on the screen */
	pixmap_expose ( 0, 0, vp_info.vx, vp_info.vy );
}

void
shift_xy ( double dx, double dy )
{
	double dlat, dlong;
	double x_pixel_scale, y_pixel_scale;
	int i;

	x_pixel_scale = info.series->maplet_long_deg / (double) info.series->xdim;
	y_pixel_scale = info.series->maplet_lat_deg / (double) info.series->ydim;

	dlat  = dy * y_pixel_scale;
	dlong = dx * x_pixel_scale;

	if ( settings.verbose & V_EVENT )
	    printf ( "Motion: delta position (lat/long) %.4f %.4f\n", dlat, dlong );

	/* Make location of the mouse click be the current position */
	if ( ! try_position ( info.long_deg - dlong, info.lat_deg + dlat ) )
	    return;

	for ( i=0; i<N_SERIES; i++ )
	    info.series_info[i].content = 0;

	/* redraw on the new center */
	pixmap_redraw ();

	/* put the new pixmap on the screen */
	pixmap_expose ( 0, 0, vp_info.vx, vp_info.vy );
}

void
move_map ( int dx, int dy )
{
	int xpos, ypos;

	/* viewport center */
	xpos = vp_info.vx / 2 + dx * vp_info.vx / 4;
	ypos = vp_info.vy / 2 + dy * vp_info.vy / 4;

	move_xy ( xpos, ypos );
}

/* flip to new series, may be able to avoid redrawing the pixmap
 * Called from the mouse handler, and from routines in archive.c
 * that are called by the keyboard_handler
 */
void
redraw_series ( void )
{
	if ( ! info.series->pixels )
	    info.series->pixels = gdk_pixmap_new ( vp_info.da->window, vp_info.vx, vp_info.vy, -1 );
	if ( ! info.series->content )
	    pixmap_redraw ();
	pixmap_expose ( 0, 0, vp_info.vx, vp_info.vy );
}

#define KV_PAGE_UP	65365
#define KV_PAGE_DOWN	65366

#define KV_LEFT		65361
#define KV_UP		65362
#define KV_RIGHT	65363
#define KV_DOWN		65364

#define KV_CTRL		65507

/* We don't use these yet, but ... */
#define KV_ESC		65307
#define KV_TAB		65289
#define KV_A		97
#define KV_A_UC		65
#define KV_S		115
#define KV_S_UC		83

/* Used to modify mouse actions */
int ctrl_key_pressed = 0;

gint
keyboard_handler ( GtkWidget *wp, GdkEventKey *event, gpointer data )
{
	if ( settings.verbose & V_EVENT ) {
	    printf ( "Keyboard event %d %s",
		event->keyval, gdk_keyval_name(event->keyval) );

	    if ( event->type == GDK_KEY_PRESS )
	    	printf ( " pressed" );
	    else if ( event->type == GDK_KEY_RELEASE )
	    	printf ( " released" );
	    else
	    	printf ( " event-type-%d", event->type );

	    if ( event->length > 0 )
		printf ( " string: %s", event->string );
	    printf ( "\n" );
	}

	if ( event->keyval == KV_CTRL ) {
	    if ( event->type == GDK_KEY_PRESS )
	    	ctrl_key_pressed = 1;
	    if ( event->type == GDK_KEY_RELEASE )
	    	ctrl_key_pressed = 0;
	    return TRUE;
	}

	if ( event->type != GDK_KEY_PRESS )
	    return TRUE;
	
	if ( event->keyval == KV_PAGE_UP )
	    up_series ();
	else if ( event->keyval == KV_PAGE_DOWN )
	    down_series ();
	else if ( event->keyval == KV_S || event->keyval == KV_S_UC )
	    snap ();
	else if ( event->keyval == KV_LEFT )
	    move_map ( -1, 0 );
	else if ( event->keyval == KV_UP )
	    move_map ( 0, -1 );
	else if ( event->keyval == KV_RIGHT )
	    move_map ( 1, 0 );
	else if ( event->keyval == KV_DOWN )
	    move_map ( 0, 1 );

	return TRUE;
}

static int cursor_mode = 0;

static void
cursor_show ( int clean )
{
	if ( ! settings.center_marker )
	    return;

	if ( clean )
	    cursor_mode = 1;
	else
	    cursor_mode = 1 - cursor_mode;

	if ( cursor_mode ) {
	    gdk_draw_line ( vp_info.da->window,
		vp_info.da->style->white_gc,
		vp_info.vxcent, vp_info.vycent-2,
		vp_info.vxcent, vp_info.vycent+2 );
	    gdk_draw_line ( vp_info.da->window,
		vp_info.da->style->white_gc,
		vp_info.vxcent-2, vp_info.vycent,
		vp_info.vxcent+2, vp_info.vycent );
	    gdk_draw_line ( vp_info.da->window,
		vp_info.da->style->black_gc,
		vp_info.vxcent, vp_info.vycent-1,
		vp_info.vxcent, vp_info.vycent+1 );
	    gdk_draw_line ( vp_info.da->window,
		vp_info.da->style->black_gc,
		vp_info.vxcent-1, vp_info.vycent,
		vp_info.vxcent+1, vp_info.vycent );
	} else {
	    gdk_draw_line ( vp_info.da->window,
		vp_info.da->style->black_gc,
		vp_info.vxcent, vp_info.vycent-2,
		vp_info.vxcent, vp_info.vycent+2 );
	    gdk_draw_line ( vp_info.da->window,
		vp_info.da->style->black_gc,
		vp_info.vxcent-2, vp_info.vycent,
		vp_info.vxcent+2, vp_info.vycent );
	}
}

/* in milliseconds */
#define TICK_DELAY	500

gint
tick_handler ( gpointer data )
{
	cursor_show ( 0 );

	return TRUE;
}

gint
mouse_handler ( GtkWidget *wp, GdkEventButton *event, gpointer data )
{
	int button;

	if ( settings.verbose & V_EVENT )
	    printf ( "Button event %d %.3f %.3f in (%d %d)\n",
		event->button, event->x, event->y, vp_info.vx, vp_info.vy );

	if ( event->button == 3 ) {
	    if ( ctrl_key_pressed )
	    	up_series ();
	    else
	    	down_series ();
	    return TRUE;
	}

	if ( event->button == 2 ) {
	    show_pos ();
	    return TRUE;
	}

	if ( settings.m1_action == M1_CENTER )
	    move_xy ( event->x, event->y );

	return TRUE;
}

/* The hint business is either poorly documented, poorly explained, or
 * poorly understood.  Or all the above.  The idea is that events get
 * lumped together and we get the first event when the mouse first enters
 * the window (more or less), and subsequent events only when and if we
 * say we are ready for them by calling gdk_window_get_pointer.
 */
gint
motion_handler ( GtkWidget *wp, GdkEventMotion *event, gpointer data )
{
	int x, y;
	GdkModifierType state;
	int dt;
	double dx, dy;

	if ( settings.verbose & V_EVENT )
	    printf ( "Motion event  %u %8x %.3f %.3f in (%d %d)\n",
		event->time, event->state, event->x, event->y, vp_info.vx, vp_info.vy );

	/*
	move_xy ( event->x, event->y );
	*/

	if ( event->state & GDK_BUTTON1_MASK ) {
	    dt = event->time - vp_info.mo_time;
	    dx = event->x - vp_info.mo_x;
	    dy = event->y - vp_info.mo_y;

	    /*
	    printf ( "dtxy = %d %.3f %.3f\n", dt, dx, dy );
	    */
	    if ( settings.m1_action == M1_GRAB && dt < 200 )
		shift_xy ( dx, dy );
	}

	vp_info.mo_x = event->x;
	vp_info.mo_y = event->y;
	vp_info.mo_time = event->time;

	/* It seems silly to call gdk_window_get_pointer since we get all
	 * the same info anyway in the event structure, BUT this is not
	 * quite true, notice the check on is_hint, This lets GDK know we
	 * are done processing this event and are ready for another one,
	 * and indeed without this call, we maybe get one event per second.
	 * Note that this function returns integer (x,y), whereas the event
	 * structure returns floating point (x,y).
	 */
	if ( event->is_hint ) {
	    gdk_window_get_pointer ( event->window, &x, &y, &state );

	    /*
	    event->x = x;
	    event->y = y;
	    */
	    /*
	    if ( x != event->x || y != event->y )
		printf ( "Motion event2 %u %8x %d %d W(%x)\n",
		    event->time, event->state, x, y, event->window );
	    */
	}


	return TRUE;
}

/* Focus events are a funky business, but there is no way to
 * get the keyboard involved without handling them.
 */
gint
focus_handler ( GtkWidget *wp, GdkEventFocus *event, gpointer data )
{
	/* We get 1 when we go in, 0 when we go out */
	if ( settings.verbose & V_EVENT )
	    printf ( "Focus event %d\n", event->in );
}

void
synch_position ( void )
{
    	double m_lat, m_long;

	/*
	int maplets;
	maplets = tp->e_long / tp->maplet_long_deg;
	tp->lat_offset = tp->e_long - maplets * tp->maplet_long_deg;
	maplets = tp->s_lat / tp->maplet_lat_deg;
	tp->long_offset = tp->s_lat - maplets * tp->maplet_lat_deg;
	*/

    	m_lat = (info.lat_deg - info.series->lat_offset) / info.series->maplet_lat_deg;
    	m_long = - (info.long_deg - info.series->long_offset) / info.series->maplet_long_deg;

	/* indices of the maplet we are in
	 */
    	info.long_maplet = m_long;
    	info.lat_maplet = m_lat;

	if ( settings.verbose & V_BASIC ) {
	    printf ( "Synch position: long/lat = %.3f %.3f\n", info.long_deg, info.lat_deg );
	    printf ( "maplet indices of position: %d %d\n",
		info.long_maplet, info.lat_maplet );
	}

	/* fractional offset of our position in that maplet
	 */
	info.fy = 1.0 - (m_lat - info.lat_maplet);
	info.fx = 1.0 - (m_long - info.long_maplet);
}

/* Used by mouse routine to check that a possible new
 * position still has map coverage.
 */
int
try_position ( double new_long, double new_lat )
{
    	double orig_long, orig_lat;

	orig_long = info.long_deg;
	orig_lat = info.lat_deg;

	info.long_deg = new_long;
	info.lat_deg = new_lat;
	synch_position ();

	if ( load_maplet ( info.long_maplet, info.lat_maplet ) )
	    return 1;

	info.long_deg = orig_long;
	info.lat_deg = orig_lat;
	synch_position ();

	return 0;
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
	GtkWidget *mw;
	GtkWidget *vb;
	GtkWidget *da;
	GtkWidget *eb;
	char *p;
	char *file_name;

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

	settings_init ();
	places_init ();

	info.series_info = series_info_buf;
	info.center_only = 0;

	while ( argc-- ) {
	    p = *argv++;
	    if ( strcmp ( p, "-V" ) == 0 )
	    	settings.verbose = 0xffff;
	    if ( strcmp ( p, "-v" ) == 0 ) {
	    	printf ( "gtopo version 1.9.x\n" );
		return 0;
	    }
	    if ( strcmp ( p, "-c" ) == 0 )
	    	info.center_only = 1;
	    if ( strcmp ( p, "-d" ) == 0 )
	    	settings.center_marker = 0;
	    if ( strcmp ( p, "-m" ) == 0 )
	    	settings.show_maplets = 1;
	    if ( strcmp ( p, "-h" ) == 0 ) {
	    	http_test ();
		return 0;
	    }
	    if ( strcmp ( p, "-t" ) == 0 ) {
	    	terra_test ();
		return 0;
	    }
	    if ( strcmp ( p, "-x" ) == 0 ) {
	    	xml_test ();
		return 0;
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

	if ( ! file_opt ) {

	    /* not strictly needed, but set_series will access
	     * these values.
	     */
	    info.long_deg = 0.0;
	    info.lat_deg = 0.0;

	    set_series ( settings.starting_series );
	    set_position ( settings.starting_long, settings.starting_lat );
	}

	/* --- set up the GTK stuff we need */

	/* ### First - the main window */
	mw = gtk_window_new ( GTK_WINDOW_TOPLEVEL );

	g_signal_connect ( mw, "delete_event",
			G_CALLBACK(destroy_handler), NULL );

	/* ### Second - a drawing area */
	vp_info.da = da = gtk_drawing_area_new ();

	/* Hook up the expose and configure signals, we could also
	 * connect to the "realize" signal, but I haven't found a need
	 * for that yet
	 */
	g_signal_connect ( da, "expose_event",
			G_CALLBACK(expose_handler), NULL );
	g_signal_connect ( da, "configure_event",
			G_CALLBACK(configure_handler), NULL );

	/* We never see the release event, unless we add the press
	 * event to the mask.
	 */
	g_signal_connect ( da, "button_release_event",
			G_CALLBACK(mouse_handler), NULL );
	gtk_widget_add_events ( GTK_WIDGET(da), GDK_BUTTON_RELEASE_MASK );
	gtk_widget_add_events ( GTK_WIDGET(da), GDK_BUTTON_PRESS_MASK );

	/* Now get this CPU intensive mouse motion stuff */
	g_signal_connect ( da, "motion_notify_event",
			G_CALLBACK(motion_handler), NULL );
	gtk_widget_add_events ( GTK_WIDGET(da), GDK_POINTER_MOTION_MASK );
	gtk_widget_add_events ( GTK_WIDGET(da), GDK_POINTER_MOTION_HINT_MASK );

	/* Now, try to work the magic to get keyboard events */
	GTK_WIDGET_SET_FLAGS ( da, GTK_CAN_FOCUS );
	gtk_widget_add_events ( GTK_WIDGET(da), GDK_FOCUS_CHANGE_MASK );
	g_signal_connect ( da, "focus_in_event", G_CALLBACK(focus_handler), NULL );
	g_signal_connect ( da, "focus_out_event", G_CALLBACK(focus_handler), NULL );

	gtk_widget_add_events ( GTK_WIDGET(da), GDK_KEY_PRESS_MASK );
	g_signal_connect ( da, "key_press_event", G_CALLBACK(keyboard_handler), NULL );
	g_signal_connect ( da, "key_release_event", G_CALLBACK(keyboard_handler), NULL );

	g_timeout_add ( TICK_DELAY, tick_handler, NULL );

#ifdef notyet
	/* I don't know where this come from,
	 * but it seems gone in gtk 2.0
	 */
	gtk_grab_focus ( focal );
#endif

	/* ### Third - a vbox to hold the drawing area */
	vb = gtk_vbox_new ( FALSE, 0 );
	gtk_box_pack_start ( GTK_BOX(vb), da, TRUE, TRUE, 0 );
	gtk_container_add ( GTK_CONTAINER(mw), vb );

	/* In case we ever need this */
	syscm = gdk_colormap_get_system ();

#ifdef notdef
	/* XXX - Someday what we would like to do is make a way so
	 * that gtopo comes up 800x800 but can be resized smaller
	 * by the user.
	 */
#endif

	gtk_drawing_area_size ( GTK_DRAWING_AREA(da), settings.x_view, settings.y_view );

	/*
	#define MINIMUM_VIEW	100
	gtk_drawing_area_size ( GTK_DRAWING_AREA(da), MINIMUM_VIEW, MINIMUM_VIEW );
	gtk_widget_set_usize ( GTK_WIDGET(da), settings.x_view, settings.y_view );
	gdk_window_resize ( da->window, settings.x_view, settings.y_view );
	*/

	gtk_widget_show_all ( mw );

	vp_info.vx = settings.x_view;
	vp_info.vy = settings.y_view;

	vp_info.mo_x = 0;
	vp_info.mo_y = 0;
	vp_info.mo_time = -10000;

	gtk_main ();

	return 0;
}

/* THE END */
