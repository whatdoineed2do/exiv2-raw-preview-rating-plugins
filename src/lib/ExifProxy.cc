#include "ExifProxy.h"

#include <glib.h>
#include <array>
#include <algorithm>


const std::string          ExifProxy::_XMPKEY { "Xmp.xmp.Rating" };
const Exiv2::XmpTextValue  ExifProxy::_XMPVAL { Exiv2::XmpTextValue("5") };

std::ostream&  operator<<(std::ostream& os_, const Exiv2::XmpData& data_) 
{
    for (Exiv2::XmpData::const_iterator md = data_.begin();
         md != data_.end(); ++md) 
    {
        os_ << "{ " << md->key() << " " << md->typeName() << " " << md->count() << " " << md->value() << " }";
    }

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
	bool  r = false;
	if (_xmp == NULL)
	{
	    // previously found no XMP data set
	    _xmp = &_img->xmpData();

	    (*_xmp)[ExifProxy::_XMPKEY] = ExifProxy::_XMPVAL;
	    _xmpkpos = _xmp->findKey(Exiv2::XmpKey(ExifProxy::_XMPKEY));

	    r = true;
	}
	else
	{
	    if (_xmpkpos == _xmp->end()) {
		(*_xmp)[ExifProxy::_XMPKEY] = ExifProxy::_XMPVAL;
		_xmpkpos = _xmp->findKey(Exiv2::XmpKey(ExifProxy::_XMPKEY));

		r = true;
	    }
	    else {
		_xmp->erase(_xmpkpos);
		_xmpkpos = _xmp->end();
		r = false;
	    }
	}
	_img->writeMetadata();

	flipped = true;

	struct utimbuf  tmput;
	memset(&tmput, 0, sizeof(tmput));
	tmput.modtime = _mtime;
	utime(_file.c_str(), &tmput);

	ExifProxy::History::iterator  h;
	for (h=_history.begin(); h!=_history.end(); ++h)
	{
	    if (h->f == _file) {
		break;
	    }
	}
	if (h != _history.end()) {
	    _history.erase(h);
	}
	else
	{
	    _history.push_back(ExifProxy::HistoryEvnt(_file, _img->exifData(), r));
	}
    }
    catch (const std::exception& ex)
    {
	g_log(_logdomain.c_str(), G_LOG_LEVEL_WARNING, "%s: failed to update XMP rating - %s", _file.c_str(), ex.what());
	flipped = false;
    }
    return flipped;
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

    os_ << obj_.f << ": " << (obj_.rated ? "R" : "U");

    std::for_each(etags.begin(), etags.end(), [&os_, &obj_](const auto& t_) {
        Exiv2::ExifData::const_iterator  e = obj_.exif.findKey(t_.tag);
        if (e == obj_.exif.end()) {
            return;
        }

        os_ << " " << t_.prfx << *e;
    });

    return os_;
}
