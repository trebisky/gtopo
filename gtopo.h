/* gtopo.h
 */

/* Structure to define our current position */
struct position {
	/* This is where we are in plain old degrees */
	double lat_deg;
	double long_deg;
	/* We need knowledge of the maplet size
	 * to scale pixels during mouse motion.
	 */
	struct maplet *maplet;
};

/* This is set up by load_maplet() and lookup_quad()
 * a lot of this junk can be moved out and kept as local
 * variables in the above routines.
 */
struct maplet {
	struct maplet *next;

	/* Used to do cache lookups */
	int maplet_index_lat;
	int maplet_index_long;

	/* size of the maplet image in pixels */
	int mxdim;
	int mydim;

	/* This is the fractional position (0.0-1.0) in the maplet
	 * of the current position.
	 */
	double maplet_fx;	/* origin in West */
	double maplet_fy;	/* origin in North */

	/* The pixels !! */
	GdkPixbuf *pixbuf;

	/* XXX - Potential junk to move follows */

	/* This is where we are in plain old degrees */
	double lat_deg;
	double long_deg;

	int latlong;	/* this is the 1x1 section we are in */

	int lat_quad;	/* a-h for the 7.5 minute quad */
	int long_quad;	/* 1-8 for the 7.5 minute quad */

	double lat_deg_quad;	/* 0 to 0.138 degrees in the quad */
	double long_deg_quad;	/* 0 to 0.138 degrees in the quad */

	/* This is which maplet in the quad contain the position */
	int x_maplet;
	int y_maplet;

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
int lookup_quad ( struct maplet * );

/* THE END */
