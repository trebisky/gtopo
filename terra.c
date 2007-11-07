/*
 *  GTopo - terra.c
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

/* This file supports SOAP RPC to terraserver.
 * 10-10-2007
 */

#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include <stdlib.h>
#include <string.h>

#include "gtopo.h"
#include "protos.h"
#include "xml.h"

#define MAX_TERRA_REQ	4096
static char terra_request[MAX_TERRA_REQ];

static char *server_name = "terraserver-usa.com";
static char *server_target = "/terraservice.asmx";
static int server_port = 80;

/* The test latitude and longitude is the same used by PyTerra in its test suite,
 * since I have "wireshark" captures of its test suite in action.
 * Long = -93.0
 * Lat  =  43.0
 * 7 km SW of Rockford, Iowa, United States (nearest place).
 * UTM is Zone 15, X = 500000, Y = 4760814.7962907264
 * (my capture for ConvertLonLatPtToUtmPt is ethpy.4)
 */

void
terra_test ( void )
{
	struct xml *xp;
	struct xml *t;
	struct xml *x;
	char *action = "http://terraserver-usa.com/terraserver/ConvertLonLatPtToUtmPt";
	int n;
	int nr;
	char *reply;
	struct xml *rp;

	xp = xml_start ( "SOAP-ENV:Envelope" );
	xml_attr ( xp, "SOAP-ENV:encodingStyle", "http://schemas.xmlsoap.org/soap/encoding/" );
	xml_attr ( xp, "xmlns:SOAP-ENC", "http://schemas.xmlsoap.org/soap/encoding/" );
	xml_attr ( xp, "xmlns:xsi", "http://www.w3.org/1999/XMLSchema-instance" );
	xml_attr ( xp, "xmlns:SOAP-ENV", "http://schemas.xmlsoap.org/soap/envelope/" );
	xml_attr ( xp, "xmlns:xsd", "http://www.w3.org/1999/XMLSchema/" );

	t = xml_tag ( xp, "SOAP-ENV:Body" );
	t = xml_tag ( t, "ns1:ConvertLonLatPtToUtmPt" );
	xml_attr ( t, "xmlns:ns1", "http://terraserver-usa.com/terraserver/" );
	xml_attr ( t, "SOAP-ENC:root", "1" );
	t = xml_tag ( t, "ns1:point" );

	x = xml_tag_stuff ( t, "ns1:Lon", "-93.0" );
	xml_attr ( x, "xsi:type", "xsd:double" );
	x = xml_tag_stuff ( t, "ns1:Lat", "43.0" );
	xml_attr ( x, "xsi:type", "xsd:double" );

	n = xml_collect ( terra_request, MAX_TERRA_REQ, xp );

	printf ( "  REQUEST:\n" );
	write ( 1, terra_request, n );

	reply = http_soap ( server_name, server_port, server_target, action, terra_request, n, &nr );

	printf ( "\n" );
	printf ( "  REPLY:\n" );
	write ( 1, reply, nr );

	printf ( "\n" );
	printf ( "  RESULT:\n" );

	rp = xml_parse_doc ( reply, nr );
	free ( reply );
}

/* THE END */
