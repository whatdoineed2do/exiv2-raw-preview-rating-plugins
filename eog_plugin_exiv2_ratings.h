#ifndef __EOG_EXIV_XMP_RATING_PLUGIN_H__
#define __EOG_EXIV_XMP_RATING_PLUGIN_H__

extern "C" {

#include <glib.h>
#include <glib-object.h>
#include <libpeas/peas-extension-base.h>
#include <libpeas/peas-object-module.h>

#include <eog/eog-window.h>

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define EOG_TYPE_EXIV_XMP_RATING_PLUGIN		(eog_exiv2_ratings_plugin_get_type ())
#define EOG_EXIV_XMP_RATING_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), EOG_TYPE_EXIV_XMP_RATING_PLUGIN, EogExiv2RatingPlugin))
#define EOG_EXIV_XMP_RATING_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k),     EOG_TYPE_EXIV_XMP_RATING_PLUGIN, EogExiv2RatingPluginClass))
#define EOG_IS_EXIV_XMP_RATING_PLUGIN(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), EOG_TYPE_EXIV_XMP_RATING_PLUGIN))
#define EOG_IS_EXIV_XMP_RATING_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k),    EOG_TYPE_EXIV_XMP_RATING_PLUGIN))
#define EOG_EXIV_XMP_RATING_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o),  EOG_TYPE_EXIV_XMP_RATING_PLUGIN, EogExiv2RatingPluginClass))

typedef struct _EogExiv2RatingPluginPrivate	EogExiv2RatingPluginPrivate;

typedef struct _EogExiv2RatingPlugin		EogExiv2RatingPlugin;

class _ExifProxy;

struct _EogExiv2RatingPlugin
{
    PeasExtensionBase parent_instance;

    EogWindow *window;
    GtkWidget *statusbar_exif;
    GtkActionGroup *ui_action_group;
    guint ui_id;

    _ExifProxy*  exifproxy;
};

typedef struct _EogExiv2RatingPluginClass	EogExiv2RatingPluginClass;

struct _EogExiv2RatingPluginClass
{
    PeasExtensionBaseClass parent_class;
};

GType	eog_exiv2_ratings_plugin_get_type	(void) G_GNUC_CONST;

G_MODULE_EXPORT void peas_register_types (PeasObjectModule *module);

G_END_DECLS

}
#endif
