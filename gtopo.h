/* gtopo.h
 */

enum series { S_STATE, S_ATLAS, S_500K, S_100K, S_24K };

/* Structure to define our current position */
struct position {
	/* This is where we are in plain old degrees */
	double lat_deg;
	double long_deg;

	/* What map series we are viewing */
	enum series series;

	/* How many maplets per TPQ file */
	int lat_count;
	int long_count;

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

	/* This is a pointer to the maplet which
	 * contains the current position.
	 */
	struct maplet *maplet;
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
GdkPixbuf *load_tpq_maplet ( char *, int, int );

/* from maplet.c */
struct maplet *load_maplet ( struct position * );
struct maplet *load_maplet_nbr ( struct position *, int, int );

/* from archive.c */
int archive_init ( char **, int );
int lookup_quad ( struct position *, struct maplet * );

/* THE END */
