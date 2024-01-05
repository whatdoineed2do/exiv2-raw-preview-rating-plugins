#include "ImgXfrm.h"

#include <array>
#include <chrono>

#include "Buf.h"
#include "DbgHlpr.h"
#include "ColorProfiles.h"
#include "Env.h"

namespace  Exiv2GdkPxBufLdr
{

bool  ImgXfrmResize::_valid() const
{
    const unsigned short  PREVIEW_LIMIT = env.previewScaleLimit();
    return (preview.width() > PREVIEW_LIMIT || preview.height() > PREVIEW_LIMIT);
}

void  ImgXfrmResize::_transform() const
{
    const unsigned short  PREVIEW_LIMIT = env.previewScaleLimit();

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
    g_log(Exiv2GdkPxBufLdr::G_DOMAIN, G_LOG_LEVEL_INFO, "  scaling preview #%d to %s", previewIdx, tmp);
	const std::chrono::time_point<std::chrono::system_clock>  start = std::chrono::system_clock::now();
    magick.resize(Magick::Geometry(tmp));
	const std::chrono::duration<double>  elapsed = std::chrono::system_clock::now() - start;
	DBG_LOG("scaling preview=", previewIdx, " secs=", elapsed.count());
    g_log(Exiv2GdkPxBufLdr::G_DOMAIN, G_LOG_LEVEL_INFO, "    scaled in %f", elapsed.count());
}

bool  ImgXfrmRotate::_valid() const
{
    return env.rotate() && d != exif.end();
}

void  ImgXfrmRotate::_transform() const
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

bool  ImgXfrmsRGB::_valid() const
{
    return env.convertSRGB();
}

void  ImgXfrmsRGB::_transform() const
{
    DBG_LOG("request to convert to SRGB");
    Exiv2::ExifData::const_iterator  d;

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

	if ((d = exif.findKey(Exiv2::ExifKey("Exif.Image.InterColorProfile")) ) != exif.end()) 
	{
	    DBG_LOG("found embedded profile, len=", d->size(), "  known sRGB profile sizes { sRGB_IEC61966_2_1=", profiles.srgb().size, " Nikon=", profiles.NKsRGB().size, " }");
	    Exiv2GdkPxBufLdr::Buf  hbuf;

	    /* check that it's not already srgb/nk srgb
	     */
	    bool  already = false;
	    const unsigned char*  srgb = NULL;
	    if ( (d->size() == profiles.srgb().size   && (srgb = profiles.srgb().data) ) ||
		 (d->size() == profiles.NKsRGB().size && (srgb = profiles.NKsRGB().data)) )
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

	    std::any_of(colrSpcs.begin(), colrSpcs.end(), [&convert, &d, this](const auto& cs_)
	    {
		if ( (d = exif.findKey(Exiv2::ExifKey(cs_.key)) ) == exif.end()) {
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
			if ( (e = exif.findKey(Exiv2::ExifKey("Exif.Image.DateTime")) ) != exif.end()) {
			    mod = e->toString();
			}
			if ( (e = exif.findKey(Exiv2::ExifKey("Exif.Photo.DateTimeOriginal")) ) != exif.end()) {
			    orig = e->toString();
			}

			// ok, we'll assume this is really still in aRGB
			doit = (orig == mod);
			DBG_LOG("   color space is ARGB");
		    }

		    if (doit) {
			convert = true;
			magick.profile("ICC", argbICC);
		    }
		}
		return true;
	    });
	}
    }
    else
    {
	const size_t  len = prevwicc.length();
	DBG_LOG("found preview image w/ICC profile, len=", len, "  known sRGB profile sizes=", profiles.srgb().size, " ", profiles.NKsRGB().size);

	convert = true;
	const unsigned char*  srgb = NULL;
	if ( (len == profiles.srgb().size   && (srgb = profiles.srgb().data) ) ||
	     (len == profiles.NKsRGB().size && (srgb = profiles.NKsRGB().data)) )
	{
	    convert = ! (memcmp(prevwicc.data(), srgb, len) == 0);
	}
    }

    if (convert) {
	// finally, try to convert to srgb
	try
	{
	    g_log(Exiv2GdkPxBufLdr::G_DOMAIN, G_LOG_LEVEL_INFO, "  converting to sRGB");
	    magick.profile("ICC", srgbICC);
	    magick.iccColorProfile(srgbICC);
	}
	catch (const std::exception& ex)
	{
	    g_log(Exiv2GdkPxBufLdr::G_DOMAIN, G_LOG_LEVEL_WARNING, "failed to convert to SRGB - %s", ex.what());
	}
    }
}

void  ImgXfrmAnnotate::_transform() const
{
    static const std::array  etags {
	Exiv2::ExifKey("Exif.Image.Model"),

	Exiv2::ExifKey("Exif.Photo.DateTimeOriginal"),

	Exiv2::ExifKey("Exif.Photo.FocalLength"),
	Exiv2::ExifKey("Exif.Photo.ExposureTime"),
	Exiv2::ExifKey("Exif.Photo.FNumber"),
	Exiv2::ExifKey("Exif.Photo.ISOSpeedRatings")
    };

    std::ostringstream  exif;
    std::for_each(etags.begin(), etags.end(), [&exif, this](const auto& etag) {
	Exiv2::ExifData::const_iterator  e = exifdata.findKey(etag);
	DBG_LOG("Exif key: ", etag.key(), "value: '",
	       [&e, this](){
	           std::ostringstream dump;
		   if (e==exifdata.end()) {
		       return std::string("<n/a>");
		   }
		   dump << *e;
		   return dump.str();
	       }(), "'");
	if (e == exifdata.end()) {
	    exif << "[N/F:" << etag.key() << "]";
	    return;
	}

	exif << *e;
	{
	    if (etag.key() == "Exif.Image.Model")
	    {
		exif << "  " << width << "x" << height << "\n";
		Exiv2::ExifData::const_iterator  ln = lensName(exifdata);
		if (ln != exifdata.end()) { 
		    exif << ln->print(&exifdata) << "\n";
		}
	    }
	    else if (etag.key() == "Exif.Photo.DateTimeOriginal")
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
}
