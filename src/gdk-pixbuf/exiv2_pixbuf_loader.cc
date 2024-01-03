/* exiv2_pixbuf_loader.cc
 *
 * a dirty hack gdk_pixbuf_loader that uses exiv2 to extract raw file embedded 
 * preview images and present them to the gdk pixbuf system via OTHER loaders
 * such as jpg.  For some reason it doesnt handle tifs pulled from the raw file
 *
 # compile and link Makefile targets
  
   exiv2_pixbuf_loader.o:  exiv2_pixbuf_loader.cc
           g++ -DNDEBUG -c -fPIC -g -Wwrite-strings $(shell pkg-config gdk-pixbuf-2.0 --cflags) $(shell pkg-config exiv2 --cflags) $^
  
   libexiv2_pixbuf_loader.so:      exiv2_pixbuf_loader.o
           g++ -shared -g $(shell pkg-config gdk-pixbuf-2.0 --libs) $(shell pkg-config exiv2 --libs) $^ -o $@
  
 # install to system area and regenerating loader cache
 # following this, the loader will be available to items usch as eog
 # ... libopenraw also provides a gdk pixbuf loader but it actually decodes
 #     the raw file - for eog/image viewers i'm not so interested

   cp libexiv2_pixbuf_loader.so /usr/lib64/gdk-pixbuf-2.0/2.10.0/loaders
   gdk-pixbuf-query-loaders-64 > /usr/lib64/gdk-pixbuf-2.0/2.10.0/loaders.cache
  
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <memory>
#include <sstream>
#include <unordered_map>
#include <utility>

#define GDK_PIXBUF_ENABLE_BACKEND
#include <gdk-pixbuf/gdk-pixbuf-io.h>
#include <gdk-pixbuf/gdk-pixbuf.h>


#include "ImgFactory.h"
#include "version.h"


extern "C" {
G_MODULE_EXPORT void fill_vtable (GdkPixbufModule *module);
G_MODULE_EXPORT void fill_info (GdkPixbufFormat *info);
}


namespace Exiv2GdkPxBufLdr
{
class Info 
{
  public:
    Info(const Info&)  = delete;
    Info(const Info&&) = delete;
    Info& operator=(const Info&)  = delete;
    Info& operator=(const Info&&) = delete;


    static const Info&  instance()
    {
        if (_instance == NULL) {
            _instance = new Info();
        }
        return *_instance;
    }

    const char* const  name;
    const char* description;


  private:
    Info() : name("exiv2 RAW preview loader")
    {
        _description = "RAW preview image loader (via exiv2) ";
        _description += Exiv2GdkPxBufLdr::version();

        description = _description.c_str();
    }

    std::string  _description;
    static Info*  _instance;
};
Info*  Info::_instance = NULL;


struct PixbufLdrBuf
{
    PixbufLdrBuf() : pixbuf(NULL), loader(NULL) { }
    PixbufLdrBuf(PixbufLdrBuf& rhs_): pixbuf(rhs_.pixbuf), loader(rhs_.loader)
    {
	DBG_LOG("unexpected call, expecting RVO usage");
	if (loader) {
	    g_object_ref(loader);
	}
    }
    ~PixbufLdrBuf()
    {
	if (loader) {
	    g_object_unref(loader);
	}
    }
    PixbufLdrBuf& operator=(const PixbufLdrBuf&) = delete;

    GdkPixbuf*  pixbuf;
    GdkPixbufLoader*  loader;
};

std::ostream& operator<<(std::ostream& os_, GError**& err_)
{
    if (err_ && *err_) {
	os_ << *err_;
    }
    return os_;
}

PixbufLdrBuf  _createPixbuf(const ImgFactory::Buf& prevwbuf_, const std::string& mimeType_, GError**& error_)
{
    PixbufLdrBuf  rvo;

    /* let the other better placed loaders deal with the RAW img
     * preview (tif/jpeg)
     */
    rvo.loader = gdk_pixbuf_loader_new_with_mime_type(mimeType_.c_str(), error_);
    DBG_LOG("pixbuf loader=", (void*)rvo.loader, " prev mime type=", mimeType_, " buf=", prevwbuf_.sz(), " err=", error_);
    if (error_ && *error_) {
	g_error_free(*error_);
	*error_ = NULL;
	g_log(Exiv2GdkPxBufLdr::G_DOMAIN, G_LOG_LEVEL_WARNING, "no known loader for explicit mimetype '%s', defaulting", mimeType_.c_str());
	if (rvo.loader != NULL) {
	    gdk_pixbuf_loader_close(rvo.loader, NULL);
	    g_object_unref(rvo.loader);
	}
	rvo.loader = gdk_pixbuf_loader_new();
    }

    if (gdk_pixbuf_loader_write(rvo.loader, prevwbuf_.buf(), prevwbuf_.sz(), error_) == TRUE)
    {
	gdk_pixbuf_loader_close(rvo.loader, NULL);
	rvo.pixbuf = gdk_pixbuf_loader_get_pixbuf(rvo.loader);
	DBG_LOG("internal load complete, pixbuf=", (void*)rvo.pixbuf);
    }

    return rvo;
}

struct Exiv2PxbufCtx
{
    GdkPixbufModuleSizeFunc     size_func;
    GdkPixbufModulePreparedFunc prepared_func;
    GdkPixbufModuleUpdatedFunc  updated_func;
    gpointer                    user_data;

#ifndef NDEBUG
    int          fd;
#endif
    GByteArray*  data;
};


extern "C" {

static 
gpointer _gpxbuf_bload(GdkPixbufModuleSizeFunc     szfunc_,
                       GdkPixbufModulePreparedFunc prepfunc_,
                       GdkPixbufModuleUpdatedFunc  updfunc_,
                       gpointer udata_,
                       GError **error_)
{
    struct Exiv2PxbufCtx*  ctx = g_new0(struct Exiv2PxbufCtx, 1);

    ctx->size_func     = szfunc_;
    ctx->prepared_func = prepfunc_;
    ctx->updated_func  = updfunc_;
    ctx->user_data     = udata_;
    ctx->data = g_byte_array_new();

    return (gpointer)ctx;
}


static 
gboolean _gpxbuf_lincr(gpointer ctx_,
                       const guchar* buf_, guint size_,
                       GError **error_)
{
    DBG_LOG("incr load, bytes=", size_);
    Exiv2PxbufCtx*  ctx = (Exiv2PxbufCtx*)ctx_;
    g_byte_array_append(ctx->data, buf_, size_);
    return TRUE;
}

static 
gboolean _gpxbuf_sload(gpointer ctx_, GError **error_)
{
    Exiv2PxbufCtx*  ctx = (Exiv2PxbufCtx*)ctx_;
    gboolean result = FALSE;

    DBG_LOG("starting buf _gpxbuf_sload, len=", ctx->data->len);

    std::string  mimeType;
    try
    {
	ImgFactory::Buf  prevwbuf;
	ImgFactory::instance().create((unsigned char*)ctx->data->data, (ssize_t)ctx->data->len, prevwbuf, mimeType);

	try
	{
	    const Exiv2GdkPxBufLdr::PixbufLdrBuf  plb = Exiv2GdkPxBufLdr::_createPixbuf(prevwbuf, mimeType, error_);
	    if (plb.pixbuf)
	    {
		if (ctx->prepared_func != NULL) {
		    (*ctx->prepared_func)(plb.pixbuf, NULL, ctx->user_data);
		}
		if (ctx->updated_func != NULL) {
		    (*ctx->updated_func)(plb.pixbuf, 0, 0,
			    gdk_pixbuf_get_width(plb.pixbuf),
			    gdk_pixbuf_get_height(plb.pixbuf),
			    ctx->user_data);
		}
		result = TRUE;
	    }
	}
	catch (const std::exception& ex)
	{
	    g_set_error(error_, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED, "failed to load/create pixbuf - %s", ex.what());
	}
    }
    catch (const std::exception& ex)
    {
	g_set_error(error_, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED, "failed to create pixbuf for %s - %s", mimeType.c_str(), ex.what());
    }
    catch (...) 
    {
	g_set_error(error_, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_UNSUPPORTED_OPERATION, "failed to load/create pixbuf - unknown exception: %s, %d", __FILE__, __LINE__);
    }
    g_byte_array_free(ctx->data, TRUE);
    g_free(ctx);
    return result;
}


void fill_vtable (GdkPixbufModule *module)
{
    module->begin_load     = _gpxbuf_bload;
    module->stop_load      = _gpxbuf_sload;
    module->load_increment = _gpxbuf_lincr;
}


void fill_info (GdkPixbufFormat *info)
{
    static GdkPixbufModulePattern signature[] = {
        { /* TIFF */
            (char*)"MM \x2a",
            (char*)"  z ",
            80
        },

        { /* CR2 */
            (char*)"II\x2a \x10   CR\x02 ",
            (char*)"   z zzz   z",
            100
        },

        { /* TIFF */
            (char*)"II\x2a ",
            (char*)"   z",
            80
        },

        { /* CRW */
            (char*)"II\x1a   HEAPCCDR",
            (char*)"   zzz        ",
            100
        },

	//DNG-TIFF
        {
            (char*)"MM \x2a   ",
            (char*)"  z zzz",
            80
        },
        {
            (char*)"II\x2a \x08   ",
            (char*)"   z zzz",
            80
        },
	{
            (char*)"MM \x2a   \x08",
            (char*)"  z zzz ",
            100 
        },
	{
            (char*)"MM \x2a   \x12",
            (char*)"  z zzz ",
            100 
        },

        { NULL, NULL, 0 }
    };
        
    // xdg-mime query filetype foo.xxx
    static const gchar *mime_types[] = {
        "image/x-canon-cr2",
        "image/x-canon-crw",
        "image/x-nikon-nef",
	"image/x-adobe-dng",
        NULL
    };
        
    static const gchar *extensions[] = {
        "cr2",
        "crw",
        "nef",
        "dng",
        NULL
    };
        
    info->name        = (char*)Exiv2GdkPxBufLdr::Info::instance().name;
    info->signature   = signature;
    info->description = (char*)Exiv2GdkPxBufLdr::Info::instance().description;
    info->mime_types  = (gchar**)mime_types;
    info->extensions  = (gchar**)extensions;
    info->flags       = GDK_PIXBUF_FORMAT_THREADSAFE;
    info->license     = (char*)"LGPL";
}
}
}
