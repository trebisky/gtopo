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

extern struct topo_info info;

/* Using this is by FAR the way to go, and eliminates all the temp file
 * baloney
 */
#define LOADER

/* ---------------------------------------------------------------- */
/*  TPQ file handling stuff					*/
/* ---------------------------------------------------------------- */

#define TPQ_HEADER_SIZE	1024
#define TPQ_FILE_SIZE	32

#define NEW_HEADER

#ifndef NEW_HEADER
/* XXX - 8-28-2007, this would not build correctly on a 64 bit
 * intel target.  It turns out that long is 8 bytes on these
 * machines.  Since this structure is a template for what is
 * on disk, a long MUST be a 4 byte integer.  int works for now.
 * ALSO, we have to introduce the packed attribute thing, since
 * gcc now wants to put holes in this structure to place the
 * doubles on 8-byte boundaries.
 */
typedef int INT4;

/* We see 3 of these embedded in the header */
struct tpq_file {
	char ext[4];		/* ".jpg" or ".png" */
	INT4 _xxx[2];
	INT4 nlong;
	INT4 nlat;
	char _xx[12];
};

struct tpq_header {
	INT4  version;		/* maybe, I always see 1 */
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
}__attribute__((packed));

/* The above is the format of the TPQ file as found on disk.
 */
#endif

#ifdef notdef
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
 And now, this is OBSOLETE !!
 */
#define TPQ_MAX_MAPLETS	7000
#define INDEX_BUFSIZE  32000
static void
build_index_OLD ( struct tpq_info *tp, int fd, long *info )
{
	int i;
	unsigned short tag;
	int num_index;
	int num_jpeg;
	struct tpq_index_e proto_index[TPQ_MAX_MAPLETS];
	struct tpq_index_e *index;

	num_index = (info[0] - 1024)/4 - 4;
	if ( num_index > TPQ_MAX_MAPLETS )
	    error ("Whoa! too many maplets %d\n", num_index );

	num_jpeg = 0;
	for ( i=0; i<num_index; i++ ) {
	    proto_index[i].offset = info[i];
	    proto_index[i].size = info[i+1] - info[i];

	    lseek ( fd, info[i], SEEK_SET );
	    if ( read( fd, &tag, sizeof(tag) ) != sizeof(tag) )
		error ( "tag read fails!\n");
	    if ( tag != JPEG_SOI_TAG )
		break;
	    num_jpeg++;
	}

        index = (struct tpq_index_e *) malloc ( num_jpeg * sizeof(struct tpq_index_e) );
        if ( ! index )
            error ("build_index, out of mem\n");

	memcpy ( index, proto_index, num_jpeg * sizeof(struct tpq_index_e) );
	tp->index = index;
	tp->index_size = num_jpeg;
}
#endif

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

#define JPEG_SOI_TAG	0xd8ff

static void
build_index ( struct tpq_info *tp, int fd )
{
	int i;
	unsigned short tag;
	int num_index;
	int num_jpeg;
	struct tpq_index_e *proto_index;
	struct tpq_index_e *index;
	long offset;
	void *fbp;

	fbp = filebuf_init ( fd, (off_t) TPQ_HEADER_SIZE );

	/* Read the offset to the first maplet, and
	 * use it to compute the size of the offset table.
	 */
	offset = filebuf_i4 ( fbp );
	filebuf_skip ( fbp, -4 );

	/* We won't need ALL of these, since some of the
	 * pointers point to the non-JPEG stuff at the
	 * end of the TPQ file, BUT we need to fetch at
	 * least one index past the JPEG stuff to be
	 * able to calculate the last maplet size.
	 */
	num_index = (offset - TPQ_HEADER_SIZE)/4;

	proto_index = (struct tpq_index_e *) malloc ( num_index * sizeof(struct tpq_index_e) );
	if ( ! proto_index )
	    error ("Build index: too many thingies %d!\n", num_index );

	/* Read the offset table, verify as we go that we
	 * point to a JPEG SOI tag, and terminate when
	 * that is no longer true.
	 */
	num_jpeg = 0;
	for ( i=0; i<num_index; i++ ) {
	    proto_index[i].offset = filebuf_i4 ( fbp );

	    tag = filebuf_i2_off ( fd, proto_index[i].offset );
	    /*
	    printf ( "Build index: offset %d = %d %x\n", i+1, proto_index[i].offset, tag );
	    */
	    if ( tag != JPEG_SOI_TAG )
		break;

	    num_jpeg++;
	}

	/* If this ever happens, I don't know how we figure the size of the last
	 * maplet.  Well actually, I do!  We can use the size of the file itself,
	 * which we can get from a stat call.  However, there doesn't seem to
	 * be any need for this complexity, as every TPQ file I have seen has
	 * at least one PNG file section after the JPEG maplets.
	 */
	if ( tag == JPEG_SOI_TAG )
	    error ( "Build index fails for %s\n", tp->path );

	/* Compute maplet sizes:
	 * Since the above search will have loaded one offset beyond
	 * the last JPEG maplet, the "look ahead" here will work.
	 */
	for ( i=0; i<num_jpeg; i++ )
	    proto_index[i].size = proto_index[i+1].offset - proto_index[i].offset;

	/* Now copy to a smaller index buffer, and free the big one
	 * we have been working in
	 */
        index = (struct tpq_index_e *) malloc ( num_jpeg * sizeof(struct tpq_index_e) );
        if ( ! index )
	    error ("Build index: too many maplets %d!\n", num_jpeg );

	memcpy ( index, proto_index, num_jpeg * sizeof(struct tpq_index_e) );
	free ( (char *) proto_index );

	tp->index = index;
	tp->index_size = num_jpeg;
}

#ifndef NEW_HEADER
static int
read_tpq_header ( struct tpq_info *tp, int fd )
{
	struct tpq_header tpq_header;
	int maplets;

	/*
	printf ( "sizeof long = %d\n", sizeof(long) );
	printf ( "sizeof int = %d\n", sizeof(int) );
	printf ( "sizeof INT4 = %d\n", sizeof(INT4) );
	printf ( "sizeof double = %d\n", sizeof(double) );
	*/

	if ( sizeof(struct tpq_file) != TPQ_FILE_SIZE )
	    error ( "Malformed TPQ file structure (my bug: %d)\n", sizeof(struct tpq_file) );

	if ( sizeof(struct tpq_header) != TPQ_HEADER_SIZE )
	    error ( "Malformed TPQ header structure (my bug: %d)\n", sizeof(struct tpq_header) );

	/* read header */
	if ( read( fd, &tpq_header, TPQ_HEADER_SIZE ) != TPQ_HEADER_SIZE )
	    return 0;

	if ( info.verbose & V_TPQ ) {
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

	/* When we need to scale pixels, we want to do all maplets on a TPQ sheet
	 * the same, and (based on the Mt. Hopkins quad) use the midpoint latitude
	 * to calculate the scaling.
	 */
	tp->mid_lat = (tp->n_lat + tp->s_lat) / 2.0;

	tp->lat_count = tpq_header.maplet.nlat;
	tp->long_count = tpq_header.maplet.nlong;

	tp->maplet_lat_deg = (tp->n_lat - tp->s_lat) / tp->lat_count;
	tp->maplet_long_deg = (tp->e_long - tp->w_long) / tp->long_count;

	/* Not all maps place their maplet grid to coincide with the origin.
	 * This is in particular the case for the level 1 full USA map,
	 * which covers long -66 to -125 in 12 maplets (4.917 degrees each)
	 *   and covers lat   24 to   50 in  8 maplets (3.25  degrees each)
	 * XXX - this junk needs to get copied into the series structure
	 *   during the archive setup phase.  For now, we hand initialize,
	 *   but someday we will be sorry about this.
	 */
	maplets = tp->e_long / tp->maplet_long_deg;
	tp->lat_offset = tp->e_long - maplets * tp->maplet_long_deg;
	maplets = tp->s_lat / tp->maplet_lat_deg;
	tp->long_offset = tp->s_lat - maplets * tp->maplet_lat_deg;

	/* Figure out the corner indices of our map */
        tp->sheet_lat = (tp->s_lat - tp->lat_offset) / tp->maplet_lat_deg;
	tp->sheet_long = - (tp->e_long - tp->long_offset) / tp->maplet_long_deg;

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
#endif

#ifdef NEW_HEADER
static int
read_tpq_header ( struct tpq_info *tp, int fd )
{
	void *fbp;
	int maplets;

	fbp = filebuf_init ( fd, (off_t) 0 );

	filebuf_skip ( fbp, 4 );	/* skip version */

	tp->w_long = filebuf_double ( fbp );
	tp->n_lat = filebuf_double ( fbp );
	tp->e_long = filebuf_double ( fbp );
	tp->s_lat = filebuf_double ( fbp );

	filebuf_skip ( fbp, 12 );	/* TOPO! string */
	filebuf_skip ( fbp, 208 );

	tp->quad = filebuf_string ( fbp, 128 );
	tp->state = filebuf_string ( fbp, 32 );

	filebuf_skip ( fbp, 32 );	/* source - like USGS */
	filebuf_skip ( fbp, 4 );	/* year1 - like 1994 */
	filebuf_skip ( fbp, 4 );	/* year2 - like 1994 */
	filebuf_skip ( fbp, 8 );	/* contour - like "20 ft" */
	filebuf_skip ( fbp, 16 );

	/* now a 32 byte tpq_file thingie
	 * for the jpeg maplets.
	 */
	filebuf_skip ( fbp, 4 );	/* string ".jpg" */
	filebuf_skip ( fbp, 8 );	/* two int4 things */

	tp->long_count = filebuf_i4 ( fbp );
	tp->lat_count = filebuf_i4 ( fbp );

	filebuf_skip ( fbp, 12 );

#ifdef notdef
	filebuf_skip ( fbp, 88 );
	filebuf_skip ( fbp, 32 );	/* TPQ file thingie for a png */
	filebuf_skip ( fbp, 28 );
	filebuf_skip ( fbp, 32 );	/* TPQ file thingie for a png */
	filebuf_skip ( fbp, 332 );
	/* End of header */
#endif
	tp->maplet_long_deg = (tp->e_long - tp->w_long) / tp->long_count;
	tp->maplet_lat_deg = (tp->n_lat - tp->s_lat) / tp->lat_count;

	if ( info.verbose & V_TPQ ) {
	    printf ( "TPQ file for %s quadrangle: %s\n", tp->state, tp->quad );
	    printf ( "TPQ file maplet counts long/lat: %d %d\n", tp->long_count, tp->lat_count );
	    printf ( "TPQ file long range: %.3f %3f\n", tp->w_long, tp->e_long );
	    printf ( "TPQ file lat range: %.3f %3f\n", tp->s_lat, tp->n_lat );
	    printf ( "TPQ maplet size: %.5f %.5f\n", tp->maplet_long_deg, tp->maplet_lat_deg );
	}

	/* When we need to scale pixels, we want to do all maplets on a TPQ sheet
	 * the same, and (based on the Mt. Hopkins quad) use the midpoint latitude
	 * to calculate the scaling.
	 */
	tp->mid_lat = (tp->n_lat + tp->s_lat) / 2.0;

	/* Not all maps place their maplet grid to coincide with the origin.
	 * This is in particular the case for the level 1 full USA map,
	 * which covers long -66 to -125 in 12 maplets (4.917 degrees each)
	 *   and covers lat   24 to   50 in  8 maplets (3.25  degrees each)
	 * XXX - this junk needs to get copied into the series structure
	 *   during the archive setup phase.  For now, we hand initialize,
	 *   but someday we will be sorry about this.
	 */
	maplets = tp->e_long / tp->maplet_long_deg;
	tp->long_offset = tp->e_long - maplets * tp->maplet_long_deg;
	maplets = tp->s_lat / tp->maplet_lat_deg;
	tp->lat_offset = tp->s_lat - maplets * tp->maplet_lat_deg;

	/* Figure out the corner indices of our map */
        tp->sheet_lat = (tp->s_lat - tp->lat_offset) / tp->maplet_lat_deg;
	tp->sheet_long = - (tp->e_long - tp->long_offset) / tp->maplet_long_deg;

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
#endif

static struct tpq_info *tpq_head = NULL;

static struct tpq_info *
tpq_new ( char *path )
{
        struct tpq_info *tp;
	int fd;

	if ( info.verbose & V_TPQ )
	    printf ( "tpq_new: %s\n", path );

        tp = (struct tpq_info *) malloc ( sizeof(struct tpq_info) );
        if ( ! tp )
            error ("tpq_new, out of mem\n");

	tp->path = strhide(path);

	fd = open ( path, O_RDONLY );
	if ( fd < 0 )
	    return NULL;

	if ( ! read_tpq_header ( tp, fd ) )
	    return NULL;

	build_index ( tp, fd );

#ifdef notdef
	{
	char buf[INDEX_BUFSIZE];
	    if ( read( fd, buf, INDEX_BUFSIZE ) != INDEX_BUFSIZE )
		return NULL;

	    build_index_OLD ( tp, fd, (long *) buf );
	}
#endif

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
	off_t off;
	int nw;
	int nlong;
	struct tpq_info *tp;
	int x_index, y_index;
	GdkPixbufLoader *loader;

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

	off = tp->index[mp->tpq_index].offset;
	lseek ( fd, off, SEEK_SET );
	size = tp->index[mp->tpq_index].size;

	/* Rumor has it that a loader cannot be reused, so
	 * we must allocate a new loader each time.
	 */
	loader = gdk_pixbuf_loader_new_with_type ( "jpeg", NULL );

	while ( size > 0 ) {
	    nw = size < BUFSIZE ? size : BUFSIZE;
	    if ( read( fd, buf, nw ) != nw )
		error ( "TPQ file read error %s %d %d\n", mp->tpq_path, off, size );
	    gdk_pixbuf_loader_write ( loader, buf, nw, NULL );
	    size -= nw;
	}

	close ( fd );

	/* The following two calls work in either order */
	gdk_pixbuf_loader_close ( loader, NULL );
	mp->pixbuf = gdk_pixbuf_loader_get_pixbuf ( loader );

	/* be a good citizen and avoid a memory leak,
	 */
	g_object_ref ( mp->pixbuf );
	g_object_unref ( loader );
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
		error ( "TPQ file read error %s %d\n", mp->tpq_path, size );
	    if ( write ( ofd, buf, nw ) != nw )
		error ( "tmp file write error %s\n", tmpname );
	    size -= nw;
	}

	close ( fd );
	close ( ofd );

	mp->pixbuf = gdk_pixbuf_new_from_file ( tmpname, NULL );
	remove ( tmpname );
#endif

	if ( ! mp->pixbuf && info.verbose & V_TPQ ) {
	    printf ("Cannot get pixbuf from %s (%d)\n", mp->tpq_path, mp->tpq_index );
	    return 0;
	}

	return 1;
}

/* THE END */
