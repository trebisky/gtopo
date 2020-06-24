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

typedef void (*mfptr) ( struct maplet * );

/* from gtopo.c */
void synch_position ( void );
void set_position ( double, double );
void redraw_series ( void );

/* from tpq_io.c */
int load_tpq_maplet ( struct maplet * );
struct tpq_info *tpq_lookup ( char * );

/* from maplet.c */
struct maplet *load_maplet ( int, int );
struct maplet *load_maplet_any ( char *, struct series * );
void state_maplet ( struct method *, mfptr );
void file_maplets ( struct method *, mfptr );

/* from archive.c */
int setup_series ( void );
void up_series ( void );
void down_series ( void );
void initial_series ( enum s_type s );
void series_init ( void );
void file_info ( char *, int );
int file_init ( char * );

int archive_init ( void );
void archive_clear ( void );
void archive_add ( char * );
int lookup_series ( struct maplet * );
struct tpq_info * lookup_tpq ( struct series * );
void set_series ( enum s_type );
char *wonk_series ( enum s_type );
int first_series ( void );
void iterate_series_method ( mfptr );
void show_statistics ( void );
void show_methods ( struct series * );

/* from settings.c */
void settings_init ( void );
void gronk_series ( int *, char * );

/* from places.c */
void places_init ( void );

/* from terra.c */
void ll_to_utm ( double, double, int *, double *, double * );
void utm_to_ll ( int, double, double, double *, double * );
void terra_test ( void );
int load_terra_maplet ( struct maplet * );

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
int split_q ( char *, char **, int );
int split_n ( char *, char **, int );

void filebuf_free ( void * );
void * filebuf_init ( int, off_t );
long filebuf_i4 ( void * );
int filebuf_i2 ( void * );
void filebuf_skip ( void *, int );
int filebuf_i2_off ( int, off_t );
char * filebuf_string ( void *, int );
double filebuf_double ( void * );

/* from http.c */
void http_test ( void );
char * http_soap ( char *, int, char *, char *, char *, int, int * );
void free_http_soap ( void * );

/* from xml.c */
void xml_test ( void );

/* from overlay.c */
void overlay_init ( void );
void overlay_redraw ( void );

/* THE END */
