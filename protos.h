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
int archive_init ( void );
void archive_clear ( void );
void archive_add ( char * );
int lookup_series ( struct maplet *, int, int );
void set_series ( enum s_type );
char *wonk_series ( enum s_type );

/* from utils.c */
void error ( char *, ... );
void *gmalloc ( size_t );
double dms2deg ( int, int, double );
char * strhide ( char * );
char * strnhide ( char *, int );
char * str_lower ( char * );
int strcmp_l ( char *, char * );
int is_directory ( char *path );
int is_file ( char * );
char * find_home ( void );
double parse_dms ( char * );

void filebuf_free ( void * );
void * filebuf_init ( int, off_t );
long filebuf_i4 ( void * );
int filebuf_i2 ( void * );
void filebuf_skip ( void *, int );
int filebuf_i2_off ( int, off_t );
char * filebuf_string ( void *, int );
double filebuf_double ( void * );

/* from http.c */
char * http_soap ( char *, int, char *, char *, char *, int, int * );
void free_http_soap ( void * );

/* THE END */
