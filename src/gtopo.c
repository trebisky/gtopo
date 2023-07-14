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
#include "xml.h"

/* The land fell away in a vast sweep like a great, empty sea
 * where no billows rolled, nor even waves.  Stiff grass stood in
 * the wind, scarcely bending, and the cedar played low, humming
 * songs with the wind.
 *   From Louis L'Amour "The Proving Trail", end of chapter 7
 */

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
 Possible series in the lower 48 are the 5 following:

    S_STATE;
    S_ATLAS;
    S_500K;
    S_100K;
    S_24K;

 Possible series in Alaska are the 5 following:

    S_STATE;
    S_ATLAS;
    S_250K;
    S_63K;
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
 * version 0.9.15 - 5/10/2011
 *	fix info window update bug
 *	fix segfault on close bug
 * version 0.9.16 - 6/15/2011
 *	begin adding new series to support Alaska (AK) set
 * version 1.0.1  - 8/24/2015
 *	add code to read and display GPX file tracks and waypoints
 * version 1.1.0  - 6/29/2020
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
struct places_info p_info;

/* This is a list of "root directories" where images of the
 * CDROMS may be found.  It is used as a kind of search path,
 * if directories do not exist they are ignored.
 * If they do exist, they are searched for subdirectories like
 * CA_D06 and az_d02, and so forth.
 * This allows a path to be set up that will work on multiple
 * machines that keep the topos in different places.
 */

GdkColormap *syscm;

struct mouse mouse_info;
struct viewport vp_info;
struct info_info i_info = { GONE };

/* Prototypes ..........
 */
static void cursor_show ( int );
static int try_position ( double, double );

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

	overlay_redraw ();
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

/* When dealing with a "state", we have typically one giant maplet
 * that comprises the entire TPQ file.  This is a callback handler
 * that will get called one by one for all maplets in a state or atlas
 * type object.  The fact that this is called once via an iterator for
 * "state" objects is indeed silly, but the iterator scheme is ideal for
 * ATLAS type TPQ files with multiple maplets.
 *
 * In particular though, note that the only variables referenced from the
 * info structure are long_deg and lat_deg.
 * This means that the maplets passed to this routine are
 *  "self identifying", which is ideal for STATE and ATLAS series
 *  which have different maplet sizes and pixel scales for each
 *  disjoint chunk (HI, AK, US) which comprises them.
 * Nothing at all is used from the series structure.
 *
 * Currently this assumes it gets one big maplet that is the
 * entire TPQ file.
 */

#ifdef notdef
/* This routine works just swell,
 * but is obsolete now that method_file works properly.
 * Anyway, with this, there was no way to navigate using the mouse.
 * DELETE ALL THIS SOMEDAY, NO LONGER USED.
 */
void
state_handler ( struct maplet *mp )
{
	struct tpq_info *tp;
	double fx, fy;
	int offx, offy;
	int origx, origy;
	double dpp_ew, dpp_ns;
	double vpe, vpw, vps, vpn;
	double me, mw, ms, mn;
	double dx, dy;

	tp = mp->tpq;

	/* maplet sizes */
	dx = (tp->e_long - tp->w_long) / tp->long_count;
	dy = (tp->n_lat - tp->s_lat) / tp->lat_count;

	/* degrees per pixel */
	dpp_ew = dx / mp->xdim;
	dpp_ns = dy / mp->ydim;

	/* viewport limits in degrees */
	vpw = info.long_deg - vp_info.vxcent * dpp_ew;
	vpe = info.long_deg + vp_info.vxcent * dpp_ew;
	vps = info.lat_deg - vp_info.vycent * dpp_ns;
	vpn = info.lat_deg + vp_info.vycent * dpp_ns;

	/* maplet limits in degrees */
	mw = tp->w_long + mp->world_x * dx;
	me = mw + dx;
	ms = tp->s_lat + mp->world_y * dy;
	mn = ms + dy;

	if ( settings.verbose & V_BASIC ) {
	    printf ( "State handler %s\n", mp->tpq->path );
	    printf ( " Center position, long, lat: %.4f %.4f\n", info.long_deg, info.lat_deg );
	    printf ( " World x, y: %d %d\n", mp->world_x, mp->world_y );
	    printf ( " Maplet, S, N: %.4f %.4f  vp: %.4f %.4f\n", ms, mn, vps, vpn );
	    printf ( " Maplet, W, E: %.4f %.4f  vp: %.4f %.4f\n", mw, me, vpw, vpe );
	}

	/* Bail out if this map is not in our viewport */ 
	if ( me < vpw )
	    return;
	if ( mw > vpe )
	    return;
	if ( mn < vps )
	    return;
	if ( ms > vpn )
	    return;

	/* Calculate the offset within the viewport of the
	 * lower left corner of this maplet.
	 */
	origx = vp_info.vxcent + (mw-info.long_deg) / dpp_ew;
	origy = vp_info.vycent + (info.lat_deg-mn) / dpp_ns;

	if ( settings.verbose & V_BASIC ) {
	    printf ( " Orig, x, y: %d %d\n", origx, origy );
	}

	draw_maplet ( mp, origx, origy );
}
#endif

/* SIGNS, Signs, signs, keeping signs straight is what this is all about!
 * Watch out for a multitude of sign conventions, here is a
 * quick orientation:
 * 	long,lat    - long increases to the east (right), lat increases to north (up)
 *		(Do not be fooled by longitude values which are negative and
 *		 become more negative to the west, this is still decreasing!!)
 * 	utm x,y     - x increases to east, y increases to north.
 * 	maplet x,y  - x increases to west (left), y increases to north (up)
 * 	pixmap x,y  - x increases to the right (east), y increases down (south)
 *	viewport    - x increases to right, y increases down
 *
 * pixmap_redraw is the guts of what goes on during a reconfigure.
 */
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
	int px, py;	/* maplet size in pixels */

	/* get the viewport size */
	vxdim = vp_info.vx;
	vydim = vp_info.vy;

	/* clear the whole pixmap to white */
	gdk_draw_rectangle ( info.series->pixels, vp_info.da->style->white_gc, TRUE, 0, 0, vxdim, vydim );
	info.series->content = 1;

#ifdef notdef
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

	if ( info.series->series == S_STATE || info.series->series == S_ATLAS ) {
	    iterate_series_method ( state_handler );
	    return;
	}
#endif

	/* XXX - hackish thing for FILE method */
	setup_series();
	synch_position();

	/* A first guess, hopefuly to be corrected
	 * as soon as we actually read a maplet
	 */
	px = info.series->xdim;
	py = info.series->ydim;

	/* load the maplet containing the current position so
	 * we can get the maplet pixel size up front.
	 * we fetch the center maplet here to get some info from
	 * it, the loop below will get it again (but from our cache).
	 */
	mp = load_maplet ( info.maplet_x, info.maplet_y );

	/* The above can fail if we have used the mouse to wander off
	 * the edge of the map coverage.  We use the series information
	 * in that case to allow something like reasonable mouse motion
	 * to move us back.  Note that we replace the "guessed" info
	 * about maplet size -- with a hopefully better guess.
	 */
	if ( mp ) {
	    info.series->xdim = px = mp->xdim;
	    info.series->ydim = py = mp->ydim;
	    if ( settings.verbose & V_DRAW )
		printf ( "Center maplet size x,y = %d, %d\n", px, py );
	} else {
	    if ( settings.verbose & V_DRAW )
		printf ( "No Center maplet\n" );
		
	}

	/* location of the center within the maplet */
	offx = info.fx * px;
	offy = info.fy * py;

	origx = vp_info.vxcent - offx;
	origy = vp_info.vycent - offy;

	if ( settings.verbose & V_DRAW ) {
	    printf ( "Maplet off, orig: %d %d -- %d %d\n", offx, offy, origx, origy );
	    printf ( "px, py = %d, %d\n", px, py );
	    printf ( "vxdim, vydim = %d, %d\n", vxdim, vydim );
	}

	if ( info.center_only ) {
	    nx1 = nx2 = 0;
	    ny1 = ny2 = 0;
	} else {
	    nx1 = - (vxdim - (origx + px) + px - 1 ) / px;
	    nx2 = + (origx + px - 1 ) / px;
	    ny1 = - (vydim - (origy + py) + py - 1 ) / py;
	    ny2 = + (origy + py - 1 ) / py;
	}

	if ( settings.verbose & V_DRAW ) {
	    printf ( "redraw -- viewport: %d %d -- maplet %d %d -- offset: %d %d\n",
		vxdim, vydim, px, py, offx, offy );
	    printf ( "redraw range: x,y = %d %d %d %d\n", nx1, nx2, ny1, ny2 );
	}

	/* This loop works in maplet indices, with
	 * x increasing to the west (left), and y increasing to the north (up).
	 * which is exactly opposite of the GTK pixel coordinates, which have
	 * the origin at the upper left of the screen.
	 */
	for ( y = ny1; y <= ny2; y++ ) {
	    for ( x = nx1; x <= nx2; x++ ) {

		if ( info.series->terra )
		    mp = load_maplet ( info.maplet_x - x, info.maplet_y + y );
		else
		    mp = load_maplet ( info.maplet_x + x, info.maplet_y + y );

		if ( ! mp ) {
		    if ( settings.verbose & V_DRAW2 )
			printf ( "redraw, no maplet at %d %d\n", x, y );
		    continue;
		}

		if ( settings.verbose & V_DRAW2 )
		    printf ( "redraw OK for %d %d, draw at %d %d\n",
			x, y, origx - mp->xdim*x, origy - mp->ydim*y );
		draw_maplet ( mp,
			origx - mp->xdim * x,
			origy - mp->ydim * y );
	    }
	}

	if ( settings.show_maplets ) {
	    for ( x = nx1+1; x <= nx2; x++ ) {
		xx = origx - px * x,
		gdk_draw_line ( info.series->pixels, vp_info.da->style->black_gc,
		    xx, 0, xx, vp_info.vy );
	    }
	    for ( y = ny1+1; y <= ny2; y++ ) {
		yy = origy - py * y,
		gdk_draw_line ( info.series->pixels, vp_info.da->style->black_gc,
		    0, yy, vp_info.vx, yy );
	    }
	}

	// This won't work here, everything gets overwritten
	//  by the map and you never see it.
	// overlay_redraw ();
}

/* We are changing location, and maybe also series, so we should not
 * take for granted we already have a pixmap allocated for that series.
 */
void
new_redraw ( void )
{
	int i;

	/* invalidate content of any prior pixmaps */
	for ( i=0; i<N_SERIES; i++ )
	    info.series_info[i].content = 0;

	if ( ! info.series->pixels )
	    info.series->pixels = gdk_pixmap_new ( vp_info.da->window, vp_info.vx, vp_info.vy, -1 );

	pixmap_redraw ();

	/* put the new pixmap on the screen */
	pixmap_expose ( 0, 0, vp_info.vx, vp_info.vy );
}

/* Since this is called from the configure handler, we have just changed
 * viewport size, so we need to toss any pixmaps and make new ones.
 * We don't expose yet, an event will take care of that.
 */
void
total_redraw ( void )
{
	int i;
	struct series *sp;

	/* invalidate all prior pixmaps */
	for ( i=0; i<N_SERIES; i++ ) {
	    sp = &info.series_info[i];
	    /* Avoid memory leak */
	    if ( sp->pixels )
		gdk_pixmap_unref ( sp->pixels );
	    sp->pixels = NULL;
	    sp->content = 0;
	}

	info.series->pixels = gdk_pixmap_new ( vp_info.da->window, vp_info.vx, vp_info.vy, -1 );

	pixmap_redraw ();
}

/* This is used when we are staying within series, but are doing a move or
 * some kind of translation
 */
void
full_redraw ( void )
{
	int i;

	/* Note that we keep any pixmaps, as they will
	 * be the correct geometry (the content is not valid).
	 * the correct size and so forth.
	 */
	for ( i=0; i<N_SERIES; i++ )
	    info.series_info[i].content = 0;

	/* redraw on the new center */
	pixmap_redraw ();

	/* put the new pixmap on the screen */
	pixmap_expose ( 0, 0, vp_info.vx, vp_info.vy );
}

static int config_count = 0;

/* This gets called when the drawing area gets created or resized.
 * (and after every one of these you get an expose event).
 */
gint
configure_handler ( GtkWidget *wp, GdkEvent *event, gpointer data )
{
	/* get the viewport size */
	vp_info.vx = wp->allocation.width;
	vp_info.vy = wp->allocation.height;
	vp_info.vxcent = vp_info.vx / 2;
	vp_info.vycent = vp_info.vy / 2;

	if ( settings.verbose & V_WINDOW )
	    printf ( "Configure event %d (%d, %d)\n", config_count++, vp_info.vx, vp_info.vy );

	total_redraw ();

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
		mp = load_maplet ( info.maplet_x, info.maplet_y );
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
	double dx, dy;

	/* viewport center */
	vxcent = vp_info.vx / 2;
	vycent = vp_info.vy / 2;

	if ( settings.verbose & V_EVENT )
	    printf ( "Button: orig position (lat/long) %.4f %.4f\n",
		info.lat_deg, info.long_deg );

	dx = (new_x - (double)vxcent) * info.series->x_pixel_scale;
	dy = (new_y - (double)vycent) * info.series->y_pixel_scale;

	if ( settings.verbose & V_EVENT )
	    printf ( "Button: delta position (x/y) %.4f %.4f\n", dx, dy );

	/* Make location of the mouse click be the current position */
	if ( ! try_position ( dx, -dy ) )
	    return;

	full_redraw ();
}

void
shift_xy ( double shift_x, double shift_y )
{
	double dx, dy;

	dx = shift_x * info.series->x_pixel_scale;
	dy = shift_y * info.series->y_pixel_scale;

	if ( settings.verbose & V_EVENT )
	    printf ( "Motion: delta position (x/y) %.4f %.4f\n", dx, dy );

	/* Make location of the mouse click be the current position */
	if ( ! try_position ( -dx, dy ) )
	    return;

	full_redraw ();
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

gint
info_destroy_handler ( GtkWidget *w, GdkEvent *event, gpointer data )
{
	i_info.status = GONE;

	return FALSE;
}

gint
go_button_handler ( GtkWidget *w, GdkEvent *event, gpointer data )
{
	const gchar *sp;
	double b_long, b_lat;

	sp = gtk_entry_get_text ( GTK_ENTRY(i_info.e_long) );
	if ( sp == 0 )
	    return FALSE;
	b_long = parse_dms ( (char *) sp );
	if ( b_long > 0.0 )
	    b_long = -b_long;

	sp = gtk_entry_get_text ( GTK_ENTRY(i_info.e_lat) );
	if ( sp == 0 )
	    return FALSE;

	b_lat = parse_dms ( (char *) sp );

	if ( b_lat < 24.0 || b_lat > 50.0 )
	    return FALSE;
	if ( b_long < -125.0 || b_long > -66.0 )
	    return FALSE;

	printf ( "long: %.4f\n", b_long );
	printf ( "lat: %.4f\n", b_lat );

	set_position ( b_long, b_lat );

	full_redraw ();

	return FALSE;
}

/* XXX - It seems to me, that in this (and many other) places there is
 * a half pixel error, we call the center of a 400 pixel screen, 200, but
 * then get coordinates as 0-399
 */
void
info_update ( void )
{
	char str[64];
	double c_lat, c_long;

	if ( settings.verbose & V_EVENT )
	    printf ( "info_update: %d\n", i_info.status );

	if ( i_info.status != UP )
	    return;

	c_long = info.long_deg + (mouse_info.x-vp_info.vxcent) * info.series->x_pixel_scale;
	sprintf ( str, "%.5f", c_long );
	gtk_label_set_text ( GTK_LABEL(i_info.l_long), str );

	c_lat = info.lat_deg - (mouse_info.y-vp_info.vycent) * info.series->y_pixel_scale;
	sprintf ( str, "%.5f", c_lat );
	gtk_label_set_text ( GTK_LABEL(i_info.l_lat), str );
}

/* This generates debug information about the
 * current mouse location.
 */
void
debug_dumper ( void )
{
	double c_lat, c_long;
    	double x, y;
	double fx, fy;
	int mx, my;
	int s;
	struct maplet *mp;
	struct tpq_info *tp;

	c_long = info.long_deg + (mouse_info.x-vp_info.vxcent) * info.series->x_pixel_scale;
	c_lat = info.lat_deg - (mouse_info.y-vp_info.vycent) * info.series->y_pixel_scale;

	printf (" Current mouse long, lat = %.5f %.5f\n", c_long, c_lat );

	/* from show_statistics, archive.c */
	printf ( "Total sections: %d\n", info.n_sections );

	for ( s=0; s<N_SERIES; s++ ) {
	    if ( s == info.series->series ) {
		printf ( "Map series %d (%s) %d maps << current series **\n", s+1, wonk_series(s), info.series_info[s].tpq_count );
		show_methods ( &info.series_info[s] );
	    } else
		printf ( "Map series %d (%s) %d maps\n", s+1, wonk_series(s), info.series_info[s].tpq_count );
	}

	printf ( "maplet size (%s): %.3f %.3f\n", wonk_series(info.series->series), info.series->maplet_long_deg, info.series->maplet_lat_deg );

	printf ( "Center long, lat = %.5f %.5f\n", info.long_deg, info.lat_deg );

	if ( tp = lookup_tpq ( info.series ) ) {
	    /* File method has the position */
	    x = - (info.long_deg - tp->e_long) / info.series->maplet_long_deg;
	    y =   (info.lat_deg - tp->s_lat) / info.series->maplet_lat_deg;
	} else {
	    /* Setion method (lat/long offsets always zero) */
	    x = - info.long_deg / info.series->maplet_long_deg;
	    y =   info.lat_deg  / info.series->maplet_lat_deg;
	}

	printf ( "Center maplet raw x,y = %.5f %.5f\n", x, y );

    	mx = x;
    	my = y;
	printf ( "Center maplet x,y = %d %d\n", mx, my );

	fx = 1.0 - (x - mx);
	fy = 1.0 - (y - my);
	printf ( "Center offset in maplet fx, fy = %.5f %.5f\n", fx, fy );

	mp = load_maplet ( info.maplet_x, info.maplet_y );
	if ( ! mp ) {
	    printf ("load maplet fails for series %s\n", wonk_series(info.series->series) );
	    return;
	}

	tp = mp->tpq;
	printf ( " maplet from file: %s\n", tp->path );
	printf ( " maplet from quad: %s (%s)\n", tp->quad, tp->state );

	printf ( " sheet, S, N: %.4f %.4f\n", tp->s_lat, tp->n_lat );
	printf ( " sheet, W, E: %.4f %.4f\n", tp->w_long, tp->e_long );
}

static void
process_file ( char *file )
{
	struct stat st;
	int fd;
	int n;
	char *xbuf;
	struct xml *xp;

	fd = open ( file, O_RDONLY );
	if ( fd < 0 )
	    return;

	if ( fstat ( fd, &st ) < 0 ) {
	    close ( fd );
	    return;
	}

	n = st.st_size;
	if ( n > 1024*1024 ) {
	    printf ( "File too big: %d\n", n );
	    close ( fd );
	    return;
	}

	xbuf = gmalloc ( n + 128 );
	if ( read ( fd, xbuf, n + 128 ) != n ) {
	    printf ( "Read error\n" );
	    free ( xbuf );
	    close ( fd );
	}

	close ( fd );

	printf ( "FILE: %s %d\n", file, n );

	xp = xml_parse_doc ( xbuf, n );
	free ( xbuf );

	if ( ! xp ) {
	    printf ( "invalid file\n" );
	    return;
	}

	xml_destroy ( xp );
}

void
file_window ( void )
{
	GtkWidget *dialog;
	char *filename;

	dialog = gtk_file_chooser_dialog_new ("Open File",
				      NULL,
				      GTK_FILE_CHOOSER_ACTION_OPEN,
				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				      NULL);

	filename = NULL;
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
	    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

	gtk_widget_destroy (dialog);

	if ( ! filename )
	    return;

	process_file (filename);
	g_free (filename);
}

void
info_window ( void )
{
	GtkWidget *w;
	GtkWidget *vb;
	GtkWidget *hb1;
	GtkWidget *hb2;

	if ( i_info.status == UP ) {
	    /* printf ( "hiding\n" ); */
	    gtk_widget_hide_all ( i_info.main );
	    i_info.status = HIDDEN;
	    return;
	}

	if ( i_info.status == HIDDEN ) {
	    /* printf ( "unhiding\n" ); */
	    gtk_widget_show_all ( i_info.main );
	    i_info.status = UP;
	    info_update ();
	    return;
	}

	if ( i_info.status == GONE ) {
	    /* printf ( "uping\n" ); */
	    i_info.main = gtk_window_new ( GTK_WINDOW_TOPLEVEL );
	    vb = gtk_vbox_new ( FALSE, 0 );
	    gtk_container_add ( GTK_CONTAINER(i_info.main), vb );

	    hb1 = gtk_hbox_new ( FALSE, 0 );
	    gtk_container_add ( GTK_CONTAINER(vb), hb1 );
	    w = gtk_label_new ( "Long:" );
	    gtk_box_pack_start ( GTK_BOX(hb1), w, TRUE, TRUE, 0 );
	    i_info.l_long = gtk_label_new ( "-A-" );
	    gtk_box_pack_start ( GTK_BOX(hb1), i_info.l_long, TRUE, TRUE, 0 );
	    i_info.e_long = gtk_entry_new_with_max_length ( 16 );
	    gtk_box_pack_start ( GTK_BOX(hb1), i_info.e_long, TRUE, TRUE, 0 );

	    hb2 = gtk_hbox_new ( FALSE, 0 );
	    gtk_container_add ( GTK_CONTAINER(vb), hb2 );
	    w = gtk_label_new ( "Lat: " );
	    gtk_box_pack_start ( GTK_BOX(hb2), w, TRUE, TRUE, 0 );
	    i_info.l_lat = gtk_label_new ( "-B-" );
	    gtk_box_pack_start ( GTK_BOX(hb2), i_info.l_lat, TRUE, TRUE, 0 );
	    i_info.e_lat = gtk_entry_new_with_max_length ( 16 );
	    gtk_box_pack_start ( GTK_BOX(hb2), i_info.e_lat, TRUE, TRUE, 0 );

	    w = gtk_button_new_with_label ( "Go" );
	    gtk_container_add ( GTK_CONTAINER(vb), w );
	    g_signal_connect ( w, "clicked",
			G_CALLBACK(go_button_handler), NULL );

	    g_signal_connect ( i_info.main, "delete_event",
			G_CALLBACK(info_destroy_handler), NULL );

	    gtk_widget_show_all ( i_info.main );
	    i_info.status = UP;

	    info_update ();
	}
}

gint
places_destroy_handler ( GtkWidget *w, GdkEvent *event, gpointer data )
{
	p_info.status = GONE;

	return FALSE;
}

static int first_sel;

/* Ask whether it is OK to change the selection status, we use it as an indicator
 * that the selection IS going to change.  We see this called twice for every time
 * an entry is selected, so it would be good to filter the two events.
 */
gint
places_select_func ( GtkTreeSelection *sel, GtkTreeModel *model, GtkTreePath *path,
			gboolean cur_selected, gpointer user_data )
{
	GtkTreeIter iter;
	gchar *name;
	gchar *s_lat;
	gchar *s_long;
	double lat, lon;
	int series;

	/* We always get a notice of the current selection when the
	 * window first comes up, we just want to ignore it.
	 */
	if ( first_sel ) {
		first_sel = 0;
		return TRUE;
	}

	/* also ignore deselections */
	if ( cur_selected )
		return TRUE;

	if ( gtk_tree_model_get_iter ( model, &iter, path ) ) {
		gtk_tree_model_get ( model, &iter,
		    NAME_COLUMN, &name,
		    LAT_COLUMN, &s_lat,
		    LONG_COLUMN, &s_long,
		    SERIES_COLUMN, &series,
		    -1 );


		lon = parse_dms ( s_long );
		lat = parse_dms ( s_lat );

		/*
		printf ( "%s will be selected (%s, %s)\n", name, s_long, s_lat );
		printf ( "long: %.4f\n", lon );
		printf ( "lat: %.4f\n", lat );
		*/

		initial_series ( series );
		set_position ( lon, lat );

		new_redraw ();

		if ( name )
		    g_free ( name );
	}

	/* allow the selection change */
	return TRUE;
}

/* PLACES */

/* gtk2 no longer has a plain old "listbox"
 * (it has one, but deprecated), so the thing to do is to use
 * a TreeView with a ListStore in the new world order.
 */
void
places_window ( void )
{
	GtkWidget *view;
	GtkTreeViewColumn *col;
	GtkCellRenderer *rend;
	GtkTreeSelection *sel;

	if ( p_info.status == UP ) {
	    /* printf ( "hiding\n" ); */
	    gtk_widget_hide_all ( p_info.main );
	    p_info.status = HIDDEN;
	    return;
	}

	if ( p_info.status == HIDDEN ) {
	    /* printf ( "unhiding\n" ); */
	    gtk_widget_show_all ( p_info.main );
	    p_info.status = UP;
	    return;
	}

	if ( p_info.status == GONE ) {
	    /* printf ( "uping\n" ); */
	    p_info.main = gtk_window_new ( GTK_WINDOW_TOPLEVEL );

	    view = gtk_tree_view_new_with_model ( GTK_TREE_MODEL(p_info.store) );

	    /* Name column */
	    col = gtk_tree_view_column_new ();
	    gtk_tree_view_column_set_title ( col, "Name" );
	    gtk_tree_view_append_column ( GTK_TREE_VIEW(view), col );
	    rend = gtk_cell_renderer_text_new ();
	    gtk_tree_view_column_pack_start ( col, rend, TRUE );
	    g_object_set ( rend, "text", "Unknown", NULL );	/* XXX */
	    gtk_tree_view_column_add_attribute ( col, rend, "text", NAME_COLUMN );

	    /* Longitude column */
	    col = gtk_tree_view_column_new ();
	    gtk_tree_view_column_set_title ( col, "Long" );
	    gtk_tree_view_append_column ( GTK_TREE_VIEW(view), col );
	    rend = gtk_cell_renderer_text_new ();
	    gtk_tree_view_column_pack_start ( col, rend, TRUE );
	    g_object_set ( rend, "text", "Ulong", NULL );	/* XXX */
	    gtk_tree_view_column_add_attribute ( col, rend, "text", LONG_COLUMN );

	    /* Latitude column */
	    col = gtk_tree_view_column_new ();
	    gtk_tree_view_column_set_title ( col, "Lat" );
	    gtk_tree_view_append_column ( GTK_TREE_VIEW(view), col );
	    rend = gtk_cell_renderer_text_new ();
	    gtk_tree_view_column_pack_start ( col, rend, TRUE );
	    g_object_set ( rend, "text", "Ulat", NULL );	/* XXX */
	    gtk_tree_view_column_add_attribute ( col, rend, "text", LAT_COLUMN );

	    /* Get the selection object */
	    first_sel = 1;
	    sel = gtk_tree_view_get_selection ( GTK_TREE_VIEW(view) );
	    gtk_tree_selection_set_mode ( sel, GTK_SELECTION_SINGLE );
	    gtk_tree_selection_set_select_function ( sel, places_select_func, NULL, NULL );

	    /*
	    g_signal_connect ( view, "changed",
			G_CALLBACK(places_select_handler), NULL );
	    */

	    gtk_container_add ( GTK_CONTAINER(p_info.main), view );

	    g_signal_connect ( p_info.main, "delete_event",
			G_CALLBACK(places_destroy_handler), NULL );

	    gtk_widget_show_all ( p_info.main );
	    p_info.status = UP;
	}
}

#define KV_LEFT		65361
#define KV_UP		65362
#define KV_RIGHT	65363
#define KV_DOWN		65364

#define KV_CTRL		65507

#define KV_A		'a'
#define KV_A_UC		'A'

#define KV_D		'd'
#define KV_D_UC		'D'

#define KV_F		'f'
#define KV_F_UC		'F'

#define KV_I		'i'
#define KV_I_UC		'I'

/* XXX - need to bind these to a help screen */
#define KV_H		'h'
#define KV_H_UC		'H'

#define KV_P		'p'
#define KV_P_UC		'P'

#define KV_S		's'
#define KV_S_UC		'S'

/* Added 6-20-2011 to display map debug info */
#define KV_M		'm'
#define KV_M_UC		'M'

/* We don't use these yet, but ... */
#define KV_ESC		65307
#define KV_TAB		65289

/* Print (to stdout) information about where we are
 * and what map(s) are under the cursor.
 */
void
kb_maps ( void )
{
	printf ( "Current series is: %s (%d)\n", wonk_series(info.series->series), info.series->series );
	show_statistics ();
}

void
local_up_series ( void )
{
	up_series ();
	info_update ();
}

void
local_down_series ( void )
{
	down_series ();
	info_update ();
}


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
	
	if ( event->keyval == settings.up_key )
	    local_up_series ();
	else if ( event->keyval == settings.down_key )
	    local_down_series ();
	else if ( event->keyval == KV_D || event->keyval == KV_D_UC )
	    debug_dumper ();
	else if ( event->keyval == KV_F || event->keyval == KV_F_UC )
	    file_window ();
	else if ( event->keyval == KV_I || event->keyval == KV_I_UC )
	    info_window ();
	else if ( event->keyval == KV_P || event->keyval == KV_P_UC )
	    places_window ();
	else if ( event->keyval == KV_S || event->keyval == KV_S_UC )
	    snap ();
	else if ( event->keyval == KV_M || event->keyval == KV_M_UC )
	    kb_maps ();
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
	int size;

	if ( ! settings.center_marker )
	    return;

	size = settings.marker_size;

	if ( clean )
	    cursor_mode = 1;
	else
	    cursor_mode = 1 - cursor_mode;

	if ( cursor_mode ) {
	    gdk_draw_line ( vp_info.da->window,
		vp_info.da->style->white_gc,
		vp_info.vxcent, vp_info.vycent-size,
		vp_info.vxcent, vp_info.vycent+size );
	    gdk_draw_line ( vp_info.da->window,
		vp_info.da->style->white_gc,
		vp_info.vxcent-size, vp_info.vycent,
		vp_info.vxcent+size, vp_info.vycent );
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
		vp_info.vxcent, vp_info.vycent-size,
		vp_info.vxcent, vp_info.vycent+size );
	    gdk_draw_line ( vp_info.da->window,
		vp_info.da->style->black_gc,
		vp_info.vxcent-size, vp_info.vycent,
		vp_info.vxcent+size, vp_info.vycent );
	}
}

/* in milliseconds */
#define TICK_DELAY	500

gint
tick_handler ( gpointer data )
{
	cursor_show ( 0 );
	remote_check ();

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
	    if ( settings.m3_action == M3_CENTER )
		move_xy ( event->x, event->y );
	    if ( settings.m3_action == M3_ZOOM ) {
		if ( ctrl_key_pressed )
		    local_up_series ();
		else
		    local_down_series ();
	    }
	}

	if ( event->button == 2 ) {
	    /* XXX */
	    show_pos ();
	}

	if ( event->button == 1 ) {
	    if ( settings.m1_action == M1_CENTER )
		move_xy ( event->x, event->y );
	}

	return TRUE;
}

#define GDK_BUTTON_MASK		(GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK)

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

	/* Stash this (mostly to cleanly startup the info window)
	 */
	mouse_info.x = event->x;
	mouse_info.y = event->y;
	mouse_info.time = event->time;

	/* Plain old mouse moving around with no buttons down.
	 * At one time we could just test == 0 here, but somewhere
	 * along the line we started always seeing the 0x10 bit set
	 * (which is GDK_MOD1_MASK, whatever that is)
	 * so we now mask and check just the buttons.
	 * see /usr/include//gtk-2.0/gdk/gdktypes.h
	 */
	if ( i_info.status == UP && (event->state & GDK_BUTTON_MASK) == 0 ) {
	    info_update ();
	    return TRUE;
	}

	/*
	move_xy ( event->x, event->y );
	*/

	/* moving mouse with left button down - shift map */
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

gint
scroll_handler ( GtkWidget *wp, GdkEventScroll *event, gpointer data )
{
	if ( event->direction == GDK_SCROLL_UP )
	    local_up_series ();
	else if ( event->direction == GDK_SCROLL_DOWN )
	    local_down_series ();
	else
	    printf ( "Scroll event %d\n", event->direction );

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

	return TRUE;
}

void
synch_position ( void )
{
    	double x, y;
	struct tpq_info *tp;

#ifdef TERRA
	if ( info.series->terra ) {
	    ll_to_utm ( info.long_deg, info.lat_deg, &info.utm_zone, &info.utm_x, &info.utm_y );
	    x = info.utm_x / ( 200.0 * info.series->x_pixel_scale );
	    y = info.utm_y / ( 200.0 * info.series->y_pixel_scale );
	} else {
#endif

	if ( tp = lookup_tpq ( info.series ) ) {
	    /* File method has the position */
	    x = - (info.long_deg - tp->e_long) / info.series->maplet_long_deg;
	    y =   (info.lat_deg - tp->s_lat) / info.series->maplet_lat_deg;
	} else {
	    /* Setion method (lat/long offsets always zero) */
	    x = - info.long_deg / info.series->maplet_long_deg;
	    y =   info.lat_deg / info.series->maplet_lat_deg;
	}

	/* indices of the maplet we are in
	 */
    	info.maplet_x = x;
    	info.maplet_y = y;

	if ( settings.verbose & V_BASIC ) {
	    printf ( "Synch position: long/lat = %.3f %.3f\n", info.long_deg, info.lat_deg );
	    printf ( "maplet size: %.3f %.3f\n", info.series->maplet_long_deg, info.series->maplet_lat_deg );
	    printf ( "maplet indices of position: %d %d for series %s\n",
		info.maplet_x, info.maplet_y, wonk_series(info.series->series) );
	}

	/* fractional offset of our position in that maplet
	 */
	info.fx = 1.0 - (x - info.maplet_x);
	info.fy = 1.0 - (y - info.maplet_y);
}

#ifdef TERRA
void
synch_position_utm ( void )
{
	double x, y;

	utm_to_ll ( info.utm_zone, info.utm_x, info.utm_y, &info.long_deg, &info.lat_deg );

	x = info.utm_x / ( 200.0 * info.series->x_pixel_scale );
	y = info.utm_y / ( 200.0 * info.series->y_pixel_scale );

	/* indices of the maplet we are in
	 */
    	info.maplet_x = x;
    	info.maplet_y = y;

	if ( settings.verbose & V_BASIC ) {
	    printf ( "Synch position: long/lat = %.3f %.3f\n", info.long_deg, info.lat_deg );
	    printf ( "maplet indices of position: %d %d\n",
		info.maplet_x, info.maplet_y );
	}

	/* fractional offset of our position in that maplet
	 */
	info.fx = 1.0 - (x - info.maplet_x);
	info.fy = 1.0 - (y - info.maplet_y);
}
#endif

/* Used by mouse routine to check that a possible new
 * position still has map coverage.
 */
static int
try_position ( double dx, double dy )
{
    	double save1, save2;

#ifdef TERRA
	if ( info.series->terra ) {
	    save1 = info.utm_x;
	    save2 = info.utm_y;

	    info.utm_x -= dx;	/* XXX - note sign */
	    info.utm_y += dy;
	    synch_position_utm ();
	} else {
#endif

	save1 = info.long_deg;
	save2 = info.lat_deg;

	info.long_deg += dx;
	info.lat_deg += dy;
	synch_position ();

	if ( load_maplet ( info.maplet_x, info.maplet_y ) )
	    return 1;

	/* Didn't like it, go back */
#ifdef TERRA
	if ( info.series->terra ) {
	    info.utm_x = save1;
	    info.utm_y = save2;
	    synch_position_utm ();
	} else {
#endif

	info.long_deg = save1;
	info.lat_deg = save2;
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

	/* Built in places to look for maps, can be overridden
	 * from the settings file (and usually is).
	 */
#ifdef INITIAL_ARCHIVE
	archive_add ( INITIAL_ARCHIVE );
#else
	archive_add ( "/u1/topo" );
	archive_add ( "/u2/topo" );
	archive_add ( "/mmt/topo" );
	archive_add ( "/home/topo" );
	archive_add ( "/topo" );
#endif

	/* Let gtk strip off any of its arguments first
	 */
	gtk_init ( &argc, &argv );

	argc--;
	argv++;

	info.center_only = 0;

	series_init ();

	gpx_init ();

	settings_init ();

	p_info.status = GONE;

	places_init ();

	remote_init ();

	while ( argc-- ) {
	    p = *argv++;
	    if ( strcmp ( p, "-V" ) == 0 )
		/* Event debugging is usually a nuisance */
	    	settings.verbose = (0xffff ^ V_EVENT);

	    /* XXX - really should dynamically generate version string at compile time */
	    if ( strcmp ( p, "-v" ) == 0 ) {
	    	printf ( "gtopo version 1.1.0\n" );
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

#ifdef TERRA
	    if ( strcmp ( p, "-t" ) == 0 ) {
	    	terra_test ();
		return 0;
	    }
#endif

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
		/* show file information, friendly and verbose */
		if ( argc < 1 )
		    usage ();
		file_info ( *argv, 0 );
		return 0;
	    }
	    if ( strcmp ( p, "-j" ) == 0 ) {
		/* show file information, single line */
		if ( argc < 1 )
		    usage ();
		file_info ( *argv, 1 );
		return 0;
	    }
	    if ( strcmp ( p, "-k" ) == 0 ) {
		/* show file information, include index to maplets (extra verbose) */
		if ( argc < 1 )
		    usage ();
		file_info ( *argv, 2 );
		return 0;
	    }
	}
	
	if ( settings.verbose ) {
		printf ( "Debug mask: %08x\n", settings.verbose );
	}

	if ( file_opt ) {
	    /* special: show a single specific .tpq file */

	    /* XXX - should check file header and avoid trying
	     * to display any old file with resultant segfault
	     * or memory error or who knows what, i.e. we should
	     * validate that the file in fact looks like a TPQ file
	     */
	    if ( ! file_init ( file_name ) ) {
		printf ( "No TOPO file: %s\n", file_name );
		return 1;
	    }
	    printf ( "Displaying single file: %s\n", file_name );

	} else {
	    /* The usual case */

	    if ( ! archive_init () ) {
		printf ( "No topo archives found\n" );
		return 1;
	    }

	    /* Just probe to see if we can display a map at the
	     * starting position and series requested.
	     * If not, we exit here and now with a message rather
	     * than presenting a blank white screen.
	     */
	    if ( ! first_series () ) {
		show_statistics ();
		printf ( "Cannot find map at Long = %.3f, Lat = %.3f for series: %s\n",
		    settings.starting_long, settings.starting_lat, wonk_series(settings.starting_series));
		return 1;
	    }
	    if ( settings.verbose & V_BASIC )
	    	printf ( "first_series() gives a green light\n" );
	}

	overlay_init ();

	/* --- set up the GTK stuff we need */

	/* ### First - the main window */
	mw = gtk_window_new ( GTK_WINDOW_TOPLEVEL );

	g_signal_connect ( mw, "delete_event",
			G_CALLBACK(destroy_handler), NULL );

	/* ### Second - a drawing area */
	vp_info.da = da = gtk_drawing_area_new ();

	/* Hook up the configure signal.
	 * We get this first on startup, and use this
	 * as an opportunity to set up the first pixbuf
	 * of a size to match the display window.
	 * (and we fill this with map pixels).
	 * This handler will get called again each
	 * time the window gets resized.
	 */
	g_signal_connect ( da, "configure_event",
			G_CALLBACK(configure_handler), NULL );

	/* On startup, we get an expose event, right after
	 * the configure event, and then put the pixels on
	 * the screen.
	 */
	g_signal_connect ( da, "expose_event",
			G_CALLBACK(expose_handler), NULL );

	/* We could also connect to the "realize" signal,
	 * but I haven't found a need for that yet.
	 */

	/* We never see the release event, unless we add
	 * the press event to the mask.
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

	/* See if we can get scroll events (like the mouse wheel) */
	g_signal_connect ( da, "scroll_event",
			G_CALLBACK(scroll_handler), NULL );
	/* Apparently not needed */
	/*
	gtk_widget_add_events ( GTK_WIDGET(da), GDK_SCROLL_MASK );
	*/

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
