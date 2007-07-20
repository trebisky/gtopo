/* gtopo.h
 */

enum s_type { S_STATE, S_ATLAS, S_500K, S_100K, S_24K };

#define N_SERIES	5

#define PI		3.141592654
#define DEGTORAD	(PI/180.0)

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

	/* This is a pointer to the maplet which
	 * contains the current position.
	 */
	struct maplet *center;

	/* How many maplets per TPQ file */
	int lat_count;
	int long_count;

	/* How many maps per section */
	int lat_count_d;
	int long_count_d;

	/* How many 7.5 minute units per map */
	int quad_lat_count;
	int quad_long_count;

	/* size of the entire TPQ file */
	double map_lat_deg;
	double map_long_deg;

	/* size of each maplet */
	double maplet_lat_deg;
	double maplet_long_deg;

	/* the following give the postion within the
	 * maplet containing it as a fraction in range [0,1]
	 * origin is in NW corner of maplet.
	 */
	double fx;
	double fy;
};

/* Structure to hold our current position */
struct topo_info {
	/* This is where we are in plain old degrees */
	double lat_deg;
	double long_deg;

	/* what series we be lookin' at */
	struct series *series;

	int verbose;

	int initial;
};

/* This is set up by load_maplet() and lookup_quad()
 * a lot of this junk can be moved out and kept as local
 * variables in the above routines.
 */
struct maplet {
	struct maplet *next;

	/* Unique indices used to do cache lookups */
	int maplet_index_lat;
	int maplet_index_long;

	/* Use for possible cache entry aging */
	int time;

	/* This is which maplet in the quad we are */
	int x_maplet;
	int y_maplet;

	/* size of the maplet image in pixels */
	int xdim;
	int ydim;

	/* The pixels !! */
	GdkPixbuf *pixbuf;

	/* pathname that gave us this maplet */
	char *tpq_path;
};

/* from tpq_io.c */
void build_tpq_index ( char * );
GdkPixbuf *load_tpq_maplet ( char *, int );

/* from maplet.c */
struct maplet *load_maplet ( void );
struct maplet *load_maplet_nbr ( int, int );

/* from archive.c */
int archive_init ( char ** );
int lookup_quad ( struct maplet * );
int lookup_quad_nbr ( struct maplet *, int, int );
void set_series ( enum s_type );

/* THE END */
