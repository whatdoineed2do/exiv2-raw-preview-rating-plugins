#ifndef __EOM_EXIF_RATING_PLUGIN_H__
#define __EOM_EXIF_RATING_PLUGIN_H__


#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>
#include <gtk/gtk.h>
#include <libpeas/peas-activatable.h>
#include <libpeas/peas-extension-base.h>
#include <libpeas/peas-object-module.h>

extern "C" {
#include <eom/eom-debug.h>
#include <eom/eom-window.h>
#include <eom/eom-window-activatable.h>
#include <eom/eom-thumb-view.h>
}

G_BEGIN_DECLS


/*
 * Type checking and casting macros
 */
#define EOM_TYPE_EXIV2_RATING_PLUGIN \
	(eom_exiv2_rating_plugin_get_type())
#define EOM_EXIV2_RATING_PLUGIN(o) \
	(G_TYPE_CHECK_INSTANCE_CAST((o), EOM_TYPE_EXIV2_RATING_PLUGIN, EomExiv2RatingPlugin))
#define EOM_EXIV2_RATING_PLUGIN_CLASS(k) \
	(G_TYPE_CHECK_CLASS_CAST((k), EOM_TYPE_EXIV2_RATING_PLUGIN, EomExiv2RatingPluginClass))
#define EOM_IS_EXIV2_RATING_PLUGIN(o) \
	(G_TYPE_CHECK_INSTANCE_TYPE((o), EOM_TYPE_EXIV2_RATING_PLUGIN))
#define EOM_IS_EXIV2_RATING_PLUGIN_CLASS(k) \
	(G_TYPE_CHECK_CLASS_TYPE((k), EOM_TYPE_EXIV2_RATING_PLUGIN))
#define EOM_EXIV2_RATING_PLUGIN_GET_CLASS(o) \
	(G_TYPE_INSTANCE_GET_CLASS((o), EOM_TYPE_EXIV2_RATING_PLUGIN, EomExiv2RatingPluginClass))

/* Private structure type */
typedef struct _EomExiv2RatingPluginPrivate EomExiv2RatingPluginPrivate;

/*
 * Main object structure
 */
typedef struct _EomExiv2RatingPlugin EomExiv2RatingPlugin;

struct _EomExiv2RatingPlugin {
	PeasExtensionBase parent_instance;

	EomWindow *window;
	GtkActionGroup *ui_action_group;
	guint ui_id;
	gulong signal_id;
};

/*
 * Class definition
 */
typedef struct _EomExiv2RatingPluginClass EomExiv2RatingPluginClass;

struct _EomExiv2RatingPluginClass {
	PeasExtensionBaseClass parent_class;
};

/*
 * Public methods
 */
GType eom_exiv2_rating_plugin_get_type (void) G_GNUC_CONST;

/* All the plugins must implement this function */
G_MODULE_EXPORT void peas_register_types (PeasObjectModule *module);

G_END_DECLS


#endif
