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

enum xml_type { XT_ROOT, XT_TAG, XT_ATTR };

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

static char *xml_bogus = "bogus";

void
xml_init ( void )
{
}

static struct xml *
new_tag ( char *name )
{
	struct xml *xp;

	xp = gmalloc ( sizeof(struct xml) );
	xp->type = XT_TAG;
	xp->name = strhide ( name );
	xp->value = xml_bogus;
	xp->attrib = NULL;
	xp->children = NULL;
	xp->next = NULL;

	return xp;
}

/* Add a tag to a document */
struct xml *
xml_tag ( struct xml *cp, char *name )
{
	struct xml *xp;

	xp = new_tag ( name );

	xp->next = cp->children;
	cp->children = xp;

	return xp;
}

/* Add an attribute to a tag node */
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
void
xml_tag_stuff ( struct xml *cp, char *name, char *stuff )
{
	struct xml *xp;

	xp = xml_tag ( cp, name );
	xml_stuff ( xp, stuff );
}

/* Declare a tag that starts an XML document */
struct xml *
xml_start ( char *name )
{
	return new_tag ( name );
}

/* THE END */
