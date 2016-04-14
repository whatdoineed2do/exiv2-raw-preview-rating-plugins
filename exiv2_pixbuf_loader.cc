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


#define GDK_PIXBUF_ENABLE_BACKEND
#include <gdk-pixbuf/gdk-pixbuf-io.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <exiv2/exiv2.hpp>

extern "C" {
G_MODULE_EXPORT void fill_vtable (GdkPixbufModule *module);
G_MODULE_EXPORT void fill_info (GdkPixbufFormat *info);
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

    printf("%s, line %ld - starting load\n", __FILE__, __LINE__);

    long  where = ftell(f_);
    fseek(f_, 0, SEEK_END);
    long sz = ftell(f_);

    fseek(f_, 0, SEEK_SET);
    char*  buf = (char*)malloc(sz);
    memset(buf, 0, sz);
    if ( (fread (buf, 1, sz, f_)) != sz) {
    }
    fseek(f_, where, SEEK_SET);

    try
    {
        Exiv2::Image::AutoPtr  orig = Exiv2::ImageFactory::open(buf, sz);
        orig->readMetadata();

        Exiv2::PreviewManager  exvprldr(*orig);
        Exiv2::PreviewPropertiesList  list =  exvprldr.getPreviewProperties();

        // grabbing the largest preview
        Exiv2::PreviewPropertiesList::iterator prevp = list.begin();
        if (prevp != list.end()) {
            advance(prevp, list.size()-1);
        }
        Exiv2::PreviewImage  preview =  exvprldr.getPreviewImage(*prevp);
        Exiv2::Image::AutoPtr  upd = Exiv2::ImageFactory::open( preview.pData(), preview.size() );
        Exiv2::BasicIo&  rawio = upd->io();
        rawio.seek(0, Exiv2::BasicIo::beg);
        rawio.mmap(), rawio.size();

        // let the other better placed loaders deal with the RAW img
        // preview (tif/jpeg/png
        //
        GdkPixbufLoader*  gpxbldr = NULL;
        
        gpxbldr = gdk_pixbuf_loader_new_with_mime_type(preview.mimeType().c_str(), err_);
        if (err_ && *err_) {
            g_error_free(*err_);
            *err_ = NULL;

            gpxbldr = gdk_pixbuf_loader_new();
        }
        gdk_pixbuf_loader_write(gpxbldr, rawio.mmap(), rawio.size(), NULL);
        pixbuf = gdk_pixbuf_loader_get_pixbuf (gpxbldr);
        gdk_pixbuf_loader_close(gpxbldr, NULL);
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

    #if 0
    struct Exiv2PxbufCtx*  ctx = (Exiv2PxbufCtx*)malloc(sizeof(struct Exiv2PxbufCtx));
    memset(ctx, 0, sizeof(struct Exiv2PxbufCtx));
    #else
    struct Exiv2PxbufCtx*  ctx = g_new0(struct Exiv2PxbufCtx, 1);
    #endif

    ctx->size_func     = szfunc_;
    ctx->prepared_func = prepfunc_;
    ctx->updated_func  = updfunc_;
    ctx->user_data     = udata_;
    ctx->data = g_byte_array_new();

#ifndef NDEBUG
    printf("%s, line %ld - begin load\n", __FILE__, __LINE__);
    const mode_t  umsk = umask(0);
    umask(umsk);
    if ( (ctx->fd = open("exiv2_pixbuf_incrload.dat", O_CREAT | O_TRUNC | O_WRONLY, umsk | 0666)) < 0) {
        printf("failed to create dump file - %s\n", strerror(errno));
        if (*error_ != NULL) {
            g_set_error_literal(error_, 0, errno, "failed to create dump file");
        }
        return NULL;
    }
#endif

    return (gpointer)ctx;
}


static 
gboolean _gpxbuf_lincr(gpointer ctx_,
                       const guchar* buf_, guint size_,
                       GError **error_)
{
#ifndef NDEBUG
    printf("%s, line %ld - incr load, %ld bytes\n", __FILE__, __LINE__, size_);
#endif
    Exiv2PxbufCtx*  ctx = (Exiv2PxbufCtx*)ctx_;
    g_byte_array_append(ctx->data, buf_, size_);
    return TRUE;
}

static 
gboolean _gpxbuf_sload(gpointer ctx_, GError **error_)
{
    Exiv2PxbufCtx*  ctx = (Exiv2PxbufCtx*)ctx_;
    gboolean result = FALSE;

#ifndef NDEBUG
    printf("%s, line %ld - stop load, data len=%ld\n", __FILE__, __LINE__, ctx->data->len);
    write(ctx->fd, ctx->data->data, ctx->data->len);
    close(ctx->fd);
#endif

    try
    {
        Exiv2::Image::AutoPtr  orig = Exiv2::ImageFactory::open(ctx->data->data, ctx->data->len);
        orig->readMetadata();

        Exiv2::PreviewManager  exvprldr(*orig);
        Exiv2::PreviewPropertiesList  list =  exvprldr.getPreviewProperties();

        /* exiv2 provides images sorted from small->large -  grabbing the 
         * largest preview
         */
        Exiv2::PreviewPropertiesList::iterator prevp = list.begin();
        if (prevp != list.end()) {
            advance(prevp, list.size()-1);
        }
        Exiv2::PreviewImage  preview =  exvprldr.getPreviewImage(*prevp);
        Exiv2::Image::AutoPtr  upd = Exiv2::ImageFactory::open( preview.pData(), preview.size() );

#if 0
        // hmm, doesnt seem to be read by the other things
        upd->setByteOrder(orig->byteOrder());
        upd->setExifData(orig->exifData());
        upd->setIptcData(orig->iptcData());
        upd->setXmpData(orig->xmpData());
        upd->writeMetadata();
#endif

        Exiv2::BasicIo&  rawio = upd->io();
        rawio.seek(0, Exiv2::BasicIo::beg);

        /* let the other better placed loaders deal with the RAW img
         * preview (tif/jpeg)
         */
#ifndef NDEBUG
        printf("%s, line %ld - sload, internal loader with buf=%ld\n", __FILE__, __LINE__, rawio.size());
        const mode_t  umsk = umask(0);
        umask(umsk);
        int  fd;
        if ( (fd = open("exiv2_pixbuf_incrload.preview.dat", O_CREAT | O_TRUNC | O_WRONLY, umsk | 0666)) < 0) {
            printf("failed to create dump file - %s\n", strerror(errno));
        }
        else
        {
            write(fd, rawio.mmap(), rawio.size());
            close(fd);
        }
#endif
        GdkPixbufLoader*  gpxbldr = gdk_pixbuf_loader_new_with_mime_type(preview.mimeType().c_str(), error_);
        if (error_ && *error_) {
            g_error_free(*error_);
            *error_ = NULL;
            printf("no known loader for explicit mimetype, defaulting\n");
            if (gpxbldr != NULL) {
                gdk_pixbuf_loader_close(gpxbldr, NULL);
            }
            gpxbldr = gdk_pixbuf_loader_new();
        }

        if (gdk_pixbuf_loader_write(gpxbldr, rawio.mmap(), rawio.size(), error_) == TRUE)
        {
            GdkPixbuf*  pixbuf = gdk_pixbuf_loader_get_pixbuf(gpxbldr);

#ifndef NDEBUG
            printf("%s, line %ld - sload, internal loader done, pixbuf=%ld\n", __FILE__, __LINE__, pixbuf);
#endif

            if (ctx->prepared_func != NULL) {
                //printf("calling prepared func\n");
                (*ctx->prepared_func)(pixbuf, NULL, ctx->user_data);
            }
            if (ctx->updated_func != NULL) {
                //printf("calling update func\n");
                (*ctx->updated_func)(pixbuf, 0, 0, 
                                     gdk_pixbuf_get_width(pixbuf), 
                                     gdk_pixbuf_get_height(pixbuf),
                                     ctx->user_data);
            }
            result = TRUE;
        }
        gdk_pixbuf_loader_close(gpxbldr, NULL);
        g_object_unref(gpxbldr);
        rawio.munmap();
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
        
    static gchar *mime_types[] = {
        "image/x-canon-cr2",
        "image/x-canon-crw",
        "image/x-nikon-nef",
        NULL
    };
        
    static gchar *extensions[] = {
        "cr2",
        "crw",
        "nef",
        NULL
    };
        
    info->name        = (char*)"exiv2 RAW preview loader";
    info->signature   = signature;
    info->description = (char*)"RAW preview image loader (via exiv2)";
    info->mime_types  = mime_types;
    info->extensions  = extensions;
    info->flags       = GDK_PIXBUF_FORMAT_THREADSAFE;
    info->license     = (char*)"LGPL";
}
}
