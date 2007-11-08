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
/* code to build an xml tree follows */
/* --------------------------------------------------------------------- */

static char *xml_init = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";

static void
init_node ( struct xml *xp, int type )
{
	xp->type = type;
	xp->value = NULL;
	xp->attrib = NULL;
	xp->children = NULL;
	xp->next = NULL;
}

static struct xml *
new_node ( int type )
{
	struct xml *xp;

	xp = gmalloc ( sizeof(struct xml) );
	init_node ( xp, type );

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
 * this becomes a child of the first argument.
 * (the first argument is the parent).
 * We keep the children in order.
 */
struct xml *
xml_tag ( struct xml *pp, char *name )
{
	struct xml *xp;

	xp = new_node ( XT_TAG );
	xp->name = strhide ( name );

	if ( pp )
	    end_link ( &pp->children, xp );

	return xp;
}

static struct xml *
xml_tag_n ( struct xml *pp, char *name, int nname )
{
	struct xml *xp;

	xp = new_node ( XT_TAG );
	xp->name = strnhide ( name, nname );

	if ( pp )
	    end_link ( &pp->children, xp );

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

	xp = new_node ( XT_TAG );
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

/* This does it right */
static struct xml *
xml_cdata_n ( struct xml *pp, char *name, int nname )
{
	struct xml *xp;

	xp = new_node ( XT_CDATA );
	xp->value = strnhide ( name, nname );

	if ( pp )
	    end_link ( &pp->children, xp );

	return xp;
}

static struct xml *
xml_cdata ( struct xml *pp, char *name )
{
	struct xml *xp;

	xp = new_node ( XT_CDATA );
	xp->value = strhide ( name );

	if ( pp )
	    end_link ( &pp->children, xp );

	return xp;
}

/* STUFF - should be called CDATA.
 * XXX - deprecated.
 * (see above for the right way to do this).
 * We can have multiple CDATA mixed with tags under
 * a xml tag as children, the above routines
 * handle this, these do not.
 */

/* Not usually directly called, if ever */
void
xml_stuff ( struct xml *xp, char *stuff )
{
	xp->value = strhide ( stuff );
}

/* XXX - stuff is really CDATA */
static void
xml_stuff_n ( struct xml *xp, char *stuff, int nstuff )
{
	xp->value = strnhide ( stuff, nstuff );
}

/* Use this for <name>stuff</name> */
/* XXX - stuff is really CDATA */
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
	    } else if ( xp->type == XT_CDATA ) {
		if ( xp->value )
		    ap += sprintf ( ap, "%s", xp->value );
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

struct xml *
xml_find_tag ( struct xml *root, char *name )
{
	struct xml *xp, *cp;

	for ( xp = root; xp; xp = xp->next ) {
	    if ( xp->type != XT_TAG )
	    	continue;

	    if ( strcmp ( xp->name, name ) == 0 )
	    	return xp;

	    if ( xp->children ) {
		cp = xml_find_tag ( xp->children, name );
		if ( cp )
		    return cp;
	    }
	}

	return NULL;
}

char *
xml_find_tag_value ( struct xml *root, char *name )
{
	struct xml *xp;
	struct xml *cp;

	xp = xml_find_tag ( root, name );
	if ( xp && xp->value )
	    return xp->value;

	for ( cp = xp->children; cp; cp = cp->next )
	    if ( cp->type == XT_CDATA )
		    return cp->value;

	return NULL;
}

/* --------------------------------------------------------------------- */
/* code to parse xml follows */
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

/* recursive tag parser
 *
 * returns - first character not parsed.
 * arguments:
 * 	xp - parent for whatever this generates.
 * 	buf - first character of text to parse
 * 	ebuf - first character beyond end of text
 *
 * This is quite simplistic, here are a couple of things
 *  it does not handle:
 *
 *  1) tags of the form <xxx/>
 *  2) mixed multiple cdata and tags.
 */
static char *
xml_parse_tag_list ( struct xml *xp, char *buf, char *ebuf )
{
	struct xml *np;
	char *p, *ep;
	int in_tag = 0;

	p = buf;
	/*
	printf ( "Start parse:%s\n", p );
	*/

	while ( p < ebuf ) {

	    if ( *p != '<' )
	    	error ( "oops; xml parse hosed:%s", p );

	    /* end tag */
	    if ( p[1] == '/' ) {
		if ( in_tag == 0 )
		    break;
		in_tag--;
		ep = skip_tag ( p+2, ebuf );
		/* could check for match against name in np */
		p = ep + 1;
		continue;
	    }

	    if ( in_tag ) {
		p = xml_parse_tag_list ( np, p, ebuf );
		continue;
	    }

	    /* start tag */
	    p++;
	    in_tag++;
	    ep = skip_tag ( p, ebuf );
	    np = xml_tag_n ( xp, p, ep-p );
	    /*
	    printf ( "New tag: %s\n", np->name );
	    */

	    /* skip attributes (for now) */
	    p = 1 + skip_to ( ep, ebuf, '>' );
	    /*
	    printf ( "after attrib:%s\n", p );
	    */

	    /* stuff (cdata) */
	    /* XXX - can be any number of these */
	    if ( *p != '<' ) {
		ep = skip_to ( p, ebuf, '<' );
		xml_stuff_n ( np, p, ep-p );
		/*
		printf ( "Cdata: %s\n", np->value );
		*/
		p = ep;
	    }
	}

	/*
	printf ( "End parse:%s\n", p );
	*/
	return p;
}

struct xml *
xml_parse_doc ( char *buf, int nbuf )
{
	char *p, *ep;
	char *end = &buf[nbuf];
	struct xml base;

	p = skip_to ( buf, end, '<' );
	if ( !p )
	    error ("xml_parse_doc: bogus xml (1)" );

	p = skip_to ( p+1, end, '>' );
	p = skip_to ( p+1, end, '<' );
	if ( !p )
	    error ("xml_parse_doc: bogus xml (1)" );

	init_node ( &base, XT_ROOT );
	ep = xml_parse_tag_list ( &base, p, end );
	if ( ep != end )
	    printf ( "Xml document parse fails: %d - %d %s\n", end, ep, ep );

	return base.children;
}

/* --------------------------------------------------------------------- */

/* Assemble an XML tree, then spew out the XML that would respresent it */
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
