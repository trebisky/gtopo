#include <gtk/gtk.h>

/* Tom Trebisky  MMT Observatory, Tucson, Arizona
 * version 0.1 - really just a JPG file display gizmo, but
 *	a starting point for what is to follow.  Did not get
 *	any bits displayed until I moved the draw calls into
 *	the expose event handler  7/2/2007
 */

char *gfile = "topo0301.jpg";

GdkColormap *syscm;

GtkWidget *main_da;
GdkPixbuf *main_pix;

gint
destroy_handler ( GtkWidget *w, gpointer data )
{
	gtk_main_quit ();
	return FALSE;
}

/* Draw a pixbuf onto a drawable.
 * The second argument is a gc (graphics context), which only needs to
 * be non-null if you expect clipping -- it sets foreground and background colors.
 * Last 3 arguments are dithering.
 */

#define SRC_X	0
#define SRC_Y	0

#define DEST_X	0
#define DEST_Y	0

void
draw_pix ( GtkWidget *da, GdkPixbuf *pix )
{
	gdk_draw_pixbuf ( GDK_DRAWABLE(da->window), NULL, pix,
		SRC_X, SRC_Y, DEST_X, DEST_Y, -1, -1,
		GDK_RGB_DITHER_NONE, 0, 0 );
}

static int expose_count = 0;

gint
expose_handler ( GtkWidget *w, gpointer data )
{
	printf ( "Expose event %d\n", expose_count++ );
	draw_pix ( main_da, main_pix );
}

/* Open up a .jpg file or some such and load it
 * into a pixmap.
 * See /gtk+-2.10.12/gdk-pixbuf/gdk-pixbuf-io.c
 * returns NULL if trouble.
 */
GdkPixbuf *
load_file ( char *name )
{
	GdkPixbuf *rv;

	rv = gdk_pixbuf_new_from_file ( name, NULL );
	if ( ! rv )
	    printf ("Cannot open %s\n", name );

	return rv;
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

	syscm = gdk_colormap_get_system ();

	main_pix = load_file ( gfile );
	w = gdk_pixbuf_get_width ( main_pix );
	h = gdk_pixbuf_get_height ( main_pix );
	gtk_drawing_area_size ( GTK_DRAWING_AREA(main_da), w, h );

	gtk_widget_show ( main_da );

	printf ( "%d by %d\n", w, h );

	gtk_main ();

	return 0;
}

/* THE END */
