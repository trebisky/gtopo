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

enum xml_type { XT_TAG, XT_ATTR };

/* We represent an XML object as a tree of these nodes.
 */
struct xml {
	struct xml *next;
	struct xml *children;
	enum xml_type type;
	char *name;
	char *value;
	struct xml *attrib;
};

static char *xml_init = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";

static struct xml *
new_tag ( char *name )
{
	struct xml *xp;

	xp = gmalloc ( sizeof(struct xml) );
	xp->type = XT_TAG;
	xp->name = strhide ( name );
	xp->value = NULL;
	xp->attrib = NULL;
	xp->children = NULL;
	xp->next = NULL;

	return xp;
}

/* Add a tag to a document,
 * this becomes a child of the first
 * argument.  We keep the children in order.
 */
struct xml *
xml_tag ( struct xml *cp, char *name )
{
	struct xml *xp;
	struct xml *yp;

	xp = new_tag ( name );

	if ( cp->children ) {
	    yp = cp->children;
	    while ( yp->next )
	    	yp = yp->next;
	    yp->next = xp;
	} else
	    cp->children = xp;

	return xp;
}

/* Like the above, but add this as a "sibling"
 * to the first argument.
 * (will this ever be useful?)
 */
struct xml *
xml_tag_next ( struct xml *cp, char *name )
{
	struct xml *xp;

	xp = new_tag ( name );

	while ( cp->next )
	    cp = cp->next;
	cp->next = xp;

	return xp;
}

/* Add an attribute to a tag node */
/* XXX - does not preserve order, does it matter ?? */
void
xml_attr ( struct xml *cp, char *name, char *value )
{
	struct xml *xp;

	xp = gmalloc ( sizeof(struct xml) );
	xp->type = XT_ATTR;
	xp->name = strhide ( name );
	xp->value = strhide ( value );
	xp->attrib = NULL;
	xp->children = NULL;

	xp->next = cp->attrib;
	cp->attrib = xp;
}

/* Not usually directly called, if ever */
void
xml_stuff ( struct xml *xp, char *stuff )
{
	xp->value = strhide ( stuff );
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
	struct xml *xp;

	xp = new_tag ( name );

	return xp;
}

/* XXX - heaven help us if we overrun this */
#define XML_BUF_SIZE	4096
static char xml_buf[XML_BUF_SIZE];

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

void
xml_emit ( struct xml *cp )
{
	char *ep;

	ep = xml_emit_list ( xml_buf, cp, 1 );
	write ( 1, xml_buf, ep-xml_buf );
	printf ( "%d written\n", ep-xml_buf );
}

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
