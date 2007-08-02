/* gtopo.h
 */

enum s_type { S_FILE, S_STATE, S_ATLAS, S_500K, S_100K, S_24K };

#define N_SERIES	6

#define PI		3.141592654
#define DEGTORAD	(PI/180.0)

/* each map series may use a different algorithm
 * or method to find maplets.  Each series has a
 * list of methods to try until one works.
 */
enum m_type { M_UNK, M_SECTION, M_FILE };

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

	/* stuff from command line options */
	int verbose;
	int center_only;
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

	/* size of the entire TPQ file */
	double map_lat_deg;
	double map_long_deg;

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
	int maplet_index_lat;
	int maplet_index_long;

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
	long	offset;
	long	size;
};

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
char *strhide ( char * );
int archive_init ( char ** );
int lookup_series ( struct maplet *, int, int );
void set_series ( enum s_type );

/* THE END */
