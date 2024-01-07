#ifndef DBG_HLPR_H
#define DBG_HLPR_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include <mutex>
#include <memory>
#include <string>
#include <sstream>

#include <glib.h>

namespace  Exiv2GdkPxBufLdr
{
const char* const  G_DOMAIN = "gdk-pixbuf.exiv2-rawpreview";

class DbgHlpr
{
  public:
    static DbgHlpr&  instance()
    {
	std::call_once(DbgHlpr::_once, [](){
	    DbgHlpr::_instance = std::make_unique<DbgHlpr>();
	    g_print("G_MESSAGES_DEBUG=%s\n", Exiv2GdkPxBufLdr::G_DOMAIN);
	});

	return *DbgHlpr::_instance;
    }

    DbgHlpr() = default;
    ~DbgHlpr() = default;

    DbgHlpr(const DbgHlpr&) = delete;
    DbgHlpr& operator=(const DbgHlpr&) = delete;


    template<typename ... Args>
    void  log(const char* file_, const int line_, Args&&...args_)
    {
	std::ostringstream  dump;
	dump << file_ << ", " << line_ << ": ";
	_log(dump, std::forward<Args>(args_)...);

	g_log(Exiv2GdkPxBufLdr::G_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", dump.str().c_str());
    }

  private:
    static std::unique_ptr<DbgHlpr>  _instance;
    static std::once_flag  _once;

    template<typename T>
    void _log(std::ostringstream& os_, T&& arg_)
    {
	os_ << arg_;
    }

    // fake recursive variadic function
    template<typename T, typename ... Args>
    void  _log(std::ostringstream& os_, T&& arg_, Args&&...args_)
    {
	os_ << arg_;
	_log(os_, std::forward<Args>(args_)...);
    }

};

#ifndef NDEBUG
  #warning "debug logging enabled"
  #define DBG_LOG(...) \
    Exiv2GdkPxBufLdr::DbgHlpr::instance().log(__FILE__, __LINE__, __VA_ARGS__)
#else
  #define DBG_LOG(...) 
#endif
}

#endif
