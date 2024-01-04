/* stadndalone test harness to test pixbuf loaders
 */

#include "../config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#if HAVE_GTK
#include <gtk/gtk.h>

void
_handle_signal(int sig)
{
  gtk_main_quit();
}

void
on_destroy (GtkWidget * widget G_GNUC_UNUSED,
	    gpointer user_data G_GNUC_UNUSED)
{
  gtk_main_quit();
}

gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
    switch (event->keyval)
    {
      case GDK_KEY_Escape:
      case GDK_KEY_Q:
      case GDK_KEY_q:
        gtk_main_quit();
        return TRUE;

      default:
    }

    return FALSE;
}
#endif

void _usage(char* argv0, int ret)
{
    printf("usage: %s  -f <file> [-m <mime-type>] [-x]\n"
	   "       -f <file>       image file to load\n"
	   "       -m <mime-type>  mime-type to force pixbuf loader\n"
#if HAVE_GTK
	   "       -x              display loaded image in GTK window (default=no)\n"
#endif
	   "\n"
	   "Examples:\n"
           "       -f foo.nef  -m image/x-nikon-nef\n"
	   "       -f foo.dng  -m image/x-adobe-dng\n"
	   "\n"
	   "Notes:\n"
	   "- specifying 'image/tiff' will most likely not use the exiv2 pixbuf loader\n"
	   "- 'G_MESSAGES_DEBUG=gdk-pixbuf.exiv2-rawpreview' for pixbuf loader messages\n", basename(argv0));
    exit(ret);
}

int
main (int argc, char *argv[])
{
    FILE *f;
    struct stat  st = { 0 };

    guint8* buffer = NULL;
    gsize length = 0;
    GdkPixbufLoader *loader = NULL;
#if HAVE_GTK
    GdkPixbuf *pixbuf;
    GtkWidget *window;
    GtkWidget *image;
#endif
    GError**  err_ = NULL;

    const char*  file = NULL;
    const char*  mime_type = NULL;
    bool  usex = false;

    int c;
    while ( (c=getopt(argc, argv, "xf:m:h")) != -1)
    {
        switch (c)
	{
	    case 'x':  usex = true;  break;
	    case 'f':  file = optarg;  break;
	    case 'm':  mime_type = optarg; break;

	    case 'h': _usage(argv[0], 0);
	    default:  _usage(argv[0], -1);
	}
    }
    if (optind != argc) {
        printf("extra args\n");
	_usage(argv[0], 2);
    }

    stat(file, &st);
    if (st.st_size == 0) {
        printf("%s is empty\n", file);
	_usage(argv[0], 1);
    }

    buffer = malloc(st.st_size);
    memset(buffer, 0, st.st_size);


    f = fopen(file, "rb");
    if ( (length = fread (buffer, 1, st.st_size, f)) != st.st_size) {
        printf("failed to read all data from %s - %s\n", file, strerror(ferror(f)));
	exit(1);
    }
    fclose (f);

    if (mime_type)
    {
        printf("requesting loader for mimetype=%s on %s\n", mime_type, file);
        loader = gdk_pixbuf_loader_new_with_mime_type(mime_type, err_);
        if (err_ && *err_) {
            g_error_free(*err_);
            *err_ = NULL;
            printf("no known loader for explicit mimetype, defaulting\n");
            loader = gdk_pixbuf_loader_new();
        }
    }
    else
    {
	GdkPixbufFormat* info = gdk_pixbuf_get_file_info(file, NULL, NULL);
	gchar*  name = info ? gdk_pixbuf_format_get_name(info) : NULL;
	gchar**  mime_types = info ? gdk_pixbuf_format_get_mime_types(info) : NULL;
        printf("requesting default loader on %s (%s)\n", file, name);

        if (mime_types) {
            printf("Supported MIME types:\n");
            for (gchar **iter = mime_types; *iter != NULL; ++iter) {
                printf("- %s\n", *iter);
                g_free(*iter);
            }
            g_free(mime_types);
	}

        loader = gdk_pixbuf_loader_new ();

	g_free(name);
    }
    gdk_pixbuf_loader_write (loader, buffer, length, NULL);
    gdk_pixbuf_loader_close(loader, err_);

    if (usex)
    {
#if HAVE_GTK
	signal(SIGINT, _handle_signal);
	gtk_init (&argc, &argv);

	pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	image = gtk_image_new_from_pixbuf (pixbuf);

	gtk_window_set_title(GTK_WINDOW(window), "GTK pixbuf loader");

	g_signal_connect(G_OBJECT(window), "key-press-event", G_CALLBACK(on_key_press), NULL);
	gtk_container_add (GTK_CONTAINER (window), image);
	gtk_widget_show_all (GTK_WIDGET (window));
	g_signal_connect (window, "destroy", G_CALLBACK (on_destroy), NULL);
	gtk_main ();
#else
        printf("GTK not available at compile time\n");
#endif
    }

    g_object_unref(loader);
    free(buffer);

    return 0;
}
