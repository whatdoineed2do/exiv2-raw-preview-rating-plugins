#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <strings.h>
#include <limits.h>

#include <string>
#include <sstream>
#include <iostream>
#include <cassert>

using namespace  std;

#include <Magick++.h>



int main(int argc, char* const argv[])
{
    int  ret = 0;
    try
    {
	Magick::Image  magick(argc == 2 ? argv[1] : "600x400", "blue");

	Magick::Image  info(Magick::Geometry(magick.columns(), magick.rows()), "grey");
	info.borderColor("grey");
	info.fontPointsize(18);
	ostringstream  os;
	os << "text\nwith\nnewline breaks";
	info.annotate(os.str(), Magick::Geometry("+10+10"), MagickCore::NorthEastGravity);
	//info.annotate("hello world\nthis is some text", Magick::Geometry("+10+10"), MagickCore::SouthWestGravity);
	info.trim();
	info.border();
        info.opacity(65535/3.0);
        info.transparent("grey");

	cout << "composite size=" << info.columns() << "x" << info.rows() << endl;

	if (info.columns() > magick.columns()-10) {
	    ostringstream  os;
	    os << magick.columns()-10 << "x";
	    info.resize(os.str());
	}


	magick.composite(info,
			    Magick::Geometry(info.columns(), info.rows(), 10, magick.rows()-info.rows()-10),
			    MagickCore::DissolveCompositeOp);

	magick.write("magick-label-composite.jpg");
    }
    catch (const std::exception& ex)
    {
	cerr << "failed to magick - " << ex.what() << endl;
	ret = 1;
    }

    return ret;
}
