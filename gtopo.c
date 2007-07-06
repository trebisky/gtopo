#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

/* Tom Trebisky  MMT Observatory, Tucson, Arizona
 * version 0.1 - really just a JPG file display gizmo, but
 *	a starting point for what is to follow.  Did not get
 *	any bits displayed until I moved the draw calls into
 *	the expose event handler  7/2/2007
 * version 0.2 - draw into a pixmap which gets copied to
 *	the drawing area on an expose event.  7/3/2007
 * version 0.3 - actually pull a single maplet out of
 *	a TPQ file and display it.  7/5/2007
 */

char *tpq_file = "../q36117h8.tpq";

GdkColormap *syscm;

/* The drawing area */
GtkWidget *main_da;
/* A pixmap backup of the drawing area */
GdkPixmap *main_pixels = NULL;

/* One maplet (will change) */
GdkPixbuf *map_buf;

/* Prototypes ..........
 */
void build_tpq_index ( char * );
GdkPixbuf *load_tpq_maplet ( char *name, int who );

void
error ( char *msg, char *arg )
{
	printf ( msg, arg );
	exit ( 1 );
}

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

	/* maplet 2 is the old topo0301.jpg */
	map_buf = load_tpq_maplet ( tpq_file, 2 );

	w = gdk_pixbuf_get_width ( map_buf );
	h = gdk_pixbuf_get_height ( map_buf );

	/* initially big enough for one maplet */
	gtk_drawing_area_size ( GTK_DRAWING_AREA(main_da), w, h );

	gtk_widget_show ( main_da );

	printf ( "%d by %d\n", w, h );

	gtk_main ();

	return 0;
}

/* ---------------------------------------------------------------- */
/*  TPQ file handling stuff
/* ---------------------------------------------------------------- */

#define TPQ_HEADER_SIZE	1024

/* We really see just 50 in the TPQ files we process,
 * but allocate a bit more room, for no particular reason.
 */
#define TPQ_NUM_MAPLETS	64

struct tpq_index_e {
	long	offset;
	long	size;
};

struct tpq_index_e tpq_index[TPQ_NUM_MAPLETS];
char tpq_index_filename[100];
int num_index = 0;

/* For a TPQ file representing a 7.5 minute quadrangle, there
 * are 50 maplets within the file.  These begin in the upper
 * left (the far NW) and then move W to E for 5 maplets
 * then down just like you read lines of text in a book.
 * There are 10 maplets N to S
 */

#define BUFSIZE	1024

void
wonk_index ( long *info )
{
	int i;

	num_index = (info[0] - 1024) - 4;
	for ( i=0; i<num_index; i++ ) {
	    tpq_index[i].offset = info[i];
	    tpq_index[i].size = info[i+1] - info[i];
	}
}

void
build_tpq_index ( char *name )
{
	int fd;
	char buf[BUFSIZE];

	fd = open ( name, O_RDONLY );
	if ( fd < 0 )
	    error ( "Cannot open: %s\n", name );

	/* skip header */
	if ( read( fd, buf, TPQ_HEADER_SIZE ) != TPQ_HEADER_SIZE )
	    error ( "Bogus TPQ file size - 1\n", "" );
	if ( read( fd, buf, BUFSIZE ) != BUFSIZE )
	    error ( "Bogus TPQ file size - 2\n", "" );

	wonk_index ( (long *) buf );
	strcpy ( tpq_index_filename, name );

	close ( fd );
}

char *tmpname = "gtopo.tmp";

/* Pull a maplet out of a TPQ file.
 * returns NULL if trouble.
 */
GdkPixbuf *
load_tpq_maplet ( char *name, int who )
{
	char buf[BUFSIZE];
	int fd, ofd;
	int size;
	int nw;
	GdkPixbuf *rv;

	if ( num_index < 1 )
	    build_tpq_index ( name );
	if ( strcmp ( tpq_index_filename, name ) != 0 )
	    build_tpq_index ( name );

	/* open a temp file for R/W */
	ofd = open ( tmpname, O_CREAT | O_TRUNC | O_WRONLY, 0600 );
	if ( ofd < 0 )
	    error ( "Cannot open tempfile: %s\n", tmpname );

	fd = open ( name, O_RDONLY );
	if ( fd < 0 )
	    error ( "Cannot open: %s\n", name );

	lseek ( fd, tpq_index[who].offset, SEEK_SET );
	size = tpq_index[who].size;

	while ( size > 0 ) {
	    if ( read( fd, buf, BUFSIZE ) != BUFSIZE )
		error ( "TPQ file read error\n", name );
	    nw = size < BUFSIZE ? size : BUFSIZE;
	    if ( write ( ofd, buf, nw ) != nw )
		error ( "tmp file write error\n", tmpname );
	    size -= nw;
	}

	close ( fd );
	close ( ofd );

	rv = gdk_pixbuf_new_from_file ( tmpname, NULL );
	if ( ! rv )
	    printf ("Cannot open %s\n", tmpname );

	remove ( tmpname );

	return rv;
}

/* THE END */
