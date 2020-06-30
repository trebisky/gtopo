/*
 *  GTopo - gpx.h
 *
 *  Copyright (C) 2020, Thomas J. Trebisky
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

#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

/* Two structures that need to be shared between gpx.c and overlay.c */

struct waypoint {
    struct waypoint *next;
    float way_lat;
    float way_long;
};

struct track {
    struct track *next;
    int count;
    float lat_min;
    float lat_max;
    float long_min;
    float long_max;
    float *data;
};

/* THE END */
