/* Tom Trebisky 6-21-2020
 *
 * prototype GPX file parser
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *file_name = "ShovelSaddle08April2018-data.gpx";

void read_gpx ( char * );
void do_wpt ( FILE *, char * );
void do_trk ( FILE *, char * );

void
error ( char *msg )
{
    fprintf ( stderr, "%s\n", msg );
    exit ( 1 );
}

void
error2 ( char *msg, char *extra )
{
    fprintf ( stderr, "%s %s\n", msg, extra );
    exit ( 1 );
}

int
main ( int argc, char **argv )
{
    read_gpx ( file_name );
    return 0;
}

/* The second line in the header is huge (376 bytes) */
#define MAX_LINE	512

enum GPX_mode { START, START2, READY };

void
read_gpx ( char *fname )
{
    char buf[MAX_LINE];
    FILE *file;
    enum GPX_mode mode = START;

    file = fopen ( fname, "r" );
    if ( ! file ) {
	error ("Cannot open input file" );
    }

    /* fgets returns the newline */
    while ( fgets ( buf, MAX_LINE, file ) ) {
	if ( mode == START ) {
	    if ( strncmp ( buf, "<?xml ", 6 ) != 0 )
		error ( "Not a GPX file" );
	    mode = START2;
	    continue;
	}
	if ( mode == START2 ) {
	    if ( strncmp ( buf, "<gpx ", 5 ) != 0 )
		error ( "Malformed GPX file" );
	    mode = READY;
	    continue;
	}
	if ( mode == READY ) {
	    if ( strncmp ( buf, "<wpt ", 5 ) == 0 ) {
		do_wpt ( file, buf );
	    }
	    else if ( strncmp ( buf, "<trk>", 5 ) == 0 ) {
		do_trk ( file, buf );
	    }
	    else if ( strncmp ( buf, "</gpx>", 6 ) == 0 ) {
		/* This ends the file, just skip it */
	    }
	    else
		error2 ( "Unexpected: ", buf );

	}
    }

    fclose ( file );
}

/* Get the two values out of a line like this:
 * <wpt lat="31.71343172" lon="-110.873604761">
 */
void
get_ll ( char *vals[], char *line )
{
    char *p = line;
    int index = 0;
    char c;
    int skip = 1;
    char *vp;

    // vp = vals[0];
    // printf ( "%016x\n", vp );
    // vp = vals[index];
    // printf ( "%016x\n", vp );

    for ( p = line; c = *p; p++ ) {
	if ( c == '"' ) {
	    if ( skip ) {
		skip = 0;
		vp = vals[index++];
	    } else {
		skip = 1;
		*vp = '\0';
	    }
	} else {
	    if ( ! skip ) {
		//printf ( "Add : %c\n", c );
		*vp++ = c;
	    } else {
		//printf ( "Skip : %c\n", c );
	    }
	}
    }
}

#define MAX_VAL	64

void do_wpt ( FILE *file, char *line )
{
    char buf[MAX_LINE];
    char *ll[2];
    char lat[MAX_VAL];
    char lon[MAX_VAL];
    // double flat, flon;

    // printf ( "WPT !!\n" );
    // printf ( line );
    ll[0] = lat;
    ll[1] = lon;
    get_ll ( ll, line );
    // printf ( "Lat = %s\n", ll[0] );
    // printf ( "Lon = %s\n", ll[1] );
    // flat = atof ( lat );
    // flon = atof ( lon );
    printf ( "Waypoint: %s, %s\n", lat, lon );

    while ( fgets ( buf, MAX_LINE, file ) ) {
	//printf ( buf );
	if ( strncmp ( buf, "</wpt>", 6 ) == 0 )
	    break;
    }
}

void
get_thing ( char *buf, char *thing )
{
    char *p;
    int skip = 1;
    char c;

    for ( p = buf; c = *p; p++ ) {
	if ( skip ) {
	    if ( c == '>' )
		skip = 0;
	} else {
	    if ( c == '<' )
		break;
	    *thing++ = c;
	}
    }
    *thing = '\0';
}

#define skip_sp(x)	while ( *x == ' ' ) x++

void do_trk ( FILE *file, char *line )
{
    char buf[MAX_LINE];
    char *p;
    char thing_buf[MAX_VAL];
    char *ll[2];
    char lat[MAX_VAL];
    char lon[MAX_VAL];

    ll[0] = lat;
    ll[1] = lon;

    printf ( "\n" );
    printf ( "TRK !!\n" );
    // printf ( line );
    while ( fgets ( buf, MAX_LINE, file ) ) {
	// printf ( buf );
	p = buf;
	skip_sp ( p );

	if ( strncmp ( buf, "</trk>", 6 ) == 0 ) {
	    break;
	} else if ( strncmp ( p, "<name", 5 ) == 0 ) {
	    get_thing ( p, thing_buf );
	    printf ( "Name: %s\n", thing_buf );
	} else if ( strncmp ( p, "<trkpt", 6 ) == 0 ) {
	    get_ll ( ll, p );
	    printf ( "Track point: %s, %s\n", lat, lon );
	} else {
	    ;
	    // printf ( p );
	}

    }
}

/* THE END */
