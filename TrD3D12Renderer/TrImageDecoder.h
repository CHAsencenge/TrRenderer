#pragma once

#include <cstdint>
#include <vector>

struct TrDecodedImageRgba8
{
    std::uint32_t Width = 0;
    std::uint32_t Height = 0;
    std::vector<std::uint8_t> Pixels;
};

class TrImageDecoder
{
public:
    // Decodes an encoded PNG/JPEG/etc. payload through Windows Imaging
    // Component into tightly packed, unpremultiplied RGBA8 pixels.
    static TrDecodedImageRgba8 DecodeRgba8(
        const std::vector<std::uint8_t>& encodedImage);
};
