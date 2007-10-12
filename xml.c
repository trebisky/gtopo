/*
 *  GTopo - xml.c
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

/* This file is used to generate and parse XML,
 * primarily to support the SOAP interface to Terraserver
 * that is used by gtopo.
 * 10-9-2007
 */

#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include <stdlib.h>
#include <string.h>

#include "gtopo.h"
#include "protos.h"
#include "xml.h"

/* --------------------------------------------------------------------- */
/* stuff to build xml datastructure follows */
/* --------------------------------------------------------------------- */

static char *xml_init = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";

static struct xml *
new_tag ( void )
{
	struct xml *xp;

	xp = gmalloc ( sizeof(struct xml) );
	xp->type = XT_TAG;
	xp->value = NULL;
	xp->attrib = NULL;
	xp->children = NULL;
	xp->next = NULL;

	return xp;
}

static void
end_link ( struct xml **pp, struct xml *xp )
{
	struct xml *yp;

	if ( *pp ) {
	    yp = *pp;
	    while ( yp->next )
	    	yp = yp->next;
	    yp->next = xp;
	} else
	    *pp = xp;
}

/* Add a tag to a document,
 * this becomes a child of the first
 * argument.  We keep the children in order.
 */
struct xml *
xml_tag ( struct xml *cp, char *name )
{
	struct xml *xp;

	xp = new_tag ();
	xp->name = strhide ( name );

	if ( cp )
	    end_link ( &cp->children, xp );

	return xp;
}

static struct xml *
xml_tag_n ( struct xml *cp, char *name, int nname )
{
	struct xml *xp;

	xp = new_tag ();
	xp->name = strnhide ( name, nname );

	if ( cp )
	    end_link ( &cp->children, xp );

	return xp;
}

#ifdef notdef
/* Like the above, but add this as a "sibling"
 * to the first argument.
 * (will this ever be useful?)
 */
struct xml *
xml_tag_next ( struct xml *cp, char *name )
{
	struct xml *xp;

	xp = new_tag ();
	xp->name = strhide ( name );

	/* maintain order */
	while ( cp->next )
	    cp = cp->next;
	cp->next = xp;

	return xp;
}
#endif

/* Add an attribute to a tag node */
void
xml_attr ( struct xml *cp, char *name, char *value )
{
	struct xml *xp;
	struct xml *yp;

	xp = gmalloc ( sizeof(struct xml) );
	xp->type = XT_ATTR;
	xp->name = strhide ( name );
	xp->value = strhide ( value );
	xp->attrib = NULL;
	xp->children = NULL;
	xp->next = NULL;

	/* maintain order */
	end_link ( &cp->attrib, xp );
}

/* Not usually directly called, if ever */
void
xml_stuff ( struct xml *xp, char *stuff )
{
	xp->value = strhide ( stuff );
}

static void
xml_stuff_n ( struct xml *xp, char *stuff, int nstuff )
{
	xp->value = strnhide ( stuff, nstuff );
}

/* Use this for <name>stuff</name> */
struct xml *
xml_tag_stuff ( struct xml *cp, char *name, char *stuff )
{
	struct xml *xp;

	xp = xml_tag ( cp, name );
	xml_stuff ( xp, stuff );
	return xp;
}

/* Declare a tag that starts an XML document */
struct xml *
xml_start ( char *name )
{
	return xml_tag ( NULL, name );
}

/* internal, for recursion */
static char *
xml_emit_list ( char *ap, struct xml *cp, int first )
{
	struct xml *xp, *tp;

	if ( first )
	    ap += sprintf ( ap, "%s\n", xml_init );

	for ( xp = cp; xp; xp = xp->next ) {
	    if ( xp->type == XT_TAG ) {
	    	ap += sprintf ( ap, "<%s", xp->name );
		for ( tp = xp->attrib; tp; tp = tp->next ) {
		    ap += sprintf ( ap, " %s=\"%s\"", tp->name, tp->value );
		}
	    	ap += sprintf ( ap, ">" );
		if ( xp->value )
		    ap += sprintf ( ap, "%s", xp->value );
		else
		    ap += sprintf ( ap, "\n" );
		if ( xp->children )
		    ap = xml_emit_list ( ap, xp->children, 0 );
	    	ap += sprintf ( ap, "</%s>\n", xp->name );
	    } else {
	    	error ( "xml emit: %s", xp->name );
	    }
	}
	return ap;
}

int
xml_collect ( char *buf, int limit, struct xml *xp )
{
	char *ep;
	int rv;

	ep = xml_emit_list ( buf, xp, 1 );
	rv = ep - buf;

	if ( rv > limit )
	    error ("xml_collect: overrun %d --> %d", limit, rv );

	return rv;
}

/* XXX - heaven help us if we overrun this */
#define XML_BUF_SIZE	4096
static char xml_buf[XML_BUF_SIZE];

void
xml_emit ( struct xml *xp )
{
	int n;

	n = xml_collect ( xml_buf, XML_BUF_SIZE, xp );
	write ( 1, xml_buf, n );
}

/* --------------------------------------------------------------------- */
/* stuff to parse xml follows */
/* --------------------------------------------------------------------- */

static char *
skip_to ( char *p, char *ep, int what )
{
	for ( ; p < ep; p++ ) {
	    if ( *p == what )
	    	return p;
	}
	return NULL;
}

static char *
skip_white ( char *p, char *ep )
{
	for ( ; p < ep; p++ ) {
	    if ( *p != ' ' )
	    	return p;
	}
	return NULL;
}

static char *
skip_tag ( char *p, char *ep )
{
	for ( ; p < ep; p++ ) {
	    if ( *p == ' ' || *p == '>' )
	    	return p;
	}
	return NULL;
}

/* recursive tag parser */
static char *
xml_parse_tag_list ( struct xml **rxp, struct xml *xp, char *buf, char *ebuf )
{
	struct xml *fp = NULL;
	struct xml *np, *cp;
	char *p, *ep, *lp;

	p = buf;

	while ( p < ebuf ) {

	    if ( *p != '<' )
	    	error ( "whoa dude; xml parse hosed: %s", p );
	    if ( p[1] == '/' )
	    	break;

	    ep = skip_tag ( p, ebuf );
	    np = xml_tag_n ( xp, p, ep-p );

	    /* skip attributes (for now) */
	    ep = skip_to ( ep, ebuf, '>' );
	    ep++;

	    if ( *ep != '<' ) {
		lp = skip_to ( ep, ebuf, '<' );
		xml_stuff_n ( np, ep, lp-ep );
		ep = lp;
	    }

	    if ( ep[1] == '/' ) {
		lp = skip_tag ( ep+1, ebuf );
		/* could check for match against name in np */
		p = lp + 1;
		continue;
	    }
	    p = xml_parse_tag_list ( &cp, np, ep, ebuf );
	}

	*rxp = fp;
	return p;
}

struct xml *
xml_parse_doc ( char *buf, int nbuf )
{
	char *p, *ep;
	char *end = &buf[nbuf];
	struct xml *rv;

	p = skip_to ( buf, end, '<' );
	if ( !p )
	    error ("xml_parse_doc: bogus xml (1)" );

	p = skip_to ( p+1, end, '>' );
	p = skip_to ( p+1, end, '<' );
	if ( !p )
	    error ("xml_parse_doc: bogus xml (1)" );

	ep = xml_parse_tag_list ( &rv, NULL, p, end );
	if ( ep == end )
	    printf ( "Xml document parse OK\n" );
	else
	    printf ( "Xml document parse fails: %d - %d %s\n", end, ep, ep );
	return rv;
}

/* --------------------------------------------------------------------- */

void
xml_test ( void )
{
	struct xml *xp;
	struct xml *t;
	struct xml *x;

	xp = xml_start ( "SOAP_ENV:Envelope" );
	xml_attr ( xp, "SOAP-ENV:encodingStyle", "http://schemas.xmlsoap.org/soap/encoding/" );
	xml_attr ( xp, "xmlns:SOAP-ENC", "http://schemas.xmlsoap.org/soap/encoding/" );
	xml_attr ( xp, "xmlns:xsi", "http://www.w3.org/1999/XMLSchema-instance" );
	xml_attr ( xp, "xmlns:SOAP-ENV", "http://schemas.xmlsoap.org/soap/envelope/" );
	xml_attr ( xp, "xmlns:xsd", "http://www.w3.org/1999/XMLSchema/" );

	t = xml_tag ( xp, "SOAP-ENV:Body" );
	t = xml_tag ( t, "ns1:ConvertLatPtToUtmPt" );
	xml_attr ( t, "xmlns:ns1", "http://terraserver-usa.com/terraserver/" );
	xml_attr ( t, "SOAP-ENC:root", "1" );
	t = xml_tag ( t, "ns1:point" );

	x = xml_tag_stuff ( t, "ns1:Lon", "-93.0" );
	xml_attr ( x, "type", "xsd:double" );
	x = xml_tag_stuff ( t, "ns1:Lat", "43.0" );
	xml_attr ( x, "type", "xsd:double" );

	xml_emit ( xp );
}

/* THE END */
