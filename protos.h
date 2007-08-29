/*
 *  GTopo
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

/* protos.h
 */

/* from gtopo.c */
void synch_position ( void );
void set_position ( double, double );

/* from tpq_io.c */
int load_tpq_maplet ( struct maplet * );
struct tpq_info *tpq_lookup ( char * );

/* from maplet.c */
struct maplet *load_maplet ( int, int );
struct maplet *load_maplet_any ( char * );

/* from archive.c */
int archive_init ( char ** );
int lookup_series ( struct maplet *, int, int );
void set_series ( enum s_type );

/* from utils.c */
void error ( char *, void * );
double dms2deg ( int, int, int );
char * strhide ( char * );
char * str_lower ( char * );
int strcmp_l ( char *, char * );
int is_directory ( char *path );
int is_file ( char * );

/* THE END */
