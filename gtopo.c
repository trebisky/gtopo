#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "gtopo.h"

#define CENTER_ONLY

#define MINIMUM_VIEW	100
#define INITIAL_VIEW	800

/* Tom Trebisky  MMT Observatory, Tucson, Arizona
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
 	works for 100K series 7/12/2007
	add series structure and reorganize 7/13/2007
 *
 *  TODO
 *   - fix bug that warps map NS in Arizona.
 *   - fix bug where if you click on a white area, the center
 *     maplet goes away, so all maplets go white.
 *   - add age field to maplet cache and expire/recycle
 *     if size grows beyond some limit.
 *   - clean up maplet structure, and related stuff in
 *     archive.c and maplet.c, maybe some common code.
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
 *  I have only the California and Arizona sets to work from.
 *  There are some unique differences in these sets on levels
 *  1 thru 3, levels 4 and 5 seem uniform, but we shall see.
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

/* This is a list of "root directories" where images of the
 * CDROMS may be found.  It is used as a kind of search path,
 * if directories do not exist they are ignored.
 * If they do exist, they are searched for subdirectories like
 * CA_D06 and az_d02, and so forth.
 * This allows a path to be set up that will work on multiple
 * machines that keep the topos in different places.
 */
char *topo_archives[] = { "/u1/topo", "/u2/topo", NULL };

#ifdef notdef
/* This one has 50 jpeg maplets in a 5x10 pattern */
char *tpq_path = "../q36117h8.tpq";

/* This one has 100 jpeg maplets in a 10x10 pattern */
/*  each is 406x480 */
/* These are found on every CD as:
 *  /u1/topo/AZ_D05/AZ1_MAP3/F30105A1.TPQ
 */
char *tpq_path = "../F30105A1.TPQ";
#endif

GdkColormap *syscm;

struct viewport {
	int vx;
	int vy;
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
/*		SRC_X, SRC_Y, x, y, -1, -1,	*/
		SRC_X, SRC_Y, x, y, mp->xdim, mp->ydim,
		GDK_RGB_DITHER_NONE, 0, 0 );
}

/* This is the guts of what goes on during a reconfigure */
void
pixmap_redraw ( void )
{
	int vxdim, vydim;
	int vxcent, vycent;
	int nx1, nx2, ny1, ny2;
	int offx, offy;
	int origx, origy;
	int x, y;
	struct maplet *mp;

	/* get the viewport size */
	vxdim = vp_info.vx;
	vydim = vp_info.vy;

	/* viewport center */
	vxcent = vxdim / 2;
	vycent = vydim / 2;

	/* clear the whole pixmap to white */
	gdk_draw_rectangle ( info.series->pixels, vp_info.da->style->white_gc, TRUE, 0, 0, vxdim, vydim );

	info.series->content = 1;

	mp = load_maplet ();
	info.series->center = mp;

	if ( mp ) {
	    /* location of the center within the maplet */
	    offx = info.series->fx * mp->xdim;
	    offy = info.series->fy * mp->ydim;

	    origx = vxcent - offx;
	    origy = vycent - offy;
	    printf ( "Maplet off, orig: %d %d -- %d %d\n", offx, offy, origx, origy );

	    draw_maplet ( mp, origx, origy );

	    nx1 = (origx + mp->xdim - 1 ) / mp->xdim;
	    nx2 = (vxdim - (origx + mp->xdim) + mp->xdim - 1 ) / mp->xdim;
	    ny1 = (origy + mp->ydim - 1 ) / mp->ydim;
	    ny2 = (vydim - (origy + mp->ydim) + mp->ydim - 1 ) / mp->ydim;

	    printf ( "redraw -- viewport: %d %d -- maplet %d %d -- offset: %d %d\n",
	    	vxdim, vydim, mp->xdim, mp->ydim, offx, offy );
	    printf ( "redraw range: x,y = %d %d %d %d\n", nx1, nx2, ny1, ny2 );

#ifndef CENTER_ONLY
	    for ( y = -ny1; y <= ny2; y++ ) {
		for ( x = -nx1; x <= nx2; x++ ) {
		    if ( x == 0 && y == 0 )
		    	continue;
		    mp = load_maplet_nbr ( x, y );
		    if ( ! mp )
			continue;
		    draw_maplet ( mp,
			    vxcent-offx + mp->xdim * x,
			    vycent-offy + mp->ydim * y );
		}
	    }
#endif
	}

	/* mark center */
	gdk_draw_rectangle ( info.series->pixels, vp_info.da->style->black_gc, TRUE, vxcent-1, vycent-1, 3, 3 );
}

static int config_count = 0;

/* This gets called when the drawing area gets created or resized.
 * (and after every one of these you get an expose event).
 */
gint
configure_handler ( GtkWidget *wp, GdkEvent *event, gpointer data )
{
	int vxdim, vydim;

	/* XXX - at this point we would like to resize ourself
	 * to INITIAL_VIEW.
	 */
	if ( info.initial ) {
	    info.initial = 0;
	}

	/* get the viewport size */
	vp_info.vx = vxdim = wp->allocation.width;
	vp_info.vy = vydim = wp->allocation.height;

	if ( info.verbose )
	    printf ( "Configure event %d (%d, %d)\n", config_count++, vxdim, vydim );

	invalidate_pixels ();

	info.series->pixels = gdk_pixmap_new ( wp->window, vxdim, vydim, -1 );
	pixmap_redraw ();

	return TRUE;
}

void
free_pixels ( struct series *sp )
{
	/* Avoid memory leak */
	if ( sp->pixels )
	    gdk_pixmap_unref ( sp->pixels );
	sp->pixels = NULL;
}

gint
mouse_handler ( GtkWidget *wp, GdkEventButton *event, gpointer data )
{
	int button;
	int vxcent, vycent;
	int mxdim, mydim;
	double dlat, dlong;
	double x_pixel_scale, y_pixel_scale;
	float x, y;

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

	printf ( "Orig position (lat/long) %.4f %.4f\n",
		info.lat_deg, info.long_deg );

	if ( info.series->center ) {
	    mxdim = info.series->center->xdim;
	    mydim = info.series->center->ydim;
	} else {
	    /* allow mouse motion if over unmapped area */
	    /* XXX - OK only for 24K maps */
	    mxdim = 410;	/* close */
	    mydim = 256;
	}

	x_pixel_scale = info.series->maplet_long_deg / (double)mxdim;
	y_pixel_scale = info.series->maplet_lat_deg / (double)mydim;

	dlat  = (event->y - (double)vycent) * y_pixel_scale;
	dlong = (event->x - (double)vxcent) * x_pixel_scale;
	printf ( "Delta position (lat/long) %.4f %.4f\n",
		dlat, dlong );

	/* Make location of the mouse click be the current position */
	info.lat_deg -= dlat;
	info.long_deg -= dlong;

	printf ( "New position (lat/long) %.4f %.4f\n",
		info.lat_deg, info.long_deg );

	invalidate_pixel_content ();

	/* redraw on the new center */
	pixmap_redraw ();

	/* put the new pixmap on the screen */
	pixmap_expose ( 0, 0, vp_info.vx, vp_info.vy );

	return TRUE;
}

gint
keyboard_handler ( GtkWidget *wp, GdkEventKey *event, gpointer data )
{
	int button;
	int vxcent, vycent;
	int mxdim, mydim;
	double dlat, dlong;
	double x_pixel_scale, y_pixel_scale;
	float x, y;

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

int
main ( int argc, char **argv )
{
	GtkWidget *main_window;
	GtkWidget *vb;
	int w, h;
	char *tpq_path;
	int xm, ym;
	char *p;

	/* Let gtk strip off any of its arguments first
	 */
	gtk_init ( &argc, &argv );

	argc--;
	argv++;

	info.verbose = 0;
	info.initial = 1;

	while ( argc-- ) {
	    p = *argv++;
	    if ( strcmp ( p, "-v" ) == 0 )
	    	info.verbose = 1;
	}

	archive_init ( topo_archives );

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

	/* In California west of Taboose Pass */
	info.lat_deg = dms2deg ( 37, 1, 0 );
	info.long_deg = dms2deg ( 118, 31, 0 );

#ifdef notdef
#endif
	/* Mt. Hopkins, Arizona */
	info.lat_deg = 31.69;
	info.long_deg = 110.88;

	set_series ( S_STATE );
	set_series ( S_ATLAS );
	set_series ( S_500K );
	set_series ( S_24K );
	set_series ( S_100K );

	set_series ( S_24K );

	vp_info.vx = MINIMUM_VIEW;
	vp_info.vy = MINIMUM_VIEW;
	gtk_drawing_area_size ( GTK_DRAWING_AREA(vp_info.da), vp_info.vx, vp_info.vy );

	gtk_widget_show ( vp_info.da );

	if ( info.verbose )
	    printf ( "single maplet size: %d by %d\n", w, h );

	gtk_main ();

	return 0;
}

/* THE END */
