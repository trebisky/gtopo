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

/* utils.c
 * Some C language odds and ends used by gtopo.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <stdarg.h>

#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include "gtopo.h"
#include "protos.h"

void
error ( char *msg, ... )
{
	va_list args;

	va_start ( args, msg );
	(void) vprintf ( msg, args );
	va_end ( args );

	exit ( 1 );
}

double
dms2deg ( int deg, int min, double sec )
{
	double rv;

	rv = deg;
	rv += ((double)min/60.0);
	rv += sec/3600.0;
	return rv;
}

/* split a string in place.
 * tosses nulls into string, trashing it.
 */
int
split ( char *buf, char **bufp, int max )
{
	int i;
	char *p;

	p = buf;
	for ( i=0; i<max; ) {
	    while ( *p && *p == ' ' )
		p++;
	    if ( ! *p )
		break;
	    bufp[i++] = p;
	    while ( *p && *p != ' ' )
		p++;
	    if ( ! *p )
		break;
	    *p++ = '\0';
	}

	return i;
}

/* Split part of a string in place.
 * tosses nulls into string, trashing it.
 * You ask for two words and if there are more
 * than two you get 3 things: the two you asked
 * for, and a third which is "everything else".
 * This means that the bufp array needs max+1 entries.
 */
int
split_n ( char *buf, char **bufp, int max, char *end )
{
	int i;
	char *p;
	char *ep;

	p = buf;
	i = 0;
	for ( ;; ) {
	    while ( *p && *p == ' ' )
		p++;
	    if ( i >= max || ! *p ) {
		ep = p;
		break;
	    }
	    bufp[i++] = p;
	    while ( *p && *p != ' ' )
		p++;
	    if ( ! *p ) {
		ep = p;
		break;
	    }
	    *p++ = '\0';
	}

	bufp[i] = ep;
	if ( *ep )
	    i++;

	return i;
}

/* This just wraps malloc with an error check
 */
void *
gmalloc ( size_t size )
{
	void *rv;

	rv = malloc ( size );
	if ( ! rv )
	    error ("Out of memory: %d\n", size );
	return rv;
}

char *
strnhide ( char *data, int n )
{
	char *rv;

	rv = gmalloc ( n + 1 );
	memcpy ( rv, data, n );
	rv[n] = '\0';
	return rv;
}


char *
strhide ( char *data )
{
	int n = strlen(data);
	char *rv;

	rv = gmalloc ( n + 1 );
	strcpy ( rv, data );
	return rv;
}

char *
str_lower ( char *data )
{
	char *rv, *p;

	p = rv = strhide ( data );
	for ( p = rv; *p; p++ )
	    *p = tolower ( *p );
	return rv;
}

/* Compare a test string to a reference string.
 * The reference string is lower case.
 * The test string may be any case.
 * We assume the test string is not too big.
 */
int
strcmp_l ( char *ref, char *test )
{
	char *lstr;
	int rv;

	lstr = str_lower ( test );
	rv = strcmp ( ref, lstr );
	free ( lstr );
	return rv;
}

int
is_directory ( char *path )
{
	struct stat stat_buf;

	if ( stat ( path, &stat_buf ) < 0 )
	    return 0;
	if ( S_ISDIR(stat_buf.st_mode) )
	    return 1;
	return 0;
}

int
is_file ( char *path )
{
	struct stat stat_buf;

	if ( stat ( path, &stat_buf ) < 0 )
	    return 0;
	if ( S_ISREG(stat_buf.st_mode) )
	    return 1;
	return 0;
}

/* figure out if we are running on
 * a big endian machine.
 * (like a sparc or powerpc).
 */
int
is_big_endian ( void )
{
	char tester[2];

	tester[0] = 0;
	tester[1] = 1;
	if ( *(short *) tester == 1 )
	    return 1;
	return 0;
}

/* Grog our home directory */
char *
find_home ( void )
{
	struct passwd *pw;

	pw = getpwuid ( getuid() );
	if ( pw )
	    return pw->pw_dir;

	return getenv ( "HOME" );
}

/* Accepts just plain degrees, or
 * d:m or d:m:s for sexigesimal
 */
double
parse_dms ( char *line )
{
    	char *p;
	int is_neg;
	char *d, *m, *s;
	int how_many_colons = 0;
	int state = 0;
	double rv;

	for ( p=line; *p; p++ )
	    if ( *p == ':' )
		how_many_colons++;

	if ( how_many_colons == 0 )
	    return atof ( line );

	d = line;
	for ( p=line; *p; p++ ) {
	    if ( *p != ':' )
		continue;
	    *p++ = '\0';

	    if ( state == 0 ) {
		m = p;
		state = 1;
	    } else {
		s = p;
	    	break;
	    }
	}

	rv = atof ( d );

	is_neg = rv < 0.0 ? 1 : 0;

	if ( is_neg )
	    rv -= atof ( m ) / 60.0;
	else
	    rv += atof ( m ) / 60.0;

	if ( how_many_colons == 1 )
	    return rv;

	if ( is_neg )
	    rv -= atof ( s ) / 3600.0;
	else
	    rv += atof ( s ) / 3600.0;

	return rv;
}

/* This whole filebuf business was added to gtopo in late August
 * of 2007 when I tried to make gtopo run on x86_64 bit machines
 * (where a long is 8 bytes), and Mac OS-X power PC machines
 * (which are big endian).  This set of filebuf routines let me
 * read a TPQ file on any of these machines, as well as a plain
 * old 32 bit x86 machine.  It is all about portability.
 * I don't need to strive for highly efficient code because
 * I only use this to read the TPQ file headers.
 */

#define FILEBUF_SIZE	1024

struct filebuf {
	unsigned char buf[FILEBUF_SIZE];
	int fd;
	int bufsize;
	off_t next_off;
	unsigned char *next;
	unsigned char *limit;
	int big_endian;
};

static int
filebuf_load_buf ( struct filebuf *fp )
{
	if ( lseek ( fp->fd, fp->next_off, SEEK_SET ) < 0 )
	    return -1;

	if ( read ( fp->fd, fp->buf, fp->bufsize ) != fp->bufsize )
	    return -1;

	fp->next_off += fp->bufsize;
	fp->next = fp->buf;
	fp->limit = &fp->buf[fp->bufsize];

	return 0;
}

/* In the unlikely case that some object we
 * are unpacking wraps around buffer boundaries,
 * this makes it easy to do the right thing.
 */
static int
filebuf_next_byte ( struct filebuf *fp )
{
	if ( fp->next < fp->limit )
	    return *fp->next++;

	/* XXX */
	if ( filebuf_load_buf ( fp ) < 0 )
	    error ( "filebuf next byte fails\n" );

	return *fp->next++;
}

static void *
filebuf_init_size ( int fd, off_t offset, int size )
{
	struct filebuf *fp;

	fp = gmalloc ( sizeof(struct filebuf) );
	if ( ! fp  )
	    return NULL;

	if ( size > FILEBUF_SIZE )
	    size = FILEBUF_SIZE;

	fp->fd = fd;
	fp->bufsize = size;
	fp->big_endian = is_big_endian();
	fp->next_off = offset;

	if ( filebuf_load_buf ( fp ) < 0 ) {
	    free ( (char *) fp );
	    return NULL;
	}

	return (void *) fp;
}

void
filebuf_free ( void *fp )
{
	free ( (char *) fp );
}

void *
filebuf_init ( int fd, off_t offset )
{
	return filebuf_init_size ( fd, offset, FILEBUF_SIZE );
}

long
filebuf_i4 ( void *cookie )
{
	struct filebuf *fp;
	long rv;

	fp = (struct filebuf *) cookie;

	rv = filebuf_next_byte ( fp );
	rv |= filebuf_next_byte ( fp ) << 8;
	rv |= filebuf_next_byte ( fp ) << 16;
	rv |= filebuf_next_byte ( fp ) << 24;

	return rv;
}

int
filebuf_i2 ( void *cookie )
{
	struct filebuf *fp;
	long rv;

	fp = (struct filebuf *) cookie;

	rv = filebuf_next_byte ( fp );
	rv |= filebuf_next_byte ( fp ) << 8;

	return rv;
}

#ifdef notdef
void
double_dump ( double val )
{
	union {
	    double d_val;
	    int i_val[2];
	} u_d;

	printf ( "sizeof double = %d\n", sizeof(double) );
	u_d.d_val = 37.00;
	printf ( "dval 37.00 = %08x  %08x\n", u_d.i_val[0], u_d.i_val[1] );
	u_d.d_val = val;
	printf ( "dval val = %08x  %08x\n", u_d.i_val[0], u_d.i_val[1] );
}
#endif

/* This is pretty gross, but amazingly seems to
 * work on all the 32 and 64 bit machines I can
 * get my hands on with IEEE floating point math.
 */
double
filebuf_double ( void *cookie )
{
	struct filebuf *fp;
	union {
		double dval;
		char cval[8];
	} u_d;

	fp = (struct filebuf *) cookie;

	if ( fp->big_endian ) {
	    u_d.cval[7] = filebuf_next_byte ( fp );
	    u_d.cval[6] = filebuf_next_byte ( fp );
	    u_d.cval[5] = filebuf_next_byte ( fp );
	    u_d.cval[4] = filebuf_next_byte ( fp );
	    u_d.cval[3] = filebuf_next_byte ( fp );
	    u_d.cval[2] = filebuf_next_byte ( fp );
	    u_d.cval[1] = filebuf_next_byte ( fp );
	    u_d.cval[0] = filebuf_next_byte ( fp );
	} else {
	    u_d.cval[0] = filebuf_next_byte ( fp );
	    u_d.cval[1] = filebuf_next_byte ( fp );
	    u_d.cval[2] = filebuf_next_byte ( fp );
	    u_d.cval[3] = filebuf_next_byte ( fp );
	    u_d.cval[4] = filebuf_next_byte ( fp );
	    u_d.cval[5] = filebuf_next_byte ( fp );
	    u_d.cval[6] = filebuf_next_byte ( fp );
	    u_d.cval[7] = filebuf_next_byte ( fp );
	}

	return u_d.dval;
}

void
filebuf_skip ( void *cookie, int count )
{
	struct filebuf *fp;

	fp = (struct filebuf *) cookie;

	/* The usual case, just moving the pointer
	 * in the current buffer
	 */
	if ( fp->next + count <= fp->limit ) {
	    fp->next += count;
	    return;
	}

	/* We are here if we are skipping right
	 * out of the present in-core buffer.
	 */
	count -= fp->limit - fp->next;
	fp->next = fp->limit;
	fp->next_off += count;
}

/* Copy out a fixed length string.
 * Allow enough room to put a "safety"
 * null byte at the end.
 */
char *
filebuf_string ( void *cookie, int count )
{
	struct filebuf *fp;
	char *rv;
	char *p;

	fp = (struct filebuf *) cookie;

	p = rv = gmalloc ( count + 1 );

	while ( count-- )
	    *p++ = filebuf_next_byte ( fp );
	*p = '\0';

	return rv;
}

/* This is swatting a fly with a sledge-hammer.
 * We want to read two bytes at some offset in
 * a file, without disturbing other filebuf
 * action that may be going on in the same file
 */
int
filebuf_i2_off ( int fd, off_t offset )
{
	void *fp;
	long rv;

	fp = filebuf_init_size ( fd, offset, 2 );
	rv = filebuf_i2 ( fp );
	filebuf_free ( fp );
	return rv;
}

/* THE END */
