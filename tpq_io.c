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

extern struct topo_info info;

/* Using this is by FAR the way to go, and eliminates all the temp file
 * baloney
 */
#define LOADER

/* ---------------------------------------------------------------- */
/*  TPQ file handling stuff					*/
/* ---------------------------------------------------------------- */

#define TPQ_HEADER_SIZE	1024

/* We see 3 of these embedded in the header */
struct tpq_file {
	char ext[4];		/* ".jpg" or ".png" */
	long _xxx[2];
	long nlong;
	long nlat;
	char _xx[12];
};

struct tpq_header {
	long version;		/* maybe, I always see 1 */
	double west_long;
	double north_lat;
	double east_long;
	double south_lat;
	char topo_name[12];	/* string TOPO! */
	char _pad1[208];

	char quad_name[128];	/* quadrangle name */
	char state[32];		/* typically "AZ" */
	char source[32];	/* typically "USGS" */
	char year1[4];		/* typically "1994" */
	char year2[4];		/* typically "1994" */
	char countour[8];	/* typically "20 ft" */
	char _pad2[16];		/* ".tpq" "DAT" ... */
	struct tpq_file maplet;
	char info[88];
	struct tpq_file png1;
	char _pad3[28];
	struct tpq_file png2;
	char _pad4[332];
};

/* The above is the format of the TPQ file as found on disk.
 */

/* The biggest thing I have seen yet was in the california
 * level 3 map (22x20), which has 440 jpeg maplets and 767
 * table entries.
#define TPQ_MAX_MAPLETS	800
#define INDEX_BUFSIZE	3200
 */

/* XXX - this is an awful header read to just be able
 to handle this one odd case properly, so should read in
 say 1K pieces and build up the index as needed, not one
 giant read.  The index array will need to be big enough
 though, but 1600 entries  would do ...
 For now, this works by brute force.
 */
#define TPQ_MAX_MAPLETS	7000
#define INDEX_BUFSIZE  32000

/* For a q-series TPQ file representing a 7.5 minute quadrangle,
 * there are 50 maplets within the file.  These begin in the upper
 * left (the far NW) and then move W to E for 5 maplets
 * then down just like you read lines of text in a book.
 * There are 10 maplets N to S
 *
 * For a F series TPQ file representing a 5x5 degree area,
 * there are 100 maplets in the file in a 10x10 pattern,
 * the same order as above.
 * (For the California level 3, it is one huge TPQ file
 *  with 440 maplets as 22x20). 440x4 is 1760 bytes.
 * (The level 2 in the Nevada set for the entire US takes
 *  the cake with 6133 maps !!  ( US1_MAP2.tpq )
 *  This boils down to 1534 maps (59x26)
 */

#ifdef BIG_ENDIAN_HACK
#define JPEG_SOI_TAG	0xffd8
#else
#define JPEG_SOI_TAG	0xd8ff
#endif

static void
build_index ( struct tpq_info *tp, int fd, long *info )
{
	int i;
	unsigned short tag;
	int num_index;
	int num_jpeg;
	struct tpq_index_e proto_index[TPQ_MAX_MAPLETS];
	struct tpq_index_e *index;
	char ebuf[16];

	num_index = (info[0] - 1024)/4 - 4;
	if ( num_index > TPQ_MAX_MAPLETS ) {
	    sprintf ( ebuf, "%d", num_index );
	    error ("Whoa! too many maplets %s\n", ebuf );
	}

	num_jpeg = 0;
	for ( i=0; i<num_index; i++ ) {
	    proto_index[i].offset = info[i];
	    proto_index[i].size = info[i+1] - info[i];

	    lseek ( fd, info[i], SEEK_SET );
	    if ( read( fd, &tag, sizeof(tag) ) != sizeof(tag) )
		error ( "tag read fails!\n", "" );
	    if ( tag != JPEG_SOI_TAG )
		break;
	    num_jpeg++;
	}

        index = (struct tpq_index_e *) malloc ( num_jpeg * sizeof(struct tpq_index_e) );
        if ( ! index )
            error ("build_index, out of mem\n", "" );

	memcpy ( index, proto_index, num_jpeg * sizeof(struct tpq_index_e) );
	tp->index = index;
	tp->index_size = num_jpeg;
}

static int
read_tpq_header ( struct tpq_info *tp, int fd, int verbose )
{
	struct tpq_header tpq_header;

	if ( sizeof(struct tpq_header) != TPQ_HEADER_SIZE )
	    error ( "Malformed TPQ file header structure (my bug)\n", "" );

	/* read header */
	if ( read( fd, &tpq_header, TPQ_HEADER_SIZE ) != TPQ_HEADER_SIZE )
	    return 0;

	if ( verbose ) {
	    printf ( "TPQ file for %s quadrangle: %s\n", tpq_header.state, tpq_header.quad_name );
	    printf ( "TPQ file maplet counts long/lat: %d %d\n", tpq_header.maplet.nlong, tpq_header.maplet.nlat );
	    printf ( "TPQ file long range: %.3f %3f\n", tpq_header.west_long, tpq_header.east_long );
	    printf ( "TPQ file lat range: %.3f %3f\n", tpq_header.south_lat, tpq_header.north_lat );
	    printf ( "TPQ maplet size: %.5f %.5f\n",
		(tpq_header.east_long-tpq_header.west_long) / tpq_header.maplet.nlong,
		(tpq_header.north_lat-tpq_header.south_lat) / tpq_header.maplet.nlat );
	}

	tp->state = strhide ( tpq_header.state );
	tp->quad = strhide ( tpq_header.quad_name );

	tp->w_long = tpq_header.west_long;
	tp->e_long = tpq_header.east_long;
	tp->n_lat = tpq_header.north_lat;
	tp->s_lat = tpq_header.south_lat;

	tp->lat_count = tpq_header.maplet.nlat;
	tp->long_count = tpq_header.maplet.nlong;

	tp->maplet_lat_deg = (tp->n_lat - tp->s_lat) / tp->lat_count;
	tp->maplet_long_deg = (tp->e_long - tp->w_long) / tp->long_count;

	/* Figure out the corner indices of our map */
        tp->sheet_lat = tp->s_lat / tp->maplet_lat_deg;
	tp->sheet_long = - tp->e_long / tp->maplet_long_deg;

	/* What we see so far, and I believe to be an invariant, is the following
	 * sizes of maplets in the given series:
	 *   24K  series = 0.0250 long by 0.0125 lat
	 *  100K  series = 0.0625 long by 0.0625 lat
	 *  500K  series = 0.5000 long by 0.5000 lat
	 *  Atlas series = 1.0000 long by 1.0000 lat
	 *  State series = quite variable
	 *  (AZ = 7x7, CA = 11x10)
	 *  (the new NV set has the entire USA as 4.9167 by 3.25)
	 */
	if ( tp->maplet_long_deg < 0.0350 )
	    tp->series = S_24K;
	else if ( tp->maplet_long_deg < 0.1 )
	    tp->series = S_100K;
	else if ( tp->maplet_long_deg < 0.75 )
	    tp->series = S_500K;
	else if ( tp->maplet_long_deg < 2.0 )
	    tp->series = S_ATLAS;
	else
	    tp->series = S_STATE;

	return 1;
}

static struct tpq_info *tpq_head = NULL;

static struct tpq_info *
tpq_new ( char *path )
{
        struct tpq_info *tp;
	char buf[INDEX_BUFSIZE];
	int fd;

        tp = (struct tpq_info *) malloc ( sizeof(struct tpq_info) );
        if ( ! tp )
            error ("tpq_new, out of mem\n", "" );

	tp->path = strhide(path);

	fd = open ( path, O_RDONLY );
	if ( fd < 0 )
	    return NULL;

	if ( ! read_tpq_header ( tp, fd, info.verbose ) )
	    return NULL;

	if ( read( fd, buf, INDEX_BUFSIZE ) != INDEX_BUFSIZE )
	    return NULL;

	build_index ( tp, fd, (long *) buf );

	close ( fd );

        return tp;
}

struct tpq_info *
tpq_lookup ( char *path )
{
	struct tpq_info *tp;

	for ( tp = tpq_head; tp; tp = tp->next )
	    if ( strcmp(tp->path,path) == 0 )
	    	return tp;

	tp = tpq_new ( path );

	if ( tp ) {
	    tp->next = tpq_head;
	    tpq_head = tp;
	}

	return tp;
}

#ifndef LOADER
static char tmpdir[64];
static char tmpname[128];

static int
temp_file_open ( void )
{
	strcpy ( tmpname, tmpdir );
	strcat ( tmpname, "/" );
	strcat ( tmpname, "gtopo.tmp" );

	return open ( tmpname, O_CREAT | O_TRUNC | O_WRONLY, 0600 );
}

static int
try_temp ( char *dir )
{
    	long magic = 0xdeadbeef;
	long rmagic;
	int tfd;

    	strcpy ( tmpdir, dir );

	strcpy ( tmpname, tmpdir );
	strcat ( tmpname, "/" );
	strcat ( tmpname, "gtopo_probe.tmp" );

	tfd = open ( tmpname, O_CREAT | O_TRUNC | O_RDWR, 0600 );
	if ( tfd < 0 )
	    return -1;

	if ( write ( tfd, &magic, sizeof(long) ) != sizeof(long) ) {
	    close ( tfd );
	    remove ( tmpname );
	    return -2;
	}
	if ( lseek ( tfd, 0, SEEK_SET ) < 0 ) {
	    close ( tfd );
	    remove ( tmpname );
	    return -3;
	}
	rmagic = 0;
	if ( read ( tfd, &rmagic, sizeof(long) ) != sizeof(long) ) {
	    close ( tfd );
	    remove ( tmpname );
	    return -4;
	}
	if ( rmagic != magic ) {
	    close ( tfd );
	    remove ( tmpname );
	    return -5;
	}

	close ( tfd );
	remove ( tmpname );
	return 1;
}

/* Try to figure out the best place to stick the temporary
 * file we need to use (in the current deplorable state of
 * things).  We try several places before giving up.
 */
int
temp_init ( void )
{
	int tfd;

	if ( try_temp ( "/tmp" ) > 0 )
	    return 1;
	/* XXX - should also try /home/<user> */
	if ( try_temp ( "." ) > 0 )
	    return 1;
	return 0;
}
#endif

#define BUFSIZE	1024

/* Pull a maplet out of a TPQ file.
 * expects mp->tpq_index and mp->tpq_path
 */
int
load_tpq_maplet ( struct maplet *mp )
{
	char buf[BUFSIZE];
	int fd, ofd;
	int size;
	int nw;
	int nlong;
	struct tpq_info *tp;
	int x_index, y_index;
	GdkPixbufLoader *loader;
	GError *load_error = NULL;

	tp = tpq_lookup ( mp->tpq_path );
	if ( ! tp )
	    return 0;

	mp->tpq = tp;

	/* XXX - Some hackery for when we are just sniffing at file
	 * contents, so that we read a maplet near the geometric
	 * center of the file.
	 */
	if ( mp->tpq_index < 0 ) {
	    x_index = tp->long_count / 2;
	    y_index = tp->lat_count / 2;
	    mp->sheet_index_long = tp->long_count - x_index - 1;
	    mp->sheet_index_lat = tp->lat_count - y_index - 1;
	    mp->world_index_long = tp->sheet_long + mp->sheet_index_long;
	    mp->world_index_lat = tp->sheet_lat + mp->sheet_index_lat;
	    mp->tpq_index = y_index * tp->long_count + x_index;
	}

	if ( mp->tpq_index < 0 || mp->tpq_index >= tp->index_size )
	    return 0;

#ifdef LOADER
	fd = open ( mp->tpq_path, O_RDONLY );
	if ( fd < 0 )
	    return 0;

	lseek ( fd, tp->index[mp->tpq_index].offset, SEEK_SET );
	size = tp->index[mp->tpq_index].size;

	loader = gdk_pixbuf_loader_new_with_type ( "jpeg", &load_error );

	while ( size > 0 ) {
	    nw = size < BUFSIZE ? size : BUFSIZE;
	    if ( read( fd, buf, nw ) != nw )
		error ( "TPQ file read error\n", mp->tpq_path );
	    gdk_pixbuf_loader_write ( loader, buf, nw, &load_error );
	    size -= nw;
	}

	close ( fd );

	/* The following two calls work in either order */
	gdk_pixbuf_loader_close ( loader, &load_error );
	mp->pixbuf = gdk_pixbuf_loader_get_pixbuf ( loader );
#else
	/* open a temp file for R/W */
	ofd = temp_file_open ();
	if ( ofd < 0 )
	    error ( "Cannot open tempfile: %s\n", tmpname );

	fd = open ( mp->tpq_path, O_RDONLY );
	if ( fd < 0 )
	    return 0;

	lseek ( fd, tp->index[mp->tpq_index].offset, SEEK_SET );
	size = tp->index[mp->tpq_index].size;

	while ( size > 0 ) {
	    nw = size < BUFSIZE ? size : BUFSIZE;
	    if ( read( fd, buf, nw ) != nw )
		error ( "TPQ file read error\n", mp->tpq_path );
	    if ( write ( ofd, buf, nw ) != nw )
		error ( "tmp file write error\n", tmpname );
	    size -= nw;
	}

	close ( fd );
	close ( ofd );

	mp->pixbuf = gdk_pixbuf_new_from_file ( tmpname, NULL );
	remove ( tmpname );
#endif

	if ( ! mp->pixbuf && info.verbose ) {
	    printf ("Cannot get pixbuf from %s (%d)\n", mp->tpq_path, mp->tpq_index );
	    return 0;
	}

	return 1;
}

/* THE END */
