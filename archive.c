#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "gtopo.h"

/* Tom Trebisky  MMT Observatory, Tucson, Arizona
 * part of gtopo.c as of version 0.5.
 * 7/6/2007
 */

/* This subsystem keeps a linked list of "sections"
 * the idea being you can give a latitude and longitude
 * to the nearest degree and be able to get a path to
 * the directory holding the stuff for that 1x1 degree
 * chunk of the world.
 */
struct _section {
	struct _section *next;
	int	latlong;
	char	*path;
};

struct _section *section_head;

static int verbose = 0;

/* Prototypes ... */
int add_archive ( char * );
int add_disk ( char *, char * );

char *
strhide ( char *data )
{
	int n = strlen(data);
	char *rv;

	rv = malloc ( n + 1 );
	strcpy ( rv, data );
	return rv;
}

int
archive_init ( char *archives[], int verbose_arg )
{
	char **p;
	int nar = 0;

	section_head = (struct _section *) NULL;
	verbose = verbose_arg;

	for ( p=archives; *p; p++ ) {
	    if ( add_archive ( *p ) )
		nar++;
	    else if ( verbose )
	    	printf ( "Not a topo archive: %s\n", *p );
	}

	return nar;
}

int
add_archive ( char *archive )
{
	struct stat stat_buf;
	DIR *dd;
	struct dirent *dp;

	if ( stat ( archive, &stat_buf ) < 0 )
	    return 0;
	if ( ! S_ISDIR(stat_buf.st_mode) )
	    return 0;

	if ( ! (dd = opendir ( archive )) )
	    return 0;

	/* Loop through this possible archive, look
	 * for files with names 6 characters in length,
	 * and the pattern: CA_D06 or AZ_D05 and in
	 * any case.
	 */
	for ( ;; ) {
	    if ( ! (dp = readdir ( dd )) )
	    	break;
	    if ( strlen(dp->d_name) != 6 )
	    	continue;
	    if ( dp->d_name[2] != '_' )
	    	continue;
	    if ( dp->d_name[3] == 'D' || dp->d_name[3] == 'd' )
	    	add_disk ( archive, dp->d_name );
	}

	closedir ( dd );
	return 1;
}

int
add_disk ( char *archive, char *disk )
{
	struct stat stat_buf;
	DIR *dd;
	struct dirent *dp;
	char disk_path[100];

	sprintf ( disk_path, "%s/%s", archive, disk );

	if ( stat ( disk_path, &stat_buf ) < 0 )
	    return 0;
	if ( ! S_ISDIR(stat_buf.st_mode) )
	    return 0;

	if ( ! (dd = opendir ( disk_path )) )
	    return 0;

	if ( verbose )
	    printf ( "Found disk: %s\n", disk_path );

	/* Loop through this disk image.
	 */
	for ( ;; ) {
	    if ( ! (dp = readdir ( dd )) )
	    	break;
	    if ( strlen(dp->d_name) != 6 )
	    	continue;
	    if ( dp->d_name[0] == 'D' || dp->d_name[0] == 'd' )
	    	add_section ( disk_path, dp->d_name );
	}

	closedir ( dd );
	return 1;
}

/* A section is a term we use only in this program for a 1x1 degree
 * piece of land, NOT a section in the one square mile land survey
 * nomenclature.  It is a directory that actually holds TPQ files
 * which represent individual 7.5 minute maps (and more!)
 */
int
add_section ( char *disk, char *section )
{
	struct stat stat_buf;
	DIR *dd;
	char section_path[100];
	struct _section *sp;

	sprintf ( section_path, "%s/%s", disk, section );

	if ( stat ( section_path, &stat_buf ) < 0 )
	    return 0;
	if ( ! S_ISDIR(stat_buf.st_mode) )
	    return 0;

	/* Just verify we can open it */
	if ( ! (dd = opendir ( section_path )) )
	    return 0;
	closedir ( dd );

	sp = (struct _section *) malloc ( sizeof(struct _section) );
	if ( ! sp )
	    error ("Section new - out of memory\n", "" );

	sp->latlong = atol ( &section[1] );
	sp->path = strhide ( section_path );
	sp->next = section_head;
	section_head = sp;

	if ( verbose )
	    printf ( "Added section: %d  %s\n", sp->latlong, section_path );

	return 1;
}

char *
lookup_section ( int lat_deg, int long_deg )
{
	int latlong = lat_deg * 1000 + long_deg;
	struct _section *sp;

	for ( sp = section_head; sp; sp = sp->next ) {
	    if ( sp->latlong == latlong )
	    	return sp->path;
	}
	return NULL;
}

char *
lookup_quad ( int lat_deg, int long_deg, char *quad_code )
{
	struct stat stat_buf;
	char *section_path;
	/* XXX non reentrant static follows */
	static char *quad_path[100];
	char qc[4];

	section_path = lookup_section ( lat_deg, long_deg );
	if ( ! section_path )
	    return NULL;

	sprintf ( (char *) quad_path, "%s/q%2d%03d%s.tpq", section_path, lat_deg, long_deg, quad_code );

	if ( stat ( (char *) quad_path, &stat_buf ) >=  0 ) {
	    if ( S_ISREG(stat_buf.st_mode) ) {
	    	return (char *)quad_path;
	    }
	}

	/* Try upper case */
	qc[0] = toupper ( quad_code[0] );
	qc[1] = quad_code[1];
	qc[2] = '\0';

	sprintf ( (char *) quad_path, "%s/Q%2d%03d%s.TPQ", section_path, lat_deg, long_deg, qc );

	if ( stat ( (char *) quad_path, &stat_buf ) >=  0 ) {
	    if ( S_ISREG(stat_buf.st_mode) ) {
	    	return (char *)quad_path;
	    }
	}

	return NULL;
}

/* THE END */
