#include "imgprextr-plugin.h"

#include <eom/eom-window.h>
#include <eom/eom-window-activatable.h>
#include <libpeas/peas-activatable.h>
#include <libpeas/peas-object-module.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>

#define PREXTR_SCHEMA "org.mate.eom.plugins.prextr"
#define KEY_COMMAND_TEMPLATE "command-template"

struct _EomPrextrPlugin {
    GObject parent_instance;
    EomWindow *window;
    guint statusbar_context_id;
    guint statusbar_message_id;
};

enum {
    PROP_0,
    PROP_WINDOW
};

static void eom_window_activatable_iface_init(EomWindowActivatableInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED(EomPrextrPlugin, eom_prextr_plugin, G_TYPE_OBJECT, 0,
                               G_IMPLEMENT_INTERFACE_DYNAMIC(EOM_TYPE_WINDOW_ACTIVATABLE,
                                                             eom_window_activatable_iface_init))

// Helper to push a thread-safe update cleanly into the native eom status bar row
static void set_statusbar_text(EomPrextrPlugin *plugin, const char *text) {
    GtkWidget *statusbar = eom_window_get_statusbar(plugin->window);
    if (!statusbar) return;

    // Clear any previous message tracking handle we pushed on this context channel
    if (plugin->statusbar_message_id != 0) {
        gtk_statusbar_remove(GTK_STATUSBAR(statusbar), plugin->statusbar_context_id, plugin->statusbar_message_id);
        plugin->statusbar_message_id = 0;
    }

    if (text && std::strlen(text) > 0) {
        // Push the new string and record its unique tracking ID identifier
        plugin->statusbar_message_id = gtk_statusbar_push(GTK_STATUSBAR(statusbar), plugin->statusbar_context_id, text);
    }
}

// Timer callback used to clear the status bar text back to default seamlessly
static gboolean on_statusbar_timeout(gpointer user_data) {
    EomPrextrPlugin *plugin = EOM_PREXTR_PLUGIN(user_data);
    set_statusbar_text(plugin, "");
    return FALSE; // Disposes the timeout handle automatically
}

static void on_process_watch_complete(GPid pid, gint status, gpointer user_data) {
    EomPrextrPlugin *plugin = EOM_PREXTR_PLUGIN(user_data);

    if (g_spawn_check_wait_status(status, nullptr)) {
        set_statusbar_text(plugin, "✓ RAW preview image extract completed.");
        // Automatically wipe the text indicator clear after 4 seconds
        g_timeout_add_seconds(2, on_statusbar_timeout, plugin);
    } else {
        set_statusbar_text(plugin, "❌ RAW preview image extract failed.");
        g_timeout_add_seconds(6, on_statusbar_timeout, plugin);
    }

    g_spawn_close_pid(pid);
}

static void execute_extractor_cmd(EomPrextrPlugin* plugin) {
    EomImage* image = eom_window_get_image(plugin->window);
    if (!image) return;

    GFile* file = eom_image_get_file(image);
    g_object_unref(image);
    if (!file) return;

    char* filepath = g_file_get_path(file);
    g_object_unref(file);
    if (!filepath) return;

    std::string out_dir = "/tmp";
    const char* env_dir = std::getenv("EOM_GDKPIXBUF_EXIV2_OUTDIR");
    if (!env_dir) env_dir = std::getenv("TMPDIR");
    if (env_dir) out_dir = env_dir;

    std::string base_cmd = "imgprextr -p " + out_dir + " -c srgb";
    
    GSettingsSchemaSource* source = g_settings_schema_source_get_default();
    GSettingsSchema* schema = source ? g_settings_schema_source_lookup(source, PREXTR_SCHEMA, TRUE) : nullptr;
    if (schema) {
        GSettings* settings = g_settings_new(PREXTR_SCHEMA);
        char* custom_template = g_settings_get_string(settings, KEY_COMMAND_TEMPLATE);
        if (custom_template && std::strlen(custom_template) > 0) {
            base_cmd = custom_template;
        }
        g_free(custom_template);
        g_object_unref(settings);
        g_settings_schema_unref(schema);
    }

    std::vector<std::string> args;
    std::size_t pos = 0, prev = 0;
    while ((pos = base_cmd.find(' ', prev)) != std::string::npos) {
        if (pos > prev) args.push_back(base_cmd.substr(prev, pos - prev));
        prev = pos + 1;
    }
    if (prev < base_cmd.length()) args.push_back(base_cmd.substr(prev));
    args.push_back(filepath);

    std::vector<char*> argv;
    for (auto& arg : args) {
        argv.push_back(&arg[0]);
    }
    argv.push_back(nullptr);

    // Notify user directly inside the native status panel
    set_statusbar_text(plugin, "⏳ Extracting full raw image in background...");

    GError* error = nullptr;
    GPid child_pid;

    g_spawn_async(nullptr, argv.data(), nullptr, 
                  static_cast<GSpawnFlags>(G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD), 
                  nullptr, nullptr, &child_pid, &error);

    if (error) {
        set_statusbar_text(plugin, "❌ Failed to launch extraction tool.");
        g_timeout_add_seconds(5, on_statusbar_timeout, plugin);
        g_error_free(error);
    } else {
        g_child_watch_add(child_pid, on_process_watch_complete, plugin);
    }

    g_free(filepath);
}

static void on_shortcut_activated(GSimpleAction* action, GVariant* parameter, gpointer user_data) {
    EomPrextrPlugin* plugin = EOM_PREXTR_PLUGIN(user_data);
    execute_extractor_cmd(plugin);
}

static void eom_prextr_plugin_activate(EomWindowActivatable *activatable) {
    EomPrextrPlugin *plugin = EOM_PREXTR_PLUGIN(activatable);

    // Fetch the native window status bar reference pointer safely
    GtkWidget *statusbar = eom_window_get_statusbar(plugin->window);
    if (statusbar) {
        // Register a clean unique namespace context ID channel string signature for our plugin
        plugin->statusbar_context_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar), "imgprextr-plugin-status");
    }

    GSimpleAction* action = g_simple_action_new("run-imgprextr", nullptr);
    g_signal_connect(action, "activate", G_CALLBACK(on_shortcut_activated), plugin);
    g_action_map_add_action(G_ACTION_MAP(plugin->window), G_ACTION(action));
    g_object_unref(action);

    GtkApplication* app = GTK_APPLICATION(g_application_get_default());
    const char* accels[] = { "x", nullptr };
    gtk_application_set_accels_for_action(app, "win.run-imgprextr", accels);
}

static void eom_prextr_plugin_deactivate(EomWindowActivatable *activatable) {
    EomPrextrPlugin *plugin = EOM_PREXTR_PLUGIN(activatable);
    
    // Clear out status bar contents before detaching
    set_statusbar_text(plugin, "");
    g_action_map_remove_action(G_ACTION_MAP(plugin->window), "run-imgprextr");
}

static void eom_prextr_plugin_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    EomPrextrPlugin *plugin = EOM_PREXTR_PLUGIN(object);
    switch (prop_id) {
        case PROP_WINDOW:
            plugin->window = EOM_WINDOW(g_value_dup_object(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void eom_prextr_plugin_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    EomPrextrPlugin *plugin = EOM_PREXTR_PLUGIN(object);
    switch (prop_id) {
        case PROP_WINDOW:
            g_value_set_object(value, plugin->window);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void eom_prextr_plugin_dispose(GObject *object) {
    EomPrextrPlugin *plugin = EOM_PREXTR_PLUGIN(object);
    if (plugin->window) {
        g_object_unref(plugin->window);
        plugin->window = nullptr;
    }
    G_OBJECT_CLASS(eom_prextr_plugin_parent_class)->dispose(object);
}

static void eom_prextr_plugin_init(EomPrextrPlugin *plugin) {
    plugin->statusbar_context_id = 0;
    plugin->statusbar_message_id = 0;
}

static void eom_prextr_plugin_class_finalize(EomPrextrPluginClass *klass) {}

static void eom_prextr_plugin_class_init(EomPrextrPluginClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = eom_prextr_plugin_dispose;
    object_class->set_property = eom_prextr_plugin_set_property;
    object_class->get_property = eom_prextr_plugin_get_property;

    g_object_class_override_property(object_class, PROP_WINDOW, "window");
}

static void eom_window_activatable_iface_init(EomWindowActivatableInterface *iface) {
    iface->activate = eom_prextr_plugin_activate;
    iface->deactivate = eom_prextr_plugin_deactivate;
}

extern "C" G_MODULE_EXPORT void peas_register_types(PeasObjectModule *module) {
    eom_prextr_plugin_register_type(G_TYPE_MODULE(module));
    peas_object_module_register_extension_type(module, EOM_TYPE_WINDOW_ACTIVATABLE, EOM_TYPE_PREXTR_PLUGIN);
}
