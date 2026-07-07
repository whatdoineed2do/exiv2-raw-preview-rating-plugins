#ifndef EOM_PREXTR_PLUGIN_H
#define EOM_PREXTR_PLUGIN_H

#include <glib-object.h>

G_BEGIN_DECLS

#define EOM_TYPE_PREXTR_PLUGIN (eom_prextr_plugin_get_type())
G_DECLARE_FINAL_TYPE(EomPrextrPlugin, eom_prextr_plugin, EOM, PREXTR_PLUGIN, GObject)

G_END_DECLS

#endif
