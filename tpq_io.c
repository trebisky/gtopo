#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "gtopo.h"

/* Tom Trebisky  MMT Observatory, Tucson, Arizona
 */
/* ---------------------------------------------------------------- */
/*  TPQ file handling stuff
/* ---------------------------------------------------------------- */

#define TPQ_HEADER_SIZE	1024

/* We see 3 of these embedded in the header */
struct _tpq_file {
	char ext[4];		/* ".jpg" or ".png" */
	long _xxx[2];
	long nlat;
	long nlong;
	char _xx[12];
};

struct _tpq_header {
	char _pad1[256];
	char name[128];		/* quadrangle name */
	char state[32];		/* typically "AZ" */
	char source[32];	/* typically "USGS" */
	char year1[4];		/* typically "1994" */
	char year2[4];		/* typically "1994" */
	char countour[8];	/* typically "20 ft" */
	char _pad2[16];		/* ".tpq" "DAT" ... */
	struct _tpq_file maplet;
	char info[88];
	struct _tpq_file png1;
	char _pad3[28];
	struct _tpq_file png2;
	char _pad4[332];
} tpq_header;

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

	printf ( "TPQ header size: %d\n", sizeof(tpq_header) );

	/* read header */
	if ( read( fd, &tpq_header, TPQ_HEADER_SIZE ) != TPQ_HEADER_SIZE )
	    error ( "Bogus TPQ file size - 1\n", "" );

	printf ( "TPQ file for %s quadrangle: %s\n", tpq_header.state, tpq_header.name );
	printf ( "TPQ file maplet counts lat/long: %d %d\n", tpq_header.maplet.nlat, tpq_header.maplet.nlong );

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
load_tpq_maplet ( char *name, int index )
{
	char buf[BUFSIZE];
	int fd, ofd;
	int size;
	int nw;
	int nlong;
	GdkPixbuf *rv;

	build_tpq_index ( name );

	/* open a temp file for R/W */
	ofd = open ( tmpname, O_CREAT | O_TRUNC | O_WRONLY, 0600 );
	if ( ofd < 0 )
	    error ( "Cannot open tempfile: %s\n", tmpname );

	fd = open ( name, O_RDONLY );
	if ( fd < 0 )
	    error ( "Cannot open: %s\n", name );

	lseek ( fd, tpq_index[index].offset, SEEK_SET );
	size = tpq_index[index].size;

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
