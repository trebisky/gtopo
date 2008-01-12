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
#include <math.h>

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

static double grs80_a = 6378137.0;
static double grs80_b = 6356752.3;
static double grs80_ee = 0.0066943800;
/*
static double grs80_f = 1.0 / 298.257;
*/
static double k0 = 0.9996;

/* This calculates the true distance along the
 * Central Meridian from the equator to the latitude given.
 */
static double
calc_m ( double lat_rad )
{
    	double m1, m2, m3, m4;
	double e2, e4, e6;
	double sin_2lat, sin_4lat, sin_6lat;

	e2 = grs80_ee;
	e4 = e2 * e2;
	e6 = e4 * e6;

	sin_2lat = sin ( 2.0 * lat_rad );
	sin_4lat = sin ( 4.0 * lat_rad );
	sin_6lat = sin ( 6.0 * lat_rad );

	m1 = ( 1.0 - e2/4.0 - 3.0 * e4 / 64.0 - 5.0 * e6 / 256.0 ) * lat_rad;
	m2 = -( 3.0 * e2 / 8.0 + 3.0 * e4 / 32.0 + 45.0 * e6 / 1024.0 ) * sin_2lat;
	m3 = ( 15.0 * e4 / 256.0 + 45.0 * e6 / 1024.0 ) * sin_4lat;
	m4 = -( 35.0 * e6 / 3072.0 ) * sin_6lat;

	return grs80_a * ( m1 + m2 + m3 + m4 );
}

int
to_utm ( struct terra_loc *tlp )
{
	int zone;
	double lon_cm, lon_cm_rad;;
	double eep;
	double lat_rad, lon_rad;
	double lon_loc_rad;
	double n, t, a, c;
	double sin_lat, cos_lat, tan_lat;
	double m, m0;
	double y1, y2;

	tlp->zone = (tlp->lon + 186.0) / 6.0;
	lon_cm = -183.0 + tlp->zone * 6.0;
	lon_cm_rad = lon_cm * DEGTORAD;

	printf ( "Central Meridian: %.2f\n", lon_cm );

	lat_rad = tlp->lat * DEGTORAD;
	lon_rad = tlp->lon * DEGTORAD;
	lon_loc_rad = lon_rad - lon_cm_rad;
	sin_lat = sin ( lat_rad );
	cos_lat = cos ( lat_rad );
	tan_lat = sin_lat / cos_lat;

	eep = grs80_ee / ( 1.0 - grs80_ee );
	n = grs80_a / sqrt ( 1.0 - grs80_ee * sin_lat * sin_lat ); 
	t = tan_lat * tan_lat;
	c = eep * cos_lat * cos_lat;
	a = lon_loc_rad * cos_lat;
	m = calc_m ( lat_rad );
	/*
	m0 = calc_m ( 0.0 );
	*/
	m0 = 0.0;

	printf ( "n = %.5f\n", n );
	printf ( "t = %.5f\n", t );
	printf ( "c = %.5f\n", c );
	printf ( "a = %.5f\n", a );
	printf ( "m = %.5f\n", m );

	tlp->x = 500000.0 + k0 * n * ( a + (1.0-t*c)*a*a*a/6.0 + (5.0-18.0*t+t*t+72.0*c-58.0*eep)*a*a*a*a*a/120.0);
	y1 = (5.0-t+9.0*c+4.0*c*c)*a*a*a*a/24.0;
	y2 = (61.0-58.0*t+t*t+600.0*c-330.0*eep)*a*a*a*a*a*a/720.0;
	tlp->y = k0 * (m - m0 + n * tan_lat * (a*a/2.0 + y1 * y2) );
	return 1;
}

int
to_ll ( struct terra_loc *tlp )
{
	double lon_cm_rad;
    	double m, mu, e1;
	double e2, e4, e6;
	double sqe;
	double eep;
	double sin_2mu, sin_4mu, sin_6mu, sin_8mu;
	double foot1, foot2, foot3, foot4;
	double l_1, l_2;
	double foot_lat;
	double sin_foot, cos_foot, tan_foot;
	double temp1, temp2;
	double c1, t1, n1, r1, d;

	lon_cm_rad = (-183.0 + tlp->zone * 6.0) * DEGTORAD;

	eep = grs80_ee / ( 1.0 - grs80_ee );

	e2 = grs80_ee;
	e4 = e2 * e2;
	e6 = e4 * e6;

	sqe = sqrt(1.0 - e2);

	m = tlp->y / k0;
	mu = m / (grs80_a*(1.0 - e2/4.0 - 3.0*e4/64.0 -5.0*e6/256.0));
	e1 = (1.0 - sqe) / (1.0 + sqe);

	sin_2mu = sin ( 2.0 * mu );
	sin_4mu = sin ( 4.0 * mu );
	sin_6mu = sin ( 6.0 * mu );
	sin_8mu = sin ( 8.0 * mu );

	foot1 = 3.0 * e1 / 2.0 - 27.0 * e1*e1*e1 / 32.0;
	foot2 = 21.0 * e1*e1 / 16.0 - 55.0 * e1*e1*e1*e1 / 32.0;
	foot3 = 151.0 * e1*e1*e1 / 96.0;
	foot4 = 1097.0 * e1*e1*e1*e1 / 512.0;

	foot_lat = mu + foot1 * sin_2mu + foot2 * sin_4mu + foot3 * sin_6mu + foot4 * sin_8mu;
	sin_foot = sin ( foot_lat );
	cos_foot = cos ( foot_lat );
	tan_foot = sin_foot / cos_foot;

	c1 = eep * cos_foot * cos_foot;
	t1 = tan_foot * tan_foot;
	temp1 = 1.0 - e2*sin_foot*sin_foot;
	temp2 = sqrt ( temp1 );
	n1 = grs80_a / temp2;
	r1 = grs80_a * ( 1.0 - e2) / (temp1 * temp2 );
	d = (tlp->x - 500000.0) / (n1 * k0);

	l_1 = 5.0 + 3.0 * t1 + 10.0 * c1 - 4.0 * c1*c1 - 9.0 * eep;
	l_2 = 61.0 + 90.0 * t1 + 298.0 * c1 + 45.0 * t1*t1 - 252.0 * eep - 3.0 * c1*c1;
	tlp->lat = (foot_lat - n1*tan(foot_lat) * ( d*d/2.0 - l_1*d*d*d*d/24.0 + l_2*d*d*d*d*d*d / 720.0)) * RADTODEG;

	l_1 = 1.0 + 2.0 * t1 + c1;
	l_2 = 5.0 - 2.0 * c1 + 28.0 * t1 - 3.0 * c1*c1 + 8.0 * eep + 24.0 * t1*t1;
	tlp->lon = (lon_cm_rad + (d - l_1 * d*d*d/6.0 + l_2 * d*d*d*d*d/120.0) / cos_foot) * RADTODEG;

	return 1;
}

/* Test using Terraserver */
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
	    printf ( "Terraserver request fails!\n" );
	    return;
	}

	printf ( "Terraserver response:\n" );
	printf ( " Zone: %d\n", loc.zone );
	printf ( "  X: %.5f\n", loc.x );
	printf ( "  Y: %.5f\n", loc.y );
}

/* Test using my functions */
static void
terra_ll_test2 ( double lon, double lat )
{
	struct terra_loc loc;
	int rv;

	loc.lon = lon;
	loc.lat = lat;

	printf ( "---------\n" );
	printf ( "  Long: %.2f\n", loc.lon );
	printf ( "  Lat:  %.2f\n", loc.lat );

	(void) to_utm ( &loc );

	printf ( "Local function :\n" );
	printf ( " Zone: %d\n", loc.zone );
	printf ( "  X: %.5f\n", loc.x );
	printf ( "  Y: %.5f\n", loc.y );

	(void) to_ll ( &loc );
	printf ( "Inverse function :\n" );
	printf ( "  Long: %.2f\n", loc.lon );
	printf ( "  Lat:  %.2f\n", loc.lat );
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
	terra_ll_test2 ( -93.0, 43.0 );

	/* This is the CN Tower in Toronto, Canada
	 * UTM is 630,084 meters east, 4,833,438 meters north in zone 17T
	 * Terraserver gives: X = 630084.30083,  Y = 4833438.58589
	 *
	 */
	/*
	terra_ll_test1 ( -dms2deg(79, 23, 13.7), dms2deg(43,38,33.24) );
	*/
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
