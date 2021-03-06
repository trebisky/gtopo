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

/* XXX
 * This works, but can be very slow, and I sometimes accidentally get it trying
 * to pull maps from terraserver, and I would rather not, so this must be turned
 * on explicitly.  Someday this will be controlled from the config file.
 * If we ever get back to fiddling with terraserver.
 */
#ifdef notdef
#define TERRA
#endif

/* XXX - The 24K_AK is a kludge I plan to get rid of */

#ifdef TERRA
enum s_type { S_STATE, S_ATLAS, S_500K, S_250K, S_100K, S_63K, S_24K, S_24K_AK,
		S_TOPO_32M, S_TOPO_8M, S_TOPO_2M };
#define N_SERIES	11
#else
/* Added 250K, 63K, and 25K_AK to support Alaska 6/15/2011 */
/* Add 50K to support Adirondack (ADK) map set 8/24/2015 */
enum s_type { S_STATE, S_ATLAS, S_500K, S_250K, S_100K, S_63K, S_50K, S_24K, S_24K_AK };
#define N_SERIES	9
#endif

#define PI		3.141592654
#define DEGTORAD	(PI/180.0)
#define RADTODEG	(180.0/PI)

/* each map series may use a different algorithm
 * or method to find maplets.  Each series has a
 * list of methods to try until one works.
 */
enum m_type { M_UNK, M_SECTION, M_FILE, M_TERRA };

/* We have a series of bits in the "verbose" variable to
 * trigger debug from different subsystems.
 */

#define	V_BASIC		0x0001
#define	V_WINDOW	0x0002
#define	V_DRAW		0x0004
#define	V_DRAW2		0x0008
#define	V_TPQ		0x0010
#define	V_ARCHIVE	0x0020
#define	V_ARCHIVE2	0x0040
#define	V_MAPLET	0x0080
#define	V_SCALE		0x0100
#define	V_EVENT		0x0200

/*
#define INITIAL_VERBOSITY	(V_BASIC | V_ARCHIVE )
#define INITIAL_VERBOSITY	(V_BASIC | V_ARCHIVE | V_SCALE )
#define INITIAL_VERBOSITY	V_EVENT
#define INITIAL_VERBOSITY	0
*/
#define INITIAL_VERBOSITY	0

enum m1_type { M1_CENTER, M1_GRAB };
enum m3_type { M3_CENTER, M3_ZOOM };

struct settings {
	int verbose;
	int x_view;
	int y_view;
	enum s_type starting_series;
	double starting_long;
	double starting_lat;

	/* also set via command line options */
	int center_marker;
	int marker_size;
	int show_maplets;
	enum m1_type m1_action;
	enum m3_type m3_action;
	int up_key;
	int down_key;
};

/* XXX - we need to introduce a tpq structure and link to it
 * from other appropriate structures (probably just the maplet
 * structure).  This is necessitated at level 3 where the TPQ
 * file format changes from state to state.
 */

struct series {
	/* What series this is */
	enum s_type series;

	int tpq_count;		/* number of files seen */

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
	struct method *cur_method;

	/* pixel size of maplet XXX */
	int xdim;
	int ydim;

	/* For NGS this is degree per pixel,
	 * For Terra this is meters per pixel.
	 */
	double x_pixel_scale;
	double y_pixel_scale;

	/* boolean, true if terraserver series
	 * (in which case the rest of this structure can
	 * be ignored)
	 */
	int terra;
	char *scale_name;

	/* How many maplets per TPQ file */
	int lat_count;
	int long_count;

	/* How many TPQ files per section */
	int lat_count_d;
	int long_count_d;

	/* Set to 1, unless we count A1 to E1 */
	int quad_lat_count;
	int quad_long_count;

	/* degrees per section, 1x1 except in Alaska */
	int lat_dps;
	int long_dps;

	/* size of each maplet */
	double maplet_lat_deg;
	double maplet_long_deg;
};

/* Structure to hold our current position */
struct topo_info {
	/* This is where we are in plain old degrees */
	double lat_deg;
	double long_deg;

	/* Here is our position in UTM */
	int utm_zone;
	double utm_x;
	double utm_y;

	/* This specifies the maplet containing the above,
	 * it changes every time we change series.
	 */
	int	maplet_x;
	int	maplet_y;

	/* fractional offset in the maplet */
	double 	fx;
	double	fy;

	/* info for all the series */
	struct series series_info[N_SERIES];

	/* the current series */
	struct series *series;

	int have_usa;

	/* stuff from command line options */
	int center_only;

	/* statistics to show when gtopo exits */
	int n_sections;
};

enum win_status { GONE, HIDDEN, UP };

struct mouse {
        double x;
        double y;
        int time;
};

/* XXX - should eliminate mo_ stuff below and use the above */
struct viewport {
        int vx;
        int vy;
        int vxcent;
        int vycent;
        double mo_x;
        double mo_y;
        int mo_time;
        GtkWidget *da;
};

struct info_info {
        enum win_status status;
        GtkWidget *main;
        GtkWidget *l_long;
        GtkWidget *l_lat;
        GtkWidget *e_long;
        GtkWidget *e_lat;
};

struct method {
	struct method *next;
	enum m_type type;
	struct section *sections;
	struct tpq_info *tpq;
	struct maplet *cache;
};

/* This is set up by load_maplet() and lookup_quad()
 * a lot of this junk can be moved out and kept as local
 * variables in the above routines.
 */
struct maplet {
	struct maplet *next;

	/* Unique indices used to do cache lookups */
	int world_x;
	int world_y;

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

	int index_size;
	struct tpq_index_e *index;
};

struct tpq_index_e {
	off_t	offset;
	long	size;
};

enum {
        NAME_COLUMN,
	LONG_COLUMN,
	LAT_COLUMN,
	SERIES_COLUMN,
	N_COLUMNS
};

struct places_info {
	enum win_status status;
	GtkWidget *main;
	GtkListStore *store;
};

/* THE END */
