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

/* gtopo.h
 */

enum s_type { S_STATE, S_ATLAS, S_500K, S_100K, S_24K };

#define N_SERIES	5

#define PI		3.141592654
#define DEGTORAD	(PI/180.0)

/* each map series may use a different algorithm
 * or method to find maplets.  Each series has a
 * list of methods to try until one works.
 */
enum m_type { M_UNK, M_SECTION, M_FILE, M_STATE };

/* We have a series of bits in the "verbose" variable to
 * trigger debug from different subsystems.
 */

#define	V_BASIC		0x0001
#define	V_WINDOW	0x0002
#define	V_DRAW		0x0004
#define	V_DRAW2		0x0008
#define	V_EVENT		0x0010
#define	V_ARCHIVE	0x0020
#define	V_MAPLET	0x0040
#define	V_SCALE		0x0080
#define	V_TPQ		0x0100

/*
#define INITIAL_VERBOSITY	(V_BASIC | V_ARCHIVE )
#define INITIAL_VERBOSITY	0
*/
#define INITIAL_VERBOSITY	(V_BASIC | V_ARCHIVE | V_SCALE )

/* Structure to hold our current position */
struct topo_info {
	/* This is where we are in plain old degrees */
	double lat_deg;
	double long_deg;

	/* This is the maplet containing the above,
	 * this changes every time we change series.
	 */
	long	lat_maplet;
	long	long_maplet;

	/* fractional offset in the maplet */
	double 	fx;
	double	fy;

	/* info for all the series */
	struct series *series_info;

	/* the current series */
	struct series *series;

	int have_usa;

	/* stuff from command line options */
	int verbose;
	int center_only;
	int center_dot;
	int show_maplets;

	/* statistics to show when gtopo exits */
	int n_sections;
};

struct method {
	struct method *next;
	enum m_type type;
	struct section *sections;
	struct tpq_info *tpq;
};

/* XXX - we need to introduce a tpq structure and link to it
 * from other appropriate structures (probably just the maplet
 * structure).  This is necessitated at level 3 where the TPQ
 * file format changes from state to state.
 */

struct series {
	/* What series this is */
	enum s_type series;

	/* pointer to the maplet cache for the
	 * current series
	 */
	struct maplet *cache;
	int cache_count;

	/* pixmap for this series */
	GdkPixmap *pixels;

	/* boolean, true if pixmap content is OK */
	int content;

	struct method *methods;

	/* pixel size of maplet XXX */
	int xdim;
	int ydim;

	/* How many maplets per TPQ file */
	int lat_count;
	int long_count;

	/* How many maps per section */
	int lat_count_d;
	int long_count_d;

	/* How many 7.5 minute units per TPQ */
	int quad_lat_count;
	int quad_long_count;

	/* size of each maplet */
	double maplet_lat_deg;
	double maplet_long_deg;
};

/* This is set up by load_maplet() and lookup_quad()
 * a lot of this junk can be moved out and kept as local
 * variables in the above routines.
 */
struct maplet {
	struct maplet *next;

	/* Unique indices used to do cache lookups */
	int world_index_lat;
	int world_index_long;

	/* indices within the TPQ file */
	int sheet_index_lat;
	int sheet_index_long;

	/* Use for possible cache entry aging */
	int time;

	/* size of the maplet image in pixels */
	int xdim;
	int ydim;

	/* The pixels !! */
	GdkPixbuf *pixbuf;

	/* pathname that gave us this maplet */
	char *tpq_path;

	/* This is the maplet index in the file (0-N) */
	int tpq_index;

	struct tpq_info *tpq;
};

/* Stuff extracted from a TPQ file header
 */
struct tpq_info {
	struct tpq_info *next;
	char *path;

	char *state;
	char *quad;

	double w_long;
	double e_long;
	double s_lat;
	double n_lat;

	double mid_lat;

	int long_count;
	int lat_count;

	double maplet_long_deg;
	double maplet_lat_deg;

	enum s_type series;

	int sheet_long;
	int sheet_lat;

	int index_size;
	struct tpq_index_e *index;
};

struct tpq_index_e {
	off_t	offset;
	long	size;
};

/* THE END */
