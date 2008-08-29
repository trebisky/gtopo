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

#include <fcntl.h>

#include "gtopo.h"
#include "protos.h"
#include "xml.h"

extern struct topo_info info;

int terra_verbose = 0;

static int from_base64 (char *, char *);

#define MAX_TERRA_REQ	4096
static char terra_request[MAX_TERRA_REQ];

static char *server_name = "terraserver-usa.com";
static char *server_target = "/terraservice.asmx";
static int server_port = 80;

/* The first image this ever received was a photo.
 * It came back as a 200x200 pixel image and used 7032 bytes
 * in the original packet.  Stripping \n\r brought this down to
 * 6852 bytes, and the base64 conversion made it 5138 bytes to
 * be saved to a local file.
 *
 * Some less than obvious notes about the API arguments.
 * "scene" is actually the UTM zone.
 * X and Y are tile coordinates, divided down from UTM.
 * So, if we are using an 8m scale, we divide the UTM coordinates
 * by 8*200 and truncate any fractional part.
 */
char *
terra_get_tile ( int zone, int tx, int ty, char *scale, char *theme, int *count )
{
	struct xml *xp;
	struct xml *t;
	struct xml *x;
	char *action = "http://terraserver-usa.com/terraserver/GetTile";
	int n;
	int nr;
	char *reply;
	struct xml *rp;
	struct xml *xx;
	char *val;
	char *buf;
	char value[64];

	xp = xml_start ( "SOAP-ENV:Envelope" );
	xml_attr ( xp, "SOAP-ENV:encodingStyle", "http://schemas.xmlsoap.org/soap/encoding/" );
	xml_attr ( xp, "xmlns:SOAP-ENC", "http://schemas.xmlsoap.org/soap/encoding/" );
	xml_attr ( xp, "xmlns:xsi", "http://www.w3.org/1999/XMLSchema-instance" );
	xml_attr ( xp, "xmlns:SOAP-ENV", "http://schemas.xmlsoap.org/soap/envelope/" );
	xml_attr ( xp, "xmlns:xsd", "http://www.w3.org/1999/XMLSchema/" );

	t = xml_tag ( xp, "SOAP-ENV:Body" );
	t = xml_tag ( t, "ns1:GetTile" );
	xml_attr ( t, "xmlns:ns1", "http://terraserver-usa.com/terraserver/" );
	xml_attr ( t, "SOAP-ENC:root", "1" );
	t = xml_tag ( t, "ns1:id" );

	/* Scale is meters per pixel as "Scale4m" */
	x = xml_tag_stuff ( t, "ns1:Scale", scale );
	xml_attr ( x, "xsi:type", "xsd:string" );

	/* Scene is UTM zone */
	sprintf ( value, "%d", zone );
	x = xml_tag_stuff ( t, "ns1:Scene", value );
	xml_attr ( x, "xsi:type", "xsd:string" );

	/* Theme is "Photo", "Topo", or "Relief" */
	x = xml_tag_stuff ( t, "ns1:Theme", theme );
	xml_attr ( x, "xsi:type", "xsd:string" );

	/* X is Easting as a pixel count */
	sprintf ( value, "%d", tx );
	x = xml_tag_stuff ( t, "ns1:X", value );
	xml_attr ( x, "xsi:type", "xsd:string" );

	/* Y is Northing as a pixel count */
	sprintf ( value, "%d", ty );
	x = xml_tag_stuff ( t, "ns1:Y", value );
	xml_attr ( x, "xsi:type", "xsd:string" );

	n = xml_collect ( terra_request, MAX_TERRA_REQ, xp );
	xml_destroy ( xp );

	if ( terra_verbose ) {
	    printf ( "  REQUEST:\n" );
	    write ( 1, terra_request, n );
	}

	reply = http_soap ( server_name, server_port, server_target, action, terra_request, n, &nr );

	/*
	if ( terra_verbose ) {
	    printf ( "\n" );
	    printf ( "  REPLY:\n" );
	    write ( 1, reply, nr );
	}
	*/

	rp = xml_parse_doc ( reply, nr );
	free_http_soap ( (void *) reply );

	val = xml_find_tag_value ( rp, "GetTileResult" );
	if ( ! val )
	    return NULL;

	buf = (char *) gmalloc ( strlen(val) );
	*count = from_base64 ( buf, val );
	xml_destroy ( rp );

	return buf;
}

int
load_terra_maplet ( struct maplet *mp )
{
	GdkPixbufLoader *loader;
	int count;
	char *buf;

	buf = terra_get_tile ( info.utm_zone, mp->world_x, mp->world_y, info.series->scale_name, "Topo", &count );
	if ( ! buf )
	    return 0;

	printf ( "Terra get tile %d %d fetches %d\n", mp->world_x, mp->world_y, count );

	loader = gdk_pixbuf_loader_new_with_type ( "gif", NULL );

	gdk_pixbuf_loader_write ( loader, (guchar *) buf, count, NULL );

	/* The following two calls work in either order */
	gdk_pixbuf_loader_close ( loader, NULL );
	mp->pixbuf = gdk_pixbuf_loader_get_pixbuf ( loader );

	mp->xdim = info.series->xdim;
	mp->ydim = info.series->ydim;

	/* be a good citizen and avoid a memory leak,
	 */
	g_object_ref ( mp->pixbuf );
	g_object_unref ( loader );
	return 1;
}

struct terra_loc {
	double lon;
	double lat;
	int zone;
	double x;
	double y;
};

/* Bogus old API from testing */
int
terra_save_tile ( struct terra_loc *tlp, char *scale, char *theme )
{
	char *buf;
	int count;
	int tfd;

	buf = terra_get_tile ( tlp->zone, (int) tlp->x, (int) tlp->y, scale, theme, &count );
	if ( ! buf )
	    return 0;

	tfd = open ( "terra.gif", O_WRONLY|O_CREAT|O_TRUNC, 0644 );
	if ( tfd < 0 ) {
	    printf ( "Open fails for terra tile image\n");
	    return 0;
	}

	write ( tfd, buf, count );
	close ( tfd );

	free ( buf );

	return 1;
}

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

/* The following lat-long to utm and inverse functions
 * are transcriptions of the formulas in 
 * USGS Professional Paper 1395 (1987)
 * pages 57-64
 * "Map Projections, A Working Manual" by John P. Snyder
 */

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

	/*
	printf ( "Central Meridian: %.2f\n", lon_cm );
	*/

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

	/*
	printf ( "n = %.5f\n", n );
	printf ( "t = %.5f\n", t );
	printf ( "c = %.5f\n", c );
	printf ( "a = %.5f\n", a );
	printf ( "m = %.5f\n", m );
	*/

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

	/*
	printf ( "lon_cm_rad = %.5f (%.4f)\n", lon_cm_rad, lon_cm_rad * RADTODEG );
	printf ( "foot_lat = %.5f (%.4f)\n", foot_lat, foot_lat * RADTODEG );
	printf ( "c1 = %.5f\n", c1 );
	printf ( "t1 = %.5f\n", t1 );
	printf ( "n1 = %.5f\n", n1 );
	printf ( "r1 = %.5f\n", r1 );
	printf ( "d = %.5f\n", d );
	*/

	l_1 = 5.0 + 3.0 * t1 + 10.0 * c1 - 4.0 * c1*c1 - 9.0 * eep;
	l_2 = 61.0 + 90.0 * t1 + 298.0 * c1 + 45.0 * t1*t1 - 252.0 * eep - 3.0 * c1*c1;
	tlp->lat = (foot_lat - n1*tan(foot_lat)/r1 * ( d*d/2.0 - l_1*d*d*d*d/24.0 + l_2*d*d*d*d*d*d / 720.0)) * RADTODEG;

	l_1 = 1.0 + 2.0 * t1 + c1;
	l_2 = 5.0 - 2.0 * c1 + 28.0 * t1 - 3.0 * c1*c1 + 8.0 * eep + 24.0 * t1*t1;
	tlp->lon = (lon_cm_rad + (d - l_1 * d*d*d/6.0 + l_2 * d*d*d*d*d/120.0) / cos_foot) * RADTODEG;

	return 1;
}

void
ll_to_utm ( double lon, double lat, int *zone, double *x, double *y )
{
	struct terra_loc loc;

	loc.lon = lon;
	loc.lat = lat;
	to_utm ( &loc );
	*zone = loc.zone;
	*x = loc.x;
	*y = loc.y;
}

void
utm_to_ll ( int zone, double x, double y, double *lon, double *lat )
{
	struct terra_loc loc;

	loc.zone = zone;
	loc.x = x;
	loc.y = y;
	to_ll ( &loc );
	*lon = loc.lon;
	*lat = loc.lat;
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
    	double scale = 32.0;
	char *scale_name = "Scale32m";
	/*
    	double scale = 8.0;
	char *scale_name = "Scale8m";
    	double scale = 2.0;
	char *scale_name = "Scale2m";
	*/

	struct terra_loc loc;
	int pix;

	/* South Geronimo Mine, Trigo Mountains */
	loc.lon = -dms2deg ( 114, 36, 48.0 );
	loc.lat =  dms2deg ( 33, 6, 59.0 );
	(void) to_utm ( &loc );
	/*
	printf ( "X (Easting) = %.2f\n", loc.x );
	printf ( "Y (Northing) = %.2f\n", loc.y );
	*/

	pix = loc.x / (200 * scale );
	loc.x = pix;
	pix = loc.y / (200 * scale );
	loc.y = pix;

    	terra_save_tile ( &loc, scale_name, "Topo" );
}

static void
terra_tile_test ( void )
{
	struct terra_loc loc;

	loc.x = 624;
	loc.y = 5951;
    	terra_save_tile ( &loc, "Scale4m", "Photo" );
}

void
terra_test_C ( void )
{
	/* This is used by PyTerra in it's test suite:
	 * 7 km SW of Rockford, Iowa, United States (nearest place).
	 * UTM is Zone 15, X = 500000, Y = 4760814.7962907264
	 * (my capture for ConvertLonLatPtToUtmPt is ethpy.4)
	 */
	terra_ll_test1 ( -93.0, 43.0 );
	terra_ll_test2 ( -93.0, 43.0 );

	printf ( "\n\n" );

	/* This is the CN Tower in Toronto, Canada
	 * UTM is 630,084 meters east, 4,833,438 meters north in zone 17T
	 * Terraserver gives: X = 630084.30083,  Y = 4833438.58589
	 *
	 */
	terra_ll_test1 ( -dms2deg(79, 23, 13.7), dms2deg(43,38,33.24) );
	terra_ll_test2 ( -dms2deg(79, 23, 13.7), dms2deg(43,38,33.24) );
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

/* This nice clean and simple base64 MIME converter was taken from
 * the mutt source code (was and is under GPL),
 * see http://www.mutt.org and track down base64.c
 */

#ifdef notdef
char B64Chars[64] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
  'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
  'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
  't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', '+', '/'
};
#endif

int Index_64[128] = {
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,	/*  0 - 15 */
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, /* 16 - 31 */
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,	/* 32 - 47 ; + and / */
    52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1, 0,-1,-1,	/* 48 - 63 ; 0-9 and = */
    -1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14, /* 64 - 79 ; A- */
    15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1, /* 80 - 95 ; -Z */
    -1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40, /* 96 - 111 ; a- */
    41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1	/* 112- 127 ; -z */
};

#define base64val(x)	Index_64[(int)x]

static void
ream_white ( char *buf )
{
    	char *p, *q;
	int c;

	p = q = buf;
	while ( c = *p++ ) {
	    if ( c != '\r' && c != '\n' )
		*q++ = c;
	}
	*q++ = '\0';
}

/* Convert '\0'-terminated base 64 string to raw bytes.
 * Returns length of returned buffer, or -1 on error.
 */
static int
from_base64 (char *out, char *in)
{
  int len = 0;
  int d1, d2, d3, d4;
  int v1, v2, v3, v4;

  /*
  printf ( "Count before white reaming: %d\n", strlen(in) );
  printf ( "Count after white reaming: %d\n", strlen(in) );
  */
  ream_white ( in );

  do {
    d1 = in[0];
    if (d1 > 127 ) {
      printf ( "Bad base64 character: %c %d %d\n", d1, d1, 0 );
      return -1;
    }

    v1 = base64val (d1);
    if ( v1 == -1 ) {
      printf ( "Bad base64 character: %c %d %d\n", d1, d1, v1 );
      return -1;
    }

    d2 = in[1];
    if (d2 > 127 ) {
      printf ( "Bad base64 character: %c %d %d\n", d2, d2, 0 );
      return -1;
    }

    v2 = base64val (d2);
    if ( v2 == -1 ) {
      printf ( "Bad base64 character: %c %d %d\n", d2, d2, v2 );
      return -1;
    }

    d3 = in[2];
    if (d3 > 127 ) {
      printf ( "Bad base64 character: %c %d %d\n", d3, d3, 0 );
      return -1;
    }

    v3 = base64val (d3);
    if ( v3 == -1 ) {
      printf ( "Bad base64 character: %c %d %d\n", d3, d3, v3 );
      return -1;
    }

    d4 = in[3];
    if (d4 > 127 ) {
      printf ( "Bad base64 character: %c %d %d\n", d4, d4, 0 );
      return -1;
    }

    v4 = base64val (d4);
    if ( v4 == -1 ) {
      printf ( "Bad base64 character: %c %d %d\n", d4, d4, v4 );
      return -1;
    }

    in += 4;

    /* digits are already sanity-checked */

    *out++ = (v1 << 2) | (v2 >> 4);
    len++;

    if ( d3 != '=' ) {
      *out++ = ((v2 << 4) & 0xf0) | (v3 >> 2);
      len++;

      if (d4 != '=') {
	*out++ = ((v3 << 6) & 0xc0) | v4;
	len++;
      }
    }

  } while (*in && d4 != '=' );

  return len;
}

/* THE END */
