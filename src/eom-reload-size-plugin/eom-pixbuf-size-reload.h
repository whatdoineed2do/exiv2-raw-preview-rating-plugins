#ifndef __EOM_PIXBUF_SIZE_RELOAD_H__
#define __EOM_PIXBUF_SIZE_RELOAD_H__

#include <glib-object.h>
#include <libpeas/peas.h>
#include <eom/eom-window.h>

G_BEGIN_DECLS

/* Define GObject Type Cast Macros */
#define EOM_TYPE_RAW_PREVIEW_PLUGIN           (eom_raw_preview_plugin_get_type ())
#define EOM_RAW_PREVIEW_PLUGIN(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), EOM_TYPE_RAW_PREVIEW_PLUGIN, EomRawPreviewPlugin))
#define EOM_RAW_PREVIEW_PLUGIN_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST ((klass), EOM_TYPE_RAW_PREVIEW_PLUGIN, EomRawPreviewPluginClass))
#define EOM_IS_RAW_PREVIEW_PLUGIN(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EOM_TYPE_RAW_PREVIEW_PLUGIN))
#define EOM_IS_RAW_PREVIEW_PLUGIN_CLASS(klass)(G_TYPE_CHECK_CLASS_TYPE ((klass), EOM_TYPE_RAW_PREVIEW_PLUGIN))
#define EOM_RAW_PREVIEW_PLUGIN_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), EOM_TYPE_RAW_PREVIEW_PLUGIN, EomRawPreviewPluginClass))

typedef struct _EomRawPreviewPlugin        EomRawPreviewPlugin;
typedef struct _EomRawPreviewPluginClass   EomRawPreviewPluginClass;

/* The Plugin Instance Structure */
struct _EomRawPreviewPlugin
{
    PeasExtensionBase parent_instance;

    EomWindow *window;
    gulong     selection_id;
    int        previewLimit;
    gchar     *boostedImageUri;
    guint      timeout_id;
};

struct _EomRawPreviewPluginClass
{
    PeasExtensionBaseClass parent_class;
};

GType eom_raw_preview_plugin_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif
