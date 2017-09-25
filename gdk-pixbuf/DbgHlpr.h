#ifndef DBG_HLPR_H
#define DBG_HLPR_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <memory>
#include <string>
#include <sstream>

namespace  Exiv2GdkPxBufLdr
{

class DbgHlpr
{
  public:
    static DbgHlpr&  instance()
    {
	if (DbgHlpr::_instance.get() == NULL) {
	    DbgHlpr::_instance = std::make_unique<DbgHlpr>();
	}
	return *DbgHlpr::_instance;
    }

    ~DbgHlpr() { close(_fd); }

    DbgHlpr(const DbgHlpr&) = delete;
    DbgHlpr& operator=(const DbgHlpr&) = delete;

    void  log(const char* _file_, const int _line_, const char* msg_)
    {
	std::ostringstream  os;
	os << _pid << ":  " << _file_ << ", " << _line_ << " :";
	if (msg_) {
	    os << "  " << msg_;
	}

#if 0
	if (err_ && *err_) {
	    os << "  err=" << (*err_)->message;
	}
#endif
	os << "\n";
	const std::string&&  what = os.str();
	write(_fd, what.c_str(), what.length());
     }

    template <typename T>
     static std::string  concat(const char* a_, const T b_)
     {
	 std::ostringstream  os;
	 os << a_ << b_;
	 return os.str();
     }

    DbgHlpr() : _fd(0), _pid(getpid())
    { 
	const mode_t  umsk = umask(0);
	umask(umsk);
	if ( (_fd = open("exiv2_pixbuf_loader.log", O_CREAT | O_WRONLY | O_APPEND, umsk | 0666)) < 0) {
	    printf("failed to create debug log - %s\n", strerror(errno));
	}
	log(__FILE__, __LINE__, "starting");
    }

  private:
    static std::unique_ptr<DbgHlpr>  _instance;

    const pid_t  _pid;
    int  _fd;
};

#ifndef NDEBUG
  #define DBG_LOG(info, err) \
    Exiv2GdkPxBufLdr::DbgHlpr::instance().log(__FILE__, __LINE__, info)
#else
  #define DBG_LOG(info, err) 
#endif

}

#endif
