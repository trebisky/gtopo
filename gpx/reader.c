/*
 *  GTopo - reader.c
 *
 *  Copyright (C) 2016, Thomas J. Trebisky
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
#include "gtopo.h"
#include "protos.h"
*/

char * strhide ( char * );
char * strnhide ( char *, int );
void * gmalloc ( int );
void error ( char *msg, ... );

struct xml_attr {
	struct xml_attr *next;
	char *name;
	char *value;
};

struct xml_element {
	char *name;
	struct xml_element *next;
	struct xml_attr *attr;
	struct xml_element *child;
	char *cdata;
};

static char *
skip_white ( char *p, char *ep )
{
	for ( ; p < ep; p++ ) {
	    if ( *p != ' ' && *p != '\n' )
	    	return p;
	}
	return NULL;
}

static char *
skip_to ( char *p, char *ep, int what )
{
	for ( ; p < ep; p++ ) {
	    if ( *p == what )
	    	return p;
	}
	return NULL;
}

/* Only used by attribute parser */
static char *
skip_to_white ( char *p, char *ep )
{
	for ( ; p < ep; p++ )
	    if ( *p == ' ' || *p == '\n' )
	    	break;

	return p;
}

/* Just for debugging */
static void
printstrn ( char *s, int n )
{
	while ( n-- )
	    putchar ( *s++ );
}

/* delimit everything in a single attribute */
static char *
scan_attr ( char *buf, char *end )
{
	char *p;
	int in_quote = 0;

	for ( p=buf; p < end; p++ ) {
	    if ( in_quote ) {
		if ( *p == '"' )
		    in_quote = 0;
		continue;
	    }
	    if ( *p == '"' ) {
		in_quote = 1;
		continue;
	    }

	    if ( *p == ' ' || *p == '\n' )
		break;
	}

	return p;
}

struct xml_attr *
attr_new ( char *start, char *end )
{
	struct xml_attr *rv;
	char *p;

	rv = gmalloc ( sizeof(struct xml_attr) );
	rv->next = NULL;

	for ( p=start; p < end; p++ )
	    if ( *p == '=' )
		break;
	rv->name = strnhide ( start, p-start );

	printf ( "attr: " ); printstrn ( start, end-start ); putchar ( '\n' );

	if ( p < end )
	    p++;
	if ( p < end )
	    p++;
	if ( p < end )
	    end--;

	rv->value = strnhide ( p, end-p );

	printf ( "attr-name: %s\n", rv->name );
	printf ( "attr-value: %s\n", rv->value );
	return rv;
}

/* Called with the guts of a start tag.
 * parse it out into the name and list
 * of attributes.
 */
static struct xml_element *
element_new ( char *start, char *end )
{
	struct xml_element *rv;
	struct xml_attr *ap;
	struct xml_attr *tmp;
	char *p;
	char *pp;
	char *extra;

	rv = gmalloc ( sizeof(struct xml_element) );
	rv->next = NULL;
	rv->attr = NULL;
	rv->child = NULL;
	rv->cdata = NULL;

	p = skip_to_white ( start, end );
	if ( p >= end ) {
	    rv->name = strnhide ( start, end-start );
	    printf ( "NEW element: %s\n", rv->name );
	    return rv;
	}

	rv->name = strnhide ( start, p-start );

	extra = strnhide ( p, end-p );
	printf ( "NEW element: %s + %s\n", rv->name, extra );
	free ( extra );

	while ( p < end ) {
	    p = skip_white ( p, end );
	    if ( ! p )
		break;
	    pp = scan_attr ( p, end );
	    ap = attr_new ( p, pp );
	    p = pp;

	    if ( ! rv->attr )
		rv->attr = ap;
	    else {
		tmp = rv->attr;
		while ( tmp->next )
		    tmp = tmp->next;
		tmp->next = ap;
	    }
	    continue;
	}

	return rv;
}

/* delimit everything in an element start tag, including attributes */
static char *
scan_tag ( char *buf, char *end )
{
	char *p;
	int in_quote = 0;

	for ( p=buf; p < end; p++ ) {
	    if ( in_quote ) {
		if ( *p == '"' )
		    in_quote = 0;
		continue;
	    }
	    if ( *p == '"' ) {
		in_quote = 1;
		continue;
	    }

	    if ( *p == '/' || *p == '>' )
		break;
	}

	return p;
}

/* Should get called pointing to
 *    the intial "<" in a start tag.
 */
char *
xml_parse_element ( char *buf, char *end, struct xml_element **ep )
{
	char *p;
	char *cp;
	struct xml_element *elem;
	struct xml_element *child;
	struct xml_element *tmp;

	/* Skip the '<' */
	buf++;
	
	p = scan_tag ( buf, end );
	elem = element_new ( buf, p );

	/* Deal with empty element tags */
	if ( *p == '/' && p[1] == '>' ) {
	    *ep = elem;
	    return p + 2;
	}

	/* Skip the ">" */
	p++;

	/* inside an element now, may have
	 * more elements and/or cdata
	 * (or the end of this element).
	 */

	while ( p < end ) {
	    p = skip_white ( p, end );

	    /* check for end of this element */
	    if ( *p == '<' && p[1] == '/' ) {
		p = skip_to ( p, end, '>' );
		*ep = elem;
		return p + 1;
	    }

	    /* check for special CDATA thing */
	    if ( *p == '<' && p[1] == '!' && strncmp ( p, "<![CDATA[", 9 ) == 0 ) {
		p += 9;
		for ( cp = p; cp < end; cp++ ) {
		    if ( *cp == ']' && strncmp ( cp, "]]>", 3 ) == 0 )
			break;
		}
		elem->cdata = strnhide ( p, cp-p );
		printf ( "CDATA2: %s\n", elem->cdata );
		p = cp + 3;
		continue;
	    }

	    /* check for and skip comments */
	    if ( *p == '<' && p[1] == '!' && strncmp ( p, "<!--", 4 ) == 0 ) {
		p += 4;
		for ( cp = p; cp < end; cp++ ) {
		    if ( *cp == '-' && strncmp ( cp, "-->", 3 ) == 0 )
			break;
		}
		p = cp + 3;
		continue;
	    }

	    /* another element */
	    if ( *p == '<' ) {
		p = xml_parse_element ( p, end, &child );
		if ( ! elem->child )
		    elem->child = child;
		else {
		    tmp = elem->child;
		    while ( tmp->next )
			tmp = tmp->next;
		    tmp->next = child;
		}
		continue;
	    }

	    /* must be cdata here */
	    cp = skip_to ( p, end, '<' );
	    elem->cdata = strnhide ( p, cp-p );
	    printf ( "CDATA1: %s\n", elem->cdata );
	    p = cp;
	}

	/* Should not return this way */
	*ep = elem;
	return p;
}

/* Check for the XML "header" at the start of the file */
int
xml_verify ( char *buf, int nbuf )
{
	char *p;

	p = skip_to ( buf, &buf[nbuf], '<' );
	if ( !p )
	    return 0;

	nbuf -= p - buf;

	if ( nbuf < 5 )
	    return 0;

	if ( strncmp ( buf, "<?xml", 5 ) != 0 )
	    return 0;

	return 1;
}

/* We could do a lot more, but we just map
 * tabs and CR to spaces.  We could also do this
 * for newlines, as well as compressing all runs
 * of white space into just one space.
 */
static void
tidy_white ( char *buf, char *end )
{
	char *p;

	for ( p=buf; p < end; p++ )
	    if ( *p == '\t' || *p == '\r' )
		*p = ' ';
}

void *
xml_parser ( char *buf, int nbuf )
{
	char *p, *ep;
	char *end = &buf[nbuf];
	struct xml_element *base;

	tidy_white ( buf, end );
	printf ( "Tidy\n" );

	if ( xml_verify ( buf, nbuf ) == 0 )
	    return NULL;

	printf ( "Tidy and verified\n" );

	/* Skip xml header */
	p = skip_to ( buf, end, '<' );
	if ( !p )
	    return NULL;

	p = skip_to ( p+1, end, '>' );

	/* Should be past <?xml thing,
	 * there may be white space to skip
	 */
	p = skip_to ( p+1, end, '<' );
	if ( !p )
	    return NULL;

	printf ( "Begin parse\n" );

	ep = xml_parse_element ( p, end, &base );
	return (void *) base;
}

static void
xml_element_free ( struct xml_element *ep )
{
	struct xml_element *xp, *x_next;
	struct xml_attr *ap, *a_next;

	free ( ep->name );
	if ( ep->cdata )
	    free ( ep->cdata );

	if ( ep->attr ) {
	    for ( ap=ep->attr; ap; ap = a_next ) {
		a_next = ap->next;
		free ( ap->name );
		free ( ap->value );
		free ( ap );
	    }
	}

	if ( ep->child ) {
	    for ( xp=ep->child; xp; xp = x_next ) {
		x_next = xp->next;
		xml_element_free ( xp );
	    }
	}
}

/* free up memory associated with an XML document */
void
xml_free ( void *arg )
{
	xml_element_free ( (struct xml_element *) arg );
}

/******************************************************************/
/******************************************************************/
/******************************************************************/

static void
waypoint ( struct xml_element *wp )
{
	struct xml_element *xp;
	struct xml_attr *ap;

	for ( ap=wp->attr; ap; ap = ap->next ) {
	    printf ( "attr: %s = %s\n", ap->name, ap->value );
	}
	for ( xp=wp->child; xp; xp = xp->next ) {
	    printf ( "%s %s\n", xp->name, xp->cdata );
	}
}

static void
track ( struct xml_element *wp )
{
	struct xml_element *xp;
	struct xml_element *sp;
	struct xml_element *tp;
	struct xml_attr *ap;
	char *lat;
	char *lon;
	char *ele;
	char *time;

	for ( xp=wp->child; xp; xp = xp->next ) {
	    if ( strcmp ( xp->name, "name" ) == 0 )
		printf ( "track %s %s\n", xp->name, xp->cdata );
	    else
		printf ( "%s\n", xp->name );
	}

	for ( xp=wp->child; xp; xp = xp->next ) {
	    if ( strcmp ( xp->name, "name" ) == 0 )
		printf ( "track-- %s %s\n", xp->name, xp->cdata );
	    if ( strcmp ( xp->name, "trkseg" ) == 0 ) {
		printf ( "trkseg\n" );
		for ( sp=xp->child; sp; sp = sp->next ) {
		    if ( strcmp ( sp->name, "trkpt" ) != 0 )
			continue;
		    lon = "-"; lat = "-";
		    for ( ap=sp->attr; ap; ap = ap->next ) {
			if ( strcmp(ap->name, "lat") == 0 )
			    lat = ap->value;
			if ( strcmp(ap->name, "lon") == 0 )
			    lon = ap->value;
		    }
		    ele = ""; time = "";
		    for ( tp=sp->child; tp; tp = tp->next ) {
			if ( strcmp(tp->name, "ele") == 0 )
			    ele = tp->cdata;
			if ( strcmp(tp->name, "time") == 0 )
			    time = tp->cdata;
		    }
		    printf ( "trkpt: %s %s %s %s\n", lon, lat, ele, time );
		}
	    }
	}

	printf ( "\n" );
}

void
xml_test ( void *arg )
{
	struct xml_element *rp;
	struct xml_element *xp;

	printf ( "\n\nTEST\n" );

	rp = arg;
	printf ( "Root: %s\n", rp->name );

	/*
	for ( xp=rp->child; xp; xp = xp->next ) {
	    printf ( "El: %s\n", xp->name );
	}
	*/

	for ( xp=rp->child; xp; xp = xp->next ) {
	    if ( strcmp ( xp->name, "wpt" ) == 0 )
		waypoint ( xp );
	}

	printf ( "\n" );

	for ( xp=rp->child; xp; xp = xp->next ) {
	    if ( strcmp ( xp->name, "trk" ) == 0 )
		track ( xp );
	}
}

/* THE END */
