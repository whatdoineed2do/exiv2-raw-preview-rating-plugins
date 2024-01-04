#include "Env.h"

#include "DbgHlpr.h"

namespace  Exiv2GdkPxBufLdr
{
void on_settings_changed(GSettings* settings_, const gchar* key_, gpointer this_)
{
    DBG_LOG("gsettings change on '%s'", key_);
    ((Env*)this_)->update(settings_, key_);
}


Env::Env() 
    : _previewScaleLimit(1632),
     /* The D800 has 4x embedded imgs (160, 530, 1632, * full 7360) - no 
      * point picking the largest one and scaling so we should deflt to 
      * something sensible
      */
      _convertSRGB(false),
      _rotate(true),
      _fontsize(16),
      _settings(NULL)
{
    const gchar *schema_id = "org.gtk.gdk-pixbuf.exiv2-rawpreview";


    g_log(Exiv2GdkPxBufLdr::G_DOMAIN, G_LOG_LEVEL_INFO, "see configurable options via: 'gsettings list-recursively %s'", schema_id);

    _settings = g_settings_new(schema_id);

    update(_settings, KEY_SCALE_LIMIT);
    update(_settings, KEY_FONT);
    update(_settings, KEY_FONT_SIZE);
    update(_settings, KEY_TRANSPARENCY);
    update(_settings, KEY_CONVERT_SRGB);
    update(_settings, KEY_AUTO_ORIENTATE);

    g_log(Exiv2GdkPxBufLdr::G_DOMAIN, G_LOG_LEVEL_INFO, "intial values for schema=%s %s=%d  %s='%s' %s=%d  %s=%d  %s=%s  %s=%s",
       schema_id,
       KEY_SCALE_LIMIT, _previewScaleLimit,
       KEY_FONT, _font.c_str(),
       KEY_FONT_SIZE, _fontsize,
       KEY_TRANSPARENCY, _transparency,
       KEY_CONVERT_SRGB, _convertSRGB ? "true" : "false",
       KEY_AUTO_ORIENTATE, _rotate ? "true" : "false");

    _changesig = g_signal_connect(G_OBJECT(_settings), "changed", G_CALLBACK(on_settings_changed), this);
}

Env::~Env()
{
    g_signal_handler_disconnect(G_OBJECT(_settings), _changesig);
    g_object_unref(_settings);
}

void  Env::update(GSettings* settings_, const gchar* key_)
{
    if (g_strcmp0(key_, KEY_SCALE_LIMIT) == 0) {
	_previewScaleLimit = g_settings_get_int(settings_, key_);
    }
    else if (g_strcmp0(key_, KEY_CONVERT_SRGB) == 0) {
	_convertSRGB = g_settings_get_boolean(settings_, key_);
    }
    else if (g_strcmp0(key_, KEY_AUTO_ORIENTATE) == 0) {
	_rotate = g_settings_get_boolean(settings_, key_);
    }
    else if (g_strcmp0(key_, KEY_TRANSPARENCY) == 0) {
	_transparency = g_settings_get_int(settings_, key_);
	if      (_transparency > 100)  _transparency = 100;
	else if (_transparency < 0)    _transparency = 0;
    }
    else if (g_strcmp0(key_, KEY_FONT_SIZE) == 0) {
	int  fontsize = g_settings_get_int(settings_, key_);
	if (fontsize > 0) {
	    _fontsize = fontsize;
	}
    }
    else if (g_strcmp0(key_, KEY_FONT) == 0) {
	gchar*  font = g_settings_get_string(settings_, key_);
	_font = font;
	g_free(font);
    }
    else 
    {
	g_log(Exiv2GdkPxBufLdr::G_DOMAIN, G_LOG_LEVEL_WARNING, "unhandled gsettings key '%s' change", key_);
    }
}
}
