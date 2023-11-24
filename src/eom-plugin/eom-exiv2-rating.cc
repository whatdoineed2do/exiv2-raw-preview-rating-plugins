#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "eom-exiv2-rating.h"

#include <iostream>
#include <ExifProxy.h>

extern "C" {

static void eom_window_activatable_iface_init (EomWindowActivatableInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EomExiv2RatingPlugin,
                                eom_exiv2_rating_plugin,
                                PEAS_TYPE_EXTENSION_BASE,
                                0,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (EOM_TYPE_WINDOW_ACTIVATABLE,
                                                               eom_window_activatable_iface_init))
}

enum {
	PROP_0,
	PROP_WINDOW
};

extern "C" {
static void
exiv2rate_cb(GtkAction *action,
           EomExiv2RatingPlugin *plugin)
{
    eom_debug_message (DEBUG_PLUGINS, "rating file");
    g_print("Rated %s %s\n", plugin->exifproxy->file().c_str(), plugin->exifproxy->fliprating() ? plugin->exifproxy->rating() : "XMP Rating: -/-");
}
}

static void
selection_changed_cb (EomThumbView         *view,
                      EomExiv2RatingPlugin *plugin);


static const gchar* const ui_definition = "<ui><menubar name=\"MainMenu\">"
	"<menu name=\"ToolsMenu\" action=\"Tools\"><separator/>"
	"<menuitem name=\"EomPluginExiv2Rating\" action=\"EomPluginRunExiv2Rating\"/>"
	"<separator/></menu></menubar>"
	"<popup name=\"ViewPopup\"><separator/>"
	"<menuitem action=\"EomPluginRunExiv2Rating\"/><separator/>"
	"</popup></ui>";

static const GtkActionEntry action_entries[] = {
	{ "EomPluginRunExiv2Rating", "view-refresh", "EXIF Rate Image", "R", "EXIF Rate current image", G_CALLBACK (exiv2rate_cb) }
};

static void
eom_exiv2_rating_plugin_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
    EomExiv2RatingPlugin *plugin = EOM_EXIV2_RATING_PLUGIN (object);

    switch (prop_id)
    {
	case PROP_WINDOW:
	    plugin->window = EOM_WINDOW (g_value_dup_object (value));
	    break;

	default:
	    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	    break;
    }
}

static void
eom_exiv2_rating_plugin_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
    EomExiv2RatingPlugin *plugin = EOM_EXIV2_RATING_PLUGIN (object);

    switch (prop_id)
    {
	case PROP_WINDOW:
	    g_value_set_object (value, plugin->window);
	    break;

	default:
	    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	    break;
    }
}

static void
eom_exiv2_rating_plugin_init (EomExiv2RatingPlugin *plugin)
{
    eom_debug_message (DEBUG_PLUGINS, "EomExiv2RatingPlugin initializing");
    plugin->exifproxy = new ExifProxy();
}

static void
eom_exiv2_rating_plugin_dispose (GObject *object)
{
    EomExiv2RatingPlugin *plugin = EOM_EXIV2_RATING_PLUGIN (object);

    eom_debug_message (DEBUG_PLUGINS, "EomExiv2RatingPlugin disposing");

    if (plugin->window != NULL) {
	g_object_unref (plugin->window);
	plugin->window = NULL;

	if (plugin->exifproxy) {
	    const ExifProxy::History&  h = plugin->exifproxy->history();
	    for ( ExifProxy::History::const_iterator i=h.begin(); i!=h.end(); ++i)
	    {
		std::cout << *i << std::endl;
	    }
	}
	delete plugin->exifproxy;
	plugin->exifproxy = NULL;
    }

    G_OBJECT_CLASS (eom_exiv2_rating_plugin_parent_class)->dispose (object);
}

static void
eom_exiv2_rating_plugin_activate (EomWindowActivatable *activatable)
{
    EomExiv2RatingPlugin *plugin = EOM_EXIV2_RATING_PLUGIN (activatable);
    GtkUIManager *manager;

    EomWindow *window = plugin->window;
    GtkWidget *thumbview = eom_window_get_thumb_view (window);

    eom_debug (DEBUG_PLUGINS);

    manager = eom_window_get_ui_manager (plugin->window);

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    plugin->ui_action_group = gtk_action_group_new ("EomExiv2RatingPluginActions");

    gtk_action_group_add_actions (plugin->ui_action_group, action_entries,
	    G_N_ELEMENTS (action_entries), plugin);
    G_GNUC_END_IGNORE_DEPRECATIONS;

    plugin->signal_id = g_signal_connect_after (G_OBJECT (thumbview), "selection_changed",
	    G_CALLBACK (selection_changed_cb), plugin);

    gtk_ui_manager_insert_action_group (manager, plugin->ui_action_group, -1);

    plugin->ui_id = gtk_ui_manager_add_ui_from_string (manager, ui_definition, -1, NULL);
    g_warn_if_fail (plugin->ui_id != 0);
}

static void
eom_exiv2_rating_plugin_deactivate (EomWindowActivatable *activatable)
{
    EomExiv2RatingPlugin *plugin = EOM_EXIV2_RATING_PLUGIN (activatable);
    EomWindow *window = plugin->window;
    GtkWidget *view = eom_window_get_thumb_view (window);
    GtkUIManager *manager;

    eom_debug (DEBUG_PLUGINS);

    manager = eom_window_get_ui_manager (plugin->window);

    gtk_ui_manager_remove_ui (manager, plugin->ui_id);

    gtk_ui_manager_remove_action_group (manager, plugin->ui_action_group);

    gtk_ui_manager_ensure_update (manager);

#if GLIB_CHECK_VERSION(2,62,0)
    g_clear_signal_handler (&plugin->signal_id, view);
#else
    if (plugin->signal_id != 0) {
	g_signal_handler_disconnect (view, plugin->signal_id);
	plugin->signal_id = 0;
    }
#endif
}

static void
eom_exiv2_rating_plugin_class_init (EomExiv2RatingPluginClass *klass)
{
    eom_debug_message (DEBUG_PLUGINS, "EomExiv2RatingPlugin class init");

    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = eom_exiv2_rating_plugin_dispose;
    object_class->set_property = eom_exiv2_rating_plugin_set_property;
    object_class->get_property = eom_exiv2_rating_plugin_get_property;

    g_object_class_override_property (object_class, PROP_WINDOW, "window");
}

static void
eom_exiv2_rating_plugin_class_finalize (EomExiv2RatingPluginClass *klass)
{
    eom_debug_message (DEBUG_PLUGINS, "EomExiv2RatingPlugin class finalise");
    /* dummy function - used by G_DEFINE_DYNAMIC_TYPE_EXTENDED */
}

extern "C" {

static void
eom_window_activatable_iface_init (EomWindowActivatableInterface *iface)
{
    eom_debug_message (DEBUG_PLUGINS, "EomExiv2RatingPlugin iface finalise");

    iface->activate = eom_exiv2_rating_plugin_activate;
    iface->deactivate = eom_exiv2_rating_plugin_deactivate;
}

G_MODULE_EXPORT void
peas_register_types (PeasObjectModule *module)
{
    eom_exiv2_rating_plugin_register_type (G_TYPE_MODULE (module));
    peas_object_module_register_extension_type (module,
	    EOM_TYPE_WINDOW_ACTIVATABLE,
	    EOM_TYPE_EXIV2_RATING_PLUGIN);
}
}


static void
selection_changed_cb (EomThumbView         *view,
                      EomExiv2RatingPlugin *plugin)
{
    if (eom_thumb_view_get_n_selected(view) == 0) {
	eom_debug_message (DEBUG_PLUGINS, "selection changed (exit previous)");
	return;
    }
    EomImage *image;

    image = eom_thumb_view_get_first_selected_image (view);

    GFile* file = eom_image_get_file(image);
    char* path = g_file_get_path(file);

    plugin->exifproxy->ref(path);

    g_object_unref(file);
    eom_debug_message (DEBUG_PLUGINS, "selection changed to %s", path);

    g_free(path);

#if 0
    gtk_statusbar_pop (statusbar, 0);

    if (!eom_image_has_data (image, EOM_IMAGE_DATA_EXIF))
    {
	if (!eom_image_load (image, EOM_IMAGE_DATA_EXIF, NULL, NULL))
	{
	    gtk_widget_hide (GTK_WIDGET (statusbar));
	}
    }

    exif_data = eom_image_get_exif_info (image);

    if (exif_data)
    {
	date = eom_exif_util_format_date (eom_exif_data_get_value (exif_data, EXIF_TAG_DATE_TIME_ORIGINAL, time_buffer, 32));
	eom_exif_data_free (exif_data);
    }

    if (date)
    {
	gtk_statusbar_push (statusbar, 0, date);
	gtk_widget_show (GTK_WIDGET (statusbar));
	g_free (date);
    }
    else
    {
	gtk_widget_hide (GTK_WIDGET (statusbar));
    }
#endif
}
