/*
 *  GTopo - http.c
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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>

static int http_verbose = 0;

/* someday, this will be used by gtopo to do SOAP web services
 * stuff to get maplets from Terraserver.
 */

#define MAX_NET_LINE	512
#define NET_BUF_SIZE	4096

static int net_debug = 0;

static int
addr_lookup ( char *ip_addr, char *server )
{
    	struct hostent *hp;

	hp = gethostbyname ( server );
	if ( hp ) {
	    memcpy ( ip_addr, hp->h_addr, hp->h_length );
	    return 1;
	}
	return 0;
}

int
net_client ( char *host, int port )
{
    int sn;
    struct sockaddr_in con_addr;

    /* printf ("net_client: %s %d\n", host, port );
     */

    if ( (sn=socket ( AF_INET, SOCK_STREAM, 0 )) < 0 ) {
	/* printf("cannot get TCP socket for %s\n", host);
	 */
	return -1;
    }

    bzero( (char *) &con_addr, sizeof(con_addr) );
    con_addr.sin_family = AF_INET;
    con_addr.sin_port = htons(port);
    if ( ! addr_lookup ( (char *) &con_addr.sin_addr.s_addr, host ) )
	return -2;

    /* Will hang in connect if trying to contact a
     * non-existing machine (or one not on local net).
     * (but may timeout fairly quickly as well)
     */
    if ( connect ( sn, (struct sockaddr *) &con_addr, sizeof(con_addr) ) < 0 ) {
	close ( sn );
	return -3;
    }

    return ( sn );
}

#ifdef notdef
int
net_read_poll ( int fd, double time )
{
	fd_set fdset;
	struct timeval tmout;
	int rv;

	FD_ZERO ( &fdset );
	FD_SET ( fd, &fdset );

	tmout.tv_sec = (long) time;
	tmout.tv_usec = 1000000 * ( time - (long) time);

	do
	    rv = select ( fd+1, &fdset, NULL, NULL, &tmout );
	while ( rv < 0 && errno == EINTR );

	return rv;
} 

int
net_read ( int sfd, char *buf, int nbuf )
{
    register n, nxfer;

    if ( net_debug ) {
	printf("rd buf = %08x - %08x\n",buf,buf+nbuf-1);
    }

    if ( ! net_read_poll ( sfd, 0.95 ) )
    	return 0;

    for ( n=0; n < nbuf; n += nxfer ) {
	nxfer = read(sfd,&buf[n],nbuf-n);
	if ( nxfer <= 0 )
	    break;
    }

    if ( net_debug ) {
	if ( n != nbuf ) {
	    printf("Rd-err: %d %d %d\n",nbuf,n,nxfer);
	    printf("Errno: %d\n",errno);
	}
    }

    return ( n );
}
#endif

int
net_write ( int sfd, char *buf, int nbuf)
{
    register int n, nxfer;

    if ( net_debug ) {
	printf("wr buf = %08x - %08x\n",buf,buf+nbuf-1);
    }

    for ( n=0; n < nbuf; n += nxfer ) {
	nxfer = write(sfd,&buf[n],nbuf-n);
	if ( nxfer <= 0 )
	    break;
    }

    if ( net_debug ) {
	if ( n != nbuf ) {
	    printf("Wr-err: %d %d %d\n",nbuf,n,nxfer);
	    printf("Errno: %d\n",errno);
	}
    }
    return ( n );
}

#ifdef notdef
/* Added 3-21-2006, to support http
 */
void
net_putline_crlf ( int sfd, char *buf )
{
    char catbuf[MAX_NET_LINE];
    int n = strlen(buf);

    strcpy ( catbuf, buf );

    catbuf[n++] = '\r';
    catbuf[n++] = '\n';
    catbuf[n] = '\0';

    net_write ( sfd, catbuf, n );
}

/* Added 10-21-99,  trivial but handy.
 */
void
net_putlinen ( int sfd, char *buf )
{
    char catbuf[MAX_NET_LINE];
    int n = strlen(buf);

    strcpy ( catbuf, buf );

    catbuf[n++] = '\n';
    catbuf[n] = '\0';

    net_write ( sfd, catbuf, n );

/*  net_write ( sfd, buf, strlen(buf) );
    net_write ( sfd, "\n", 1 );
*/
}

/* Added 10-21-99,  trivial but handy.
 */
void
net_putline ( int sfd, char *buf )
{
    net_write ( sfd, buf, strlen(buf) );
}

/* Note: does NOT add gratuitous newline
 */
void
net_printf ( int sfd, char *fmt, ... )
{
    va_list args;
    char msgbuf[MAX_NET_LINE];

    va_start ( args, fmt );
    (void) vsprintf ( msgbuf, fmt, args ); 
    net_write ( sfd, msgbuf, strlen(msgbuf) );
    va_end ( args );
}

void
net_printfn ( int sfd, char *fmt, ... )
{
    va_list args;
    char msgbuf[MAX_NET_LINE];
    int n;

    va_start ( args, fmt );
    (void) vsprintf ( msgbuf, fmt, args ); 
    va_end ( args );

    n = strlen(msgbuf);
    msgbuf[n++] = '\n';
    msgbuf[n] = '\0';
    net_write ( sfd, msgbuf, n );
}
#endif

void
net_printf_crlf ( int sfd, char *fmt, ... )
{
    va_list args;
    char msgbuf[MAX_NET_LINE];
    int n;

    va_start ( args, fmt );
    (void) vsprintf ( msgbuf, fmt, args ); 
    va_end ( args );

    n = strlen(msgbuf);
    msgbuf[n++] = '\r';
    msgbuf[n++] = '\n';
    msgbuf[n] = '\0';

    net_write ( sfd, msgbuf, n );
}

void
dumpit ( char *buf, int n )
{
    write ( 1, buf, n );
}

struct net_buf {
	char buf[NET_BUF_SIZE];
	char *cur_p;
	char *last_p;	/* beyond end */
	int nbuf;
	int sock;
};

static struct net_buf net_buf;

int
net_buf_reload ( void )
{
	net_buf.last_p = net_buf.cur_p = net_buf.buf;

	net_buf.nbuf = read ( net_buf.sock, net_buf.buf, NET_BUF_SIZE );
	if ( net_buf.nbuf <= 0 )
	    return 0;

	net_buf.last_p = &net_buf.buf[net_buf.nbuf];
	return 1;
}

int
net_buf_init ( int fd )
{
	net_buf.sock = fd;
	return net_buf_reload ();
}

int
net_buf_getc ( void )
{
	if ( net_buf.cur_p < net_buf.last_p )
	    return *net_buf.cur_p++;
	(void) net_buf_reload ();
	if ( net_buf.cur_p < net_buf.last_p )
	    return *net_buf.cur_p++;
	return -1;
}

char *
net_buf_dregs ( int *nio )
{
	*nio = net_buf.last_p - net_buf.cur_p;
	return net_buf.cur_p;
}

int
read_http_header_line ( char *buf )
{
	char *p = buf;
	int c;

	for ( ;; ) {
	    c = net_buf_getc();
	    if ( c < 0 )
	    	break;
	    if ( c == '\r' ) {
		(void) net_buf_getc();
	    	break;
	    }
	    *p++ = c;
	}
	*p = '\0';
	return p - buf;
}

struct http_header {
	int parsed;
	struct http_header *next;
	char *data;
	char *tag;
	char *value;
};

struct http_head {
	struct http_header *lines;
	int count;
};

struct http_head header = { NULL, 0 };

/* parse in place, pop a null into middle of data */
void
parse_header ( struct http_header *hh )
{
	char *p;

	hh->parsed = 1;

	hh->tag = hh->data;
	p = hh->data;
	while ( *p && *p != ':' )
	    p++;

	/* whack the ':' */
	*p++ = '\0';

	while ( *p && *p == ' ' )
	    p++;

	hh->value = p;
}

int
read_http_headers ( void )
{
	char buf[MAX_NET_LINE];
	struct http_header *hh;
	struct http_header *hn;
	int n;

	header.lines = NULL;
	header.count = 0;

	for ( ;; ) {
	    n = read_http_header_line ( buf );
	    if ( n == 0 )
	    	break;
	    hh = (struct http_header *) malloc ( sizeof(struct http_header) );
	    hh->next = NULL;
	    hh->data = malloc ( n+1 );
	    hh->parsed = 0;
	    memcpy ( hh->data, buf, n+1 );

	    if ( ! header.lines )
	    	header.lines = hh;
	    else {
		hn = header.lines;
		while ( hn->next )
		    hn = hn->next;
	    	hn->next = hh;
		parse_header ( hh );
	    }
	    header.count++;
	}

	return header.count;
}

void
free_http_headers ( void )
{
	struct http_header *hh;
	struct http_header *h_next;

	for ( hh = header.lines; hh; hh = h_next ) {
	    h_next = hh->next;
	    free ( (void *) hh->data );
	    free ( (void *) hh );
	}

	header.lines = NULL;
	header.count = 0;
}

void
show_headers ( void )
{
	struct http_header *hh;

	printf ( "Got %d header lines\n", header.count );

	for ( hh = header.lines; hh; hh = hh->next )
	    if ( hh->parsed )
		printf ( "%s: %s\n", hh->tag, hh->value );
	    else
		printf ( "%s\n", hh->data );
}

char *
get_header_val ( char *tag )
{
	struct http_header *hh;

	for ( hh = header.lines; hh; hh = hh->next ) {
	    if ( ! hh->parsed )
	    	continue;
	    if ( strcmp ( hh->tag, tag ) == 0 )
	    	return hh->value;
	}
	return NULL;
}

int
get_http_payload_size ( void )
{
	char *p;

	p = get_header_val ( "Content-Length" );
	if ( ! p )
	    return 0;

	return atol ( p );
}

char *
get_http_payload ( void )
{
	char *rv;
	char *p;
	int npay;
	int n;

	npay = get_http_payload_size ();

	if ( http_verbose )
	    printf ( "Payload size: %d\n", npay );

	p = rv = malloc ( npay );

	n = net_buf.last_p - net_buf.cur_p;
	if ( n ) {
	    memcpy ( p, net_buf.cur_p, n );
	    p += n;
	    npay -= n;
	}

	while ( npay > 0 ) {
	    n = read ( net_buf.sock, p, npay );
	    if ( n <= 0 ) {
	    	free ( rv );
	    	return NULL;
	    }
	    p += n;
	    npay -= n;
	}

	return rv;
}

void
free_http_soap ( void *pay )
{
	free ( pay );
}

void
http_get ( char *server, int port, char *document )
{
    	int sock;
	char *p;
	int npay;

	sock = net_client ( server, port );

	/* This seems to be the minimum,
	 * at least lighttpd wants the Host line
	 */
	net_printf_crlf ( sock, "GET %s HTTP/1.1", document );
	net_printf_crlf ( sock, "Host: %s", server );
	net_printf_crlf ( sock, "User-agent: gTopo" );
	net_printf_crlf ( sock, "" );

	/* XXX - use a static (non-reentrant) net_buf */
	if ( ! net_buf_init ( sock ) ) {
	    printf ( "Trouble!\n" );
	    close ( sock );
	}

	(void) read_http_headers ();

	if ( http_verbose )
	    show_headers ();

	npay = get_http_payload_size ();
	p = get_http_payload ();
	close ( sock );

	if ( p )
	    dumpit ( p, npay );
}

char *
http_soap ( char *server, int port, char *target, char *action,
	char *req, int nreq, int *nreply )
{
    	int sock;
	char *rv;

	sock = net_client ( server, port );

	net_printf_crlf ( sock, "POST %s HTTP/1.0", target );
	net_printf_crlf ( sock, "Host: %s", server );
	net_printf_crlf ( sock, "User-agent: gTopo" );
	net_printf_crlf ( sock, "Content-type: text/xml; charset=\"UTF-8\"" );
	net_printf_crlf ( sock, "Content-length: %d", nreq );
	net_printf_crlf ( sock, "SOAPAction: \"%s\"", action );
	net_printf_crlf ( sock, "" );

	net_write ( sock, req, nreq );

	if ( ! net_buf_init ( sock ) ) {
	    printf ( "Trouble!\n" );
	    close ( sock );
	}

	(void) read_http_headers ();

	if ( http_verbose )
	    show_headers ();

	*nreply = get_http_payload_size ();
	rv = get_http_payload ();

	free_http_headers ();
	close ( sock );

	return rv;
}

/*
static char *server_name = "www.mmto.org";
static char *server_name = "terraserver-usa.com";
static char *server_name = "cholla.mmto.org";
*/
static char *server_name = "cholla.mmto.org";

/*
static char *doc_name = "/index.html";
*/
static char *doc_name = "/";

void
http_test ( void )
{
	(void) http_get ( server_name, 80, doc_name );
}

/* THE END */
