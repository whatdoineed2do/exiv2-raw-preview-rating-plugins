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

#include <sstream>

#define GDK_PIXBUF_ENABLE_BACKEND
#include <gdk-pixbuf/gdk-pixbuf-io.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <exiv2/exiv2.hpp>
#include <Magick++.h>


extern "C" {
G_MODULE_EXPORT void fill_vtable (GdkPixbufModule *module);
G_MODULE_EXPORT void fill_info (GdkPixbufFormat *info);
}



class DbgHlpr
{
  public:
    static DbgHlpr&  instance()
    {
	if (DbgHlpr::_instance == NULL) {
	    DbgHlpr::_instance = new DbgHlpr();
	}
	return *DbgHlpr::_instance;
    }

    ~DbgHlpr() { close(_fd); }

    DbgHlpr(const DbgHlpr&) = delete;
    DbgHlpr& operator=(const DbgHlpr&) = delete;

    void  log(const char* _file_, const int _line_, const char* msg_, GError** err_)
    {
	std::ostringstream  os;
	os << _pid << ":  " << _file_ << ", " << _line_ << " :";
	if (msg_) {
	    os << "  " << msg_;
	}

	if (err_ && *err_) {
	    os << "  err=" << (*err_)->message;
	}
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

  private:
    DbgHlpr() : _fd(0), _pid(getpid())
    { 
	const mode_t  umsk = umask(0);
	umask(umsk);
	if ( (_fd = open("/tmp/exiv2_pixbuf_loader.log", O_CREAT | O_WRONLY | O_APPEND, umsk | 0666)) < 0) {
	    printf("failed to create debug log - %s\n", strerror(errno));
	}
	log(__FILE__, __LINE__, "starting", NULL);
    }

    static DbgHlpr*  _instance;

    const pid_t  _pid;
    int  _fd;
};
DbgHlpr*  DbgHlpr::_instance = NULL;

#ifndef NDEBUG
  #define DBG_LOG(info, err) \
    DbgHlpr::instance().log(__FILE__, __LINE__, info, err)
#else
  #define DBG_LOG(info, err) 
#endif


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


struct PrevwBuf
{
    PrevwBuf()  = default;
    ~PrevwBuf() = default;

    PrevwBuf(const PrevwBuf&)            = delete;
    PrevwBuf& operator=(const PrevwBuf&) = delete;

    struct {
	Exiv2::DataBuf  dbuf;
	Exiv2::MemIo    memio;
    } exiv2;

    struct {
	Magick::Blob    blob;
    } magick;
};


static void  _previewImage(Exiv2::PreviewManager&&  exvprldr_, PrevwBuf&  prevwBuf_, std::string& mimeType_, 
			   int width_, int height_, const Exiv2::ExifData& exif_)
{
    Exiv2::PreviewPropertiesList  list =  exvprldr_.getPreviewProperties();

    DBG_LOG(DbgHlpr::concat("#previews=", list.size()).c_str(), NULL);

    /* exiv2 provides images sorted from small->large -  grabbing the 
     * largest preview but try to avoid getting somethign too large due
     * a bug in cairo-xlib-surface-shm.c:619 mem pool exhausting bug
     *
     * if the prev img is larger than D300 megapxls (3840pxls on longest
     * edge) then try to either get another preview image or to scale it 
     * using Magick++
     */
    const char*  PREVIEW_LIMIT_ENV = "EXIV2_PIXBUF_LOADER_SCALE_LIMIT";
    unsigned short  PREVIEW_LIMIT = getenv(PREVIEW_LIMIT_ENV) == NULL ? 4288 : (unsigned short)atoi(getenv(PREVIEW_LIMIT_ENV));  // D300's 12mpxl limit
    Exiv2::PreviewPropertiesList::reverse_iterator  p = list.rbegin();
    Exiv2::PreviewPropertiesList::reverse_iterator  pp = list.rend();
    unsigned  i = 0;
    while (p != list.rend())
    {
	if (p->width_ > PREVIEW_LIMIT || p->height_ > PREVIEW_LIMIT) {
	    pp = p;
	}
	++p;
	++i;
    }
    if (pp == list.rend()) {
	pp = list.rbegin();
    }

    Exiv2::PreviewImage  preview =  exvprldr_.getPreviewImage(*pp);
    mimeType_ = preview.mimeType();

    const unsigned char*  pr = NULL;
    size_t  prsz = 0;

    if (pp->width_ > PREVIEW_LIMIT|| pp->height_ > PREVIEW_LIMIT)
    {
	DBG_LOG(DbgHlpr::concat("scaling preview=", i).c_str(), NULL);

	static const Exiv2::ExifKey  etags[] = {
	    Exiv2::ExifKey("Exif.Image.Model"),

	    Exiv2::ExifKey("Exif.Image.DateTime"), 

	    Exiv2::ExifKey("Exif.Photo.FocalLength"),
	    Exiv2::ExifKey("Exif.Photo.ExposureTime"), 
	    Exiv2::ExifKey("Exif.Photo.FNumber"), 
	    Exiv2::ExifKey("Exif.Photo.ISOSpeedRatings")
	};

	Magick::Image  magick( Magick::Blob(preview.pData(), preview.size()) );

	magick.filterType(Magick::LanczosFilter);
	magick.quality(70);
	char  tmp[15];
	snprintf(tmp, 14, "%ld", PREVIEW_LIMIT);
	magick.resize(Magick::Geometry(tmp));

	std::ostringstream  exif;
	for (int i=0; i<sizeof(etags)/sizeof(Exiv2::ExifKey); i++) {
	    Exiv2::ExifData::const_iterator  e = exif_.findKey(etags[i]);
	    if (e == exif_.end()) {
		exif << "[N/F:" << etags[i].key() << "]";
		continue;
	    }

	    exif << *e;
	    switch (i)
	    {
		case 0:
		{
		    exif << "  " << width_ << "x" << height_ << "\n";
		    Exiv2::ExifData::const_iterator  ln = lensName(exif_);
		    if (ln != exif_.end()) { 
			exif << ln->print(&exif_) << "\n";
		    }
		} break;

		case 1:
		    exif << '\n';
		    break;

		case 5:
		    exif << "ISO";
		default:
		    exif << "  ";
	    }
	}

	Magick::Image  info(Magick::Geometry(magick.columns(), magick.rows()), "grey");
        info.borderColor("grey");
        info.fontPointsize(18);
	info.annotate(exif.str(), Magick::Geometry("+10+10"), MagickCore::WestGravity);
        info.trim();
        info.border();
        info.opacity(65535/3.0);
	info.transparent("grey");

	if (info.columns() > magick.columns()-10) {
	    std::ostringstream  os;
            os << magick.columns()-10 << "x";
            info.resize(os.str());
        }

	magick.composite(info,
		         Magick::Geometry(info.columns(), info.rows(), 10, magick.rows()-info.rows()-10),
			 MagickCore::DissolveCompositeOp);

	magick.write(&prevwBuf_.magick.blob);

	prsz = prevwBuf_.magick.blob.length();
	pr = (const unsigned char*)prevwBuf_.magick.blob.data();
    }
    else
    {
	prevwBuf_.exiv2.dbuf = preview.copy();

	prsz = prevwBuf_.exiv2.dbuf.size_;
	pr = prevwBuf_.exiv2.dbuf.pData_;
    }
    
    Exiv2::Image::AutoPtr  upd = Exiv2::ImageFactory::open(pr, prsz);
    prevwBuf_.exiv2.memio.transfer(upd->io());
}


extern "C" {

static
GdkPixbuf*  _gpxbf_load(FILE* f_, GError** err_)
{
    GdkPixbuf*  pixbuf = NULL;

    DBG_LOG("starting load", NULL);

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
	std::string  mimeType;
	PrevwBuf  prevwbuf;
	{
	    Exiv2::Image::AutoPtr  orig = Exiv2::ImageFactory::open(buf, sz);
	    orig->readMetadata();
	    _previewImage(Exiv2::PreviewManager(*orig), prevwbuf, mimeType, orig->pixelWidth(), orig->pixelHeight(), orig->exifData());
	}

	Exiv2::BasicIo&  rawio = prevwbuf.exiv2.memio;
        rawio.seek(0, Exiv2::BasicIo::beg);

        // let the other better placed loaders deal with the RAW img
        // preview (tif/jpeg/png
        //
        GdkPixbufLoader*  gpxbldr = NULL;
        gpxbldr = gdk_pixbuf_loader_new_with_mime_type(mimeType.c_str(), err_);
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
    free(buf);
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

    DBG_LOG(DbgHlpr::concat("_gpxbuf_sload, len=", ctx->data->len).c_str(), NULL);

    try
    {
	std::string  mimeType;
	PrevwBuf  prevwbuf;
	{
	    Exiv2::Image::AutoPtr  orig = Exiv2::ImageFactory::open(ctx->data->data, ctx->data->len);
	    orig->readMetadata();
	    _previewImage(Exiv2::PreviewManager(*orig), prevwbuf, mimeType, orig->pixelWidth(), orig->pixelHeight(), orig->exifData());
	}

#if 0
        /* this doesnt work as EOG is/has already tried to read the EXIF from 
	 * the main image (ie the RAW file using libexif
	 */
        upd->setByteOrder(orig->byteOrder());
        upd->setExifData(orig->exifData());
        upd->setIptcData(orig->iptcData());
        upd->setXmpData(orig->xmpData());
        upd->writeMetadata();
#endif
	Exiv2::BasicIo&  rawio = prevwbuf.exiv2.memio;
        rawio.seek(0, Exiv2::BasicIo::beg);

        /* let the other better placed loaders deal with the RAW img
         * preview (tif/jpeg)
         */
#if 0
	DBG_LOG(DbgHlpr::concat("sload internal loader, buflen=", rawio.size()).c_str(), NULL);
        const mode_t  umsk = umask(0);
        umask(umsk);
        int  fd;
        if ( (fd = open("exiv2_pixbuf_incrload.preview.dat", O_CREAT | O_TRUNC | O_WRONLY | O_SYNC, umsk | 0666)) < 0) {
            printf("failed to create dump file - %s\n", strerror(errno));
        }
        else
        {
            write(fd, rawio.mmap(), rawio.size());
            close(fd);
        }
#endif
        GdkPixbufLoader*  gpxbldr = gdk_pixbuf_loader_new_with_mime_type(mimeType.c_str(), error_);
	DBG_LOG(DbgHlpr::concat("pixbuf loader=", (void*)gpxbldr).c_str(), error_);
	DBG_LOG(DbgHlpr::concat("prev mime type=", mimeType.c_str()).c_str(), NULL);
	DBG_LOG(DbgHlpr::concat("buf=", rawio.size()).c_str(), NULL);
        if (error_ && *error_) {
            g_error_free(*error_);
            *error_ = NULL;
            printf("no known loader for explicit mimetype, defaulting\n");
            if (gpxbldr != NULL) {
                gdk_pixbuf_loader_close(gpxbldr, NULL);
		g_object_unref(gpxbldr);
            }
            gpxbldr = gdk_pixbuf_loader_new();
        }

        if (gdk_pixbuf_loader_write(gpxbldr, rawio.mmap(), rawio.size(), error_) != TRUE)
	{
            printf("%s, line %ld - sload, internal loader failed, rawio buf=%ld size=%ld  err=%s\n", __FILE__, __LINE__, rawio.mmap(), rawio.size(), (error_ == NULL || error_ && *error_ == NULL ? "<>" : (*error_)->message));
	}
	else
        {
	    gdk_pixbuf_loader_close(gpxbldr, NULL);
            GdkPixbuf*  pixbuf = gdk_pixbuf_loader_get_pixbuf(gpxbldr);
	    DBG_LOG(DbgHlpr::concat("sload, internal load complete, pixbuf=", (void*)pixbuf).c_str(), NULL);

	    if (pixbuf == NULL) {
	    }
	    else
	    {
		if (ctx->prepared_func != NULL) {
		    (*ctx->prepared_func)(pixbuf, NULL, ctx->user_data);
		}
		if (ctx->updated_func != NULL) {
		    (*ctx->updated_func)(pixbuf, 0, 0, 
					 gdk_pixbuf_get_width(pixbuf), 
					 gdk_pixbuf_get_height(pixbuf),
					 ctx->user_data);
		}
		result = TRUE;
	    }
        }
        g_object_unref(gpxbldr);
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
