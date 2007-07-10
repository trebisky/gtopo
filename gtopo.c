#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "gtopo.h"

/* Tom Trebisky  MMT Observatory, Tucson, Arizona
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
 *
 *  TODO
 *   - add age field to maplet cache and expire/recycle
 *     if size grows beyond some limit.
 *   - clean up maplet structure, and related stuff in
 *     archive.c and maplet.c, maybe some common code.
 *   - generalize the 5x10 maplet business
 *   - display other than 7.5 minute quad maplets
 *   - handle maplet size discontinuity.
 *   - display more than center maplet in new scheme,
 *     need a routine to find maplet given maplet offset
 *     from center (will need to jump to new quads and
 *     sections as needed)
 *   - be able to run off of mounted CDrom
 *   - put temp file in cwd, home, then /tmp
 *     give it a .topo.tmp name.
 */

/* Some notes on map series:
 * The full state maps are found in
 * /u1/topo/AZ_D01/AZ1_MAP1/AZ1_MAP1.TPQ
 *  lat 31-38  long 108-115  422x549 jpeg
 * /u1/topo/CA_D01/CA1_MAP1/CA1_MAP1.TPQ
 *  lat 32-42  long 114-125  687x789 jpeg
 */

int verbose_opt = 0;

struct position cur_pos;

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
	GdkPixmap *pixels;
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
		vp_info.pixels,
		x, y, x, y, nx, ny );
}

static int expose_count = 0;

gint
expose_handler ( GtkWidget *wp, GdkEventExpose *ep, gpointer data )
{
	if ( verbose_opt && expose_count < 4 )
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
draw_maplet ( GdkPixbuf *map, int x, int y )
{
	gdk_draw_pixbuf ( vp_info.pixels, NULL, map,
		SRC_X, SRC_Y, x, y, -1, -1,
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
	gdk_draw_rectangle ( vp_info.pixels, vp_info.da->style->white_gc, TRUE, 0, 0, vxdim, vydim );

	mp = load_maplet ( &cur_pos );
	cur_pos.maplet = mp;

	if ( mp ) {
	    /* location of the center within the maplet */
	    offx = cur_pos.fx * mp->xdim;
	    offy = cur_pos.fy * mp->ydim;;

	    origx = vxcent - offx;
	    origy = vycent - offy;
	    printf ( "Maplet off, orig: %d %d -- %d %d\n", offx, offy, origx, origy );

	    draw_maplet ( mp->pixbuf, origx, origy );

	    nx1 = (origx + mp->xdim - 1 ) / mp->xdim;
	    nx2 = (vxdim - (origx + mp->xdim) + mp->xdim - 1 ) / mp->xdim;
	    ny1 = (origy + mp->ydim - 1 ) / mp->ydim;
	    ny2 = (vydim - (origy + mp->ydim) + mp->ydim - 1 ) / mp->ydim;

	    printf ( "redraw -- viewport: %d %d -- maplet %d %d -- offset: %d %d\n",
	    	vxdim, vydim, mp->xdim, mp->ydim, offx, offy );
	    printf ( "redraw range: x,y = %d %d %d %d\n", nx1, nx2, ny1, ny2 );

	    for ( y = -ny1; y <= ny2; y++ )
		for ( x = -nx1; x <= nx2; x++ ) {
		    if ( x == 0 && y == 0 )
		    	continue;
		    mp = load_maplet_nbr ( &cur_pos, x, y );
		    if ( ! mp )
			continue;
		    draw_maplet ( mp->pixbuf,
			    vxcent-offx + mp->xdim * x,
			    vycent-offy + mp->ydim * y );
		}
	}

	/* mark center */
	gdk_draw_rectangle ( vp_info.pixels, vp_info.da->style->black_gc, TRUE, vxcent-1, vycent-1, 3, 3 );
}

static int config_count = 0;

/* This gets called when the drawing area gets created or resized.
 * (and after every one of these you get an expose event).
 */
gint
configure_handler ( GtkWidget *wp, GdkEvent *event, gpointer data )
{
	int vxdim, vydim;

	/* get the viewport size */
	vp_info.vx = vxdim = wp->allocation.width;
	vp_info.vy = vydim = wp->allocation.height;

	if ( verbose_opt )
	    printf ( "Configure event %d (%d, %d)\n", config_count++, vxdim, vydim );

	/* Avoid memory leak */
	if ( vp_info.pixels )
	    gdk_pixmap_unref ( vp_info.pixels );

	vp_info.pixels = gdk_pixmap_new ( wp->window, vxdim, vydim, -1 );

	pixmap_redraw ();

	return TRUE;
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

	/* viewport center */
	vxcent = vp_info.vx / 2;
	vycent = vp_info.vy / 2;

	printf ( "Orig position (lat/long) %.4f %.4f\n",
		cur_pos.lat_deg, cur_pos.long_deg );

	if ( cur_pos.maplet ) {
	    mxdim = cur_pos.maplet->xdim;
	    mydim = cur_pos.maplet->ydim;
	} else {
	    /* allow mouse motion if over unmapped area */
	    mxdim = 410;	/* close */
	    mydim = 256;
	}

	/* We break a 7.5 minute quadrangle (1/8 of a degree)
	 * into 10 maplets in latitude, (5 in longitude).
	 */
	x_pixel_scale = 1.0 / (8.0 * 5.0 * (double)mxdim);
	y_pixel_scale = 1.0 / (8.0 * 10.0 * (double)mydim);

	dlat  = (event->y - (double)vycent) * y_pixel_scale;
	dlong = (event->x - (double)vxcent) * x_pixel_scale;
	printf ( "Delta position (lat/long) %.4f %.4f\n",
		dlat, dlong );

	/* Make location of the mouse click be the current position */
	cur_pos.lat_deg -= dlat;
	cur_pos.long_deg -= dlong;

	printf ( "New position (lat/long) %.4f %.4f\n",
		cur_pos.lat_deg, cur_pos.long_deg );

	/* redraw on the new center */
	pixmap_redraw ();

	/* put the new pixmap on the screen */
	pixmap_expose ( 0, 0, vp_info.vx, vp_info.vy );


	return TRUE;
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

	while ( argc-- ) {
	    p = *argv++;
	    if ( strcmp ( p, "-v" ) == 0 )
	    	verbose_opt = 1;
	}

	archive_init ( topo_archives, verbose_opt );

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

	syscm = gdk_colormap_get_system ();

	cur_pos.lat_deg = dms2deg ( 37, 1, 0 );
	cur_pos.long_deg = dms2deg ( 118, 31, 0 );
	/* */
	set_series ( &cur_pos, S_STATE );
	set_series ( &cur_pos, S_24K );

	vp_info.vx = 800;
	vp_info.vy = 800;
	gtk_drawing_area_size ( GTK_DRAWING_AREA(vp_info.da), vp_info.vx, vp_info.vy );

	gtk_widget_show ( vp_info.da );

	if ( verbose_opt )
	    printf ( "single maplet size: %d by %d\n", w, h );

	gtk_main ();

	return 0;
}

/* THE END */
