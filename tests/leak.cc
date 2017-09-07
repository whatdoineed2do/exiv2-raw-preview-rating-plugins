#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <strings.h>
#include <limits.h>
#include <libgen.h>
#include <limits.h>

#include <string>
#include <sstream>
#include <iostream>
#include <cassert>

using namespace  std;

#include <exiv2/exiv2.hpp>
#include <Magick++.h>

/*
    g++ -O3 -s -DHAVE_SAMPLE_ICC -I/usr/local/include \
      imgprextr.cc ICCprofiles.c \
	    $(/usr/local/bin/Magick++-config --cppflags --cxxflags --libs) \
	    -lexiv2 -lexpat -lz -lSampleICC \
      -o imgprextr.exe
*/

typedef unsigned char  uchar_t;
typedef unsigned int   uint_t;


class _Buf
{
  public:
    _Buf() : buf(NULL), sz(0), bufsz(0) { }
    _Buf(size_t sz_) : buf(NULL), sz(0), bufsz(0) { alloc(sz_); }

    ~_Buf() { free(); }

    uchar_t*  buf;
    size_t    bufsz;
    size_t    sz;

    void  alloc(size_t sz_)
    {
	if (sz_ > sz) {
	    delete [] buf;
	    sz = sz_;
	    bufsz = sz;
	    buf = new uchar_t[sz];
	}
	memset(buf, 0, sz);
    }

    void  free()
    {
	delete []  buf;
	buf = NULL;
	sz = 0;
	bufsz = 0;
    }

    const uchar_t*  copy(uchar_t* buf_, size_t sz_)
    {
	alloc(sz_);
	memcpy(buf, buf_, sz_);
	bufsz = sz_;
	return buf;
    }

    void  clear()
    {
	memset(buf, 0, sz);
    }

  private:
    _Buf(const _Buf&);
    void operator=(const _Buf&);
};


  #define DBG_LOG(info, err) 



static void  _previewImage(Exiv2::PreviewManager&  exvprldr_, Exiv2::DataBuf& dbuf_, Exiv2::BasicIo& bio_, std::string& mimeType_)
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
    unsigned short  PREVIEW_LIMIT = getenv("EXIV2_PIXBUF_LOADER_SCALE_LIMIT") == NULL ? 4288 : (unsigned short)atoi(getenv("EXIV2_PIXBUF_LOADER_SCALE_LIMIT"));  // D300's 12mpxl limit
    Exiv2::PreviewPropertiesList::reverse_iterator  p = list.rbegin();
    Exiv2::PreviewPropertiesList::reverse_iterator  pp = list.rend();
    unsigned  i = 0;
    while (p != list.rend())
    {
	cout << "preview #" << i++ << " w=" << p->width_ << " h=" << p->height_ << endl;
	if (p->width_ > PREVIEW_LIMIT || p->height_ > PREVIEW_LIMIT) {
	    pp = p;
	}
	++p;
    }
    if (pp == list.rend()) {
	pp = list.rbegin();
    }

    Exiv2::PreviewImage  preview =  exvprldr_.getPreviewImage(*pp);
    mimeType_ = preview.mimeType();

    Magick::Blob   mgkblob;
    if (pp->width_ > PREVIEW_LIMIT|| pp->height_ > PREVIEW_LIMIT)
    {
	cout << "scaling, limit=" << PREVIEW_LIMIT << " w=" << pp->width_ << " h=" << pp->height_ << endl;
	Magick::Image  magick( Magick::Blob(preview.pData(), preview.size()) );

	magick.filterType(Magick::LanczosFilter);
	magick.quality(70);
	char  tmp[5];
	sprintf(tmp, "%ld", PREVIEW_LIMIT);
	magick.resize(Magick::Geometry(tmp));

	magick.write(&mgkblob);
    }
    else
    {
	dbuf_ = preview.copy();
    }
    
    Exiv2::Image::AutoPtr  upd = Exiv2::ImageFactory::open(
				    mgkblob.length() > 0 ? (const unsigned char*)mgkblob.data() : dbuf_.pData_, 
				    mgkblob.length() > 0 ? mgkblob.length() : dbuf_.size_ );
    bio_.transfer(upd->io());
}


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
    unsigned short  PREVIEW_LIMIT = getenv("EXIV2_PIXBUF_LOADER_SCALE_LIMIT") == NULL ? 4288 : (unsigned short)atoi(getenv("EXIV2_PIXBUF_LOADER_SCALE_LIMIT"));  // D300's 12mpxl limit
    Exiv2::PreviewPropertiesList::reverse_iterator  p = list.rbegin();
    Exiv2::PreviewPropertiesList::reverse_iterator  pp = list.rend();
    unsigned  i = 0;
    while (p != list.rend())
    {
	cout << "preview #" << i++ << " w=" << p->width_ << " h=" << p->height_ << endl;
	if (p->width_ > PREVIEW_LIMIT || p->height_ > PREVIEW_LIMIT) {
	    pp = p;
	}
	++p;
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
	// annote
	static const Exiv2::ExifKey  etags[] = {
	    Exiv2::ExifKey("Exif.Image.Model"),
	    Exiv2::ExifKey("Exif.Image.DateTime"), 
	    Exiv2::ExifKey("Exif.Photo.ExposureTime"), 
	    Exiv2::ExifKey("Exif.Photo.FNumber"), 
	    Exiv2::ExifKey("Exif.Photo.ISOSpeedRatings")
	};

	cout << "scaling, limit=" << PREVIEW_LIMIT << " w=" << pp->width_ << " h=" << pp->height_ << endl;

	Magick::Image  magick( Magick::Blob(preview.pData(), preview.size()) );

	magick.filterType(Magick::LanczosFilter);
	magick.quality(70);
	char  tmp[15];
	snprintf(tmp, 14, "%ld", PREVIEW_LIMIT);
	magick.resize(Magick::Geometry(tmp));

	std::ostringstream  exif;
	for (int i=0; i<5; i++) {
	    Exiv2::ExifData::const_iterator  e = exif_.findKey(etags[i]);
	    if (e == exif_.end()) {
		continue;
	    }
	    exif << *e << " ";
	}
	exif << width_ << "x" << height_;

	Magick::Image  info("600x30", "grey");
	//info.font("@Arial.ttf");
	//info.matte(true);
	//info.channel(MagickCore::OpacityChannel);
	//info.colorFuzz(MaxRGB*0.5);
	//info.opaque("black", "grey");

	info.fontPointsize(18);
	info.annotate(exif.str(), Magick::Geometry("+10+10"), MagickCore::WestGravity);
	info.opacity(65535/3.0);
	info.transparent("grey");

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





int main(int argc, char* const argv[])
{
    const char*  argv0 = basename(argv[0]);
    char c;
    mode_t  msk = umask(0);
    umask(msk);


    errno = 0;
    int  a = 1;
    while (a < argc)
    {
	const char* const  filename = argv[a++];

	if (false)  // if scaling occurs, the magick blob needs to exist otherwise the rawio access is invalid
	{
	    try
	    {
		string  mimeType;
		Exiv2::DataBuf dbuf;
		Exiv2::MemIo  rawio;
		{
		    Exiv2::Image::AutoPtr  orig = Exiv2::ImageFactory::open(filename);
		    orig->readMetadata();

		    Exiv2::PreviewManager loader(*orig);
		    _previewImage(loader, dbuf, rawio, mimeType);
		}

		cout << filename << ": " << mimeType << " " << rawio.size() << endl;

		rawio.seek(0, Exiv2::BasicIo::beg);

		unsigned char*  buf = new unsigned char[rawio.size()];
		memcpy(buf, rawio.mmap(), rawio.size());
		//rawio.munmap();
		delete []  buf;
	    }
	    catch (const Exiv2::AnyError& e)
	    {
		cout << filename << ":  unable to extract preview/reset exif - " << e << endl;
		continue;
	    }
	}


	{
	    try
	    {
		string  mimeType;
		PrevwBuf  prevwbuf;
		{
		    static const string  mimeNEF = "image/x-nikon-nef";

		    Exiv2::Image::AutoPtr  orig = Exiv2::ImageFactory::open(filename);
		    orig->readMetadata();
		    cout << filename << ": x=" << orig->pixelWidth() << " y=" << orig->pixelHeight() << "  " << orig->mimeType() << endl;

		    const Exiv2::ExifData  exif = orig->exifData();

		    Exiv2::ExifData::const_iterator  ln = exif.end();
		    if (false && orig->mimeType() == mimeNEF)
		    {
			static const char*  keys[] = { 
			    "Exif.NikonLd1.LensIDNumber",
			    "Exif.NikonLd2.LensIDNumber",
			    "Exif.NikonLd3.LensIDNumber",
			    NULL
			};

			const char**  pp = keys;
			while (*pp) {
			    ++pp;
			}
		    }
		    else {
			ln = Exiv2::lensName(exif);
		    }

		    if (ln != exif.end()) {
			cout << *ln << " / " << ln->print(&exif) << endl;
		    }

		    _previewImage(Exiv2::PreviewManager(*orig), prevwbuf, mimeType, orig->pixelWidth(), orig->pixelHeight(), orig->exifData());
		}

		cout << filename << ": " << mimeType << " " << prevwbuf.exiv2.memio.size() << endl;

		prevwbuf.exiv2.memio.seek(0, Exiv2::BasicIo::beg);

		//unsigned char*  buf = new unsigned char[prevwbuf.exiv2.memio.size()];
		//memcpy(buf, prevwbuf.exiv2.memio.mmap(), prevwbuf.exiv2.memio.size());
		//prevwbuf.exiv2.memio.munmap(); -- this is an apparent error in valgrind


		char  path[PATH_MAX];
		sprintf(path, "%s-%ld-%02ld.dat", argv0, getpid(), a-1);

		int  fd;
		if ( (fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, msk | 0666)) < 0) {
		    printf("failed to create gen'd file, %s - %s\n", path, strerror(errno));
		}
		else
		{
		    write(fd, prevwbuf.exiv2.memio.mmap(), prevwbuf.exiv2.memio.size());
		    close(fd);
		}

		//delete []  buf;
	    }
	    catch (const Exiv2::AnyError& e)
	    {
		cout << filename << ":  unable to extract preview/reset exif - " << e << endl;
		continue;
	    }
	}
    }

    return 0;
}
