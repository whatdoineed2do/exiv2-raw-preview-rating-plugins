#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <string.h>

#include <gdk-pixbuf/gdk-pixbuf.h>


int main(int argc, char *argv[])
{
    GError *gerror = NULL;
    GdkPixbuf*  gpxbuf = NULL;
    char* filename = NULL;

    long  hei, wid;
    int  a = 1;

    if (argc == 1) {
        fprintf(stderr, "expects one file to load\n");
	exit(1);
    }
    

    while (a < argc)
    {
	filename = argv[a++];

	if ( (gpxbuf = gdk_pixbuf_new_from_file(filename ,&gerror) ) == NULL) {
	    printf("failed to load %s - %s\n", filename, gerror->message);
	    exit(1);
	}

	hei = gdk_pixbuf_get_height(gpxbuf);
	wid = gdk_pixbuf_get_width(gpxbuf);
	printf("loaded imgs=%s, height=%ld width=%ld\n", filename, hei, wid);

	g_object_unref(gpxbuf);
    }
    return 0;
}
