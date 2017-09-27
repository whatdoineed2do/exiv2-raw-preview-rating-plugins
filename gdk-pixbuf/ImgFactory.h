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

	Buf(Buf& rhs_) : _magick(rhs_._magick), _exiv2(rhs_._exiv2)
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

	    _buf = _exiv2.pData_;
	    _sz  = _exiv2.size_;
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
		_sz  = _exiv2.size_;
		_buf = _exiv2.pData_;
	    }
	}

	unsigned char*  _buf;
	ssize_t  _sz;

	Exiv2::DataBuf  _exiv2;
	Magick::Blob    _magick;
    };


    static ImgFactory&  instance();

    ImgFactory();

    ImgFactory(ImgFactory&) = delete;
    ImgFactory& operator=(ImgFactory&) = delete;


    ImgFactory::Buf&  create(FILE*, ImgFactory::Buf&, std::string& mimeType_);
    ImgFactory::Buf&  create(const unsigned char* buf_, ssize_t bufsz_, ImgFactory::Buf&, std::string& mimeType_);

  private:
    static std::unique_ptr<ImgFactory>  _instance;
    static std::once_flag  _once;

    const Magick::Blob  _srgbICC;
    const Magick::Blob  _argbICC;

    mutable Exiv2GdkPxBufLdr::Buf  _tmp;
};

}

#endif
