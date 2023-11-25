#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "eom-exiv2-rating.h"

#include <sstream>
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

static void
statusbar_set_rating(GtkStatusbar *statusbar,
                    EomThumbView *view,
		    const char* rating)
{
    if (eom_thumb_view_get_n_selected (view) == 0) {
	return;
    }

    char  buf[19] = { 0 };
    snprintf(buf, sizeof(buf), "EXIF rating: %s", rating);
    gtk_statusbar_pop (statusbar, 0);
    gtk_statusbar_push (statusbar, 0, buf);
    gtk_widget_show (GTK_WIDGET (statusbar));
}


extern "C" {
static void
exiv2rate_cb(GtkAction *action,
           EomExiv2RatingPlugin *plugin)
{
    const char*  info = plugin->exifproxy->fliprating() ? plugin->exifproxy->rating() : "-/";
    eom_debug_message(DEBUG_PLUGINS, "Updating rating  %s %s\n", plugin->exifproxy->file().c_str(), info); 

    statusbar_set_rating(GTK_STATUSBAR(plugin->statusbar),
			 EOM_THUMB_VIEW(eom_window_get_thumb_view(plugin->window)),
			 info);
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
	    std::ostringstream  os;
	    std::for_each(h.begin(), h.end(), [&os](const auto& e) {
		os << e;
		g_print("%s\n", os.str().c_str());
		os.str("");
		os.clear();
	    });
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
    GtkWidget *statusbar = eom_window_get_statusbar (window);

    eom_debug (DEBUG_PLUGINS);

    manager = eom_window_get_ui_manager (plugin->window);

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    plugin->ui_action_group = gtk_action_group_new ("EomExiv2RatingPluginActions");

    gtk_action_group_add_actions (plugin->ui_action_group, action_entries,
	    G_N_ELEMENTS (action_entries), plugin);
    G_GNUC_END_IGNORE_DEPRECATIONS;


    plugin->statusbar = gtk_statusbar_new ();
    gint  minh, nath;
    gtk_widget_get_preferred_height(plugin->statusbar, &minh, &nath);
    gint  minw, natw;
    gtk_widget_get_preferred_width(plugin->statusbar, &minw, &natw);
    gtk_widget_set_size_request (plugin->statusbar, natw, nath);
    gtk_box_pack_end (GTK_BOX (statusbar), plugin->statusbar, FALSE, FALSE, 0);

    plugin->signal_id = g_signal_connect_after (G_OBJECT (thumbview), "selection_changed",
	    G_CALLBACK (selection_changed_cb), plugin);

    gtk_ui_manager_insert_action_group (manager, plugin->ui_action_group, -1);

    plugin->ui_id = gtk_ui_manager_add_ui_from_string (manager, ui_definition, -1, NULL);
    g_warn_if_fail (plugin->ui_id != 0);

    selection_changed_cb(EOM_THUMB_VIEW(eom_window_get_thumb_view(window)), plugin);
}

static void
eom_exiv2_rating_plugin_deactivate (EomWindowActivatable *activatable)
{
    EomExiv2RatingPlugin *plugin = EOM_EXIV2_RATING_PLUGIN (activatable);
    EomWindow *window = plugin->window;
    GtkWidget *statusbar = eom_window_get_statusbar (window);
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

    gtk_container_remove (GTK_CONTAINER (statusbar), plugin->statusbar);
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
    EomImage*  image = eom_thumb_view_get_first_selected_image (view);

    GFile* file = eom_image_get_file(image);
    char* path = g_file_get_path(file);

    plugin->exifproxy->ref(path);

    g_object_unref(file);
    eom_debug_message (DEBUG_PLUGINS, "selection changed to %s", path);

    g_free(path);

    statusbar_set_rating(GTK_STATUSBAR(plugin->statusbar),
			 EOM_THUMB_VIEW(eom_window_get_thumb_view(plugin->window)),
			 plugin->exifproxy->rating());
}