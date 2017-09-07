#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>

void
on_destroy (GtkWidget * widget G_GNUC_UNUSED,
	    gpointer user_data G_GNUC_UNUSED)
{
  gtk_main_quit ();
}

int
main (int argc, char *argv[])
{
    FILE *f;
    struct stat  st;

    guint8* buffer = NULL;
    gsize length = 0;
    GdkPixbufLoader *loader = NULL;
    GdkPixbuf *pixbuf;
    GtkWidget *window;
    GtkWidget *image;
    GError**  err_ = NULL;

    if (argc == 1) {
        printf("no files\n");
	exit(1);
    }
    memset(&st, sizeof(st), 0);
    stat(argv[1], &st);
    if (st.st_size == 0) {
        printf("%s is empty\n", argv[1]);
	exit(1);
    }

    buffer = malloc(st.st_size);
    memset(buffer, 0, st.st_size);

    gtk_init (&argc, &argv);

    f = fopen (argv[1], "rb");
    if ( (length = fread (buffer, 1, st.st_size, f)) != st.st_size) {
        printf("failed to read all data from %s\n", argv[1]);
	exit(1);
    }
    fclose (f);

    if (argc == 3) {
        printf("requesting loader for mimetype=%s on %s\n", argv[2], argv[1]);
        loader = gdk_pixbuf_loader_new_with_mime_type(argv[2], err_);
        if (err_ && *err_) {
            g_error_free(*err_);
            *err_ = NULL;
            printf("no known loader for explicit mimetype, defaulting\n");
            loader = gdk_pixbuf_loader_new();
        }
    }
    else
    {
        loader = gdk_pixbuf_loader_new ();
    }
    gdk_pixbuf_loader_write (loader, buffer, length, NULL);
    gdk_pixbuf_loader_close(loader, err_);
    pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    image = gtk_image_new_from_pixbuf (pixbuf);
    gtk_container_add (GTK_CONTAINER (window), image);
    gtk_widget_show_all (GTK_WIDGET (window));
    g_signal_connect (window, "destroy", G_CALLBACK (on_destroy), NULL);
    gtk_main ();

    free(buffer);
    return 0;
}
