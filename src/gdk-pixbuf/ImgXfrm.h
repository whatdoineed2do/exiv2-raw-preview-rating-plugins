#ifndef IMG_XFRM_H
#define IMG_XFRM_H

#include <exiv2/exiv2.hpp>
#include <Magick++.h>


namespace  Exiv2GdkPxBufLdr
{

class Env;
class ColorProfiles;

class ImgXfrm {
  public:
    virtual ~ImgXfrm() = default;
    ImgXfrm(const ImgXfrm& rhs_) = default;
    ImgXfrm& operator=(const ImgXfrm&) = default;


    const Exiv2::PreviewImage&  preview;
    const Env&  env;
    Magick::Image&  magick;

    void  transform() const
    {
	if (!_valid()) {
	    return;
	}

	if (!magick.isValid()) {
	    magick.read( Magick::Blob(preview.pData(), preview.size()) );
	}
	_transform();
    }

  protected:
    ImgXfrm(const Exiv2::PreviewImage&  preview_, const Env&  env_, Magick::Image& magick_)
        : preview(preview_), env(env_), magick(magick_)
    { }

    virtual void  _transform() const = 0;

    // conditions for tranform op are met
    virtual bool  _valid() const { return true; }
};

struct ImgXfrmResize : public ImgXfrm
{
    ImgXfrmResize(const Exiv2::PreviewImage&  preview_, const Env&  env_, Magick::Image& magick_, unsigned previewIdx_)
	: ImgXfrm(preview_, env_, magick_), previewIdx(previewIdx_)
    { }

    const unsigned  previewIdx;  // which preivew this was
    
    void  _transform() const override;
    bool  _valid() const override;
};

struct ImgXfrmRotate : public ImgXfrm
{
    ImgXfrmRotate(const Exiv2::PreviewImage&  preview_, const Env&  env_, Magick::Image& magick_, const Exiv2::ExifData& exif_)
	: ImgXfrm(preview_, env_, magick_), exif(exif_), d(exif.findKey(Exiv2::ExifKey("Exif.Image.Orientation")))
    { }

    const Exiv2::ExifData&  exif;
    const Exiv2::ExifData::const_iterator  d;
    
    void  _transform() const override;
    bool  _valid() const override;
};

struct ImgXfrmsRGB : public ImgXfrm
{
    ImgXfrmsRGB(const Exiv2::PreviewImage&  preview_, const Env&  env_, Magick::Image& magick_, const Exiv2::ExifData& exif_, const Exiv2GdkPxBufLdr::ColorProfiles& profiles_, const Magick::Blob& srgbICC_, const Magick::Blob& argbICC_)
	: ImgXfrm(preview_, env_, magick_), exif(exif_), profiles(profiles_), srgbICC(srgbICC_), argbICC(argbICC_)
    { }

    const Exiv2::ExifData&  exif;
    const Exiv2GdkPxBufLdr::ColorProfiles&  profiles;
    const Magick::Blob&  srgbICC;
    const Magick::Blob&  argbICC;
    
    void  _transform() const override;
    bool  _valid() const override;
};

struct ImgXfrmAnnotate : public ImgXfrm
{
    ImgXfrmAnnotate(const Exiv2::PreviewImage&  preview_, const Env&  env_, Magick::Image& magick_, const Exiv2::ExifData&  exif_, unsigned height_, unsigned width_)
	: ImgXfrm(preview_, env_, magick_), exifdata(exif_), height(height_), width(width_)
    { }

    const Exiv2::ExifData&  exifdata;
    const unsigned  height;
    const unsigned  width;
    
    void  _transform() const override;
};

}

#endif
