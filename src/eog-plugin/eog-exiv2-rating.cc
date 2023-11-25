#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "eog-exiv2-rating.h"


#include <string>
#include <list>
#include <strstream>
#include <cassert>
#include <cmath>
#include <mutex>

#include <exiv2/exiv2.hpp>

#include <ExifProxy.h>


namespace {
const char*  _version()
{
    return VERSION;
}

const char* eog_exiv2_rating_version = _version();
}


enum {
  PROP_0,
  PROP_WINDOW
};

#define EOG_EXIV_XMP_RATING_PLUGIN_MENU_ID  "EogPluginRunExiv2Rating"
#define EOG_EXIV_XMP_RATING_PLUGIN_ACTION   "exiv2rating"
#define EOG_EXIV_XMP_RATING_PLUGIN_ACTION_U "exiv2rating_u"

static void eog_window_activatable_iface_init (EogWindowActivatableInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EogExiv2RatingPlugin,
                                eog_exiv2_ratings_plugin,
                                PEAS_TYPE_EXTENSION_BASE,
                                0,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (EOG_TYPE_WINDOW_ACTIVATABLE,
                                                               eog_window_activatable_iface_init))


static void
_upd_statusbar_exif(GtkStatusbar* statusbar_,
                    const char* rating_)
{
    gtk_statusbar_pop(statusbar_, 0);
    if (rating_ == NULL) {
        gtk_widget_hide(GTK_WIDGET(statusbar_));
        return;
    }
    char  buf[19] = { 0 };
    snprintf(buf, sizeof(buf), "EXIF rating: %s", rating_);

    gtk_statusbar_push(statusbar_, 0, buf);
    gtk_widget_show(GTK_WIDGET(statusbar_));
}


static void
_exiv2_rating_setunset(GSimpleAction *simple, GVariant *parameter, gpointer user_data, const bool set_)
{
    EogExiv2RatingPlugin *plugin = EOG_EXIV_XMP_RATING_PLUGIN (user_data);

    _upd_statusbar_exif(GTK_STATUSBAR(plugin->statusbar_exif),
                        plugin->exifproxy->fliprating() ? plugin->exifproxy->rating() : "-/-");
}

static void
_exiv2_rating_set_cb(GSimpleAction *simple, GVariant      *parameter, gpointer       user_data)
{
    _exiv2_rating_setunset(simple, parameter, user_data, true);
}

static void
_exiv2_rating_unset_cb(GSimpleAction *simple, GVariant *parameter, gpointer user_data)
{
    _exiv2_rating_setunset(simple, parameter, user_data, false);
}


static void
eog_exiv2_ratings_plugin_set_property (GObject      *object,
				guint         prop_id,
				const GValue *value,
				GParamSpec   *pspec)
{
	EogExiv2RatingPlugin *plugin = EOG_EXIV_XMP_RATING_PLUGIN (object);

	switch (prop_id)
	{
	case PROP_WINDOW:
		plugin->window = EOG_WINDOW (g_value_dup_object (value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
eog_exiv2_ratings_plugin_get_property (GObject    *object,
				guint       prop_id,
				GValue     *value,
				GParamSpec *pspec)
{
	EogExiv2RatingPlugin *plugin = EOG_EXIV_XMP_RATING_PLUGIN (object);

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
eog_exiv2_ratings_plugin_init (EogExiv2RatingPlugin *plugin)
{
#ifdef EOG_PLUGIN_DEBUG 
	eog_debug_message (DEBUG_PLUGINS, "EogExiv2RatingPlugin initializing");
#endif
        plugin->exifproxy = new ExifProxy();
}

static void
eog_exiv2_ratings_plugin_dispose (GObject *object)
{
	EogExiv2RatingPlugin *plugin = EOG_EXIV_XMP_RATING_PLUGIN (object);

#ifdef EOG_PLUGIN_DEBUG 
	eog_debug_message (DEBUG_PLUGINS, "EogExiv2RatingPlugin disposing");
#endif

	if (plugin->window != NULL) 
        {
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

	G_OBJECT_CLASS (eog_exiv2_ratings_plugin_parent_class)->dispose (object);
}

static void
eog_exiv2_ratings_plugin_update_action_state (EogExiv2RatingPlugin *plugin, EogThumbView *view)
{
    GAction *action;
    EogThumbView *thumbview;
    gboolean enable = FALSE;

    thumbview = view == NULL ? EOG_THUMB_VIEW (eog_window_get_thumb_view (plugin->window)) : view;

    if (G_LIKELY (thumbview))
    {
        enable = (eog_thumb_view_get_n_selected (thumbview) != 0);
        if (enable) {
	    EogImage*  image = eog_thumb_view_get_first_selected_image(thumbview);
	    GFile*  f = eog_image_get_file(image);
	    char* const  fpath = g_file_get_path(f);

	    plugin->exifproxy->ref(fpath);
	    g_free(fpath);

            _upd_statusbar_exif(GTK_STATUSBAR(plugin->statusbar_exif),
                                plugin->exifproxy->valid() ? 
                                    plugin->exifproxy->rating() : "-/-");
        }
    }

    action = g_action_map_lookup_action (G_ACTION_MAP (plugin->window),
                                         EOG_EXIV_XMP_RATING_PLUGIN_ACTION);
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enable);
}

static void
_selection_changed_cb (EogThumbView *thumbview, gpointer user_data)
{
    EogExiv2RatingPlugin *plugin = EOG_EXIV_XMP_RATING_PLUGIN (user_data);

    if (G_LIKELY (plugin)) {
        eog_exiv2_ratings_plugin_update_action_state (plugin, thumbview);
    }
}

static void
eog_exiv2_ratings_plugin_activate (EogWindowActivatable *activatable)
{
    const gchar * const accel_keys[] = { "R", NULL };
    EogExiv2RatingPlugin *plugin = EOG_EXIV_XMP_RATING_PLUGIN (activatable);
    GMenu *model;
    GSimpleAction *action;


    GtkWidget *statusbar = eog_window_get_statusbar(plugin->window);
    plugin->statusbar_exif = gtk_statusbar_new();

    gint  minh, nath;
    gtk_widget_get_preferred_height(plugin->statusbar_exif, &minh, &nath);
    gint  minw, natw;
    gtk_widget_get_preferred_width(plugin->statusbar_exif, &minw, &natw);

    gtk_widget_set_size_request (plugin->statusbar_exif, natw, nath);
    gtk_box_pack_end (GTK_BOX(statusbar), plugin->statusbar_exif, FALSE, FALSE, 0);


#ifdef EOG_PLUGIN_DEBUG 
    eog_debug (DEBUG_PLUGINS);
#endif

    model= eog_window_get_gear_menu_section (plugin->window,
                                             "plugins-section");

    g_return_if_fail (G_IS_MENU (model));

    /* Setup and inject action */
    action = g_simple_action_new (EOG_EXIV_XMP_RATING_PLUGIN_ACTION, NULL);
#ifndef MULTI_CB
    g_signal_connect(action, "activate", G_CALLBACK (_exiv2_rating_set_cb), plugin);
    g_action_map_add_action (G_ACTION_MAP (plugin->window), G_ACTION (action));
#else
    g_action_map_add_action_entries(G_ACTION_MAP(plugin->window),
                                    actions, G_N_ELEMENTS(actions), plugin->window);
#endif

    g_object_unref (action);

    g_signal_connect (G_OBJECT (eog_window_get_thumb_view (plugin->window)),
                      "selection-changed",
                      G_CALLBACK (_selection_changed_cb),
                      plugin);
    eog_exiv2_ratings_plugin_update_action_state (plugin, EOG_THUMB_VIEW (eog_window_get_thumb_view (plugin->window)));


    /* Define accelerator keys */
    gtk_application_set_accels_for_action (GTK_APPLICATION (EOG_APP),
                                           "win." EOG_EXIV_XMP_RATING_PLUGIN_ACTION,
                                           accel_keys);
}

static void
eog_exiv2_ratings_plugin_deactivate (EogWindowActivatable *activatable)
{
    const gchar * const empty_accels[1] = { NULL };
    EogExiv2RatingPlugin *plugin = EOG_EXIV_XMP_RATING_PLUGIN (activatable);

#ifdef EOG_PLUGIN_DEBUG
    eog_debug (DEBUG_PLUGINS);
#endif

    /* Unset accelerator */
    gtk_application_set_accels_for_action(GTK_APPLICATION (EOG_APP),
                                          "win." EOG_EXIV_XMP_RATING_PLUGIN_ACTION,
                                          empty_accels);

    /* Disconnect selection-changed handler as the thumbview would
     * otherwise still cause callbacks during its own disposal */
    g_signal_handlers_disconnect_by_func (eog_window_get_thumb_view (plugin->window),
                                          (gpointer)_selection_changed_cb, plugin);

    /* Finally remove action */
    g_action_map_remove_action (G_ACTION_MAP (plugin->window),
                                EOG_EXIV_XMP_RATING_PLUGIN_ACTION);

    GtkWidget *statusbar = eog_window_get_statusbar (plugin->window);
    gtk_container_remove(GTK_CONTAINER (statusbar), plugin->statusbar_exif);
}

static void
eog_exiv2_ratings_plugin_class_init (EogExiv2RatingPluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose= eog_exiv2_ratings_plugin_dispose;
	object_class->set_property = eog_exiv2_ratings_plugin_set_property;
	object_class->get_property = eog_exiv2_ratings_plugin_get_property;

	g_object_class_override_property (object_class, PROP_WINDOW, "window");
}

static void
eog_exiv2_ratings_plugin_class_finalize (EogExiv2RatingPluginClass *klass)
{
}

static void
eog_window_activatable_iface_init (EogWindowActivatableInterface *iface)
{
	iface->activate = eog_exiv2_ratings_plugin_activate;
	iface->deactivate = eog_exiv2_ratings_plugin_deactivate;
}

extern "C" 
{
G_MODULE_EXPORT void
peas_register_types (PeasObjectModule *module)
{
	eog_exiv2_ratings_plugin_register_type (G_TYPE_MODULE (module));
	peas_object_module_register_extension_type (module,
						    EOG_TYPE_WINDOW_ACTIVATABLE,
						    EOG_TYPE_EXIV_XMP_RATING_PLUGIN);
}
}
