/*
 *  GTopo - settings.c
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

#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <pwd.h>

#include "gtopo.h"
#include "protos.h"

extern struct settings settings;

char *
find_home ( void )
{
	struct passwd *pw;

	pw = getpwuid ( getuid() );
	if ( pw )
	    return pw->pw_name;

	return getenv ( "HOME" );
}

static void
settings_default ( void )
{
	settings.verbose = 0;
	settings.x_view = 640;
	settings.y_view = 800;
	settings.starting_series = S_STATE;

	settings.center_only = 0;
	settings.center_dot = 1;
	settings.show_maplets = 0;
}

void
settings_init ( void )
{
	settings_default ();
}

/* THE END */
