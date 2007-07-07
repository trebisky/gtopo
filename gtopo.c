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

/* The drawing area */
GtkWidget *main_da;
/* A pixmap backup of the drawing area */
GdkPixmap *main_pixels = NULL;

/* The maplets */
GdkPixbuf *map_buf[4];

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

static int expose_count = 0;

gint
expose_handler ( GtkWidget *wp, GdkEventExpose *ep, gpointer data )
{
	/*
	if ( expose_count < 4 )
	    printf ( "Expose event %d\n", expose_count++ );
	    */

	gdk_draw_pixmap ( wp->window,
		wp->style->fg_gc[GTK_WIDGET_STATE(wp)],
		main_pixels,
		ep->area.x, ep->area.y,
		ep->area.x, ep->area.y,
		ep->area.width, ep->area.height );

	return FALSE;
}

/* This gets called when the drawing area gets created or resized.
 * (and after every one of these you get an expose event).
 */
static int config_count = 0;

/* Draw a pixbuf onto a drawable.
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
	gdk_draw_pixbuf ( main_pixels, NULL, map,
		SRC_X, SRC_Y, x, y, -1, -1,
		GDK_RGB_DITHER_NONE, 0, 0 );
}

gint
configure_handler ( GtkWidget *wp, GdkEvent *event, gpointer data )
{
	int vxdim, vydim;
	int mxdim, mydim;
	int vxcent, vycent;
	int offx, offy;

	/* get the viewport size */
	vxdim = wp->allocation.width;
	vydim = wp->allocation.height;

	/* viewport center */
	vxcent = vxdim / 2;
	vycent = vydim / 2;

	if ( verbose_opt )
	    printf ( "Configure event %d (%d, %d)\n", config_count++, vxdim, vydim );

	/* Avoid memory leak */
	if ( main_pixels )
	    gdk_pixmap_unref ( main_pixels );

	main_pixels = gdk_pixmap_new ( wp->window, vxdim, vydim, -1 );

	/* clear the whole pixmap to white */
	gdk_draw_rectangle ( main_pixels, wp->style->white_gc, TRUE, 0, 0, vxdim, vydim );

	/* get the maplet size */
	mxdim = gdk_pixbuf_get_width ( map_buf[0] );
	mydim = gdk_pixbuf_get_height ( map_buf[0] );

	offx = cur_pos.maplet_fx * mxdim;
	offy = cur_pos.maplet_fy * mydim;;
	printf ( "Maplet offsets: %d %d\n", offx, offy );

	draw_maplet ( map_buf[0], vxcent-offx, vycent-offy );

#ifdef notdef
	draw_maplet ( map_buf[0], 0, 0 );
	draw_maplet ( map_buf[1], mxdim, 0 );
	draw_maplet ( map_buf[2], 0, mydim );
	draw_maplet ( map_buf[3], mxdim, mydim );
#endif

	/* mark center */
	gdk_draw_rectangle ( main_pixels, wp->style->black_gc, TRUE, vxcent-1, vycent-1, 3, 3 );

	return TRUE;
}

gint
mouse_handler ( GtkWidget *wp, GdkEventButton *event, gpointer data )
{
	int button;
	float x, y;

	button = event->button;
	x = event->x;
	y = event->y;
	printf ( "Button event %d %.3f %.3f\n", button, x, y );
	    
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

	main_da = gtk_drawing_area_new ();
	gtk_box_pack_start ( GTK_BOX(vb), main_da, TRUE, TRUE, 0 );

	gtk_signal_connect ( GTK_OBJECT(main_da), "expose_event",
			GTK_SIGNAL_FUNC(expose_handler), NULL );
	gtk_signal_connect ( GTK_OBJECT(main_da), "configure_event",
			GTK_SIGNAL_FUNC(configure_handler), NULL );

	/* We never see the release event, unless we add the press
	 * event to the mask.
	 */
	gtk_signal_connect ( GTK_OBJECT(main_da), "button_release_event",
			GTK_SIGNAL_FUNC(mouse_handler), NULL );
	gtk_widget_add_events ( GTK_WIDGET(main_da), GDK_BUTTON_RELEASE_MASK );
	gtk_widget_add_events ( GTK_WIDGET(main_da), GDK_BUTTON_PRESS_MASK );

	syscm = gdk_colormap_get_system ();

	cur_pos.lat_deg = dms2deg ( 37, 1, 0 );
	cur_pos.long_deg = dms2deg ( 118, 31, 0 );

	tpq_path = lookup_quad ( &cur_pos );
	if ( ! tpq_path )
	    error ("Cannot find your quad!\n", "" );

	xm = cur_pos.x_maplet;
	ym = cur_pos.y_maplet;
	printf ( "x,y maplet = %d %d\n", xm, ym );

	map_buf[0] = load_tpq_maplet ( tpq_path, xm, ym );

#ifdef notdef
	/* what this will do is given the lat and long above,
	 * will display a 2x2 maplet section that contains
	 * the coordinate, but will not cross map sheets.
	 */

	if ( xm == 4 ) xm--;
	if ( ym == 9 ) ym--;

	map_buf[0] = load_tpq_maplet ( tpq_path, xm, ym );
	map_buf[1] = load_tpq_maplet ( tpq_path, xm+1, ym );

	map_buf[2] = load_tpq_maplet ( tpq_path, xm, ym+1 );
	map_buf[3] = load_tpq_maplet ( tpq_path, xm+1, ym+1 );
#endif

	w = gdk_pixbuf_get_width ( map_buf[0] );
	h = gdk_pixbuf_get_height ( map_buf[0] );

	/* big enough for four maplets */
	gtk_drawing_area_size ( GTK_DRAWING_AREA(main_da), w*2, h*2 );

	gtk_widget_show ( main_da );

	if ( verbose_opt )
	    printf ( "single maplet size: %d by %d\n", w, h );

	gtk_main ();

	return 0;
}

/* THE END */
