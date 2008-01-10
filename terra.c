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

int terra_verbose = 0;

#define MAX_TERRA_REQ	4096
static char terra_request[MAX_TERRA_REQ];

static char *server_name = "terraserver-usa.com";
static char *server_target = "/terraservice.asmx";
static int server_port = 80;

struct terra_loc {
	double lon;
	double lat;
	int zone;
	double x;
	double y;
};

int
terra_to_utm ( struct terra_loc *tlp )
{
	struct xml *xp;
	struct xml *t;
	struct xml *x;
	char *action = "http://terraserver-usa.com/terraserver/ConvertLonLatPtToUtmPt";
	int n;
	int nr;
	char *reply;
	struct xml *rp;
	struct xml *xx;
	char *val;
	char value[64];

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

	sprintf ( value, "%.6f", tlp->lon );
	x = xml_tag_stuff ( t, "ns1:Lon", value );
	xml_attr ( x, "xsi:type", "xsd:double" );

	sprintf ( value, "%.6f", tlp->lat );
	x = xml_tag_stuff ( t, "ns1:Lat", value );
	xml_attr ( x, "xsi:type", "xsd:double" );

	n = xml_collect ( terra_request, MAX_TERRA_REQ, xp );

	if ( terra_verbose ) {
	    printf ( "  REQUEST:\n" );
	    write ( 1, terra_request, n );
	}

	reply = http_soap ( server_name, server_port, server_target, action, terra_request, n, &nr );

	if ( terra_verbose ) {
	    printf ( "\n" );
	    printf ( "  REPLY:\n" );
	    write ( 1, reply, nr );

	    printf ( "\n" );
	    printf ( "  RESULT:\n" );
	}

	rp = xml_parse_doc ( reply, nr );
	free_http_soap ( (void *) reply );

	xx = xml_find_tag ( rp, "ConvertLonLatPtToUtmPtResult" );
	if ( ! xx )
	    return 0;

	val = xml_find_tag_value ( xx, "Zone" );
	if ( ! val )
	    return 0;
	sscanf ( val, "%d", &tlp->zone );

	val = xml_find_tag_value ( xx, "X" );
	if ( ! val )
	    return 0;
	sscanf ( val, "%lf", &tlp->x );

	val = xml_find_tag_value ( xx, "Y" );
	if ( ! val )
	    return 0;
	sscanf ( val, "%lf", &tlp->y );

	return 1;
}

int
to_utm ( struct terra_loc *tlp )
{
	int zone;
	double lon_cm;

	tlp->zone = (tlp->lon + 186.0) / 6.0;
	lon_cm = -183.0 + tlp->zone * 6.0;
	printf ( "Central Meridian: %.2f\n", lon_cm );
	return 1;
}

static void
terra_ll_test1 ( double lon, double lat )
{
	struct terra_loc loc;
	int rv;

	loc.lon = lon;
	loc.lat = lat;

	printf ( "---------\n" );
	printf ( "  Long: %.2f\n", loc.lon );
	printf ( "  Lat:  %.2f\n", loc.lat );

	rv = terra_to_utm ( &loc );

	if ( ! rv ) {
	    printf ( "Fails!\n" );
	    return;
	}


	printf ( " Zone: %d\n", loc.zone );
	printf ( "  X: %.5f\n", loc.x );
	printf ( "  Y: %.5f\n", loc.y );

	(void) to_utm ( &loc );
	printf ( "TZone: %d\n", loc.zone );
}

void
terra_test ( void )
{
	/* This is used by PyTerra in it's test suite:
	 * 7 km SW of Rockford, Iowa, United States (nearest place).
	 * UTM is Zone 15, X = 500000, Y = 4760814.7962907264
	 * (my capture for ConvertLonLatPtToUtmPt is ethpy.4)
	 */
	terra_ll_test1 ( -93.0, 43.0 );
	terra_ll_test1 ( -93.0, 43.0 );

	/* This is the CN Tower in Toronto, Canada
	 */
	terra_ll_test1 ( -dms2deg(79, 23, 13.7), dms2deg(43,38,33.24) );
}

void
terra_test_B ( void )
{
	struct xml *xp;
	struct xml *t;
	struct xml *x;
	char *action = "http://terraserver-usa.com/terraserver/ConvertLonLatPtToUtmPt";
	int n;
	int nr;
	char *reply;
	struct xml *rp;
	struct xml *xx;
	char *val;

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

	if ( terra_verbose ) {
	    printf ( "  REQUEST:\n" );
	    write ( 1, terra_request, n );
	}

	reply = http_soap ( server_name, server_port, server_target, action, terra_request, n, &nr );

	if ( terra_verbose ) {
	    printf ( "\n" );
	    printf ( "  REPLY:\n" );
	    write ( 1, reply, nr );

	    printf ( "\n" );
	    printf ( "  RESULT:\n" );
	}

	rp = xml_parse_doc ( reply, nr );
	free_http_soap ( reply );

	xx = xml_find_tag ( rp, "ConvertLonLatPtToUtmPtResult" );
	if ( ! xx )
	    printf ( "Could not find it\n" );

	val = xml_find_tag_value ( xx, "Zone" );
	if ( val )
	    printf ( "Zone: %s\n", val );

	val = xml_find_tag_value ( xx, "X" );
	if ( val )
	    printf ( "X: %s\n", val );
	val = xml_find_tag_value ( xx, "Y" );
	if ( val )
	    printf ( "Y: %s\n", val );
}

/* THE END */
