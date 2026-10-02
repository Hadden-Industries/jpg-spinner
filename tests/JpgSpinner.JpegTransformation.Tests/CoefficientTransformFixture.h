#pragma once

#include <array>
#include <cstddef>
#include <cstdio>
#include <jpeglib.h>
#include <csetjmp>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>
#include <turbojpeg.h>

namespace jpg_spinner::test_support
{
/// Independent public-libjpeg observation of quantized coefficients and tables.
/// This test-only representation never calls the production transform mapping.
struct ComponentCoefficients final
{
    unsigned int blockColumns{};
    unsigned int blockRows{};
    std::array<unsigned int, 64> quantization{};
    std::vector<std::array<short, 64>> blocks;
};

struct JpegCoefficients final
{
    int precision{};
    J_COLOR_SPACE colorSpace{};
    std::vector<ComponentCoefficients> components;
};

// Keep all state modified after setjmp on the heap. C/C++ makes modified
// non-volatile automatic locals indeterminate after longjmp. No nontrivial
// automatic object is constructed across a native call that can jump.
#pragma warning(push)
#pragma warning(disable : 4324)
struct CoefficientReaderState final
{
    jpeg_decompress_struct decoder{};
    jpeg_error_mgr errors{};
    std::jmp_buf failure{};
    bool created{};
    JpegCoefficients result;

    ~CoefficientReaderState()
    {
        // Also release the native allocation if a C++ container allocation
        // throws while copying the observed coefficient field.
        if (created)
            jpeg_destroy_decompress(&decoder);
    }
};
#pragma warning(pop)

inline JpegCoefficients readCoefficients(std::span<const std::byte> encoded)
{
    const auto state = std::make_unique<CoefficientReaderState>();
    state->decoder.err = jpeg_std_error(&state->errors);
    state->decoder.client_data = state.get();
    state->errors.error_exit = [](j_common_ptr codec) {
        auto *reader = static_cast<CoefficientReaderState *>(codec->client_data);
        std::longjmp(reader->failure, 1);
    };
#pragma warning(suppress : 4611)
    if (setjmp(state->failure) != 0)
    {
        throw std::runtime_error("Independent coefficient fixture reader failed.");
    }
    jpeg_create_decompress(&state->decoder);
    state->created = true;
    jpeg_mem_src(&state->decoder, reinterpret_cast<const unsigned char *>(encoded.data()),
                 static_cast<unsigned long>(encoded.size()));
    jpeg_read_header(&state->decoder, TRUE);
    const auto arrays = jpeg_read_coefficients(&state->decoder);
    state->result.precision = state->decoder.data_precision;
    state->result.colorSpace = state->decoder.jpeg_color_space;
    state->result.components.resize(static_cast<std::size_t>(state->decoder.num_components));
    for (int componentIndex = 0; componentIndex < state->decoder.num_components; ++componentIndex)
    {
        auto &output = state->result.components[static_cast<std::size_t>(componentIndex)];
        const auto &component = state->decoder.comp_info[componentIndex];
        output.blockColumns = component.width_in_blocks;
        output.blockRows = component.height_in_blocks;
        output.blocks.resize(static_cast<std::size_t>(output.blockColumns) * output.blockRows);
        for (std::size_t frequency = 0; frequency < 64; ++frequency)
            output.quantization[frequency] = state->decoder.quant_tbl_ptrs[component.quant_tbl_no]->quantval[frequency];
        for (unsigned int row = 0; row < output.blockRows; ++row)
        {
            const auto blocks = state->decoder.mem->access_virt_barray(reinterpret_cast<j_common_ptr>(&state->decoder),
                                                                       arrays[componentIndex], row, 1, FALSE);
            for (unsigned int column = 0; column < output.blockColumns; ++column)
                for (std::size_t frequency = 0; frequency < 64; ++frequency)
                    output.blocks[static_cast<std::size_t>(row) * output.blockColumns + column][frequency] =
                        blocks[0][column][frequency];
        }
    }
    jpeg_finish_decompress(&state->decoder);
    jpeg_destroy_decompress(&state->decoder);
    state->created = false;
    return std::move(state->result);
}

/// Encode asymmetric, non-square pixels with the native codec. Expected
/// transformed coefficients are derived separately from the cosine basis.
inline std::vector<std::byte> createTransformFixture(int precision, int subsampling, int colorSpace,
                                                     bool progressive = false, bool arithmetic = false, int width = 48,
                                                     int height = 32, int restartInterval = 0)
{
    const std::unique_ptr<void, decltype(&tj3Destroy)> codec(tj3Init(TJINIT_COMPRESS), &tj3Destroy);
    const auto set = [&](int parameter, int value) {
        if (!codec || tj3Set(codec.get(), parameter, value) != 0)
            throw std::runtime_error("Could not configure native transform fixture.");
    };
    set(TJPARAM_QUALITY, 91);
    set(TJPARAM_SUBSAMP, subsampling);
    set(TJPARAM_COLORSPACE, colorSpace);
    set(TJPARAM_PROGRESSIVE, progressive ? 1 : 0);
    set(TJPARAM_ARITHMETIC, arithmetic ? 1 : 0);
    set(TJPARAM_RESTARTBLOCKS, restartInterval);
    const int pixelFormat = colorSpace == TJCS_GRAY
                                ? TJPF_GRAY
                                : (colorSpace == TJCS_CMYK || colorSpace == TJCS_YCCK ? TJPF_CMYK : TJPF_RGB);
    const auto channels = tjPixelSize[pixelFormat];
    std::vector<short> samples(static_cast<std::size_t>(width * height * channels));
    for (int row = 0; row < height; ++row)
        for (int column = 0; column < width; ++column)
            for (int channel = 0; channel < channels; ++channel)
                samples[static_cast<std::size_t>((row * width + column) * channels + channel)] = static_cast<short>(
                    (17 + row * 11 + column * 7 + row * column * 3 + channel * 47) % (1 << precision));
    std::vector<unsigned char> bytes(samples.size());
    for (std::size_t index = 0; index < samples.size(); ++index)
        bytes[index] = static_cast<unsigned char>(samples[index]);
    unsigned char *encoded = nullptr;
    std::size_t length = 0;
    const int status =
        precision == 12 ? tj3Compress12(codec.get(), samples.data(), width, 0, height, pixelFormat, &encoded, &length)
                        : tj3Compress8(codec.get(), bytes.data(), width, 0, height, pixelFormat, &encoded, &length);
    const std::unique_ptr<unsigned char, decltype(&tj3Free)> owned(encoded, &tj3Free);
    if (status != 0)
        throw std::runtime_error(tj3GetErrorStr(codec.get()));
    const auto *first = reinterpret_cast<const std::byte *>(owned.get());
    return {first, first + length};
}
} // namespace jpg_spinner::test_support
