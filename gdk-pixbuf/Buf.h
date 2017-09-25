#ifndef BUF_H
#define BUF_H

#include <sys/types.h>
#include <string.h>

namespace  Exiv2GdkPxBufLdr
{

class Buf
{
  public:
    Buf() : _buf(NULL), _sz(0), _bufsz(0) { }
    Buf(size_t sz_) : _buf(NULL), _sz(0), _bufsz(0) { alloc(sz_); }

    ~Buf() { free(); }

    Buf(const Buf&) = delete;
    void operator=(const Buf&) = delete;


    void  alloc(size_t sz_)
    {
	if (sz_ > _sz) {
	    delete [] _buf;
	    _sz = sz_;
	    _bufsz = _sz;
	    _buf = new unsigned char[_sz];
	}
	memset(_buf, 0, _sz);
    }

    void  free()
    {
	delete []  _buf;
	_buf = NULL;
	_sz = 0;
	_bufsz = 0;
    }

    const unsigned char*  copy(unsigned char* buf_, size_t sz_)
    {
	alloc(sz_);
	memcpy(_buf, buf_, sz_);
	_bufsz = sz_;
	return _buf;
    }

    void  clear()
    {
	memset(_buf, 0, _sz);
    }

    unsigned char*&  buf() { return _buf; }
    size_t  bufsz() const { return _bufsz; }
    size_t  sz() const { return _sz; }

  private:
    unsigned char*  _buf;
    size_t    _bufsz;
    size_t    _sz;
};
}
#endif
