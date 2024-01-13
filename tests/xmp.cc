#ifdef HAVE_CONFIG
#include <config.h>
#endif

#include <string>
#include <iostream>
#include <iomanip>
#include <list>
#include <utility>
#include <functional>

#include <limits.h>
#include <stdlib.h>

#include <exiv2/exiv2.hpp>
#include <Magick++.h>

#include "ExifProxy.h"


const char* const  key = "Xmp.xmp.Rating";


std::ostream&  operator<<(std::ostream& os_, const Exiv2::Image::UniquePtr& obj_)
{
    try {
	obj_->readMetadata();
	Exiv2::XmpData&  xmpData = obj_->xmpData();
	Exiv2::XmpData::iterator  kpos = xmpData.findKey(Exiv2::XmpKey(key));

	os_ << key << ": " << (kpos == xmpData.end() ? "<na>" : kpos->toString());
    }
    catch (const std::exception& ex)
    {
        os_ << "err: " << ex.what();
    }
    return os_;
}


int main(int argc, char* argv[])
{
    ExifProxy  ep("dummy");

    auto fFlipNoChange = [](ExifProxy& ep_) {
	if ( !(ep_.fliprating() && ep_.fliprating()) ) {
	    std::cerr << "failed to flip rating (no change)\n";
	}
    };

    auto fFlipChange = [](ExifProxy& ep_) {
	if ( !ep_.fliprating()) {
	    std::cerr << "failed to flip rating (single change)\n";
	}
    };

    auto fUnsetRating = [](ExifProxy& ep_) {
	if ( !(ep_.unsetRating() && ep_.syncRating()) ) {
	    std::cerr << "failed to unset rating\n";
	}

    };

    auto fCycleRating = [](ExifProxy& ep_) {
	if ( ! (ep_.cycleRating() && ep_.cycleRating() && ep_.cycleRating() && ep_.syncRating()) ) {
	    std::cerr << "failed to cycle rating\n";
	}
    };


    std::list<std::pair<std::string, std::function<void(ExifProxy&)>>>  lf {
	std::make_pair("flip, no change",   fFlipNoChange),
	std::make_pair("flip, with change", fFlipChange),
	std::make_pair("unset rating", fUnsetRating),
	std::make_pair("cycle rating", fCycleRating)
    };


    std::for_each(lf.begin(), lf.end(), [&ep](auto& f_)
    {
	std::cout << "==== " << f_.first << '\n';
	for (int i=0; i<2; ++i)
	{
	    char  path[PATH_MAX] = { 0 };
	    strcpy(path, "/tmp/exiv2-rprp-xmp.XXXXXX.jpg");
	    mkstemps(path, 4);
	    try
	    {
		Magick::Image  magick("600x400", "blue");
		magick.write(path);

		Exiv2::Image::UniquePtr  img = Exiv2::ImageFactory::open(path);
		if (i == 0) {
		    img->readMetadata();
		    Exiv2::XmpData&  xmpData = img->xmpData();
		    xmpData[key] = 3;
		    img->writeMetadata();

		    // reopen for fresh state
		    img = Exiv2::ImageFactory::open(path);
		}

		std::cout << "ExifProxy: before - " << path << ":" << img << '\n';
		ep.ref(path);

		f_.second(ep);

		std::cout << "ExifProxy: after  - " << path << ":" << img << '\n';
	    }
	    catch (const std::exception& ex)
	    {
		std::cerr << "ExifProxy: " << ex.what() << '\n';
	    }
	}
	std::cout << '\n';
    });



    if (argc > 1)
    {
	const char*  filename = argv[1];

	std::for_each(lf.begin(), lf.end(), [&ep,&path=filename](auto& f_) {
	    std::cout << "==== " << f_.first << " on " << path << '\n';
	    try
	    {
		Exiv2::Image::UniquePtr  img = Exiv2::ImageFactory::open(path);
		img->readMetadata();

		std::cout << "ExifProxy: before - " << path << ":" << img << '\n';
		ep.ref(path);

		f_.second(ep);

		std::cout << "ExifProxy: after  - " << path << ":" << img << '\n';
	    }
	    catch (const std::exception& ex)
	    {
		std::cerr << "ExifProxy: " << ex.what() << '\n';
	    }
	    std::cout << '\n';
	});

	std::cout << "==== setting XMP value directly\n";
	try
	{
	    Exiv2::Image::UniquePtr  img = Exiv2::ImageFactory::open(filename);

	    img->readMetadata();
	    Exiv2::XmpData&  xmpData = img->xmpData();
	    std::cout << "before: " << xmpData << std::endl;

	    Exiv2::XmpTextValue  xmpval(argc == 3 ? argv[2] : "5");


	    bool  doit = true;
	    Exiv2::XmpData::iterator  kpos = xmpData.findKey(Exiv2::XmpKey(key));
	    if (kpos == xmpData.end()) {
		std::cout << filename << ": no such key" << std::endl;
	    }
	    else {
		std::cout << "current value=" << kpos->toString() << "  req'd value=" << xmpval.toString() << std::endl;
		if (kpos->toString() == xmpval.toString()) {
		    doit = false;
		}
		else {
		    xmpData.erase(kpos);
		}
	    }

	    if (doit) {
		xmpData[key] = xmpval;

		//img->setXmpData(xmpData);
		img->writeMetadata();
	    }
	    std::cout << "after: " << xmpData << std::endl;
	}
	catch(Exiv2::Error & e) {
	    std::cout << "Caught Exiv2 exception '" << e << "'\n";
	    return -1;
	}
    }

}
