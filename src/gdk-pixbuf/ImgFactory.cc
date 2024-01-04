#include "ImgFactory.h"

#include <omp.h>

#include <array>
#include <functional>
#include <chrono>
#include <thread>

#include <glib.h>
#include <gio/gio.h>

#include "Buf.h"


namespace  Exiv2GdkPxBufLdr
{
const gchar* const  KEY_FONT = "font";
const gchar* const  KEY_FONT_SIZE = "font-size";
const gchar* const  KEY_TRANSPARENCY = "transparency";
const gchar* const  KEY_SCALE_LIMIT = "scale-limit";
const gchar* const  KEY_CONVERT_SRGB = "convert-srgb";
const gchar* const  KEY_AUTO_ORIENTATE = "auto-orientate";


void on_settings_changed(GSettings* settings_, const gchar* key_, gpointer this_);

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

    Env() 
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

    ~Env()
    {
	g_signal_handler_disconnect(G_OBJECT(_settings), _changesig);
	g_object_unref(_settings);
    }

    void  update(GSettings* settings_, const gchar* key_)
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

    unsigned short  previewScaleLimit() const
    { return _previewScaleLimit; }

    bool  convertSRGB() const
    { return _convertSRGB; }

    bool  rotate() const
    { return _rotate; }

    const std::string&  font() const
    { return _font; }

    unsigned short  fontsize() const
    { return _fontsize; }

    unsigned short  transparency() const
    { return _transparency; }
    
  private:
    unsigned short  _previewScaleLimit;
    bool   _convertSRGB;
    bool   _rotate;
    std::string  _font;
    unsigned short  _fontsize;
    unsigned short  _transparency;

    GSettings* _settings;
    gulong  _changesig;
};

void on_settings_changed(GSettings* settings_, const gchar* key_, gpointer this_)
{
    DBG_LOG("gsettings change on '%s'", key_);
    ((Env*)this_)->update(settings_, key_);
}



ImgFactory&  ImgFactory::instance()
{
    static ImgFactory  instance;
    return instance;
}


ImgFactory::ImgFactory()
    : _argbICC(_profiles.NKaRGB().data, _profiles.NKaRGB().size),
      _srgbICC(_profiles.srgb().data, _profiles.srgb().size)
{
    Magick::InitializeMagick("");
    DBG_LOG("OMP: cpus=", omp_get_num_procs(), " threads=", omp_get_max_threads(), "  setting resource limits to #threads");
    Magick::ResourceLimits::thread(std::thread::hardware_concurrency());
}

ImgFactory::~ImgFactory()
{
    Magick::TerminateMagick();
}

ImgFactory::Buf&  ImgFactory::create(FILE* f_, ImgFactory::Buf& buf_, std::string& mimeType_)
{
    const long  where = ftell(f_);
    fseek(f_, 0, SEEK_END);
    const size_t  sz = ftell(f_);
    fseek(f_, 0, SEEK_SET);
    _tmp.alloc(sz);
    if ( (fread (_tmp.buf(), 1, sz, f_)) != sz) {
    }
    fseek(f_, where, SEEK_SET);

    g_log(Exiv2GdkPxBufLdr::G_DOMAIN, G_LOG_LEVEL_DEBUG, "processing via FILE");
    
    return create(_tmp.buf(), sz, buf_, mimeType_);
}

ImgFactory::Buf&  ImgFactory::create(const unsigned char* buf_, ssize_t bufsz_, ImgFactory::Buf& imgbuf_, std::string& mimeType_)
{
    if (bufsz_ <= 0) {
	throw std::invalid_argument("invalid image data");
    }

    const Exiv2GdkPxBufLdr::Env&  env = Exiv2GdkPxBufLdr::Env::instance();


#if EXIV2_VERSION >= EXIV2_MAKE_VERSION(0,28,0)
    Exiv2::Image::UniquePtr
#else
    Exiv2::Image::AutoPtr
#endif
    orig = Exiv2::ImageFactory::open(buf_, bufsz_);
    if (!orig->good()) {
	std::ostringstream  err;
	err << "invalid image data (len=" << bufsz_ << ")";
        throw std::invalid_argument(err.str());
    }
    orig->readMetadata();

    Exiv2::PreviewManager  exvprldr_(*orig);

    const int  width_ = orig->pixelWidth();
    const int  height_ = orig->pixelHeight();
    const Exiv2::ExifData  exif_ = orig->exifData();


    Exiv2::PreviewPropertiesList  list =  exvprldr_.getPreviewProperties();

    g_log(Exiv2GdkPxBufLdr::G_DOMAIN, G_LOG_LEVEL_INFO, "previews=%ld, mime-type=%s", list.size(), orig->mimeType().c_str());
    DBG_LOG("#previews=", list.size());
    if (list.empty()) {
	throw std::underflow_error("no embedded previews");
    }

    /* exiv2 provides images sorted from small->large -  grabbing the 
     * largest preview but try to avoid getting somethign too large due
     * a bug in cairo-xlib-surface-shm.c:619 mem pool exhausting bug
     *
     * if the prev img is larger than PREVIEW_LIMIT megapxls (on longest
     * edge) then try to either get another preview image or to scale it 
     * using Magick++
     */
    const unsigned short  PREVIEW_LIMIT = env.previewScaleLimit();

    Exiv2::PreviewPropertiesList::iterator  p = list.begin();
    Exiv2::PreviewPropertiesList::iterator  pp = list.end();
    unsigned  i = 0;
    while (p != list.end())
    {
        g_log(Exiv2GdkPxBufLdr::G_DOMAIN, G_LOG_LEVEL_INFO, "  preview #%d width=%ld height=%ld", i, p->width_, p->height_);
	if (p->width_ >= PREVIEW_LIMIT || p->height_ >= PREVIEW_LIMIT) {
	    pp = p;
            DBG_LOG("preview #", i, "  SELECTED");
            break;
	}
	++p;
	++i;
    }
    if (pp == list.end()) {
	pp = list.begin();
        std::advance(pp, --i);
    }

    Exiv2::PreviewImage  preview =  exvprldr_.getPreviewImage(*pp);
    mimeType_ = preview.mimeType();
    g_log(Exiv2GdkPxBufLdr::G_DOMAIN, G_LOG_LEVEL_INFO, "  using preview #%d width=%ld height=%ld mime-type=%s", i, pp->width_, pp->height_, mimeType_.c_str());
    Exiv2::ExifData::const_iterator  d;

    Magick::Image  magick;

    if (pp->width_ > PREVIEW_LIMIT || pp->height_ > PREVIEW_LIMIT)
    {
	if (!magick.isValid()) {
            DBG_LOG("loading from preview for scaling");
	    magick.read( Magick::Blob(preview.pData(), preview.size()) );
	}

	magick.filterType(Magick::LanczosFilter);
	magick.quality(70);
	char  tmp[8];  // its a short, can't be more than 65535
	const int  len = snprintf(tmp, sizeof(tmp), " %d ", PREVIEW_LIMIT);
        if (magick.rows() < magick.columns()) {
            tmp[len-1] = 'x';
        }
        else {
            tmp[0] = 'x';
        }
	g_log(Exiv2GdkPxBufLdr::G_DOMAIN, G_LOG_LEVEL_INFO, "  scaling preview #%d to %s", i, tmp);
            const std::chrono::time_point<std::chrono::system_clock>  start = std::chrono::system_clock::now();
	magick.resize(Magick::Geometry(tmp));
            const std::chrono::duration<double>  elapsed = std::chrono::system_clock::now() - start;
            DBG_LOG("scaling preview=", i, " secs=", elapsed.count());
	g_log(Exiv2GdkPxBufLdr::G_DOMAIN, G_LOG_LEVEL_INFO, "  scaled in %f", elapsed.count());
    }

    if (env.rotate() && 
        (d = exif_.findKey(Exiv2::ExifKey("Exif.Image.Orientation")) ) != exif_.end()) 
    {
        long  orientation =
#if EXIV2_VERSION >= EXIV2_MAKE_VERSION(0,28,0)
	    d->toInt64();
#else
	    d->toLong();
#endif
        switch (orientation) {
            case 3:  orientation = 180;  break;
            case 6:  orientation =  90;  break;
            case 8:  orientation = -90;  break;

            case 1:
            case 2: // flip horz
            case 4: // flip vert
            case 5: // transpose
            case 7: // traverse
            default:
                 orientation = 0;
        }

        if (orientation != 0) {
            DBG_LOG("rotating=", orientation);
	    if (!magick.isValid()) {
		magick.read(Magick::Blob(preview.pData(), preview.size()) );
	    }
            magick.rotate(orientation);
        }
    }

    if (env.convertSRGB()) 
    {
	DBG_LOG("request to convert to SRGB");

	/* to convert, we need to have the current ICC and target ICC
	 * if the img doesn't have an embedded ICC, we can guess and assign
	 * an src ICC and then our target sRGB ICC
	 *
	 *    Magick::Image  magick("adobe.jpg");
         *    magick.renderingIntent(Magick::PerceptualIntent);
	 *    ... no profile
         *    magick.profile("ICC", Magick::Blob(aRGB, aRGBsz));
         *    const Magick::Blob  targetICC(sRGB, sRGBsz);
         *    magick.profile("ICC", targetICC);
         *    magick.iccColorProfile(targetICC);
         *
         * exiftool -icc_profile -b -w icc file.{jpg,nef,tif}
	 */
        if (!magick.isValid()) {
            magick.read(Magick::Blob(preview.pData(), preview.size()) );
        }

	bool  convert = false;

        const Magick::Blob  prevwicc = magick.iccColorProfile();
	if (prevwicc.length() == 0)
	{
	    // no ICC, try to guess what it is...

	    if ((d = exif_.findKey(Exiv2::ExifKey("Exif.Image.InterColorProfile")) ) != exif_.end()) 
            {
		DBG_LOG("found embedded profile, len=", d->size(), "  known sRGB profile sizes=", _profiles.srgb().size, " ", _profiles.NKsRGB().size);
                Exiv2GdkPxBufLdr::Buf  hbuf;

                /* check that it's not already srgb/nk srgb
                 */
                bool  already = false;
                const unsigned char*  srgb = NULL;
                if ( (d->size() == _profiles.srgb().size   && (srgb = _profiles.srgb().data) ) ||
                     (d->size() == _profiles.NKsRGB().size && (srgb = _profiles.NKsRGB().data)) )
                {
                    hbuf.alloc(d->size());
                    d->copy(hbuf.buf(), Exiv2::invalidByteOrder);
                    already = memcmp(hbuf.buf(), srgb, d->size()) == 0;

                    DBG_LOG("  match to SRGB=", already);
                }

                if (!already)
                {
                    if (hbuf.sz() < d->size()) {
                        hbuf.alloc(d->size());
                    }
                    DBG_LOG("setting src profile to embedded (non SRGB)");
                    
                    d->copy(hbuf.buf(), Exiv2::invalidByteOrder);
                    magick.profile("ICC", Magick::Blob(hbuf.buf(), d->size()));
                    convert = true;
                }
	    }
	    else
	    {
                struct _ColrSpc {
                    const char*  key;
                    bool  ckunmod;
                };
                static const std::array colrSpcs {
                    _ColrSpc{ "Exif.Nikon3.ColorSpace", true  },
                    _ColrSpc{ "Exif.Canon.ColorSpace",  false }
                };

		/* no embedded ICC so can't do any conversion
		 * image if its not a Nikon RAW with the colr space set
		 */

		std::any_of(colrSpcs.begin(), colrSpcs.end(), [&convert, &d, &exif_, &magick, this](const auto& cs_)
		{
                    if ( (d = exif_.findKey(Exiv2::ExifKey(cs_.key)) ) == exif_.end()) {
			return false;
                    }
                    const long  l = 
#if EXIV2_VERSION >= EXIV2_MAKE_VERSION(0,28,0)
			d->toInt64();
#else
			d->toLong();
#endif

		    DBG_LOG("found color space ", cs_.key, " val=", l);

                    /* check if this is as-shot with no further mods (ie 
                     * colorspace conv)
                     * Nikon CNX2 the only thing that can edit the RAW file???
		     */
		    if (l == 2)
		    {
                        bool  doit = true;

                        if (cs_.ckunmod) {
                            // it was shot as aRGB - check the orig vs mod times
                            std::string  orig;
                            std::string  mod;

                            Exiv2::ExifData::const_iterator  e;
                            if ( (e = exif_.findKey(Exiv2::ExifKey("Exif.Image.DateTime")) ) != exif_.end()) {
                                mod = e->toString();
                            }
                            if ( (e = exif_.findKey(Exiv2::ExifKey("Exif.Photo.DateTimeOriginal")) ) != exif_.end()) {
                                orig = e->toString();
                            }

                            // ok, we'll assume this is really still in aRGB
                            doit = (orig == mod);
                            DBG_LOG("   color space is ARGB");
                        }

                        if (doit) {
                            convert = true;
                            magick.profile("ICC", _argbICC);
                        }
		    }
                    return true;
                });
	    }
	}
        else
        {
            const size_t  len = prevwicc.length();
            DBG_LOG("found preview image w/ICC profile, len=", len, "  known sRGB profile sizes=", _profiles.srgb().size, " ", _profiles.NKsRGB().size);

            convert = true;
            const unsigned char*  srgb = NULL;
	    if ( (len == _profiles.srgb().size   && (srgb = _profiles.srgb().data) ) ||
		 (len == _profiles.NKsRGB().size && (srgb = _profiles.NKsRGB().data)) )
            {
                convert = ! (memcmp(prevwicc.data(), srgb, len) == 0);
            }
        }

	if (convert) {
	    // finally, try to convert to srgb
	    try
	    {
		g_log(Exiv2GdkPxBufLdr::G_DOMAIN, G_LOG_LEVEL_INFO, "  converting to sRGB");
		magick.profile("ICC", _srgbICC);
		magick.iccColorProfile(_srgbICC);
	    }
	    catch (const std::exception& ex)
	    {
		g_printerr("failed to convert to SRGB - %s\n", ex.what());
	    }
	}
    }


    // annotate
    {
	static const std::array  etags {
	    Exiv2::ExifKey("Exif.Image.Model"),

	    Exiv2::ExifKey("Exif.Image.DateTime"),

	    Exiv2::ExifKey("Exif.Photo.FocalLength"),
	    Exiv2::ExifKey("Exif.Photo.ExposureTime"),
	    Exiv2::ExifKey("Exif.Photo.FNumber"),
	    Exiv2::ExifKey("Exif.Photo.ISOSpeedRatings")
	};

	if (!magick.isValid()) {
            DBG_LOG("loading from for annotating");
	    magick.read( Magick::Blob(preview.pData(), preview.size()) );
	}

	std::ostringstream  exif;
	std::for_each(etags.begin(), etags.end(), [&exif, &exif_, &width_, &height_](const auto& etag) {
	    Exiv2::ExifData::const_iterator  e = exif_.findKey(etag);
	    if (e == exif_.end()) {
		exif << "[N/F:" << etag.key() << "]";
		return;
	    }

	    exif << *e;
	    {
		if (etag.key() == "Exif.Image.Model")
		{
		    exif << "  " << width_ << "x" << height_ << "\n";
		    Exiv2::ExifData::const_iterator  ln = lensName(exif_);
		    if (ln != exif_.end()) { 
			exif << ln->print(&exif_) << "\n";
		    }
		}
		else if (etag.key() == "Exif.Image.DateTime")
		{
		    exif << '\n';
		}
		else if (etag.key() == "Exif.Photo.ISOSpeedRatings")
		{
		    exif << "ISO";
		}
		else {
		    exif << "  ";
		}
	    }
	});

	std::ostringstream transcolour;
	transcolour << "graya(" << env.transparency() << "%)";
	Magick::Image  info(magick.size(), transcolour.str().c_str());
        info.borderColor(transcolour.str().c_str());
	if (env.font().length() > 0) {
	    info.font(env.font());
	}
	info.fillColor("black");
	info.strokeColor("none");
        info.fontPointsize(env.fontsize());
	try
	{
	    info.annotate(exif.str(), Magick::Geometry("+10+10"), Magick::WestGravity);
	}
	catch (const Magick::ErrorType& ex)
	{
	    g_log(Exiv2GdkPxBufLdr::G_DOMAIN, G_LOG_LEVEL_WARNING, "unable to use specified font, attempting with default - %s", ex.what());
	    // font releated exception...
	    info.font("");
	    info.annotate(exif.str(), Magick::Geometry("+10+10"), Magick::WestGravity);
	}
        info.trim();
        info.border();
#if MagickLibInterface == 5  // IM 6.x
        info.opacity(65535/3.0);
        info.transparent("grey");
#elif MagickLibInterface == 6  // IM 7
        info.opaque(Magick::Color("none"), Magick::Color("grey"));
        info.backgroundColor(Magick::Color("grey"));
#endif

	if (info.columns() > magick.columns()-10) {
	    std::ostringstream  os;
            os << magick.columns()-10 << "x";
            info.resize(os.str());
        }

	magick.composite(info,
		         Magick::Geometry(info.columns(), info.rows(), 10, magick.rows()-info.rows()-10),
			 Magick::OverlayCompositeOp);
    }

    if (magick.isValid())  imgbuf_.finalize(magick);
    else                   imgbuf_.finalize(preview);

    return imgbuf_;
}

}
