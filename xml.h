/*
 *  GTopo - xml.h
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

enum xml_type { XT_ROOT, XT_TAG, XT_ATTR, XT_CDATA };

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

struct xml * xml_start ( char * );
struct xml * xml_tag ( struct xml *, char * );
struct xml * xml_tag_stuff ( struct xml *, char *, char * );
void xml_attr ( struct xml *, char *, char * );
int xml_collect ( char *, int, struct xml * );
struct xml * xml_find_tag ( struct xml *, char * );
char * xml_find_tag_value ( struct xml *, char * );

struct xml * xml_parse_doc ( char *, int );
void xml_destroy ( struct xml * );

/* THE END */
