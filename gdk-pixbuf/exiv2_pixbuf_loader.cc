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


extern "C" {
G_MODULE_EXPORT void fill_vtable (GdkPixbufModule *module);
G_MODULE_EXPORT void fill_info (GdkPixbufFormat *info);
}


namespace Exiv2GdkPxBufLdr
{
class PixbufLdrCache {
  public:
    using Cache = std::unordered_map<std::string, GdkPixbufLoader*>;

    PixbufLdrCache() = default;
    ~PixbufLdrCache()
    {
	for (auto& c : _cache) {
	    g_object_unref(c.second);
	}
    }

    PixbufLdrCache(const PixbufLdrCache&) = delete;
    PixbufLdrCache& operator=(const PixbufLdrCache&) = delete;

    GdkPixbufLoader*  operator[](const std::string& idx_)
    {
	auto e = _cache.find(idx_);
	if (e != _cache.end()) {
	    return e->second;
	}

	GdkPixbufLoader*  ldr = NULL;
	if ( (ldr = gdk_pixbuf_loader_new_with_mime_type(idx_.c_str(), NULL)) ) {
	    _cache.insert(std::make_pair(idx_, ldr));
	}
	return ldr;
    }

  private:
    PixbufLdrCache::Cache  _cache;
};


struct PixbufLdrBuf
{
    PixbufLdrBuf() : pixbuf(NULL), loader(NULL) { }
    PixbufLdrBuf(PixbufLdrBuf& rhs_): pixbuf(rhs_.pixbuf), loader(rhs_.loader)
    {
	DBG_LOG("unexpected call, expecting RVO usage", NULL);
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

PixbufLdrBuf  _createPixbuf(const ImgFactory::Buf& prevwbuf_, const std::string& mimeType_, GError**& error_)
{
    PixbufLdrBuf  rvo;

    /* let the other better placed loaders deal with the RAW img
     * preview (tif/jpeg)
     */
    rvo.loader = gdk_pixbuf_loader_new_with_mime_type(mimeType_.c_str(), error_);
    DBG_LOG(DbgHlpr::concat("pixbuf loader=", (void*)rvo.loader).c_str(), error_);
    DBG_LOG(DbgHlpr::concat("prev mime type=", mimeType_.c_str()).c_str(), NULL);
    DBG_LOG(DbgHlpr::concat("buf=", prevwbuf_.sz()).c_str(), NULL);
    if (error_ && *error_) {
	g_error_free(*error_);
	*error_ = NULL;
	printf("no known loader for explicit mimetype, defaulting\n");
	if (rvo.loader != NULL) {
	    gdk_pixbuf_loader_close(rvo.loader, NULL);
	    g_object_unref(rvo.loader);
	}
	rvo.loader = gdk_pixbuf_loader_new();
    }

    if (gdk_pixbuf_loader_write(rvo.loader, prevwbuf_.buf(), prevwbuf_.sz(), error_) != TRUE)
    {
	printf("%s, line %ld - sload, internal loader failed, rawio buf=%ld size=%ld  err=%s\n", __FILE__, __LINE__, prevwbuf_.buf(), prevwbuf_.sz(), (error_ == NULL || error_ && *error_ == NULL ? "<>" : (*error_)->message));
    }
    else
    {
	gdk_pixbuf_loader_close(rvo.loader, NULL);
	rvo.pixbuf = gdk_pixbuf_loader_get_pixbuf(rvo.loader);
	DBG_LOG(DbgHlpr::concat("internal load complete, pixbuf=", (void*)rvo.pixbuf).c_str(), NULL);
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
GdkPixbuf*  _gpxbf_load(FILE* f_, GError** err_)
{
    GdkPixbuf*  pixbuf = NULL;

    DBG_LOG("starting FILE load", NULL);

    try
    {
	std::string  mimeType;
	ImgFactory::Buf  prevwbuf;
	ImgFactory::instance().create(f_, prevwbuf, mimeType);

	const Exiv2GdkPxBufLdr::PixbufLdrBuf&&  plb = Exiv2GdkPxBufLdr::_createPixbuf(prevwbuf, mimeType, err_);
	pixbuf = plb.pixbuf;
    }
    catch (const std::exception& ex)
    {
        printf("%s, line %ld - %s\n", __FILE__, __LINE__, ex.what());
    }
    catch (...) 
    {
        printf("%s, line %ld - unknown exception\n", __FILE__, __LINE__);
    }
    return pixbuf;
}

// hmmm, never called even if we assign in vfunc_fill()
static
GdkPixbuf* _gpxbf_load_xpm_data(const char **data)
{
    printf("%s, line %ld - load xpm data\n", __FILE__, __LINE__);
    return NULL;
}

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

#ifndef NDEBUG
    DBG_LOG("begin load", NULL);
    const mode_t  umsk = umask(0);
    umask(umsk);
    if ( (ctx->fd = open("exiv2_pixbuf_incrload.dat", O_CREAT | O_TRUNC | O_WRONLY, umsk | 0666)) < 0) {
        printf("failed to create dump file - %s\n", strerror(errno));
        if (*error_ != NULL) {
            g_set_error_literal(error_, 0, errno, "failed to create dump file");
        }
    }
#endif

    return (gpointer)ctx;
}


static 
gboolean _gpxbuf_lincr(gpointer ctx_,
                       const guchar* buf_, guint size_,
                       GError **error_)
{
    DBG_LOG(DbgHlpr::concat("incr load, bytes=", size_).c_str(), NULL);
    Exiv2PxbufCtx*  ctx = (Exiv2PxbufCtx*)ctx_;
    g_byte_array_append(ctx->data, buf_, size_);
    return TRUE;
}

static 
gboolean _gpxbuf_sload(gpointer ctx_, GError **error_)
{
    Exiv2PxbufCtx*  ctx = (Exiv2PxbufCtx*)ctx_;
    gboolean result = FALSE;

    DBG_LOG(DbgHlpr::concat("starting buf _gpxbuf_sload, len=", ctx->data->len).c_str(), NULL);

    try
    {
	std::string  mimeType;
	ImgFactory::Buf  prevwbuf;
	ImgFactory::instance().create((unsigned char*)ctx->data->data, (ssize_t)ctx->data->len, prevwbuf, mimeType);

	const Exiv2GdkPxBufLdr::PixbufLdrBuf&&  plb = Exiv2GdkPxBufLdr::_createPixbuf(prevwbuf, mimeType, error_);
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
        printf("%s, line %ld - %s\n", __FILE__, __LINE__, ex.what());
    }
    catch (...) 
    {
        printf("%s, line %ld - unknown exception\n", __FILE__, __LINE__);
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
        { "MM \x2a", "  z ", 80 }, /* TIFF */
        { "II\x2a \x10   CR\x02 ", "   z zzz   z", 100 }, /* CR2 */
        { "II\x2a ", "   z", 80 }, /* TIFF */
        { "II\x1a   HEAPCCDR", "   zzz        ", 100 }, /* CRW */
        { NULL, NULL, 0 }
    };
        
    static const gchar *mime_types[] = {
        "image/x-canon-cr2",
        "image/x-canon-crw",
        "image/x-nikon-nef",
        NULL
    };
        
    static const gchar *extensions[] = {
        "cr2",
        "crw",
        "nef",
        NULL
    };
        
    info->name        = (char*)"exiv2 RAW preview loader";
    info->signature   = signature;
    info->description = (char*)"RAW preview image loader (via exiv2)";
    info->mime_types  = (gchar**)mime_types;
    info->extensions  = (gchar**)extensions;
    info->flags       = GDK_PIXBUF_FORMAT_THREADSAFE;
    info->license     = (char*)"LGPL";
}
}
}
