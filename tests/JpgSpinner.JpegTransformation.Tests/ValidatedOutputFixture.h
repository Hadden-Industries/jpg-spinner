#pragma once

#include "CoefficientTransformFixture.h"
#include "internal/JpegSegmentScanner.h"
#include "internal/LibJpegTurboCoefficientTransformer.h"
#include "internal/MetadataReconciler.h"
#include <algorithm>
#include <array>
#include <exiv2/exiv2.hpp>
#include <string_view>

namespace jpg_spinner::test_support
{
/// Framed JPEG fixture builder. It only supplies bytes; expected validation
/// decisions and dimensions remain literals in each test.
inline std::vector<std::byte> frameMetadataMarker(unsigned char marker, std::span<const std::byte> payload)
{
    if (payload.size() > 65533)
        throw std::invalid_argument("Test marker payload exceeds JPEG framing.");
    const auto length = payload.size() + 2;
    std::vector<std::byte> result{std::byte{0xff}, std::byte{marker}, static_cast<std::byte>(length >> 8),
                                  static_cast<std::byte>(length & 0xff)};
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}
inline std::span<const std::byte> textBytes(std::string_view text)
{
    return {reinterpret_cast<const std::byte *>(text.data()), text.size()};
}

/// Native fixture metadata includes dimensions, an optional thumbnail and an
/// unrelated camera model. This is independent of production serialization.
inline std::vector<std::byte> createMetadataSource(unsigned short orientation = 6, bool thumbnail = false)
{
    auto source = createTransformFixture(8, TJSAMP_420, TJCS_YCbCr);
    Exiv2::ExifData exif;
    exif["Exif.Image.Orientation"] = orientation;
    exif["Exif.Image.ImageWidth"] = static_cast<std::uint32_t>(48);
    exif["Exif.Image.ImageLength"] = static_cast<std::uint32_t>(32);
    exif["Exif.Photo.PixelXDimension"] = static_cast<std::uint32_t>(48);
    exif["Exif.Photo.PixelYDimension"] = static_cast<std::uint32_t>(32);
    exif["Exif.Image.Model"] = "Preservation camera";
    if (thumbnail)
    {
        const auto preview = createTransformFixture(8, TJSAMP_GRAY, TJCS_GRAY);
        Exiv2::ExifThumb{exif}.setJpegThumbnail(reinterpret_cast<const Exiv2::byte *>(preview.data()), preview.size());
    }
    Exiv2::Blob tiff;
    Exiv2::ExifParser::encode(tiff, Exiv2::littleEndian, exif);
    const auto exifIdentifier = textBytes({"Exif\0\0", 6});
    std::vector<std::byte> payload(exifIdentifier.begin(), exifIdentifier.end());
    for (auto value : tiff)
        payload.push_back(static_cast<std::byte>(value));
    const auto marker = frameMetadataMarker(0xe1, payload);
    source.insert(source.begin() + 2, marker.begin(), marker.end());

    const std::string packet =
        "<x:xmpmeta xmlns:x='adobe:ns:meta/'><rdf:RDF xmlns:rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#'>"
        "<rdf:Description rdf:about='' xmlns:tiff='http://ns.adobe.com/tiff/1.0/' "
        "xmlns:exif='http://ns.adobe.com/exif/1.0/' xmlns:dc='http://purl.org/dc/elements/1.1/' "
        "tiff:Orientation='" +
        std::to_string(orientation) +
        "' tiff:ImageWidth='48' tiff:ImageLength='32' exif:PixelXDimension='48' exif:PixelYDimension='32' "
        "dc:format='image/jpeg'/></rdf:RDF></x:xmpmeta>";
    std::string xmpPayload{"http://ns.adobe.com/xap/1.0/\0", 29};
    xmpPayload += packet;
    const auto xmp = frameMetadataMarker(0xe1, textBytes(xmpPayload));
    source.insert(source.begin() + 2, xmp.begin(), xmp.end());
    for (const auto text : {"first comment", "second comment"})
    {
        const auto comment = frameMetadataMarker(0xfe, textBytes(text));
        source.insert(source.begin() + 2, comment.begin(), comment.end());
    }
    std::string iccPayload{"ICC_PROFILE\0\1\1", 14};
    iccPayload += "retained profile bytes";
    const auto icc = frameMetadataMarker(0xe2, textBytes(iccPayload));
    source.insert(source.begin() + 2, icc.begin(), icc.end());
    return source;
}

/// Builds a completed candidate through existing writer components. Tests
/// independently corrupt/inspect it; this builder never decides correctness.
inline std::vector<std::byte> createReconciledOutput(std::span<const std::byte> source,
                                                     const jpg_spinner::domain::JpegTransformPlan &plan)
{
    using namespace jpg_spinner::jpeg::internal;
    const auto scan = JpegSegmentScanner::scan(source);
    if (!scan.valueIfPresent())
        throw std::runtime_error("Source fixture is not structurally valid.");
    std::vector<std::byte> codecOutput(65536);
    const auto transformed = LibJpegTurboCoefficientTransformer::transformCoefficients(source, plan, codecOutput);
    if (!transformed.valueIfPresent())
        throw std::runtime_error("Source fixture coefficient transformation failed.");
    codecOutput.resize(*transformed.valueIfPresent());
    const auto metadata = MetadataReconciler::reconcileMarkerSegments(source, *scan.valueIfPresent(), plan);
    if (!metadata.valueIfPresent())
        throw std::runtime_error("Source fixture metadata reconciliation failed.");
    const auto codecScan = JpegSegmentScanner::scan(codecOutput);
    if (!codecScan.valueIfPresent())
        throw std::runtime_error("Codec fixture output is not structurally valid.");
    std::vector<std::byte> output{codecOutput[0], codecOutput[1]};
    const auto &segments = metadata.valueIfPresent()->encodedBytes;
    output.insert(output.end(), segments.begin(), segments.end());
    std::size_t cursor = 2;
    // The codec may create JFIF/Adobe color hints despite COPYNONE. Metadata
    // ownership stays with the reconciler, so retain only the codec codestream.
    for (const auto &marker : codecScan.valueIfPresent()->markers())
        if ((marker.markerCode >= 0xe0 && marker.markerCode <= 0xef) || marker.markerCode == 0xfe)
        {
            const auto offset = static_cast<std::size_t>(marker.encodedRange.offsetBytes);
            output.insert(output.end(), codecOutput.begin() + static_cast<std::ptrdiff_t>(cursor),
                          codecOutput.begin() + static_cast<std::ptrdiff_t>(offset));
            cursor = offset + static_cast<std::size_t>(marker.encodedRange.lengthBytes);
        }
    output.insert(output.end(), codecOutput.begin() + static_cast<std::ptrdiff_t>(cursor), codecOutput.end());
    return output;
}

/// Replace a scanner-proven range without touching other properties. Calling
/// tests assert the targeted metadata field's native precondition separately.
inline void replaceFixtureRange(std::vector<std::byte> &bytes, jpg_spinner::jpeg::internal::EncodedByteRange range,
                                std::span<const std::byte> replacement)
{
    const auto offset = static_cast<std::ptrdiff_t>(range.offsetBytes);
    bytes.erase(bytes.begin() + offset, bytes.begin() + offset + static_cast<std::ptrdiff_t>(range.lengthBytes));
    bytes.insert(bytes.begin() + offset, replacement.begin(), replacement.end());
}
} // namespace jpg_spinner::test_support
