#ifndef COLOR_PROFILES_H
#define COLOR_PROFILES_H

#include <cstddef>

namespace Exiv2GdkPxBufLdr {

// simple abstraction over known profiles
class ColorProfiles
{
  public:
    struct _Profile {
	const unsigned char* const data;
	const size_t  size;

	_Profile(const unsigned char data_[], const size_t size_) : data(data_), size(size_)
	{ }
    };

    ColorProfiles();
    ~ColorProfiles() = default;

    ColorProfiles(const ColorProfiles&) = default;
    ColorProfiles& operator=(const ColorProfiles&) = default;


    const _Profile&  srgb() const
    { return _srgb; }

    const _Profile&  NKsRGB() const
    { return _nknsrgb; }

    const _Profile&  NKaRGB() const
    { return _nknargb; }

  private:
    _Profile  _srgb;
    _Profile  _nknsrgb;
    _Profile  _nknargb;
};
}

#endif
