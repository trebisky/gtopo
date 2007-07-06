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
 * version 0.6 - try to get mouse events 7/6/2007
 */

int verbose_opt = 0;

double cur_lat_deg;
double cur_long_deg;

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
void build_tpq_index ( char * );
GdkPixbuf *load_tpq_maplet ( char *, int, int );

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
	int w, h;
	int mw, mh;

	w = wp->allocation.width;
	h = wp->allocation.height;

	if ( verbose_opt )
	    printf ( "Configure event %d (%d, %d)\n", config_count++, w, h );

	/* Avoid memory leak */
	if ( main_pixels )
	    gdk_pixmap_unref ( main_pixels );

	main_pixels = gdk_pixmap_new ( wp->window, w, h, -1 );

	/* clear the whole pixmap to white */
	gdk_draw_rectangle ( main_pixels, wp->style->white_gc, TRUE, 0, 0, w, h );

	mw = gdk_pixbuf_get_width ( map_buf[0] );
	mh = gdk_pixbuf_get_height ( map_buf[0] );

	draw_maplet ( map_buf[0], 0, 0 );
	draw_maplet ( map_buf[1], mw, 0 );
	draw_maplet ( map_buf[2], 0, mh );
	draw_maplet ( map_buf[3], mw, mh );

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

	cur_lat_deg = dms2deg ( 37, 1, 0 );
	cur_long_deg = dms2deg ( 118, 31, 0 );
	printf ( "LL %.4f %.4f\n", cur_lat_deg, cur_long_deg );

	tpq_path = lookup_quad ( cur_lat_deg, cur_long_deg );
	if ( ! tpq_path )
	    error ("Cannot find your quad!\n", "" );

	map_buf[0] = load_tpq_maplet ( tpq_path, 2, 0 );
	map_buf[1] = load_tpq_maplet ( tpq_path, 3, 0 );

	map_buf[2] = load_tpq_maplet ( tpq_path, 2, 1 );
	map_buf[3] = load_tpq_maplet ( tpq_path, 3, 1 );

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

/* ---------------------------------------------------------------- */
/*  TPQ file handling stuff
/* ---------------------------------------------------------------- */

#define TPQ_HEADER_SIZE	1024

/* We really see just 50 in the TPQ files we process,
 * but allocate a bit more room, for no particular reason.
 */
#define TPQ_MAX_MAPLETS	200

struct tpq_index_e {
	long	offset;
	long	size;
};

struct tpq_index_e tpq_index[TPQ_MAX_MAPLETS];
char tpq_index_filename[100];
int num_index = 0;

/* For a q-series TPQ file representing a 7.5 minute quadrangle,
 * there are 50 maplets within the file.  These begin in the upper
 * left (the far NW) and then move W to E for 5 maplets
 * then down just like you read lines of text in a book.
 * There are 10 maplets N to S
 *
 * For a F series TPQ file representing a 5x5 degree area,
 * there are 100 maplets in the file in a 10x10 pattern,
 * the same order as above.
 */

#define BUFSIZE	1024

#ifdef BIG_ENDIAN_HACK
#define JPEG_SOI_TAG	0xffd8
#else
#define JPEG_SOI_TAG	0xd8ff
#endif

void
wonk_index ( int fd, long *info )
{
	int i;
	unsigned short tag;
	int num_jpeg;

	num_index = (info[0] - 1024)/4 - 4;
	/*
	printf ( "table entries: %d\n", num_index );
	*/
	if ( num_index > TPQ_MAX_MAPLETS )
	    error ("Whoa! too many maplets\n", "" );

	num_jpeg = 0;
	for ( i=0; i<num_index; i++ ) {
	    tpq_index[i].offset = info[i];
	    tpq_index[i].size = info[i+1] - info[i];
	    lseek ( fd, info[i], SEEK_SET );
	    if ( read( fd, &tag, sizeof(tag) ) != sizeof(tag) )
		error ( "tag read fails!\n", "" );
	    if ( tag != JPEG_SOI_TAG )
		break;
	    num_jpeg++;
	}

	num_index = num_jpeg;
	/*
	printf ( "jpeg table entries: %d\n", num_index );
	*/
}

void
build_tpq_index ( char *name )
{
	int fd;
	char buf[BUFSIZE];

	if ( num_index > 0 && strcmp ( tpq_index_filename, name ) == 0 )
	    return;

	fd = open ( name, O_RDONLY );
	if ( fd < 0 )
	    error ( "Cannot open: %s\n", name );

	/* skip header */
	if ( read( fd, buf, TPQ_HEADER_SIZE ) != TPQ_HEADER_SIZE )
	    error ( "Bogus TPQ file size - 1\n", "" );
	if ( read( fd, buf, BUFSIZE ) != BUFSIZE )
	    error ( "Bogus TPQ file size - 2\n", "" );

	wonk_index ( fd, (long *) buf );
	strcpy ( tpq_index_filename, name );

	close ( fd );
}

char *tmpname = "gtopo.tmp";

/* Pull a maplet out of a TPQ file.
 * returns NULL if trouble.
 */
GdkPixbuf *
load_tpq_maplet ( char *name, int x_index, int y_index )
{
	char buf[BUFSIZE];
	int fd, ofd;
	int size;
	int nw;
	int nlong;
	int who;
	GdkPixbuf *rv;

	build_tpq_index ( name );

	nlong = 5;
	if ( num_index == 100 )
	    nlong = 10;

	who = y_index * nlong + x_index;

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
