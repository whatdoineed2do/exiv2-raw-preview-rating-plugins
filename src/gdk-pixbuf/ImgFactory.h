#ifndef IMG_FACTORY_H
#define IMG_FACTORY_H

#include <sys/types.h>
#include <stdio.h>
#include <string>

#include <exiv2/exiv2.hpp>
#include <Magick++.h>

#include "DbgHlpr.h"
#include "Buf.h"


namespace  Exiv2GdkPxBufLdr
{

class ImgFactory
{
  public:
    struct Buf
    {
	Buf() : _buf(NULL), _sz(0) { }
	~Buf() = default;

	Buf(Buf& rhs_) : _exiv2(rhs_._exiv2), _magick(rhs_._magick)
	{ _assign(rhs_); }

	Buf&  operator=(Buf& rhs_)
	{
	    if (this != &rhs_) {
		_magick = rhs_._magick;
		_exiv2  = rhs_._exiv2;
		_assign(rhs_);
	    }
	    return *this;
	}


	unsigned char*  buf() const { return _buf; }
	ssize_t          sz() const { return _sz; }

	void  finalize(const Exiv2::PreviewImage&  data_)
	{
	    _exiv2 = data_.copy();

#if EXIV2_VERSION >= EXIV2_MAKE_VERSION(0,28,0) 
	    _buf = (unsigned char*)_exiv2.c_str(0);
	    _sz  = _exiv2.size();
#else
	    _buf = _exiv2.pData_;
	    _sz  = _exiv2.size_;
#endif
	}

	void  finalize(Magick::Image&  data_)
	{
	    data_.write(&_magick);

	    _buf = (unsigned char*)_magick.data();
	    _sz  = _magick.length();
	}

      private: 
	void  _assign(const Buf& rhs_)
	{ 
	    if (rhs_._buf == NULL) {
		return;
	    }

	    if (rhs_._buf == rhs_._magick.data()) {
		_sz  = _magick.length();
		_buf = (unsigned char*)_magick.data();
	    }
	    else {
#if EXIV2_VERSION >= EXIV2_MAKE_VERSION(0,28,0) 
		_buf = (unsigned char*)_exiv2.c_str(0);
		_sz  = _exiv2.size();
#else
		_buf = _exiv2.pData_;
		_sz  = _exiv2.size_;
#endif
	    }
	}

	unsigned char*  _buf;
	ssize_t  _sz;

	Exiv2::DataBuf  _exiv2;
	Magick::Blob    _magick;
    };


    static ImgFactory&  instance();

    ImgFactory();
    ~ImgFactory();

    ImgFactory(ImgFactory&) = delete;
    ImgFactory& operator=(ImgFactory&) = delete;


    ImgFactory::Buf&  create(FILE*, ImgFactory::Buf&, std::string& mimeType_);
    ImgFactory::Buf&  create(const unsigned char* buf_, ssize_t bufsz_, ImgFactory::Buf&, std::string& mimeType_);

  private:
    static std::unique_ptr<ImgFactory>  _instance;
    static std::once_flag  _once;

    const Magick::Blob  _argbICC;
    const Magick::Blob  _srgbICC;

    mutable Exiv2GdkPxBufLdr::Buf  _tmp;
};

}

#endif
