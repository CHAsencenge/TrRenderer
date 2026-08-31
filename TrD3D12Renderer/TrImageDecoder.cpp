#include "TrImageDecoder.h"

#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <limits>
#include <stdexcept>
#include <string>

namespace
{
    void CheckResult(HRESULT result, const char* operation)
    {
        if(FAILED(result))
        {
            throw std::runtime_error(
                std::string(operation) + " failed with HRESULT " +
                std::to_string(static_cast<unsigned long>(result)) + ".");
        }
    }

    class ComApartment
    {
    public:
        ComApartment()
        {
            const HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if(SUCCEEDED(result))
            {
                mMustUninitialize = true;
            }
            else if(result != RPC_E_CHANGED_MODE)
            {
                CheckResult(result, "CoInitializeEx");
            }
        }

        ~ComApartment()
        {
            if(mMustUninitialize)
            {
                CoUninitialize();
            }
        }

    private:
        bool mMustUninitialize = false;
    };
}

TrDecodedImageRgba8 TrImageDecoder::DecodeRgba8(
    const std::vector<std::uint8_t>& encodedImage)
{
    if(encodedImage.empty() ||
       encodedImage.size() > std::numeric_limits<DWORD>::max())
    {
        throw std::invalid_argument("Encoded image payload is empty or too large.");
    }

    const ComApartment comApartment;
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    CheckResult(CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory)),
        "Creating the WIC imaging factory");

    Microsoft::WRL::ComPtr<IWICStream> stream;
    CheckResult(factory->CreateStream(&stream), "Creating the WIC stream");
    CheckResult(stream->InitializeFromMemory(
        const_cast<BYTE*>(encodedImage.data()),
        static_cast<DWORD>(encodedImage.size())),
        "Initializing the WIC stream");

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    CheckResult(factory->CreateDecoderFromStream(
        stream.Get(),
        nullptr,
        WICDecodeMetadataCacheOnDemand,
        &decoder),
        "Creating the WIC decoder");

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    CheckResult(decoder->GetFrame(0, &frame), "Reading the first image frame");

    UINT width = 0;
    UINT height = 0;
    CheckResult(frame->GetSize(&width, &height), "Reading image dimensions");
    if(width == 0 || height == 0 || width > std::numeric_limits<UINT>::max() / 4)
    {
        throw std::runtime_error("Decoded image dimensions are invalid.");
    }
    const UINT rowPitch = width * 4;
    if(height > std::numeric_limits<UINT>::max() / rowPitch)
    {
        throw std::overflow_error("Decoded image exceeds the WIC copy size limit.");
    }
    const UINT byteSize = rowPitch * height;

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    CheckResult(factory->CreateFormatConverter(&converter), "Creating the WIC format converter");
    CheckResult(converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom),
        "Converting the image to RGBA8");

    TrDecodedImageRgba8 result;
    result.Width = width;
    result.Height = height;
    result.Pixels.resize(byteSize);
    CheckResult(converter->CopyPixels(
        nullptr,
        rowPitch,
        byteSize,
        result.Pixels.data()),
        "Copying decoded image pixels");
    return result;
}
