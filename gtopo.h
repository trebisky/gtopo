/* gtopo.h
 */

/* Structure to define our current position */
struct position {
	double lat_deg;
	double long_deg;
	/* These are filled in by lookup_quad */
	int latlong;
	int lat_quad;	/* a-h */
	int long_quad;	/* 1-8 */
	double lat_deg_quad;
	double long_deg_quad;
	/* These are filled in by locate_maplet */
	int x_maplet;
	int y_maplet;
};

int archive_init ( char **, int );
char *lookup_section ( int, int );
char *lookup_quad ( struct position * );

/* THE END */
