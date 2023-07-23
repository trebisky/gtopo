/*
 *  GTopo - remote.c
 *
 *  Copyright (C) 2020, Thomas J. Trebisky
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

#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "remote.h"

/* This implements a protocol (server) listening on TCP port 5555
 * Commands are ascii as follows:
 *
 * m = mark at given coords
 * c = center at given coords
 *  The above are usually used together as:
 * MC -110.87813 31.71917
 *
 * e = erase path
 * d = draw path
 * p add coordinate to path
 */

struct remote remote_info;

/* South of the Old Madera Mine */
static double xx_long = -110.8597;
static double xx_lat = 31.7038;

/* New commands are posted in these variables to
 * be picked up on a timer tick.
 */
static double x_long;
static double x_lat;
static int mark;
static int center;
static int new_cmd = 0;;
static int new_path = 0;;

/* This implements a socket listener that handles remote commands
 * Tom Trebisky 7-13-2023
 */

#define REM_PORT 5555

/* sleep for 10 milliseconds */
static void
sleeper ( void )
{
	struct timespec ts;

	ts.tv_sec = 0;
	ts.tv_nsec = 10 * 1000;

	nanosleep ( &ts, NULL );
}

static void
rem_reply ( int ss, char *xx )
{
	int n = strlen ( xx );
	// xx[n++] = '\n';
	write ( ss, xx, n );
}

static void
dump ( char *buf, int n )
{
	int i;

	// printf ( "dump %d\n", n );

	for ( i=0; i<n; i++ ) {
	    // printf ( "loop\n" );
	    // printf ( "%d %d\n", i, n );
	    printf ( "buf %2d: %04x\n", i, buf[i] );
	}
}

static void
erase_path ( void )
{
	remote_info.path = 0;
	remote_info.npath = 0;
	new_path = 1;
}

static void
draw_path ( void )
{
	remote_info.path = 1;
	new_path = 1;
}

static void
add_to_path ( double lon, double lat )
{
	/* Silently discard excess points */
	if ( remote_info.npath >= MAX_REM_POINTS )
	    return;
	
	remote_info.data[remote_info.npath][0] = lat;
	remote_info.data[remote_info.npath][1] = lon;
	remote_info.npath++;
}

/* Handle a single line of input.
 */
static void
cmd_handler ( int ss, char *buf )
{
	int nw;
	char *wp[4];
	char *p;

	int valid;
	int point;

	// printf ( "CMD: %s\n", buf );
	nw = split_n ( buf, wp, 4 );
	// printf ( "Split: %d\n", nw );

	if ( nw < 1 ) {
	    rem_reply ( ss, "ERR\r\n" );
	    return;
	}

	mark = 0;
	center = 0;
	point = 0;
	valid = 0;

	for ( p = wp[0]; *p; p++ ) {
	    /* mark */
	    if ( *p == 'm' || *p == 'M' ) {
		mark = 1;
		valid = 1;
	    }
	    /* center */
	    if ( *p == 'c' || *p == 'C' ) {
		center = 1;
		valid = 1;
	    }

	    /* erase path */
	    if ( *p == 'e' || *p == 'E' ) {
		erase_path ();
		rem_reply ( ss, "OK\r\n" );
		return;
	    }
	    /* add point to path */
	    if ( *p == 'p' || *p == 'P' ) {
		point = 1;
		valid = 1;
	    }
	    /* draw path */
	    if ( *p == 'd' || *p == 'D' ) {
		draw_path ();
		rem_reply ( ss, "OK\r\n" );
		return;
	    }
	}

	if ( ! valid ) {
	    rem_reply ( ss, "ERR\r\n" );
	    return;
	}

	if ( nw != 3 ) {
	    rem_reply ( ss, "ERR\r\n" );
	    return;
	}

	/* That is all the validation we do.
	 */
	x_long = atof ( wp[1] );
	x_lat = atof ( wp[2] );

	if ( point ) {
	    add_to_path ( x_long, x_lat );
	    rem_reply ( ss, "OK\r\n" );
	    return;
	}

	/* Fall through to handle M and C */

	/* Flag the timer to pick this up */
	new_cmd = 1;

	/* Now we wait for the timer to clear new_cmd
	 * so that our OK response truly indicates that
	 * we are ready to accept a new command.
	 */
	while ( new_cmd )
	    sleeper ();

	rem_reply ( ss, "OK\r\n" );
}

/* This handles a single connection.
 * Note that telnet terminates lines with \r\n
 *  whereas commands from python or such end with just \n
 */
static void
rem_handler ( int ss )
{
	char buf[128];
	int n;

	// printf ( "Connection on %d\n", ss );

	for ( ;; ) {
	    n = read ( ss, buf, 128 );
	    if ( n == 0 )
		break;

	    // printf ( "Read got %d\n", n );
	    // dump ( buf, n );

	    if ( n < 1 || n > 100 ) {
		rem_reply ( ss, "ERR\r\n" );
		continue;
	    }

	    if ( buf[n-1] == '\r' || buf[n-1] == '\n' )
		n--;
	    if ( buf[n-1] == '\r' || buf[n-1] == '\n' )
		n--;
	    buf[n] = '\0';

	    // printf ( "Received: %d %s\n", n, buf );

	    cmd_handler ( ss, buf );
	}
	// printf ( "Connection closed\n" );
}

static void
rem_server ( void )
{
	int s;
	struct sockaddr_in server;
	struct sockaddr_in client;
	int namelen;
	int ss;

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	    return;

	server.sin_family = AF_INET;
	server.sin_port   = htons(REM_PORT);
	server.sin_addr.s_addr = INADDR_ANY;

	if (bind(s, (struct sockaddr *)&server, sizeof(server)) < 0)
	    return;

	if (listen(s, 4) != 0)
	    return;

	// printf ( "Listening on port %d\n", REM_PORT );
	for ( ;; ) {
	    namelen = sizeof(client);
	    if ((ss = accept(s, (struct sockaddr *)&client, &namelen)) == -1)
		break;
	    rem_handler ( ss );
	    close ( ss );
	}
}

void *
rem_func ( void *arg )
{
	// printf ( "Thread running\n" );

	new_cmd = 0;
	rem_server ();

	// printf ( "Server thread exited\n" );
	return NULL;
}

/*
 * ==========================================================
 * Everything above here runs in the remote thread.
 * below here runs in the main thread.
 * ==========================================================
 */

void
remote_init ( void )
{
	pthread_t rem_thread;
	int stat;

	remote_info.active = 0;
	remote_info.path = 0;
	remote_info.npath = 0;

	stat = pthread_create( &rem_thread, NULL, rem_func, NULL );
	// printf ( "Thread stat %d\n", stat );
}

static void
draw_mark ( double a_long, double a_lat )
{
	remote_info.r_long = a_long;
	remote_info.r_lat = a_lat;
	remote_info.active = 1;
	remote_redraw ();
}

/* This is what is done in gtopo.c
 * in places_select_func ()
 * when we handle a user selection of a places file
 * coordinate.
 */

static void
center_on ( double lon, double lat )
{
	// initial_series ( series );
	set_position ( lon, lat );

	new_redraw ();
}

/* Called at 10 Hz from the GTK timer */
void
remote_check ( void )
{
	if ( new_path ) {
	    remote_redraw ();
	    new_path = 0;
	}

	if ( ! new_cmd )
	    return;

	if ( mark )
	    draw_mark ( x_long, x_lat );
	if ( center )
	    center_on ( x_long, x_lat );

	new_cmd = 0;
}

/* THE END */
