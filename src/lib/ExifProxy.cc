#include "ExifProxy.h"

#include <glib.h>
#include <array>
#include <algorithm>

#include "DbgHlpr.h"


const std::string  ExifProxy::_XMPKEY { "Xmp.xmp.Rating" };

std::ostream&  operator<<(std::ostream& os_, const Exiv2::XmpData& data_) 
{
    std::for_each(data_.begin(), data_.end(), [&os_](const auto& e) {
        os_ << "{ " << e.key() << " " << e.typeName() << " " << e.count() << " " << e.value() << " }";
    });

    return os_;
}

std::ostream& operator<<(std::ostream& os_, const ExifProxy::Rating& r_)
{
    return os_ << (int)r_;
}

ExifProxy::Rating& operator<<(ExifProxy::Rating& r_, Exiv2::XmpData::iterator& v_)
{
    r_ = static_cast<ExifProxy::Rating>(
#if EXIV2_VERSION >= EXIV2_MAKE_VERSION(0,28,0) 
	v_->toInt64()
#else
	v_->toLong()
#endif
	);
    return r_;
}

ExifProxy::ExifProxy(const char* logdomain_) : _logdomain(logdomain_)
{
#ifdef EOG_PLUGIN_XMP_INIT_LOCK
    // not thread safe!!!!  need to initialize XMPtoolkit
    Exiv2::XmpParser::initialize(_XmpLock::lockUnlock, &_xmplock);
#endif
#if EXIV2_VERSION >= EXIV2_MAKE_VERSION(0,27,4)
    Exiv2::enableBMFF();
#endif

    _clear();
}

ExifProxy&  ExifProxy::ref(const char* fpath_)
{
    syncRating();

    if ( fpath_ == NULL) {
	_clear();
	return *this;
    }

    if (strcmp(fpath_, _file.c_str()) == 0) {
	return *this;
    }

    _clear();
    struct stat  st = { 0 };
    if (stat(fpath_, &st) < 0) {
	return *this;
    }

    _mtime = st.st_mtime;

    _file = fpath_;
    try
    {
	_img = Exiv2::ImageFactory::open(fpath_);
	_img->readMetadata();

	_writable = access(fpath_, W_OK) == 0;

	Exiv2::XmpData&  xmpData = _img->xmpData();
	Exiv2::XmpData::iterator  kpos = xmpData.findKey(Exiv2::XmpKey(ExifProxy::_XMPKEY));
	if (kpos == xmpData.end()) {
	    _rating = Rating::UNSET;
	}
	else {
	    _xmp = &xmpData;
	    _rating << kpos;
	}
        DBG_LOG("new file ", fpath_, " rating=", _rating);
    }
#if EXIV2_VERSION >= EXIV2_MAKE_VERSION(0,28,0) 
    catch(Exiv2::Error & e) {
	switch (e.code())
	{
	    case Exiv2::ErrorCode::kerDataSourceOpenFailed:
	    case Exiv2::ErrorCode::kerFileContainsUnknownImageType:
	    case Exiv2::ErrorCode::kerUnsupportedImageType:
	        break;
#else
    catch(Exiv2::AnyError & e) {
	switch (e.code())
	{
	    case Exiv2::kerDataSourceOpenFailed:
	    case Exiv2::kerFileContainsUnknownImageType:
	    case Exiv2::kerUnsupportedImageType:
	        break;
#endif

	    default:
	        g_log(_logdomain.c_str(), G_LOG_LEVEL_WARNING, "%s: failed to set XMP rating - (%d) %s", fpath_, (int)e.code(), e.what());
	}
    }

    return *this;
}


const char*  ExifProxy::rating()
{
    if (_img.get() == 0) {
        return "<no image>";
    }

    if (!_writable) {
        return "<no perm>";
    }

    static const std::string  DEFLT_RATING = "-----";
    _ratingBuf = DEFLT_RATING;

    switch (_rating)
    {
        case Rating::ZERO:
	case Rating::ONE:
	case Rating::TWO:
	case Rating::THREE:
	case Rating::FOUR:
	case Rating::FIVE:
	{
	    short N = (short)_rating;
	    if (N < 0) {
	    }
	    else
	    {
		if (N > 5) {
		    N = 5;
		}
		char*  n = ((char*)_ratingBuf.c_str());
		for (int i=0; i<N; i++) {
		    n[i] = '*';
		}
	    }
	} break;

	default:
	    return "-/-";
    }

    return _ratingBuf.c_str();
}

bool  ExifProxy::_updateRating(const ExifProxy::Rating rating_)
{
    if (!_writable) {
	g_log(_logdomain.c_str(), G_LOG_LEVEL_WARNING, "%s: not writable", _file.c_str());
	return false;
    }

    try
    {
	ExifProxy::History::iterator  h =
	    std::find_if( _history.begin(), _history.end(), [this](const auto& e){
		return e.f == _file;
	    });

	if (h == _history.end()) {
	    // lazy init...
	    DBG_LOG("update rating", _file, " from ", _rating, " to ", rating_, "  FIRST TIME");
	    _history.emplace_back(ExifProxy::HistoryEvnt(_file, _img->exifData(), _rating, rating_));
	}
	else {
	    DBG_LOG("update rating", _file, " from ", _rating, " to ", rating_);
	    h->rating = rating_;
	}
	_rating = rating_;
    }
    catch (const std::exception& ex)
    {
	g_log(_logdomain.c_str(), G_LOG_LEVEL_WARNING, "%s: failed to update XMP rating - %s", _file.c_str(), ex.what());
	return false;
    }
    return true;
}


bool  ExifProxy::syncRating()
{
    if (_file.empty()) {
        return true;
    }

    if (_img.get() == 0) {
	// something went wrong earlier/no img ... do nothing
	DBG_LOG("sync: no image to sync on ", _file);
        return false;
    }

    if (!_writable) {
	DBG_LOG("sync: not writable on ", _file);
        return true;
    }


    ExifProxy::History::const_iterator  h =
	std::find_if( _history.cbegin(), _history.cend(), [this](const auto& e){
	    return e.f == _file;
	});
    if (h == _history.cend()) {
	DBG_LOG("sync: no rating on ", _file);
        return true;
    }
    if (!h->changed()) {
	DBG_LOG("sync: no updates on ", _file);
        return true;
    }
    DBG_LOG("sync: syncing on ", _file);

    try
    {
	switch (_rating)
	{
	    case Rating::UNSET:
	    case Rating::ZERO:
	    case Rating::MAX:
	    {
		Exiv2::XmpData&  xmpData = _img->xmpData();
		Exiv2::XmpData::iterator  kpos = xmpData.findKey(Exiv2::XmpKey(ExifProxy::_XMPKEY));
		if (kpos != xmpData.end()) {
		    xmpData.erase(kpos);
		}
	    } break;

	    default:
	    {
		char  rating[2];
		snprintf(rating, sizeof(rating), "%u", static_cast<int>(_rating));

		_img->xmpData()[ ExifProxy::_XMPKEY ] = Exiv2::XmpTextValue(rating);
	    }
	}
	_img->writeMetadata();

	struct utimbuf  tmput;
	memset(&tmput, 0, sizeof(tmput));
	tmput.modtime = _mtime;
	utime(_file.c_str(), &tmput);
    }
    catch (const std::exception& ex)
    {
	g_log(_logdomain.c_str(), G_LOG_LEVEL_WARNING, "%s: failed to update XMP rating - %s", _file.c_str(), ex.what());
        return false;
    }
    return true;
}

/* mark the image if its not already rated
*/
bool  ExifProxy::fliprating()
{
    bool  flipped = false;

    if ((flipped = _updateRating(_rating == Rating::UNSET ? Rating::FIVE : Rating::UNSET)) ) {
         flipped = syncRating();
    }
    return flipped;
}


bool ExifProxy::cycleRating()
{
    if (!_writable) {
        return false;
    }

    ExifProxy::Rating  rating;
    if (_rating == Rating::UNSET) {
        rating = Rating::ONE;
    }
    else {
	int  r = static_cast<int>(_rating);
	r = (r + 1) % static_cast<int>(Rating::MAX) + static_cast<int>(Rating::ZERO);
	rating = static_cast<Rating>(r);
    }

    _updateRating(rating);
    return true;
}


bool  ExifProxy::unsetRating()
{
    if (!_writable) {
	return false;
    }
    _updateRating(Rating::UNSET);
    return true;
}


void  ExifProxy::_clear()
{
    _xmp = NULL;
    _file.clear();
    _writable = false;
    _img.reset();
    _mtime = 0;
    _ratingBuf.clear();
    _rating = Rating::UNSET;
}


std::ostream&  operator<<(std::ostream& os_, const ExifProxy& obj_)
{
    os_ << obj_._file;
    if (obj_._xmp == NULL) {
        return os_ << " [ <nil> ]";
    }
    return os_ << " [ " << *obj_._xmp << " ]";
}

std::ostream&  operator<<(std::ostream& os_, const ExifProxy::HistoryEvnt& obj_)
{
    struct _ETag {
        const Exiv2::ExifKey  tag;
        const char*  prfx;

        _ETag(const Exiv2::ExifKey&  tag_, const char* prfx_) : tag(tag_), prfx(prfx_) { }
    };
    static const std::array  etags {
        _ETag(Exiv2::ExifKey("Exif.Image.Model"), ""),
        _ETag(Exiv2::ExifKey("Exif.Photo.DateTimeOriginal"), ""),
        _ETag(Exiv2::ExifKey("Exif.Photo.ExposureTime"), ""),
        _ETag(Exiv2::ExifKey("Exif.Photo.FNumber"), ""),
        _ETag(Exiv2::ExifKey("Exif.Photo.ISOSpeedRatings"), "ISO")
    };

    os_ << (obj_.rating == ExifProxy::Rating::UNSET ? "---" : "+++") << ' ' << obj_.rating << ' ' << obj_.f << " : { ";

    std::for_each(etags.begin(), etags.end(), [&os_, &obj_](const auto& t_) {
        Exiv2::ExifData::const_iterator  e = obj_.exif.findKey(t_.tag);
        if (e == obj_.exif.end()) {
            return;
        }

        os_ << " " << t_.prfx << *e;
    });
    os_ << " }";

    return os_;
}
