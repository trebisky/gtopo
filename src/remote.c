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
draw_mark ( double a_long, double a_lat )
{
	remote_info.r_long = a_long;
	remote_info.r_lat = a_lat;
	remote_info.active = 1;
	remote_redraw ();
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
cmd_handler ( int ss, char *buf )
{
	int nw;
	char *wp[4];
	char *p;
	int valid;

	nw = split_n ( buf, wp, 4 );
	// printf ( "Split: %d\n", nw );

	if ( nw != 3 ) {
	    rem_reply ( ss, "ERR\r\n" );
	    return;
	}

	mark = 0;
	center = 0;
	valid = 0;

	for ( p = wp[0]; *p; p++ ) {
	    if ( *p == 'm' || *p == 'M' ) {
		mark = 1;
		valid = 1;
	    }
	    if ( *p == 'c' || *p == 'C' ) {
		center = 1;
		valid = 1;
	    }
	}

	if ( ! valid ) {
	    rem_reply ( ss, "ERR\r\n" );
	    return;
	}

	/* That is all the validation we do.
	 */
	x_long = atof ( wp[1] );
	x_lat = atof ( wp[2] );

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

	    if ( n < 4 || n > 100 ) {
		rem_reply ( ss, "ERR\r\n" );
		return;
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

	stat = pthread_create( &rem_thread, NULL, rem_func, NULL );
	// printf ( "Thread stat %d\n", stat );
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
	if ( ! new_cmd )
	    return;

	if ( mark )
	    draw_mark ( x_long, x_lat );
	if ( center )
	    center_on ( x_long, x_lat );

	new_cmd = 0;
}

#ifdef notdef
static int xxx = 0;

void
remote_check ( void )
{
	if ( xxx ) {
	    draw_mark ( x_long, x_lat );
	    xxx = 0;
	} else {
	    draw_mark ( x_long + 0.004, x_lat - 0.004 );
	    xxx = 1;
	}
}
#endif

/* THE END */
