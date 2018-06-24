#include "version.h"

namespace Exiv2GdkPxBufLdr
{

const char*  version()
{
#ifdef EXIV2_PXBUF_LDR_VERSION
#define EXIV2_PXBUF_LDR_STRM(x)  #x
#define EXIV2_PXBUF_LDR_STR(x)   EXIV2_PXBUF_LDR_STRM(x)
#define EXIV2_PXBUF_LDR_VERSION_STR  EXIV2_PXBUF_LDR_STR(EXIV2_PXBUF_LDR_VERSION)
#else
#define EXIV2_PXBUF_LDR_VERSION_STR  "v0.0.0-unknown"
#endif
    return EXIV2_PXBUF_LDR_VERSION_STR;
}

}
