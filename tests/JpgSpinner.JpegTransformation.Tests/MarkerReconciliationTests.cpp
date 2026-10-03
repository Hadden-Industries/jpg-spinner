#include "CoefficientTransformFixture.h"
#include "internal/JpegSegmentScanner.h"
#include "internal/MetadataReconciler.h"

#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <exiv2/exiv2.hpp>
#include <string>

using namespace jpg_spinner::domain;
using namespace jpg_spinner::jpeg::internal;
using namespace jpg_spinner::test_support;

namespace
{
const JpegTransformPlan clockwisePlan{LosslessTransform::Rotate90Clockwise,
                                      {{48}, {32}},
                                      {{32}, {48}},
                                      EdgeHandlingPolicy::RequirePerfectCoefficientTransform,
                                      OutputScanOrganization::PreserveSource,
                                      {{16}, {16}},
                                      {{0}, {0}}};

void appendMarker(std::vector<std::byte> &destination, unsigned char code, const std::string &payload)
{
    REQUIRE(payload.size() <= 65533);
    const auto length = payload.size() + 2;
    destination.insert(destination.end(), {std::byte{0xff}, static_cast<std::byte>(code),
                                           static_cast<std::byte>(length >> 8), static_cast<std::byte>(length & 255)});
    for (unsigned char value : payload)
        destination.push_back(static_cast<std::byte>(value));
}

std::string resource(unsigned int identifier, const std::string &data)
{
    std::string result{"8BIM"};
    result.push_back(static_cast<char>(identifier >> 8));
    result.push_back(static_cast<char>(identifier & 255));
    result.append(2, '\0'); // Empty Pascal name plus its required padding.
    for (int shift = 24; shift >= 0; shift -= 8)
        result.push_back(static_cast<char>((data.size() >> shift) & 255));
    result += data;
    if (data.size() % 2 != 0)
        result.push_back('\0');
    return result;
}

std::vector<std::byte> jpegWithMarkers(const std::vector<std::byte> &markers)
{
    auto source = createTransformFixture(8, TJSAMP_420, TJCS_YCbCr);
    const auto scan = JpegSegmentScanner::scan(source);
    REQUIRE(scan.valueIfPresent() != nullptr);
    // Remove the encoder's JFIF before inserting the independently authored one.
    for (auto marker = scan.valueIfPresent()->markers().rbegin(); marker != scan.valueIfPresent()->markers().rend();
         ++marker)
        if (marker->markerCode >= 0xe0 && marker->markerCode <= 0xef)
            source.erase(source.begin() + static_cast<std::ptrdiff_t>(marker->encodedRange.offsetBytes),
                         source.begin() + static_cast<std::ptrdiff_t>(marker->encodedRange.offsetBytes +
                                                                      marker->encodedRange.lengthBytes));
    source.insert(source.begin() + 2, markers.begin(), markers.end());
    return source;
}
} // namespace

TEST_CASE("Marker reconciliation removes previews without changing ICC or unrelated resource order",
          "[jpeg][metadata][markers]")
{
    std::vector<std::byte> inputMarkers;
    std::vector<std::byte> expectedMarkers;
    const std::string jfifWithoutPreview{"JFIF\0\1\2\1\0\72\0\144\0\0", 14};
    auto jfifWithPreview = jfifWithoutPreview;
    jfifWithPreview[12] = 1;
    jfifWithPreview[13] = 1;
    jfifWithPreview.append("\20\40\60", 3);
    appendMarker(inputMarkers, 0xe0, jfifWithPreview);
    appendMarker(expectedMarkers, 0xe0, jfifWithoutPreview);
    appendMarker(inputMarkers, 0xe0, std::string{"JFXX\0\23\1\1\20\40\60", 11});
    const std::string psHeader{"Photoshop 3.0\0", 14};
    // The preserved IIM payload is a valid ObjectName dataset; its bytes are
    // opaque to the reconciler. Preview resource bodies need not be decoded to
    // remove their complete, native-validated resource envelopes.
    const auto rights = resource(0x0404, std::string{"\34\2\5\0\3abc", 8});
    const auto unrelated = resource(0x040a, std::string(1, '\1'));
    appendMarker(inputMarkers, 0xed,
                 psHeader + rights + resource(1033, "preview-old") + unrelated + resource(1036, "preview-new"));
    appendMarker(expectedMarkers, 0xed, psHeader + rights + unrelated);
    for (const auto &[code, payload] :
         {std::pair{0xe7, std::string{"opaque APP7"}}, std::pair{0xfe, std::string{"source comment"}},
          std::pair{0xe2, std::string{"ICC_PROFILE\0\2\2second", 20}},
          std::pair{0xe2, std::string{"ICC_PROFILE\0\1\2first", 19}}})
    {
        appendMarker(inputMarkers, static_cast<unsigned char>(code), payload);
        appendMarker(expectedMarkers, static_cast<unsigned char>(code), payload);
    }
    const auto source = jpegWithMarkers(inputMarkers);
    const auto original = source;
    const auto scan = JpegSegmentScanner::scan(source);
    REQUIRE(scan.valueIfPresent() != nullptr);
    REQUIRE(scan.valueIfPresent()->iccProfileChunks().size() == 2);
    const auto result = MetadataReconciler::reconcileMarkerSegments(source, *scan.valueIfPresent(), clockwisePlan);
    REQUIRE(result.valueIfPresent() != nullptr);
    CHECK(result.valueIfPresent()->encodedBytes == expectedMarkers);
    CHECK(result.valueIfPresent()->removedEmbeddedThumbnail);
    CHECK(source == original);
}

TEST_CASE("Uninterpretable known previews fail instead of being retained or discarded", "[jpeg][metadata][markers]")
{
    for (const auto &[code, payload] :
         {std::pair{0xe0, std::string{"JFIF\0", 5}}, std::pair{0xe0, std::string{"JFXX\0\77", 6}},
          std::pair{0xed, std::string{"Photoshop 3.0\0broken", 20}}})
    {
        std::vector<std::byte> markers;
        appendMarker(markers, static_cast<unsigned char>(code), payload);
        const auto source = jpegWithMarkers(markers);
        const auto scan = JpegSegmentScanner::scan(source);
        REQUIRE(scan.valueIfPresent() != nullptr);
        const auto result = MetadataReconciler::reconcileMarkerSegments(source, *scan.valueIfPresent(), clockwisePlan);
        REQUIRE(result.errorIfPresent() != nullptr);
        CHECK(result.errorIfPresent()->code == ImageProcessingErrorCode::UnsupportedEmbeddedPreviewMetadata);
    }
}

TEST_CASE("Reconciled marker output obeys its aggregate metadata budget", "[jpeg][metadata][markers]")
{
    std::vector<std::byte> markers;
    appendMarker(markers, 0xfe, "preserved comment");
    const auto source = jpegWithMarkers(markers);
    const auto scan = JpegSegmentScanner::scan(source);
    REQUIRE(scan.valueIfPresent() != nullptr);
    const auto defaults = JpegResourceLimits::production();
    const JpegResourceLimits limits{defaults.maximumEncodedFileLengthBytes, defaults.maximumPixelCount,
                                    markers.size() - 1, defaults.maximumProgressiveScanCount};
    const auto result =
        MetadataReconciler::reconcileMarkerSegments(source, *scan.valueIfPresent(), clockwisePlan, limits);
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == ImageProcessingErrorCode::MetadataLengthLimitExceeded);
    const auto *violation = std::get_if<JpegResourceLimitViolation>(&result.errorIfPresent()->diagnosticContext);
    REQUIRE(violation != nullptr);
    CHECK(violation->observedValue == markers.size());
}

TEST_CASE("JPEG-coded JFXX previews cannot contain nested JFIF markers", "[jpeg][metadata][markers]")
{
    const auto thumbnail = createTransformFixture(8, TJSAMP_444, TJCS_YCbCr);
    const auto thumbnailScan = JpegSegmentScanner::scan(thumbnail);
    REQUIRE(thumbnailScan.valueIfPresent() != nullptr);
    REQUIRE(std::ranges::any_of(thumbnailScan.valueIfPresent()->markers(),
                                [](const auto &marker) { return marker.markerCode == 0xe0; }));
    std::string payload{"JFXX\0\20", 6};
    payload.append(reinterpret_cast<const char *>(thumbnail.data()), thumbnail.size());
    std::vector<std::byte> markers;
    appendMarker(markers, 0xe0, payload);
    const auto source = jpegWithMarkers(markers);
    const auto scan = JpegSegmentScanner::scan(source);
    REQUIRE(scan.valueIfPresent() != nullptr);
    const auto result = MetadataReconciler::reconcileMarkerSegments(source, *scan.valueIfPresent(), clockwisePlan);
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == ImageProcessingErrorCode::UnsupportedEmbeddedPreviewMetadata);
}

TEST_CASE("Maximum legal opaque marker payload survives reconciliation exactly", "[jpeg][metadata][markers]")
{
    std::vector<std::byte> markers;
    appendMarker(markers, 0xe7, std::string(65533, 'x'));
    const auto source = jpegWithMarkers(markers);
    const auto scan = JpegSegmentScanner::scan(source);
    REQUIRE(scan.valueIfPresent() != nullptr);
    const auto defaults = JpegResourceLimits::production();
    const JpegResourceLimits exactBudget{defaults.maximumEncodedFileLengthBytes, defaults.maximumPixelCount,
                                         markers.size(), defaults.maximumProgressiveScanCount};
    const auto result =
        MetadataReconciler::reconcileMarkerSegments(source, *scan.valueIfPresent(), clockwisePlan, exactBudget);
    REQUIRE(result.valueIfPresent() != nullptr);
    CHECK(result.valueIfPresent()->encodedBytes == markers);
    CHECK_FALSE(result.valueIfPresent()->removedEmbeddedThumbnail);
}

TEST_CASE("Combined metadata is truthful after the real APP1 framing path", "[jpeg][metadata][markers]")
{
    Exiv2::ExifData exif;
    exif["Exif.Image.Orientation"] = static_cast<std::uint16_t>(6);
    exif["Exif.Photo.PixelXDimension"] = static_cast<std::uint32_t>(48);
    exif["Exif.Photo.PixelYDimension"] = static_cast<std::uint32_t>(32);
    exif["Exif.Image.Copyright"] = "Retain these rights";
    const auto thumbnail = createTransformFixture(8, TJSAMP_444, TJCS_YCbCr);
    Exiv2::ExifThumb{exif}.setJpegThumbnail(reinterpret_cast<const Exiv2::byte *>(thumbnail.data()), thumbnail.size());
    Exiv2::Blob tiff;
    Exiv2::ExifParser::encode(tiff, Exiv2::littleEndian, exif);
    std::string exifPayload{"Exif\0\0", 6};
    exifPayload.append(reinterpret_cast<const char *>(tiff.data()), tiff.size());
    const std::string packet = R"(<x:xmpmeta xmlns:x="adobe:ns:meta/">
<rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#"><rdf:Description rdf:about=""
xmlns:tiff="http://ns.adobe.com/tiff/1.0/" xmlns:dc="http://purl.org/dc/elements/1.1/"
xmlns:xmp="http://ns.adobe.com/xap/1.0/" xmlns:iptc="http://iptc.org/std/Iptc4xmpExt/2008-02-29/"
tiff:Orientation="6" tiff:ImageWidth="48" tiff:ImageLength="32"
iptc:AIPromptInformation="Synthetic prompt" iptc:AIPromptWriterName="Synthetic writer"
iptc:AISystemUsed="Synthetic system" iptc:AISystemVersionUsed="1">
<dc:rights><rdf:Alt><rdf:li xml:lang="x-default">Retain these rights</rdf:li></rdf:Alt></dc:rights>
<xmp:Thumbnails><rdf:Alt><rdf:li rdf:parseType="Resource"/></rdf:Alt></xmp:Thumbnails>
</rdf:Description></rdf:RDF></x:xmpmeta>)";
    Exiv2::XmpData inputXmp;
    REQUIRE(Exiv2::XmpParser::decode(inputXmp, packet) == 0);
    // XML prefixes are aliases, not property identities. Use the native
    // registry's prefix for the namespace in both input and output checks.
    const auto iptcKeyPrefix =
        "Xmp." + Exiv2::XmpProperties::prefix("http://iptc.org/std/Iptc4xmpExt/2008-02-29/") + ".";
    REQUIRE(inputXmp[iptcKeyPrefix + "AIPromptInformation"].toString() == "Synthetic prompt");
    REQUIRE(inputXmp[iptcKeyPrefix + "AIPromptWriterName"].toString() == "Synthetic writer");
    REQUIRE(inputXmp[iptcKeyPrefix + "AISystemUsed"].toString() == "Synthetic system");
    REQUIRE(inputXmp[iptcKeyPrefix + "AISystemVersionUsed"].toString() == "1");
    std::vector<std::byte> markers;
    appendMarker(markers, 0xe1, exifPayload);
    appendMarker(markers, 0xe1, std::string{"http://ns.adobe.com/xap/1.0/\0", 29} + packet);
    appendMarker(markers, 0xe0, std::string{"JFIF\0\1\2\1\0\72\0\144\1\1\20\40\60", 17});
    appendMarker(markers, 0xe0, std::string{"JFXX\0\23\1\1\20\40\60", 11});
    const std::string photoshopHeader{"Photoshop 3.0\0", 14};
    const auto iim = resource(0x0404, std::string{"\34\2\5\0\3abc", 8});
    appendMarker(markers, 0xed, photoshopHeader + iim + resource(1036, "derived preview"));
    std::vector<std::byte> expectedOpaqueMarkers;
    appendMarker(expectedOpaqueMarkers, 0xe2, std::string{"ICC_PROFILE\0\2\2second", 20});
    appendMarker(expectedOpaqueMarkers, 0xfe, "source comment");
    appendMarker(expectedOpaqueMarkers, 0xe7, "opaque APP7");
    appendMarker(expectedOpaqueMarkers, 0xe2, std::string{"ICC_PROFILE\0\1\2first", 19});
    markers.insert(markers.end(), expectedOpaqueMarkers.begin(), expectedOpaqueMarkers.end());
    const auto source = jpegWithMarkers(markers);
    const auto scan = JpegSegmentScanner::scan(source);
    REQUIRE(scan.valueIfPresent() != nullptr);
    REQUIRE(scan.valueIfPresent()->exifTiffDataRanges().size() == 1);
    REQUIRE(scan.valueIfPresent()->standardXmpPacketRanges().size() == 1);
    REQUIRE(scan.valueIfPresent()->iccProfileChunks().size() == 2);
    const auto result = MetadataReconciler::reconcileMarkerSegments(source, *scan.valueIfPresent(), clockwisePlan);
    REQUIRE(result.valueIfPresent() != nullptr);
    CHECK(result.valueIfPresent()->removedEmbeddedThumbnail);
    const auto output = jpegWithMarkers(result.valueIfPresent()->encodedBytes);
    const auto outputScan = JpegSegmentScanner::scan(output);
    REQUIRE(outputScan.valueIfPresent() != nullptr);
    REQUIRE(outputScan.valueIfPresent()->exifTiffDataRanges().size() == 1);
    REQUIRE(outputScan.valueIfPresent()->standardXmpPacketRanges().size() == 1);
    const auto exifRange = outputScan.valueIfPresent()->exifTiffDataRanges().front();
    // Scanner ranges use uint64_t, while Exiv2 consumes native size_t. Prove
    // containment in this actual vector before narrowing on 32-bit builds.
    REQUIRE(exifRange.offsetBytes <= output.size());
    REQUIRE(exifRange.lengthBytes <= output.size() - static_cast<std::size_t>(exifRange.offsetBytes));
    Exiv2::ExifData observedExif;
    REQUIRE(Exiv2::ExifParser::decode(observedExif,
                                      reinterpret_cast<const Exiv2::byte *>(output.data() + exifRange.offsetBytes),
                                      static_cast<std::size_t>(exifRange.lengthBytes)) == Exiv2::littleEndian);
    CHECK(observedExif["Exif.Image.Orientation"].toInt64() == 1);
    CHECK(observedExif["Exif.Photo.PixelXDimension"].toInt64() == 32);
    CHECK(observedExif["Exif.Photo.PixelYDimension"].toInt64() == 48);
    CHECK(observedExif["Exif.Image.Copyright"].toString() == "Retain these rights");
    CHECK(Exiv2::ExifThumbC{observedExif}.copy().size() == 0);
    const auto xmpRange = outputScan.valueIfPresent()->standardXmpPacketRanges().front();
    Exiv2::XmpData observedXmp;
    REQUIRE(Exiv2::XmpParser::decode(observedXmp, {reinterpret_cast<const char *>(output.data() + xmpRange.offsetBytes),
                                                   static_cast<std::size_t>(xmpRange.lengthBytes)}) == 0);
    CHECK(observedXmp["Xmp.tiff.Orientation"].toString() == "1");
    CHECK(observedXmp["Xmp.tiff.ImageWidth"].toString() == "32");
    CHECK(observedXmp["Xmp.tiff.ImageLength"].toString() == "48");
    CHECK(observedXmp[iptcKeyPrefix + "AIPromptInformation"].toString() == "Synthetic prompt");
    CHECK(observedXmp[iptcKeyPrefix + "AIPromptWriterName"].toString() == "Synthetic writer");
    CHECK(observedXmp[iptcKeyPrefix + "AISystemUsed"].toString() == "Synthetic system");
    CHECK(observedXmp[iptcKeyPrefix + "AISystemVersionUsed"].toString() == "1");
    const auto &rights = dynamic_cast<const Exiv2::LangAltValue &>(observedXmp["Xmp.dc.rights"].value());
    CHECK(rights.value_.at("x-default") == "Retain these rights");
    CHECK(observedXmp.findKey(Exiv2::XmpKey{"Xmp.xmp.Thumbnails"}) == observedXmp.end());
    std::vector<std::byte> observedOpaqueMarkers;
    std::size_t jfifMarkerCount = 0;
    std::size_t photoshopMarkerCount = 0;
    for (const auto &marker : outputScan.valueIfPresent()->markers())
    {
        const auto payload = std::span{output}.subspan(static_cast<std::size_t>(marker.payloadRange.offsetBytes),
                                                       static_cast<std::size_t>(marker.payloadRange.lengthBytes));
        if (marker.markerCode == 0xed)
        {
            ++photoshopMarkerCount;
            CHECK(std::string{reinterpret_cast<const char *>(payload.data()), payload.size()} == photoshopHeader + iim);
        }
        if (marker.markerCode == 0xe0)
        {
            ++jfifMarkerCount;
            CHECK(std::string{reinterpret_cast<const char *>(payload.data()), payload.size()} ==
                  std::string{"JFIF\0\1\2\1\0\72\0\144\0\0", 14});
        }
        if (marker.markerCode == 0xe2 || marker.markerCode == 0xfe || marker.markerCode == 0xe7)
        {
            const auto bytes = std::span{output}.subspan(static_cast<std::size_t>(marker.encodedRange.offsetBytes),
                                                         static_cast<std::size_t>(marker.encodedRange.lengthBytes));
            observedOpaqueMarkers.insert(observedOpaqueMarkers.end(), bytes.begin(), bytes.end());
        }
    }
    CHECK(observedOpaqueMarkers == expectedOpaqueMarkers);
    CHECK(jfifMarkerCount == 1);
    CHECK(photoshopMarkerCount == 1);
}
