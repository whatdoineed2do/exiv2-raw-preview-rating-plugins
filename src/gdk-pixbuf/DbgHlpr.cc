#include "DbgHlpr.h"

#include <string>

namespace  Exiv2GdkPxBufLdr
{
std::unique_ptr<DbgHlpr>  DbgHlpr::_instance;
std::once_flag  DbgHlpr::_once;

DbgHlpr::DbgHlpr() : _fd(0), _pid(getpid())
{ 
    const mode_t  umsk = umask(0);
    umask(umsk);
    if ( (_fd = open("exiv2_pixbuf_loader.log", O_CREAT | O_WRONLY | O_APPEND, umsk | 0666)) < 0) {
	printf("failed to create debug log - %s\n", strerror(errno));
    }
    log(__FILE__, __LINE__, "starting");
}


}
