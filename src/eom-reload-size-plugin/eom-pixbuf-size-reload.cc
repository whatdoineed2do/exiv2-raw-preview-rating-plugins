#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <libpeas/peas.h>
#include <eom/eom-window-activatable.h>
#include <eom/eom-window.h>
#include <eom/eom-image.h>
#include <eom/eom-thumb-view.h>

#include "eom-pixbuf-size-reload.h"

#define G_LOG_DOMAIN_EOM_RAW_PREVIEW "eom:largest-rawpreview"
#define RAW_PREVIEW_ACTION_NAME      "raw-load-largest"

/* GSettings Constants */
#define RAW_PREVIEW_SCHEMA           "org.gtk.gdk-pixbuf.exiv2-rawpreview"
#define SCALE_LIMIT_KEY              "scale-limit"
#define SCALE_LIMIT_LARGEST          0

extern "C" {
static void eom_window_activatable_iface_init (EomWindowActivatableInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EomRawPreviewPlugin,
                                eom_raw_preview_plugin,
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
static gboolean
on_restore_timeout_cb (gpointer user_data)
{
    EomRawPreviewPlugin *plugin = EOM_RAW_PREVIEW_PLUGIN (user_data);

    if (plugin->previewLimit != -1) {
        g_log (G_LOG_DOMAIN_EOM_RAW_PREVIEW, G_LOG_LEVEL_DEBUG, 
               "Background load safety window expired. Restoring GSetting to: %d", plugin->previewLimit);

        GSettings *settings = g_settings_new (RAW_PREVIEW_SCHEMA);
        g_settings_set_int (settings, SCALE_LIMIT_KEY, plugin->previewLimit);
        g_settings_sync ();
        g_object_unref (settings);

        // Reset our stash guard variable
        plugin->previewLimit = -1;
    }

    // Return FALSE so this timer only executes ONCE
    return G_SOURCE_REMOVE;
}

static void
on_load_largest_preview_triggered (GSimpleAction *action,
                                   GVariant      *parameter,
                                   gpointer       user_data)
{
    EomRawPreviewPlugin *plugin = EOM_RAW_PREVIEW_PLUGIN (user_data);
    EomWindow *window = plugin->window;

    EomImage *image = eom_window_get_image (window);
    if (!image) {
        g_log (G_LOG_DOMAIN_EOM_RAW_PREVIEW, G_LOG_LEVEL_DEBUG, "No active image found to reload.");
        return;
    }

    gchar *current_uri = NULL;
    GFile *file = eom_image_get_file (image);
    if (file != NULL) {
        current_uri = g_file_get_uri (file);
        g_object_unref (file);
    }

    if (current_uri != NULL && plugin->boostedImageUri != NULL) {
        if (g_strcmp0 (current_uri, plugin->boostedImageUri) == 0 || plugin->previewLimit != -1) {
            g_log (G_LOG_DOMAIN_EOM_RAW_PREVIEW, G_LOG_LEVEL_INFO,
                   "%s. Image is already maximized. Bypassing wasteful reload.", current_uri);
            g_free (current_uri);
            return;
        }
    }

    GSettings *settings = g_settings_new (RAW_PREVIEW_SCHEMA);

    plugin->previewLimit = g_settings_get_int (settings, SCALE_LIMIT_KEY);

    g_settings_set_int (settings, SCALE_LIMIT_KEY, SCALE_LIMIT_LARGEST);
    g_settings_sync ();
    g_object_unref (settings);

    // Synchronize the Env proxy singleton cache immediately
    while (g_main_context_pending (NULL)) {
        g_main_context_iteration (NULL, FALSE);
    }

    g_log (G_LOG_DOMAIN_EOM_RAW_PREVIEW, G_LOG_LEVEL_DEBUG, "Triggering core window view image reload, preview size=0...");

    // Save the active URI to our persistent tracker string
    if (plugin->boostedImageUri != NULL) {
        g_free (plugin->boostedImageUri);
    }
    plugin->boostedImageUri = current_uri; // Transfers ownership of the string allocated by g_file_get_uri

    eom_window_reload_image (window);

    // Schedule a safety window of 300ms to allow the background thread to run
    g_timeout_add (300, on_restore_timeout_cb, plugin);
}
}

static const GActionEntry plugin_actions[] = {
    { RAW_PREVIEW_ACTION_NAME, on_load_largest_preview_triggered, NULL, NULL, NULL, {0} }
};

static void
eom_raw_preview_plugin_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
    EomRawPreviewPlugin *plugin = EOM_RAW_PREVIEW_PLUGIN (object);

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
eom_raw_preview_plugin_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
    EomRawPreviewPlugin *plugin = EOM_RAW_PREVIEW_PLUGIN (object);

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
eom_raw_preview_plugin_init (EomRawPreviewPlugin *plugin)
{
    plugin->previewLimit = -1;
    plugin->boostedImageUri = NULL;
}

static void
eom_raw_preview_plugin_dispose (GObject *object)
{
    EomRawPreviewPlugin *plugin = EOM_RAW_PREVIEW_PLUGIN (object);

    if (plugin->boostedImageUri != NULL) {
        g_free (plugin->boostedImageUri);
        plugin->boostedImageUri = NULL;
    }

    if (plugin->window != NULL) {
        g_object_unref (plugin->window);
        plugin->window = NULL;
    }

    G_OBJECT_CLASS (eom_raw_preview_plugin_parent_class)->dispose (object);
}


static void
eom_raw_preview_plugin_activate (EomWindowActivatable *activatable)
{
    EomRawPreviewPlugin *plugin = EOM_RAW_PREVIEW_PLUGIN (activatable);
    EomWindow *window = plugin->window;

    g_action_map_add_action_entries (G_ACTION_MAP (window),
                                     plugin_actions,
                                     G_N_ELEMENTS (plugin_actions),
                                     plugin);

    GtkApplication *app = GTK_APPLICATION (g_application_get_default ());
    if (app != NULL) {
        const gchar *accel_keys[] = { "V", NULL };
        gtk_application_set_accels_for_action (app, "win." RAW_PREVIEW_ACTION_NAME, accel_keys);
    }
}

static void
eom_raw_preview_plugin_deactivate (EomWindowActivatable *activatable)
{
    EomRawPreviewPlugin *plugin = EOM_RAW_PREVIEW_PLUGIN (activatable);
    EomWindow *window = plugin->window;

    GtkApplication *app = GTK_APPLICATION (g_application_get_default ());
    if (app != NULL) {
        const gchar *empty_accel[] = { NULL };
        gtk_application_set_accels_for_action (app, "win." RAW_PREVIEW_ACTION_NAME, empty_accel);
    }

    for (guint i = 0; i < G_N_ELEMENTS (plugin_actions); i++) {
        g_action_map_remove_action (G_ACTION_MAP (window), plugin_actions[i].name);
    }
}

static void
eom_raw_preview_plugin_class_init (EomRawPreviewPluginClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = eom_raw_preview_plugin_dispose;
    object_class->set_property = eom_raw_preview_plugin_set_property;
    object_class->get_property = eom_raw_preview_plugin_get_property;

    g_object_class_override_property (object_class, PROP_WINDOW, "window");
}

static void
eom_raw_preview_plugin_class_finalize (EomRawPreviewPluginClass *klass)
{
    /* dummy function - used by G_DEFINE_DYNAMIC_TYPE_EXTENDED */
}

extern "C" {
static void
eom_window_activatable_iface_init (EomWindowActivatableInterface *iface)
{
    iface->activate = eom_raw_preview_plugin_activate;
    iface->deactivate = eom_raw_preview_plugin_deactivate;
}

G_MODULE_EXPORT void
peas_register_types (PeasObjectModule *module)
{
    eom_raw_preview_plugin_register_type (G_TYPE_MODULE (module));
    peas_object_module_register_extension_type (module,
                                                EOM_TYPE_WINDOW_ACTIVATABLE,
                                                EOM_TYPE_RAW_PREVIEW_PLUGIN);
}
}
