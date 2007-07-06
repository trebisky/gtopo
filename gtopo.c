#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>

/* Tom Trebisky  MMT Observatory, Tucson, Arizona
 * version 0.1 - really just a JPG file display gizmo, but
 *	a starting point for what is to follow.  Did not get
 *	any bits displayed until I moved the draw calls into
 *	the expose event handler  7/2/2007
 * version 0.2 - draw into a pixmap which gets copied to
 *	the drawing area on an expose event.  7/3/2007
 */

GdkPixbuf *load_maplet ( char * );

char *gfile = "topo0301.jpg";

GdkColormap *syscm;

/* The drawing area */
GtkWidget *main_da;
/* A pixmap backup of the drawing area */
GdkPixmap *main_pixels = NULL;

/* One maplet (will change) */
GdkPixbuf *map_buf;

gint
destroy_handler ( GtkWidget *w, gpointer data )
{
	gtk_main_quit ();
	return FALSE;
}

static int expose_count = 0;

gint
expose_handler ( GtkWidget *wp, GdkEventExpose *ep )
{
	if ( expose_count < 4 )
	    printf ( "Expose event %d\n", expose_count++ );

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

#define DEST_X	0
#define DEST_Y	0

void
draw_maplet ( GdkPixbuf *map )
{
	gdk_draw_pixbuf ( main_pixels, NULL, map,
		SRC_X, SRC_Y, DEST_X, DEST_Y, -1, -1,
		GDK_RGB_DITHER_NONE, 0, 0 );
}

gint
configure_handler ( GtkWidget *wp, gpointer data )
{
	int w, h;

	w = wp->allocation.width;
	h = wp->allocation.height;

	printf ( "Configure event %d (%d, %d)\n", config_count++, w, h );

	/* Avoid memory leak */
	if ( main_pixels )
	    gdk_pixmap_unref ( main_pixels );

	main_pixels = gdk_pixmap_new ( wp->window, w, h, -1 );

	/* clear the whole pixmap to white */
	gdk_draw_rectangle ( main_pixels, wp->style->white_gc, TRUE, 0, 0, w, h );

	draw_maplet ( map_buf );

	return TRUE;
}

int
main ( int argc, char **argv )
{
	GtkWidget *main_window;
	GtkWidget *vb;
	int w, h;

	gtk_init ( &argc, &argv );

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

	syscm = gdk_colormap_get_system ();

	map_buf = load_maplet ( gfile );
	w = gdk_pixbuf_get_width ( map_buf );
	h = gdk_pixbuf_get_height ( map_buf );

	/* initially big enough for one maplet */
	gtk_drawing_area_size ( GTK_DRAWING_AREA(main_da), w, h );

	gtk_widget_show ( main_da );

	printf ( "%d by %d\n", w, h );

	gtk_main ();

	return 0;
}

/* Open up a .jpg file or some such and load it
 * into a pixmap.
 * returns NULL if trouble.
 */
GdkPixbuf *
load_maplet_1 ( char *name )
{
	GdkPixbuf *rv;

	rv = gdk_pixbuf_new_from_file ( name, NULL );
	if ( ! rv )
	    printf ("Cannot open %s\n", name );

	return rv;
}

void
error ( char *msg, char *arg )
{
	printf ( msg, arg );
	exit ( 1 );
}

/* Basically this code was stolen from
 * archive/gtk+-2.10.12/gdk-pixbuf/gdk-pixbuf-io.c
 * and hacked on.
 */
GdkPixbuf *
load_maplet ( char *name )
{
	GdkPixbuf *rv;
	/* This is in /u1/archive/gtk+-2.10.12/gdk-picbuf/gdk-pixbuf-io.h
	GdkPixbufModule *im;
	 */
	void *im;
	FILE *f;
	int size;
	guchar buffer[1024];

	rv = NULL;

	f = g_fopen ( name, "rb" );
	if ( ! f ) {
	    error ( "Cannot open maplet: %s\n", name );
	    return rv;
	}

	size = fread ( buffer, 1, sizeof(buffer), f );
	fseek ( f, 0, SEEK_SET );
	if ( size == 0 ) {
	    error ( "Empty maplet file: %s\n", name );
	    fclose ( f );
	    return rv;
	}

	im = _gdk_pixbuf_get_module ( buffer, size, name, error );
	if ( im == NULL ) {
	    error ( "Weird file type for: %s\n", name );
	    fclose ( f );
	    return rv;
	}

#ifdef notdef
	if ( im->module == NULL && ! _gdk_pixbuf_load_module (im, NULL)) {
	    error ( "No module for: %s\n", name );
	    fclose ( f );
	    return rv;
	}
#endif

	rv = _gdk_pixbuf_generic_image_load ( im, f, NULL );
	fclose (f);
	return rv;
}

/* THE END */
