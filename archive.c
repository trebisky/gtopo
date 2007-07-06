#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

/* Tom Trebisky  MMT Observatory, Tucson, Arizona
 * part of gtopo.c as of version 0.5.
 * 7/6/2007
 */

int add_archive ( char * );
int add_disk ( char *, char * );

int
archive_init ( char *archives[] )
{
	char **p;
	int nar = 0;

	for ( p=archives; *p; p++ ) {
	    if ( add_archive ( *p ) )
		nar++;
	    else
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

	sprintf ( section_path, "%s/%s", disk, section );

	if ( stat ( section_path, &stat_buf ) < 0 )
	    return 0;
	if ( ! S_ISDIR(stat_buf.st_mode) )
	    return 0;

	/* Just verify we can open it */
	if ( ! (dd = opendir ( section_path )) )
	    return 0;
	closedir ( dd );

	printf ( "Found section: %s\n", section_path );
	return 1;
}

/* THE END */
