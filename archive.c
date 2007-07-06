#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

/* Tom Trebisky  MMT Observatory, Tucson, Arizona
 * part of gtopo.c as of version 0.5.
 * 7/6/2007
 */

void
archive_init ( char *archives[] )
{
	char **p;

	for ( p=archives; *p; p++ ) {
	    printf ( "Checking archive: %s\n", *p );
	}
}

/* THE END */
