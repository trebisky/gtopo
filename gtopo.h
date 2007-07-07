/* gtopo.h
 */

/* Structure to define our current position */
struct position {
	/* This is where we are in plain old degrees */
	double lat_deg;
	double long_deg;
	/* These are filled in by lookup_quad */
	int latlong;	/* this is the 1x1 section we are in */
	int lat_quad;	/* a-h for the 7.5 minute quad */
	int long_quad;	/* 1-8 for the 7.5 minute quad */
	double lat_deg_quad;	/* 0 to 0.138 degrees in the quad */
	double long_deg_quad;	/* 0 to 0.138 degrees in the quad */
	/* This is which maplet in the quad contain the position */
	int x_maplet;
	int y_maplet;
	/* This is a fractional position (0.0-1.0) in the maplet */
	double maplet_fx;	/* origin in West */
	double maplet_fy;	/* origin in North */
};

/* from tpq_io.c */
void build_tpq_index ( char * );
GdkPixbuf *load_tpq_maplet ( char *, int, int );

/* from archive.c */
int archive_init ( char **, int );
char *lookup_section ( int, int );
char *lookup_quad ( struct position * );

/* THE END */