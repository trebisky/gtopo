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
/*
#include <fcntl.h>
*/
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

/* XXX - needs varargs */
void
error ( char *msg, void *arg )
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

char *
strhide ( char *data )
{
	int n = strlen(data);
	char *rv;

	rv = malloc ( n + 1 );
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

/* THE END */
