#ifndef EXIV2_GDKPIXBUF_BUILDR_ENV_H
#define EXIV2_GDKPIXBUF_BUILDR_ENV_H

#include <glib.h>
#include <gio/gio.h>

#include <string>


namespace  Exiv2GdkPxBufLdr
{
const gchar* const  KEY_ANNOTATION_FONT = "annotation-font";
const gchar* const  KEY_ANNOTATION_PERCENT_HEIGHT  = "annotation-percent-height";
const gchar* const  KEY_ANNOTATION_FONT_SIZE = "annotation-font-size";
const gchar* const  KEY_TRANSPARENCY = "transparency";
const gchar* const  KEY_SCALE_LIMIT = "scale-limit";
const gchar* const  KEY_CONVERT_SRGB = "convert-srgb";
const gchar* const  KEY_AUTO_ORIENTATE = "auto-orientate";


class Env
{
  public:
    Env(const Env&) = delete;
    Env& operator=(const Env&) = delete;

    static Env&  instance()
    {
	static Env  env;
	return env;
    }

    Env();
    ~Env();

    void  update(GSettings* settings_, const gchar* key_);

    unsigned short  previewScaleLimit() const
    { return _previewScaleLimit; }

    bool  convertSRGB() const
    { return _convertSRGB; }

    bool  rotate() const
    { return _rotate; }

    const std::string&  font() const
    { return _annotation.font; }

    const float  fontpcnt() const
    { return _annotation.percent; }

    unsigned short  fontsize() const
    { return _annotation.fontsize; }

    unsigned short  transparency() const
    { return _transparency; }
    
  private:
    unsigned short  _previewScaleLimit;
    bool   _convertSRGB;
    bool   _rotate;
    struct _Annotation {
	std::string  font;
	float  percent;
	unsigned short  fontsize;

        _Annotation(float percent_, unsigned short fontsize_) : percent(percent_), fontsize(fontsize_) { }
    } _annotation;
    unsigned short  _transparency;

    GSettings* _settings;
    gulong  _changesig;
};
}

#endif
