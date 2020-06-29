#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void * gmalloc ( int );

int xml_verify ( char *, int );
void * xml_parser ( char *, int );
void xml_free ( void * );
void xml_test ( void * );

static void
process_file ( char *file )
{
	struct stat st;
	int fd;
	int n;
	char *xbuf;
	void *xp;

	fd = open ( file, O_RDONLY );
	if ( fd < 0 )
	    return;

	if ( fstat ( fd, &st ) < 0 ) {
	    close ( fd );
	    return;
	}

	n = st.st_size;
	printf ( "File has %d bytes\n", n );

	if ( n > 1024*1024 ) {
	    printf ( "File too big: %d\n", n );
	    close ( fd );
	    return;
	}

	xbuf = gmalloc ( n + 128 );
	if ( read ( fd, xbuf, n + 128 ) != n ) {
	    printf ( "Read error\n" );
	    free ( xbuf );
	    close ( fd );
	    return;
	}

	close ( fd );

	printf ( "FILE: %s %d\n", file, n );

	if ( ! xml_verify ( xbuf, n ) ) {
	    printf ( "Not a valid file\n" );
	    free ( xbuf );
	    return;
	}

	xp = xml_parser ( xbuf, n );
	free ( xbuf );

	if ( ! xp ) {
	    printf ( "invalid file\n" );
	    return;
	}

	xml_test ( xp );

	xml_free ( xp );
}

int
main ( int argc, char **argv )
{
	process_file ( "rainbow.gpx" );
	// process_file ( "rainbow.kml" );
}

/* THE END */
