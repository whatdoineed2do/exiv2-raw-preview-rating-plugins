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

    Magick::Image  magick(file_ == NULL ? "6000x4000" : file_, "blue");
    magick.filterType(Magick::LanczosFilter);
    magick.quality(70);
    const char*  env;
    if ( (env = getenv("SET_MAGICK_RESOURCE_LIMIT")) ) {
	Magick::ResourceLimits::thread(4);
    }
    const std::chrono::time_point<std::chrono::system_clock>  start = std::chrono::system_clock::now();
    magick.resize(Magick::Geometry("x4288"));
    const std::chrono::duration<double>  elapsed = std::chrono::system_clock::now() - start;
    cout << "secs=" << elapsed.count() << " " << (env ? "WITH" : "without") << " OMP resource override" << endl;

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
