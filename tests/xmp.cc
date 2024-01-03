#include <string>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cmath>

#include <exiv2/exiv2.hpp>

bool isEqual(float a, float b)
{
    double d = std::fabs(a - b);
    return d < 0.00001;
}

using namespace std;

ostream&  operator<<(ostream& os_, const Exiv2::XmpData& data_) 
{
    for (Exiv2::XmpData::const_iterator md = data_.begin();
         md != data_.end(); ++md) 
    {
        os_ << std::setfill(' ') << std::left << std::setw(44)
            << md->key() << " "
            << std::setw(9) << std::setfill(' ') << std::left
            << md->typeName() << " " << std::dec << std::setw(3)
            << std::setfill(' ') << std::right
            << md->count() << "  " << std::dec << md->
            value() << std::endl;
    }

    return os_;
}

int main(int argc, char* argv[])
{
    const char* const  key = "Xmp.xmp.Rating";

    if (argc == 1) {
        cerr << "no input" << endl;
        return 1;
    }

    const char*  filename = argv[1];
    try
    {
        Exiv2::Image::UniquePtr  img = Exiv2::ImageFactory::open(filename);

        img->readMetadata();
	Exiv2::XmpData&  xmpData = img->xmpData();
        cout << "before:\n" << xmpData << endl;

	Exiv2::XmpTextValue  xmpval(argc == 3 ? argv[2] : "5");


        bool  doit = true;
	Exiv2::XmpData::iterator  kpos = xmpData.findKey(Exiv2::XmpKey(key));
	if (kpos == xmpData.end()) {
            cout << filename << ": no such key" << endl;
        }
        else {
            cout << "current value=" << kpos->toString() << "  req'd value=" << xmpval.toString() << endl;
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
        cout << "after:\n" << xmpData << endl;
    }
    catch(Exiv2::Error & e) {
	std::cout << "Caught Exiv2 exception '" << e << "'\n";
	return -1;
    }
}
