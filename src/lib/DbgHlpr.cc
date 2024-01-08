#include "DbgHlpr.h"


#include <string>

namespace  Exiv2GdkPxBufLdr
{
std::unique_ptr<DbgHlpr>  DbgHlpr::_instance;
std::once_flag  DbgHlpr::_once;
}
