#include "pch.h"
#include "Imaging/ProgressiveJpegExample.h"
#include <wincodec.h>
#include <wil/com.h>
#include <wil/result.h>

namespace jpg_spinner::presentation
{
std::array<ProgressiveJpegExampleFrame, 3> decodeProgressiveJpegExample(const std::filesystem::path &path)
{
    const auto factory = wil::CoCreateInstance<IWICImagingFactory>(CLSID_WICImagingFactory);
    wil::com_ptr<IWICBitmapDecoder> decoder;
    THROW_IF_FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                       WICDecodeMetadataCacheOnDemand, decoder.put()));
    wil::com_ptr<IWICBitmapFrameDecode> frame;
    THROW_IF_FAILED(decoder->GetFrame(0, frame.put()));
    const auto progressive = frame.query<IWICProgressiveLevelControl>();
    UINT levelCount{};
    // The full packaged file is already local. GetLevelCount cannot delay a
    // network download here; WIC itself determines the available native levels.
    THROW_IF_FAILED(progressive->GetLevelCount(&levelCount));
    if (levelCount < 3 || levelCount > 100)
        THROW_HR(E_INVALIDARG);
    UINT width{}, height{};
    THROW_IF_FAILED(frame->GetSize(&width, &height));
    if (width == 0 || height == 0 || static_cast<std::uint64_t>(width) * height > 1'048'576)
        THROW_HR(E_INVALIDARG);
    const UINT stride = width * 4;
    const UINT byteCount = stride * height; // The pixel-count bound proves this fits UINT.

    UINT contextCount{};
    THROW_IF_FAILED(frame->GetColorContexts(0, nullptr, &contextCount));
    wil::com_ptr<IWICColorContext> sourceContext;
    wil::com_ptr<IWICColorContext> destinationContext;
    if (contextCount != 0)
    {
        // Use the embedded source profile and a native sRGB destination, as the
        // original example did. WIC owns profile interpretation and conversion.
        THROW_IF_FAILED(factory->CreateColorContext(sourceContext.put()));
        auto *context = sourceContext.get();
        UINT retrieved{};
        THROW_IF_FAILED(frame->GetColorContexts(1, &context, &retrieved));
        THROW_IF_FAILED(factory->CreateColorContext(destinationContext.put()));
        THROW_IF_FAILED(destinationContext->InitializeFromExifColorSpace(1));
    }
    const std::array<UINT, 3> levels{0, (levelCount - 1) / 2, levelCount - 1};
    std::array<ProgressiveJpegExampleFrame, 3> result;
    for (std::size_t index = 0; index < levels.size(); ++index)
    {
        THROW_IF_FAILED(progressive->SetCurrentLevel(levels[index]));
        auto &output = result[index];
        output = {width, height, levels[index], levelCount, std::vector<std::uint8_t>(byteCount)};
        // Recreate the conversion node per level so an earlier decoded buffer
        // cannot be reused as if it represented the newly selected WIC level.
        if (sourceContext)
        {
            wil::com_ptr<IWICColorTransform> transform;
            THROW_IF_FAILED(factory->CreateColorTransformer(transform.put()));
            THROW_IF_FAILED(transform->Initialize(frame.get(), sourceContext.get(), destinationContext.get(),
                                                  GUID_WICPixelFormat32bppPBGRA));
            THROW_IF_FAILED(transform->CopyPixels(nullptr, stride, byteCount, output.premultipliedBgraPixels.data()));
        }
        else
        {
            wil::com_ptr<IWICFormatConverter> converter;
            THROW_IF_FAILED(factory->CreateFormatConverter(converter.put()));
            THROW_IF_FAILED(converter->Initialize(frame.get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
                                                  nullptr, 0, WICBitmapPaletteTypeCustom));
            THROW_IF_FAILED(converter->CopyPixels(nullptr, stride, byteCount, output.premultipliedBgraPixels.data()));
        }
    }
    return result;
}
} // namespace jpg_spinner::presentation
