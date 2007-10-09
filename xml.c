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

enum xml_type { XT_ROOT, XT_TAG };

/* We represent an XML object as a tree of these nodes.
 */
struct xml {
	struct xml *children;
	enum xml_type type;
	char *tagname;
};

void
xml_init ( void )
{
}

struct xml *
xml_start ( char *tagname )
{
	struct xml *xp;

	xp = gmalloc ( sizeof(struct xml) );
	xp->type = XT_ROOT;
	xp->tagname = strhide ( tagname );

	return xp;
}

void
xml_attr ( struct xml *xp, char *attrname, char *attrvalue )
{
}

/* THE END */
