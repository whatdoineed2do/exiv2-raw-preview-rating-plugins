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

    DbgHlpr();
    ~DbgHlpr() { close(_fd); }

    DbgHlpr(const DbgHlpr&) = delete;
    DbgHlpr& operator=(const DbgHlpr&) = delete;


    template<typename ... Args>
    void  log(const char* file_, const int line_, Args&&...args_)
    {
	std::ostringstream  dump;
	dump << _pid << ": " << file_ << ", " << line_ << ": ";
	_log(file_, line_, dump, std::forward<Args>(args_)...);
	dump << '\n';

	const std::string&&  what = dump.str();
	write(_fd, what.c_str(), what.length());
    }

  private:
    static std::unique_ptr<DbgHlpr>  _instance;

    const pid_t  _pid;
    int  _fd;


    template<typename T>
    void _log(const char*, const int, std::ostringstream& os_, T&& arg_)
    {
	os_ << arg_;
    }

    // fake recursive variadic function
    template<typename T, typename ... Args>
    void  _log(const char* file_, const int line_, std::ostringstream& os_, T&& arg_, Args&&...args_)
    {
	os_ << arg_;
	_log(file_, line_, os_, std::forward<Args>(args_)...);
    }

};

#ifndef NDEBUG
  #define DBG_LOG(...) \
    Exiv2GdkPxBufLdr::DbgHlpr::instance().log(__FILE__, __LINE__, __VA_ARGS__)
#else
  #define DBG_LOG(...) 
#endif

}

#endif
