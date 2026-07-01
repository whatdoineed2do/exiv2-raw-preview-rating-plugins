#include "ExifProxy.h"

#include <glib.h>
#include <array>
#include <algorithm>


const std::string  ExifProxy::_XMPKEY { "Xmp.xmp.Rating" };


std::ostream&  operator<<(std::ostream& os_, const Exiv2::XmpData& data_) 
{
    std::for_each(data_.begin(), data_.end(), [&os_](const auto& e) {
        os_ << "{ " << e.key() << " " << e.typeName() << " " << e.count() << " " << e.value() << " }";
    });

    return os_;
}

ExifProxy::ExifProxy(const char* logdomain_) : _logdomain(logdomain_), _mtime(0), _xmp(NULL), _xmpkpos(NULL)
{
#ifdef EOG_PLUGIN_XMP_INIT_LOCK
    // not thread safe!!!!  need to initialize XMPtoolkit
    Exiv2::XmpParser::initialize(_XmpLock::lockUnlock, &_xmplock);
#endif
#if EXIV2_VERSION >= EXIV2_MAKE_VERSION(0,27,4)
    Exiv2::enableBMFF();
#endif
}

ExifProxy&  ExifProxy::ref(const char* fpath_)
{
    if ( fpath_ == NULL) {
	_clear();
	return *this;
    }

    if (strcmp(fpath_, _file.c_str()) == 0) {
	return *this;
    }
    struct stat  st;
    memset(&st, 0, sizeof(st));
    if (stat(fpath_, &st) < 0) {
	_clear();
	return *this;
    }

    _clear();
    _mtime = st.st_mtime;

    _file = fpath_;
    try
    {
	_img = Exiv2::ImageFactory::open(fpath_);
	_img->readMetadata();
	//_xmpkpos = NULL;

	Exiv2::XmpData&  xmpData = _img->xmpData();
	Exiv2::XmpData::iterator  kpos = xmpData.findKey(Exiv2::XmpKey(ExifProxy::_XMPKEY));
	if (kpos != xmpData.end()) {
	    _xmp = &xmpData;
	    _xmpkpos = kpos;
	}
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
        return NULL;
    }

    static const std::string  DEFLT_RATING = "-----";
    _rating = DEFLT_RATING;

    if (rated())
    {
	// spec say -1..5
#if EXIV2_VERSION >= EXIV2_MAKE_VERSION(0,28,0)
	long  N = _xmpkpos->toInt64();
#else
	long  N = _xmpkpos->toLong();
#endif
	if (N < 0) {
	}
	else
	{
	    if (N > 5) {
		N = 5;
	    }
	    char*  n = ((char*)_rating.c_str());
	    for (int i=0; i<N; i++) {
		n[i] = '*';
	    }
	}
    }
    return _rating.c_str();
}

bool  ExifProxy::rate(unsigned short r_)
{
    return rate([r_] {
        switch (r_) {
	    case 1: return ExifProxy::Rating::ONE;
	    case 2: return ExifProxy::Rating::TWO;
	    case 3: return ExifProxy::Rating::THREE;
	    case 4: return ExifProxy::Rating::FOUR;
	    case 5: return ExifProxy::Rating::FIVE;
	    case 0:
	    default:
		return ExifProxy::Rating::UNSET;
	}
    }());
}

bool ExifProxy::rate(const ExifProxy::Rating r_)
{
    bool  flipped = false;
    if (_img.get() == 0) {
	// something went wrong earlier/no img ... do nothing
	return flipped;
    }

    const Exiv2::XmpTextValue  rating = [r_] {
        switch (r_) {
	    case ExifProxy::Rating::ONE:   return Exiv2::XmpTextValue("1");
	    case ExifProxy::Rating::TWO:   return Exiv2::XmpTextValue("2");
	    case ExifProxy::Rating::THREE: return Exiv2::XmpTextValue("3");
	    case ExifProxy::Rating::FOUR:  return Exiv2::XmpTextValue("4");
	    case ExifProxy::Rating::FIVE:  return Exiv2::XmpTextValue("5");

	    case ExifProxy::Rating::UNSET:
	    default:
		return Exiv2::XmpTextValue("0");
	}
    }();

    try
    {
	std::string  before;
	if (_xmp == NULL)
	{
	    // previously found no XMP data set
	    _xmp = &_img->xmpData();

	    if (r_ == ExifProxy::Rating::UNSET) {
		g_log(_logdomain.c_str(), G_LOG_LEVEL_DEBUG, "%s: requested rating  <> -> %d ignored",  _file.c_str(), static_cast<int>(r_));
	    }
	    else {
		g_log(_logdomain.c_str(), G_LOG_LEVEL_DEBUG, "%s: requested rating  <> -> %d",  _file.c_str(), static_cast<int>(r_));

		before = (*_xmp)[ExifProxy::_XMPKEY].toString();
		(*_xmp)[ExifProxy::_XMPKEY] = rating;
		_xmpkpos = _xmp->findKey(Exiv2::XmpKey(ExifProxy::_XMPKEY));
	    }
	}
	else
	{
	    if (_xmpkpos == _xmp->end()) {
		if (r_ != ExifProxy::Rating::UNSET) {
		    g_log(_logdomain.c_str(), G_LOG_LEVEL_DEBUG, "%s: requested rating  %s -> %d",  _file.c_str(), (*_xmp)[ExifProxy::_XMPKEY].toString().c_str(), static_cast<int>(r_));

		    before = (*_xmp)[ExifProxy::_XMPKEY].toString();
		    (*_xmp)[ExifProxy::_XMPKEY] = rating;
		    _xmpkpos = _xmp->findKey(Exiv2::XmpKey(ExifProxy::_XMPKEY));
		}
	    }
	    else
	    {
		before = (*_xmp)[ExifProxy::_XMPKEY].toString();
		if (r_ == ExifProxy::Rating::UNSET) {
		    g_log(_logdomain.c_str(), G_LOG_LEVEL_DEBUG, "%s: requested rating  %s -> %d removing",  _file.c_str(), (*_xmp)[ExifProxy::_XMPKEY].toString().c_str(), static_cast<int>(r_));

		    _xmp->erase(_xmpkpos);
		    _xmpkpos = _xmp->end();
		}
		else
		{
		    if ((*_xmp)[ExifProxy::_XMPKEY].toString() == rating.toString()) {
			g_log(_logdomain.c_str(), G_LOG_LEVEL_DEBUG, "%s: requested rating  %s -> %d ignored",  _file.c_str(), (*_xmp)[ExifProxy::_XMPKEY].toString().c_str(), static_cast<int>(r_));
		    }
		    else {
			g_log(_logdomain.c_str(), G_LOG_LEVEL_DEBUG, "%s: requested rating  %s -> %d updating",  _file.c_str(), (*_xmp)[ExifProxy::_XMPKEY].toString().c_str(), static_cast<int>(r_));

			(*_xmp)[ExifProxy::_XMPKEY] = rating;
			_xmpkpos = _xmp->findKey(Exiv2::XmpKey(ExifProxy::_XMPKEY));
		    }
		}
	    }
	}
	_img->writeMetadata();

	flipped = true;

	struct utimbuf  tmput;
	memset(&tmput, 0, sizeof(tmput));
	tmput.modtime = _mtime;
	utime(_file.c_str(), &tmput);

	ExifProxy::History::iterator  h =
	    std::find_if( _history.begin(), _history.end(), [this](const auto& e){
		return e.f == _file;
	    });

	if (h == _history.end()) {
	    _history.push_back(ExifProxy::HistoryEvnt(_file, _img->exifData(), before, rating.toString()));
	}
	else {
	    const std::string  latestRating = rating.toString();
	    if (h->rating == latestRating ) {
	        _history.erase(h);
	    }
	    else {
		if (r_ == ExifProxy::Rating::UNSET) {
		    h->finalRating.clear();
		}
		else {
		    h->finalRating = latestRating;
		}
	    }
	}
    }
    catch (const std::exception& ex)
    {
	g_log(_logdomain.c_str(), G_LOG_LEVEL_WARNING, "%s: failed to update XMP rating - %s", _file.c_str(), ex.what());
	flipped = false;
    }
    return flipped;
}


/* mark the image if its not already rated
*/
bool  ExifProxy::fliprating()
{
    bool  flipped = false;
    if (_img.get() == 0) {
	// something went wrong earlier/no img ... do nothing
	return flipped;
    }

    try
    {
	return rate(
	    (_xmp == NULL || _xmpkpos == _xmp->end()) ?
		ExifProxy::Rating::FIVE :
		ExifProxy::Rating::UNSET);
    }
    catch (const std::exception& ex)
    {
	g_log(_logdomain.c_str(), G_LOG_LEVEL_WARNING, "%s: failed to flip XMP rating - %s", _file.c_str(), ex.what());
    }
    return false;
}


void  ExifProxy::_clear()
{
    _xmp = NULL;
    _file.clear();
    _img.reset();
    _mtime = 0;
    _rating.clear();
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
    const std::array  etags {
        _ETag(Exiv2::ExifKey("Exif.Image.Model"), ""),
        _ETag(Exiv2::ExifKey("Exif.Photo.DateTimeOriginal"), ""),
        _ETag(Exiv2::ExifKey("Exif.Photo.ExposureTime"), ""),
        _ETag(Exiv2::ExifKey("Exif.Photo.FNumber"), ""),
        _ETag(Exiv2::ExifKey("Exif.Photo.ISOSpeedRatings"), "ISO")
    };

    os_ << (obj_.finalRating.empty() || obj_.finalRating == "0" ? "---" :  "+++") << " { " << obj_.rating << " -> " << obj_.finalRating << " }" << ' ' << obj_.f << " : { ";

    std::for_each(etags.begin(), etags.end(), [&os_, &obj_](const auto& t_) {
        Exiv2::ExifData::const_iterator  e = obj_.exif.findKey(t_.tag);
        if (e == obj_.exif.end()) {
            return;
        }

        os_ << " " << t_.prfx << *e;
    });
    os_ << '}';

    return os_;
}
