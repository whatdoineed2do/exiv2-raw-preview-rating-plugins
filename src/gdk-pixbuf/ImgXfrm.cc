#include "ImgXfrm.h"

#include <array>
#include <chrono>
#include <functional>
#include <sstream>
#include <algorithm>
#include <string>

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
    using ExifTransformer = std::function<void(const Exiv2::ExifData::const_iterator&, const Exiv2::ExifData&, std::ostringstream&)>;

    struct ExifTagConfig {
	Exiv2::ExifKey key;
	ExifTransformer transform = nullptr;
    };
    static const std::array  etags {
	ExifTagConfig {
	    Exiv2::ExifKey("Exif.Image.Model"),
	    [this](const auto& e, const auto& data, auto& os) {
		os << *e;
		auto ln = lensName(data);
		if (ln != data.end()) {
		    os << "  " << ln->print(&data);
		}
		os << '\n' << width << "x" << height << "\n";
	    }
	},
	ExifTagConfig { 
	    Exiv2::ExifKey("Exif.Photo.DateTimeOriginal"), 
	    [](const auto& e, const auto& data, auto& os) {
		std::string dateStr = e->toString();

		// 1. Reformat colons to dashes
		if (dateStr.length() >= 10) {
		    if (dateStr[4] == ':') dateStr[4] = '-';
		    if (dateStr[7] == ':') dateStr[7] = '-';
		}

		std::string offsetStr = "";

		// Fallback 1: Standard EXIF 2.3+ (or sidecar files)
		if (auto tz = data.findKey(Exiv2::ExifKey("Exif.Photo.OffsetTimeOriginal")); tz != data.end()) {
		    offsetStr = tz->toString();
		}
		// Fallback 2: Modern Nikon World Time (D800, Z series)
		else if (auto ntz = data.findKey(Exiv2::ExifKey("Exif.NikonWt.Timezone")); ntz != data.end()) {
		    offsetStr = ntz->toString();
		}
		// Fallback 3: Older Nikon Binary World Time structure (D300, D700, D3)
		else if (auto nwt = data.findKey(Exiv2::ExifKey("Exif.Nikon3.WorldTime")); nwt != data.end() && nwt->count() >= 4) {
		    int16_t total_minutes = static_cast<int16_t>(nwt->value().toInt64(0));
		    int dst_flag = static_cast<int>(nwt->value().toInt64(2));

		    if (dst_flag == 1) {
			total_minutes += 60;
		    }

		    if (total_minutes != 0) {
			int hours = std::abs(total_minutes) / 60;
			int mins = std::abs(total_minutes) % 60;
			char sign = (total_minutes >= 0) ? '+' : '-';

			char buf[16];
			std::snprintf(buf, sizeof(buf), "GMT%c%02d:%02d", sign, hours, mins);
			offsetStr = buf;
		    }
		}

		// Clean up trailing spaces from string formats
		while (!offsetStr.empty() && std::isspace(offsetStr.back())) {
		    offsetStr.pop_back();
		}

		// Clean up leading spaces just in case (e.g., " 0" -> "0")
		while (!offsetStr.empty() && std::isspace(offsetStr.front())) {
		    offsetStr.erase(0, 1);
		}

		// 2. Clear out ANY explicit zero/GMT markers (including a raw "0")
		if (offsetStr == "0" || offsetStr == "+00:00" || offsetStr == "-00:00" || offsetStr == "00:00" || offsetStr == "Z") {
		    offsetStr = "";
		}

		// Only append to output if there is a non-zero holiday/local offset
		if (!offsetStr.empty()) {
		    dateStr += " (" + offsetStr + ")";
		}

		os << dateStr << '\n';
	    }
	},
	ExifTagConfig {
	    Exiv2::ExifKey("Exif.Photo.FocalLength"),
	    [](const auto& e, const auto& data, auto& os) {
		Exiv2::Rational rational = e->toRational();

		if (rational.second != 0) {
		    if (rational.first % rational.second == 0) {
			os << (rational.first / rational.second) << "mm  ";
		    }
		    else {
			float value = static_cast<float>(rational.first) / rational.second;
			os << value << "mm  ";
		    }
		}
	    }
	},
	ExifTagConfig {
	    Exiv2::ExifKey("Exif.Photo.ExposureTime"),
	    [](const auto& e, const auto& data, auto& os) {
		std::string exp = e->print(&data); // e.g., "1/500 s" or "2 s"

		// Remove the " s" suffix if present
		if (exp.size() >= 2 && exp.compare(exp.size() - 2, 2, " s") == 0) {
		    exp.erase(exp.size() - 2);
		}
		// Handle single-character "s" just in case of weird formatting (e.g., "2s")
		else if (!exp.empty() && exp.back() == 's') {
		    exp.pop_back();
		}

		// Clean up any trailing whitespace left over (like from "2 s" -> "2 ")
		while (!exp.empty() && std::isspace(exp.back())) {
		    exp.pop_back();
		}

		os << exp << "  ";
	    }
	},
	ExifTagConfig {
	    Exiv2::ExifKey("Exif.Photo.FNumber"),
	    [](const auto& e, const auto& data, auto& os) {
		std::string fnum = e->print(&data);
		if (!fnum.empty() && fnum[0] == 'F') {
		    os << "f/" << fnum.substr(1) << "  ";
		} else {
		    os << fnum << "  ";
		}
	    }
	},
	ExifTagConfig {
	    Exiv2::ExifKey("Exif.Photo.ISOSpeedRatings"),
	    [](const auto& e, const auto&, auto& os) { os << "ISO" << *e; }
	}
    };

    std::ostringstream  exif;
    std::for_each(etags.begin(), etags.end(), [&exif, this](const ExifTagConfig& tag) {
	auto e = exifdata.findKey(tag.key);

	if (e == exifdata.end()) {
	    DBG_LOG("Exif key: ", tag.key.key(), " value: '<n/a>'");
	    exif << "[N/F:" << tag.key.key() << "]";
	    return;
	}

	DBG_LOG("Exif key: ", tag.key.key(), " value: '", e->toString(), "'");

	if (tag.transform) {
	    tag.transform(e, exifdata, exif);
	} else {
	    exif << *e << "  ";
	}
    });

    std::ostringstream transcolour;
    transcolour << "graya(" << env.transparency() << "%)";

    const auto exifstr = exif.str();
 
    auto imagefactory = [&]() {
        auto g = magick.size();

        if (env.fontpcnt()) {
            g.height( g.height() * env.fontpcnt()/100.0 );
            Magick::Image info(g, "none"); // Initialize as pure transparent instead of graya

            const auto lines = std::count(exifstr.begin(), exifstr.end(), '\n') + 1;
            info.fontPointsize( ceil( ((info.rows()/lines ) * 0.75)) );

            return info;
        }
        else {
            Magick::Image info(g, "none"); // Initialize as pure transparent
            info.fontPointsize(env.fontsize());
            return info;
        }
    };

    Magick::Image info = imagefactory();

    if (env.font().length() > 0) {
        info.font(env.font());
    }

    // Use your opacity variable dynamically (convert 0-100% config to 0.0-1.0 float)
    double box_opacity = env.transparency() / 100.0; 
    if (box_opacity <= 0.0 || box_opacity > 1.0) box_opacity = 0.6; // Fallback default

    try
    {
        // Render text directly onto the large transparent canvas first
        info.fillColor("white");
        info.strokeColor("none");
        info.annotate(exifstr, Magick::Geometry("+0+0"), Magick::WestGravity);

        // Trim tightly to the text boundaries
        info.trim();

        // Calculate and apply dynamic transparent padding (5% of text width)
        size_t pad_x = static_cast<size_t>(info.columns() * 0.05);
        size_t pad_y = static_cast<size_t>(info.columns() * 0.05 * 0.8);

        info.borderColor("none");
        info.border(Magick::Geometry(pad_x, pad_y));

        // Generate the background box layer to match the padded text block
        Magick::Image bg(Magick::Geometry(info.columns(), info.rows()), "none");

        std::vector<Magick::Drawable> draw_list;
        draw_list.push_back(Magick::DrawableFillColor("grey20"));
        draw_list.push_back(Magick::DrawableStrokeColor("none"));
        draw_list.push_back(Magick::DrawableRoundRectangle(
            0, 0,
            bg.columns() - 1, bg.rows() - 1,
            12, 12
        ));
        bg.draw(draw_list);

        // Apply the transparency ONLY to the background box layer first
        bg.evaluate(Magick::AlphaChannel, Magick::MultiplyEvaluateOperator, box_opacity);

        // Composite the 100% solid white text layer ON TOP of the semi-transparent box
        bg.composite(info, 0, 0, Magick::OverCompositeOp);

        // Replace info with our finalized badge layer
        info = bg;
    }
    catch (const Magick::ErrorType& ex)
    {
        g_log(Exiv2GdkPxBufLdr::G_DOMAIN, G_LOG_LEVEL_WARNING, "unable to use specified font, attempting with default - %s", ex.what());
        info.font("");
        info.annotate(exifstr, Magick::Geometry("+10+10"), Magick::WestGravity);
        info.trim();
    }

    const double  xOffsetRatio = env.xOffset()/100.0;
    const double  yOffsetRatio = env.yOffset()/100.0;

    // Compute pixel layout offsets based on target canvas resolution
    const size_t  xOffset = static_cast<size_t>(magick.columns() * xOffsetRatio);
    const size_t  yOffset = static_cast<size_t>(magick.rows() * yOffsetRatio);

    if (info.columns() > magick.columns() - (xOffset * 2)) {
        std::ostringstream os;
        os << (magick.columns() - (xOffset * 2)) << "x";
        info.resize(os.str());
    }

    // Calculate vertical offset relative to the bottom edge of the photo frame (SouthWest style placement)
    const size_t  compositeY = magick.rows() - info.rows() - yOffset;

    // Drop the badge overlay onto the main photo matrix dynamically using the calculated geometry rules
    magick.composite(info,
                     Magick::Geometry(info.columns(), info.rows(), xOffset, compositeY),
                     Magick::OverCompositeOp);
}
}
