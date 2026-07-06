#include "ImgFactory.h"

#include <omp.h>

#include <list>
#include <iterator>
#include <algorithm>
#include <functional>
#include <thread>
#include <jpeglib.h>

#include <glib.h>

#include "Buf.h"
#include "ImgXfrm.h"
#include "Env.h"


namespace  Exiv2GdkPxBufLdr
{
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
    g_log(Exiv2GdkPxBufLdr::G_DOMAIN, G_LOG_LEVEL_DEBUG, "OMP: cpus=%d threads=%d h/w concurrency=%d setting resource limits to threads", omp_get_num_procs(), omp_get_max_threads(), std::thread::hardware_concurrency());
#if defined(WITH_SIMD) || defined(LIBJPEG_TURBO_VERSION)
    g_log(Exiv2GdkPxBufLdr::G_DOMAIN, G_LOG_LEVEL_DEBUG, "JPEG Decoder: Hardware Optimized (libjpeg-turbo SIMD, %d)", LIBJPEG_TURBO_VERSION_NUMBER);
#else
    g_log(Exiv2GdkPxBufLdr::G_DOMAIN, G_LOG_LEVEL_DEBUG, "JPEG Decoder: WARNING - Legacy Scalar Software Fallback Active (Slow)");
#endif

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
;


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

    unsigned  i = 0;
    auto  pp = PREVIEW_LIMIT == 0 ? list.end() : std::find_if(list.begin(), list.end(), [i = 0, PREVIEW_LIMIT](const auto& preview) mutable {
	g_log(Exiv2GdkPxBufLdr::G_DOMAIN, G_LOG_LEVEL_INFO, "  preview #%d width=%ld height=%ld", i, preview.width_, preview.height_);

	if (preview.width_ >= PREVIEW_LIMIT || preview.height_ >= PREVIEW_LIMIT) {
	    DBG_LOG("preview #", i, "  SELECTED");
	    return true;
	}

	++i;
	return false;
    });

    /* nothing matched and the list isn't empty, default to the last element
     */
    if (pp == list.end() && !list.empty()) {
	pp = std::prev(list.end());
    }

    Exiv2::PreviewImage  preview =  exvprldr_.getPreviewImage(*pp);
    mimeType_ = preview.mimeType();
    g_log(Exiv2GdkPxBufLdr::G_DOMAIN, G_LOG_LEVEL_INFO, "  using preview #%d width=%ld height=%ld mime-type=%s", i, pp->width_, pp->height_, mimeType_.c_str());

    Magick::Image  magick;

    const Exiv2::ExifData  exif = orig->exifData();

    const ImgXfrmResize  xResize{preview, env, magick, i};
    const ImgXfrmRotate  xRotate{preview, env, magick, exif};
    const ImgXfrmsRGB    xsRGB{preview, env, magick, exif, _profiles, _srgbICC, _argbICC};
    const ImgXfrmAnnotate  xAnnotate{preview, env, magick, exif, orig->pixelHeight(), orig->pixelWidth()};

    const std::list<std::reference_wrapper<const ImgXfrm>>  xfrms = {
	xResize, xRotate, xsRGB, xAnnotate
    };
    std::for_each(xfrms.begin(), xfrms.end(), [](auto& x) { x.get().transform(); });

    if (magick.isValid())  imgbuf_.finalize(magick);
    else                   imgbuf_.finalize(preview);

    return imgbuf_;
}

}
