#include <sys/types.h>
#include <string.h>
#include <strings.h>

#include <iostream>
#include <chrono>

#include <omp.h>

using namespace  std;



#ifdef USE_MAGICK_PLUSPLUS
#include <Magick++.h>
void  _magick(const char* file_)
{
    cout << "using Magick++" << endl;
    const char*  env;
	Magick::InitializeMagick("");
	cout << "  initializing.." << endl;

    cout << "Magick resources:"
	 << "  area=" << Magick::ResourceLimits::area() 
	 << "  disk=" << Magick::ResourceLimits::disk()
	 << "  map=" << Magick::ResourceLimits::map()
	 << "  memory=" << Magick::ResourceLimits::memory()
	 << "  threads=" << Magick::ResourceLimits::thread() << endl;



    Magick::Image  magick(file_ == NULL ? "6000x4000" : file_, "blue");
    magick.filterType(Magick::LanczosFilter);
    magick.quality(70);
    if ( (env = getenv("SET_MAGICK_RESOURCE_LIMIT")) ) {
	int  v = atol(env);
        std::cout << "SET_MAGICK_RESOURCE_LIMIT=" << env << "\n";
	switch (v) {
	  case -1:
	    break;
	  case 0:
	    Magick::ResourceLimits::thread(omp_get_num_procs());
	    break;
	  default:
	    Magick::ResourceLimits::thread(v);
	}
    }
    Magick::Geometry  g("x4288");
    {
	const std::chrono::time_point<std::chrono::system_clock>  start = std::chrono::system_clock::now();
	magick.scale(g);
	const std::chrono::duration<double>  elapsed = std::chrono::system_clock::now() - start;
	cout << "scale()  secs=" << elapsed.count() << " " << (env ? "WITH" : "without") << " OMP resource override" << endl;
    }
    {
	const std::chrono::time_point<std::chrono::system_clock>  start = std::chrono::system_clock::now();
	magick.resize(g);
	const std::chrono::duration<double>  elapsed = std::chrono::system_clock::now() - start;
	cout << "resize() secs=" << elapsed.count() << " " << (env ? "WITH" : "without") << " OMP resource override" << endl;
    }

    if ( (env = getenv("MAGICK_CLEANUP")) ) {
	cout << "  cleaning up" << endl;
	Magick::TerminateMagick();
    }

}
#else
#include <magick/MagickCore.h>
void  _magick(const char* file_)
{
    cout << "using MagickCore" << endl;

    // from https://github.com/ImageMagick/ImageMagick/blob/d06c8899b44a0592ff7b6f4ad14a595b8de2084a/www/source/core.c
    ExceptionInfo
	*exception;

    Image
	*image,
	*images,
	*resize_image;

    ImageInfo
	*image_info;

    /*
       Initialize the image info structure and read an image.
       */
    MagickCoreGenesis(NULL, MagickTrue);
    exception=AcquireExceptionInfo();
    image_info=CloneImageInfo((ImageInfo *) NULL);
    strcpy(image_info->filename, file_);
    images = ReadImage(image_info,exception);

    if (exception->severity != UndefinedException)
	CatchException(exception);

    if (images == (Image *) NULL) {
	cout << "no image" << endl;
	return;
    }

    while ((image=RemoveFirstImageFromList(&images)) != (Image *) NULL)
    {
	resize_image = ResizeImage(image,4288,80,LanczosFilter,1.0,exception);
	if (resize_image == (Image *) NULL)
	    MagickError(exception->severity,exception->reason,exception->description);
	DestroyImage(image);
    }
}
#endif


int main(int argc, char* const argv[])
{
    int  ret = 0;
    try
    {
	 cout << "OMP: cpus=" << omp_get_num_procs() << " threads=" << omp_get_max_threads() << endl;
	_magick(argc == 2 ? argv[1] : NULL);
    }
    catch (const std::exception& ex)
    {
	cerr << "failed to magick - " << ex.what() << endl;
	ret = 1;
    }

    return ret;
}
