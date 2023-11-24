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


class ExifProxy
{
  public:
    friend std::ostream&  operator<<(std::ostream&, const ExifProxy&);

    struct HistoryEvnt {
        HistoryEvnt(const std::string& f_, const Exiv2::ExifData& exif_, bool rated_)
            : f(f_), exif(exif_), rated(rated_)
        { }

        HistoryEvnt(const std::string& f_, const Exiv2::ExifData* exif_ = NULL, bool rated_=false)
            : f(f_), exif(exif_ == NULL ? Exiv2::ExifData() : *exif_), rated(rated_)
        { }

        const std::string      f;
        const Exiv2::ExifData  exif;
        const bool             rated;
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

    ExifProxy();
    virtual ~ExifProxy() = default;

    ExifProxy(const ExifProxy&) = delete;
    void operator=(const ExifProxy&) = delete;

    ExifProxy&  ref(const char*);

    bool  valid() const
    {
        return _img.get() != 0;
    }

    bool  rated() const
    {
        return _xmp == NULL ? false : _xmpkpos != _xmp->end();
    }

    const char*  rating();

    /* mark the image if its not already rated
     */
    bool  fliprating();


    const std::string&  file() const
    { return _file; }

    const ExifProxy::History&  history() const
    { return _history; }


  private:
    static const std::string          _XMPKEY;
    static const Exiv2::XmpTextValue  _XMPVAL;

    time_t  _mtime;

    Exiv2::Image::AutoPtr  _img;
    Exiv2::XmpData*  _xmp;
    Exiv2::XmpData::iterator  _xmpkpos;

    std::string  _file;
    std::string  _rating;

#ifdef EOG_PLUGIN_XMP_INIT_LOCK
    _XmpLock  _xmplock;
#endif

    void  _clear();

    ExifProxy::History  _history;
};


std::ostream&  operator<<(std::ostream& os_, const ExifProxy& obj_);
std::ostream&  operator<<(std::ostream& os_, const ExifProxy::HistoryEvnt& obj_);

#endif
