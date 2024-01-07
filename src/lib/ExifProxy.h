#ifndef EXIF_PROXY_H
#define EXIF_PROXY_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>

#include <string>
#include <list>
#include <iosfwd>
#include <mutex>

#include <exiv2/exiv2.hpp>


std::ostream&  operator<<(std::ostream& os_, const Exiv2::XmpData& data_); 


/* This object represents a mutable underlying image and its exif rating; we
 * use this to update the exif rating as ncessary
 */
class ExifProxy
{
  public:
    friend std::ostream&  operator<<(std::ostream&, const ExifProxy&);

    enum class Rating {
        UNSET = -1, ZERO = 0, ONE, TWO, THREE, FOUR, FIVE
    };

    struct HistoryEvnt {
        HistoryEvnt(const std::string& f_, const Exiv2::ExifData& exif_, const Rating initial_, const Rating rating_)
            : f(f_), exif(exif_), initial(initial_), rating(rating_)
        { }

        const std::string      f;
        const Exiv2::ExifData  exif;
	const ExifProxy::Rating  initial;
	ExifProxy::Rating        rating;

	bool  changed() const
	{ return initial != rating; }
    };

    using History = std::list<HistoryEvnt>;

#ifdef EOG_PLUGIN_XMP_INIT_LOCK
    class _XmpLock
    {
      public:
        _XmpLock()  = default;
        ~_XmpLock() = default;

        _XmpLock(const _XmpLock&)  = delete;
        _XmpLock(const _XmpLock&&) = delete;


        static void  lockUnlock(void* obj_, bool lock_)
        {
            _XmpLock*  obj;
            if ( (obj = reinterpret_cast<_XmpLock*>(obj_)) ) {
                  if (lock_) {
                      obj->_m.lock();
                  }
                  else {
                      obj->_m.unlock();
                  }
            }
        }

      private:
        std::mutex  _m;
    };
#endif

    ExifProxy(const char* logdomain_="exiv2-rating-ExifProxy");
    virtual ~ExifProxy() = default;

    ExifProxy(const ExifProxy&) = delete;
    void operator=(const ExifProxy&) = delete;

    // file represent new file
    ExifProxy&  ref(const char*);

    bool  valid() const
    {
        return _img.get() != 0;
    }

    // does the underlying file have a rating, does not reflect on any changes weve made
    bool  rated() const
    {
        return _xmp == NULL ? false : _xmpkpos != _xmp->end();
    }

    // string rep of rating: **---, *****, ----- etc
    const char*  rating();

    /* mark the image if its not already rated
     */
    bool  fliprating();

    bool  cycleRating();
    bool  unsetRating();

    // write rating to file
    bool  syncRating();


    const std::string&  file() const
    { return _file; }

    const ExifProxy::History&  history() const
    { return _history; }


  private:
    static const std::string          _XMPKEY;
    static const Exiv2::XmpTextValue  _XMPVAL;

    const std::string  _logdomain;

    time_t  _mtime;

#if EXIV2_VERSION >= EXIV2_MAKE_VERSION(0,28,0) 
    Exiv2::Image::UniquePtr  _img;
#else
    Exiv2::Image::AutoPtr  _img;
#endif
    Exiv2::XmpData*  _xmp;
    Exiv2::XmpData::iterator  _xmpkpos;

    std::string  _file;
    std::string  _ratingBuf;
    Rating  _rating;

#ifdef EOG_PLUGIN_XMP_INIT_LOCK
    _XmpLock  _xmplock;
#endif

    void  _clear();

    bool  _updateRating(const ExifProxy::Rating);

    ExifProxy::History  _history;
};


std::ostream&  operator<<(std::ostream& os_, const ExifProxy& obj_);
std::ostream&  operator<<(std::ostream& os_, const ExifProxy::HistoryEvnt& obj_);

#endif
