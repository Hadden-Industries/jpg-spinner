#include "internal/MetadataReconciler.h"
#include "CoefficientTransformFixture.h"
#include "internal/JpegSegmentScanner.h"

#include <catch2/catch_test_macros.hpp>
#include <exiv2/exiv2.hpp>
#include <algorithm>
#include <array>
#include <barrier>
#include <future>
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
// A literal TIFF IFD0 (little-endian SHORT Orientation, count 1), not output
// synthesized by the reconciler or Exiv2. Defaults to orientation 6.
std::vector<std::byte> orientationFixture(unsigned int rawExifOrientation = 6,
                                          const std::string &rawXmpOrientation = "3", bool includeExif = true,
                                          bool includeXmp = true)
{
    auto source = createTransformFixture(8, TJSAMP_420, TJCS_YCbCr);
    std::array<unsigned char, 36> exif{0xff, 0xe1, 0,    34, 'E', 'x', 'i', 'f', 0, 0, 'I', 'I', 42, 0, 8, 0, 0, 0,
                                       1,    0,    0x12, 1,  3,   0,   1,   0,   0, 0, 6,   0,   0,  0, 0, 0, 0, 0};
    exif[28] = static_cast<unsigned char>(rawExifOrientation & 0xff);
    exif[29] = static_cast<unsigned char>(rawExifOrientation >> 8);
    std::vector<std::byte> segment;
    for (auto value : exif)
        segment.push_back(static_cast<std::byte>(value));
    if (includeExif)
        source.insert(source.begin() + 2, segment.begin(), segment.end());

    // Use a noncanonical XML prefix: namespace identity, not prefix spelling,
    // must select the XMP property. The default value conflicts with Exif.
    const std::string packet = "<x:xmpmeta xmlns:x='adobe:ns:meta/'>"
                               "<rdf:RDF xmlns:rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#'>"
                               "<rdf:Description rdf:about='' xmlns:image='http://ns.adobe.com/tiff/1.0/' "
                               "image:Orientation='" +
                               rawXmpOrientation + "'/></rdf:RDF></x:xmpmeta>";
    std::string payload{"http://ns.adobe.com/xap/1.0/\0", 29};
    payload += packet;
    const auto length = payload.size() + 2;
    segment = {std::byte{0xff}, std::byte{0xe1}, static_cast<std::byte>(length >> 8),
               static_cast<std::byte>(length & 0xff)};
    for (unsigned char value : payload)
        segment.push_back(static_cast<std::byte>(value));
    if (includeXmp)
        source.insert(source.begin() + 2, segment.begin(), segment.end());
    return source;
}

TEST_CASE("Absent metadata differs from invalid orientation", "[jpeg][metadata]")
{
    for (const auto includeExif : {false, true})
        for (const auto includeXmp : {false, true})
        {
            const auto source = orientationFixture(6, "3", includeExif, includeXmp);
            const auto scan = JpegSegmentScanner::scan(source);
            REQUIRE(scan.valueIfPresent() != nullptr);
            const auto result = MetadataReconciler::analyzeOrientation(source, *scan.valueIfPresent());
            REQUIRE(result.valueIfPresent() != nullptr);
            CHECK(result.valueIfPresent()->exifOrientation.has_value() == includeExif);
            CHECK(result.valueIfPresent()->xmpOrientation.has_value() == includeXmp);
            CHECK(result.valueIfPresent()->authoritativeOrientation == (includeExif  ? ExifOrientation::RightTop
                                                                        : includeXmp ? ExifOrientation::BottomRight
                                                                                     : ExifOrientation::TopLeft));
        }
    for (const auto invalidExif : {0U, 9U, 65535U})
    {
        const auto source = orientationFixture(invalidExif);
        const auto scan = JpegSegmentScanner::scan(source);
        REQUIRE(scan.valueIfPresent() != nullptr);
        const auto result = MetadataReconciler::analyzeOrientation(source, *scan.valueIfPresent());
        REQUIRE(result.errorIfPresent() != nullptr);
        CHECK(result.errorIfPresent()->code == ImageProcessingErrorCode::InvalidOrientationMetadata);
    }
}

TEST_CASE("XMP orientation accepts integers but never rounded values or coercions", "[jpeg][metadata]")
{
    for (const std::string invalidXmp : {"", "0", "9", "6.1", "6/1", "true", "6suffix", "-1"})
    {
        CAPTURE(invalidXmp);
        const auto source = orientationFixture(6, invalidXmp, false);
        const auto scan = JpegSegmentScanner::scan(source);
        REQUIRE(scan.valueIfPresent() != nullptr);
        const auto result = MetadataReconciler::analyzeOrientation(source, *scan.valueIfPresent());
        REQUIRE(result.errorIfPresent() != nullptr);
        CHECK(result.errorIfPresent()->code == ImageProcessingErrorCode::InvalidOrientationMetadata);
    }
    const auto source = orientationFixture(6, " +006 ", false);
    const auto scan = JpegSegmentScanner::scan(source);
    REQUIRE(scan.valueIfPresent() != nullptr);
    const auto result = MetadataReconciler::analyzeOrientation(source, *scan.valueIfPresent());
    REQUIRE(result.valueIfPresent() != nullptr);
    CHECK(result.valueIfPresent()->authoritativeOrientation == ExifOrientation::RightTop);
}

TEST_CASE("Malformed TIFF offsets return a redacted metadata error", "[jpeg][metadata]")
{
    auto source = orientationFixture();
    const auto scan = JpegSegmentScanner::scan(source);
    REQUIRE(scan.valueIfPresent() != nullptr);
    const auto offset = static_cast<std::size_t>(scan.valueIfPresent()->exifTiffDataRanges().front().offsetBytes);
    // The TIFF's IFD0 pointer now lies beyond its isolated payload. Outer JPEG
    // framing remains valid; only the real metadata parser can reject this.
    source[offset + 4] = std::byte{0xff};
    source[offset + 5] = std::byte{0xff};
    const auto malformedScan = JpegSegmentScanner::scan(source);
    REQUIRE(malformedScan.valueIfPresent() != nullptr);
    const auto result = MetadataReconciler::analyzeOrientation(source, *malformedScan.valueIfPresent());
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == ImageProcessingErrorCode::MalformedImageMetadata);
    CHECK(result.errorIfPresent()->stage == ImageProcessingStage::MetadataAnalysis);
    CHECK(std::holds_alternative<std::monostate>(result.errorIfPresent()->nativeErrorProjection));
}

TEST_CASE("Concurrent metadata reads keep native failure state isolated", "[jpeg][metadata]")
{
    const auto valid = orientationFixture();
    auto malformed = valid;
    const auto scan = JpegSegmentScanner::scan(valid);
    REQUIRE(scan.valueIfPresent() != nullptr);
    const auto offset = static_cast<std::size_t>(scan.valueIfPresent()->exifTiffDataRanges().front().offsetBytes);
    malformed[offset + 4] = std::byte{0xff};
    malformed[offset + 5] = std::byte{0xff};
    const auto malformedScan = JpegSegmentScanner::scan(malformed);
    REQUIRE(malformedScan.valueIfPresent() != nullptr);
    std::barrier start{8};
    std::vector<std::future<bool>> workers;
    for (int worker = 0; worker < 8; ++worker)
        workers.push_back(std::async(std::launch::async, [&, worker] {
            start.arrive_and_wait();
            for (int iteration = 0; iteration < 16; ++iteration)
            {
                const bool invalidInput = (worker + iteration) % 2 == 0;
                const auto result = MetadataReconciler::analyzeOrientation(
                    invalidInput ? malformed : valid,
                    invalidInput ? *malformedScan.valueIfPresent() : *scan.valueIfPresent());
                if (invalidInput ? (result.errorIfPresent() == nullptr ||
                                    result.errorIfPresent()->code != ImageProcessingErrorCode::MalformedImageMetadata)
                                 : (result.valueIfPresent() == nullptr ||
                                    result.valueIfPresent()->authoritativeOrientation != ExifOrientation::RightTop))
                    return false;
            }
            return true;
        }));
    for (auto &worker : workers)
        CHECK(worker.get());
}

TEST_CASE("Multiple authoritative metadata packets are rejected as an ambiguous container", "[jpeg][metadata]")
{
    for (const bool duplicateExif : {false, true})
    {
        auto source = orientationFixture();
        const auto scan = JpegSegmentScanner::scan(source);
        REQUIRE(scan.valueIfPresent() != nullptr);
        const auto range = duplicateExif ? scan.valueIfPresent()->exifTiffDataRanges().front()
                                         : scan.valueIfPresent()->standardXmpPacketRanges().front();
        for (const auto &marker : scan.valueIfPresent()->markers())
            if (range.offsetBytes >= marker.payloadRange.offsetBytes &&
                range.offsetBytes < marker.payloadRange.offsetBytes + marker.payloadRange.lengthBytes)
            {
                const auto begin = source.begin() + static_cast<std::ptrdiff_t>(marker.encodedRange.offsetBytes);
                const std::vector<std::byte> duplicate{
                    begin, begin + static_cast<std::ptrdiff_t>(marker.encodedRange.lengthBytes)};
                source.insert(source.begin() + 2, duplicate.begin(), duplicate.end());
                break;
            }
        const auto duplicatedScan = JpegSegmentScanner::scan(source);
        REQUIRE(duplicatedScan.valueIfPresent() != nullptr);
        REQUIRE((duplicateExif ? duplicatedScan.valueIfPresent()->exifTiffDataRanges().size()
                               : duplicatedScan.valueIfPresent()->standardXmpPacketRanges().size()) == 2);
        const auto result = MetadataReconciler::analyzeOrientation(source, *duplicatedScan.valueIfPresent());
        REQUIRE(result.errorIfPresent() != nullptr);
        CHECK(result.errorIfPresent()->code == ImageProcessingErrorCode::MalformedImageMetadata);
    }
}
} // namespace

TEST_CASE("Exif orientation is normalized through an owned non-intrusive update", "[jpeg][metadata]")
{
    const auto source = orientationFixture();
    const auto scan = JpegSegmentScanner::scan(source);
    REQUIRE(scan.valueIfPresent() != nullptr);
    const auto range = scan.valueIfPresent()->exifTiffDataRanges().front();
    const auto tiff = std::span{source}.subspan(static_cast<std::size_t>(range.offsetBytes),
                                                static_cast<std::size_t>(range.lengthBytes));
    auto expected = std::vector<std::byte>{tiff.begin(), tiff.end()};
    REQUIRE(expected.at(18) == std::byte{6});
    expected[18] = std::byte{1};
    const auto original = source;
    const auto result = MetadataReconciler::reconcileExifPayload(tiff, clockwisePlan);
    REQUIRE(result.valueIfPresent() != nullptr);
    CHECK(result.valueIfPresent()->encodedBytes == expected);
    CHECK(source == original);
}

TEST_CASE("Exif reconciliation updates dimensions and removes only the derived thumbnail", "[jpeg][metadata]")
{
    Exiv2::ExifData exif;
    exif["Exif.Image.Orientation"] = static_cast<std::uint16_t>(6);
    exif["Exif.Photo.PixelXDimension"] = static_cast<std::uint32_t>(48);
    exif["Exif.Photo.PixelYDimension"] = static_cast<std::uint32_t>(32);
    exif["Exif.Image.Copyright"] = "Synthetic fixture rights retained";
    const auto thumbnail = createTransformFixture(8, TJSAMP_444, TJCS_YCbCr);
    Exiv2::ExifThumb{exif}.setJpegThumbnail(reinterpret_cast<const Exiv2::byte *>(thumbnail.data()), thumbnail.size());
    Exiv2::Blob encoded;
    Exiv2::ExifParser::encode(encoded, Exiv2::littleEndian, exif);
    Exiv2::ExifData observedInput;
    REQUIRE(Exiv2::ExifParser::decode(observedInput, encoded.data(), encoded.size()) == Exiv2::littleEndian);
    REQUIRE(Exiv2::ExifThumbC{observedInput}.copy().size() == thumbnail.size());
    REQUIRE(observedInput["Exif.Photo.PixelXDimension"].toInt64() == 48);
    REQUIRE(observedInput["Exif.Photo.PixelYDimension"].toInt64() == 32);
    const auto original = encoded;
    const auto input = std::span{reinterpret_cast<const std::byte *>(encoded.data()), encoded.size()};
    const auto result = MetadataReconciler::reconcileExifPayload(input, clockwisePlan);
    REQUIRE(result.valueIfPresent() != nullptr);
    CHECK(result.valueIfPresent()->removedEmbeddedThumbnail);
    Exiv2::ExifData output;
    REQUIRE(Exiv2::ExifParser::decode(
                output, reinterpret_cast<const Exiv2::byte *>(result.valueIfPresent()->encodedBytes.data()),
                result.valueIfPresent()->encodedBytes.size()) == Exiv2::littleEndian);
    CHECK(output["Exif.Image.Orientation"].toInt64() == 1);
    CHECK(output["Exif.Photo.PixelXDimension"].toInt64() == 32);
    CHECK(output["Exif.Photo.PixelYDimension"].toInt64() == 48);
    CHECK(output["Exif.Image.Copyright"].toString() == "Synthetic fixture rights retained");
    CHECK(Exiv2::ExifThumbC{output}.copy().size() == 0);
    CHECK(std::ranges::none_of(output, [](const auto &entry) { return entry.groupName() == "Thumbnail"; }));
    CHECK(encoded == original);
}

TEST_CASE("Standard XMP reconciliation preserves structured rights while removing stale previews", "[jpeg][metadata]")
{
    // Literal RDF is independent of the serializer under test. Arrays, language
    // alternatives and structured thumbnails exercise more than scalar strings.
    const std::string packet = R"(<x:xmpmeta xmlns:x="adobe:ns:meta/">
<rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
<rdf:Description rdf:about="" xmlns:tiff="http://ns.adobe.com/tiff/1.0/"
 xmlns:exif="http://ns.adobe.com/exif/1.0/" xmlns:dc="http://purl.org/dc/elements/1.1/"
 xmlns:xmp="http://ns.adobe.com/xap/1.0/" xmlns:image="http://ns.adobe.com/xap/1.0/g/img/"
 tiff:Orientation="6" tiff:ImageWidth="48" tiff:ImageLength="32"
 exif:PixelXDimension="48" exif:PixelYDimension="32">
 <dc:rights><rdf:Alt><rdf:li xml:lang="x-default">All rights retained</rdf:li>
 <rdf:li xml:lang="fr">Droits conservés</rdf:li></rdf:Alt></dc:rights>
 <dc:creator><rdf:Seq><rdf:li>First author</rdf:li><rdf:li>Second author</rdf:li></rdf:Seq></dc:creator>
 <xmp:Thumbnails><rdf:Alt><rdf:li rdf:parseType="Resource">
 <image:width>1</image:width><image:height>1</image:height><image:format>JPEG</image:format>
 <image:image>/9j/2Q==</image:image></rdf:li></rdf:Alt></xmp:Thumbnails>
</rdf:Description></rdf:RDF></x:xmpmeta>)";
    Exiv2::XmpData input;
    REQUIRE(Exiv2::XmpParser::decode(input, packet) == 0);
    REQUIRE(input["Xmp.tiff.Orientation"].toString() == "6");
    // Check the native decoder against the literal fixture before using its
    // model as a preservation baseline; a lossy decode must not validate itself.
    const auto &inputRights = dynamic_cast<const Exiv2::LangAltValue &>(input["Xmp.dc.rights"].value());
    REQUIRE(inputRights.value_.at("x-default") == "All rights retained");
    REQUIRE(inputRights.value_.at("fr") == "Droits conservés");
    REQUIRE(
        std::ranges::any_of(input, [](const auto &entry) { return entry.key().starts_with("Xmp.xmp.Thumbnails"); }));
    const auto bytes = std::as_bytes(std::span{packet.data(), packet.size()});
    const auto result = MetadataReconciler::reconcileStandardXmpPacket(bytes, clockwisePlan);
    REQUIRE(result.valueIfPresent() != nullptr);
    CHECK(result.valueIfPresent()->removedEmbeddedThumbnail);
    Exiv2::XmpData output;
    REQUIRE(
        Exiv2::XmpParser::decode(output, {reinterpret_cast<const char *>(result.valueIfPresent()->encodedBytes.data()),
                                          result.valueIfPresent()->encodedBytes.size()}) == 0);
    CHECK(output["Xmp.tiff.Orientation"].toString() == "1");
    CHECK(output["Xmp.tiff.ImageWidth"].toString() == "32");
    CHECK(output["Xmp.tiff.ImageLength"].toString() == "48");
    CHECK(output["Xmp.exif.PixelXDimension"].toString() == "32");
    CHECK(output["Xmp.exif.PixelYDimension"].toString() == "48");
    const auto &rights = dynamic_cast<const Exiv2::LangAltValue &>(output["Xmp.dc.rights"].value());
    CHECK(rights.value_.at("x-default") == "All rights retained");
    CHECK(rights.value_.at("fr") == "Droits conservés");
    CHECK(output["Xmp.dc.creator"].count() == 2);
    CHECK(output["Xmp.dc.creator"].toString(0) == "First author");
    CHECK(output["Xmp.dc.creator"].toString(1) == "Second author");
    CHECK(std::ranges::none_of(output, [](const auto &entry) {
        return entry.key() == "Xmp.xmp.Thumbnails" || entry.key().starts_with("Xmp.xmp.Thumbnails[");
    }));
    CHECK(input["Xmp.tiff.Orientation"].toString() == "6");
}

TEST_CASE("Native XMP language normalization preserves translations and default ordering", "[jpeg][metadata]")
{
    // Literal values are the oracle, not another round trip through the same
    // parser. Default-last also checks that the preservation patch retains the
    // SDK's required ordering normalization rather than disabling the function.
    std::string alternatives;
    SECTION("Default precedes a different translation")
    {
        alternatives =
            R"(<rdf:li xml:lang="x-default">Default rights</rdf:li><rdf:li xml:lang="fr">Droits français</rdf:li>)";
    }
    SECTION("Default follows a different translation")
    {
        alternatives =
            R"(<rdf:li xml:lang="fr">Droits français</rdf:li><rdf:li xml:lang="x-default">Default rights</rdf:li>)";
    }
    SECTION("Three language alternatives retain distinct values")
    {
        alternatives =
            R"(<rdf:li xml:lang="x-default">Default rights</rdf:li><rdf:li xml:lang="fr">Droits français</rdf:li><rdf:li xml:lang="en">Default rights</rdf:li>)";
    }
    const std::string packet =
        R"(<x:xmpmeta xmlns:x="adobe:ns:meta/"><rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#"><rdf:Description rdf:about="" xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:rights><rdf:Alt>)" +
        alternatives + R"(</rdf:Alt></dc:rights></rdf:Description></rdf:RDF></x:xmpmeta>)";
    Exiv2::XmpData decoded;
    REQUIRE(Exiv2::XmpParser::decode(decoded, packet) == 0);
    const auto &inputRights = dynamic_cast<const Exiv2::LangAltValue &>(decoded["Xmp.dc.rights"].value());
    CHECK(inputRights.value_.at("x-default") == "Default rights");
    CHECK(inputRights.value_.at("fr") == "Droits français");
    std::string serialized;
    REQUIRE(Exiv2::XmpParser::encode(serialized, decoded) == 0);
    // This asserts the native serializer's observable ordering; it is not a
    // production XML parser. Both qualifier spellings must actually be present.
    const auto defaultPosition = serialized.find("xml:lang=\"x-default\"");
    const auto frenchPosition = serialized.find("xml:lang=\"fr\"");
    REQUIRE(defaultPosition != std::string::npos);
    REQUIRE(frenchPosition != std::string::npos);
    CHECK(defaultPosition < frenchPosition);
    Exiv2::XmpData reparsed;
    REQUIRE(Exiv2::XmpParser::decode(reparsed, serialized) == 0);
    const auto &outputRights = dynamic_cast<const Exiv2::LangAltValue &>(reparsed["Xmp.dc.rights"].value());
    CHECK(outputRights.value_ == inputRights.value_);
}

TEST_CASE("Invalid XMP dimensions are not silently repaired during rotation", "[jpeg][metadata]")
{
    for (const std::string invalid : {"forty-eight", "48.5", "-1", "18446744073709551616"})
    {
        CAPTURE(invalid);
        const std::string packet =
            "<x:xmpmeta xmlns:x='adobe:ns:meta/'><rdf:RDF xmlns:rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#'>"
            "<rdf:Description rdf:about='' xmlns:tiff='http://ns.adobe.com/tiff/1.0/' tiff:ImageWidth='" +
            invalid + "'/></rdf:RDF></x:xmpmeta>";
        const auto result = MetadataReconciler::reconcileStandardXmpPacket(
            std::as_bytes(std::span{packet.data(), packet.size()}), clockwisePlan);
        REQUIRE(result.errorIfPresent() != nullptr);
        CHECK(result.errorIfPresent()->code == ImageProcessingErrorCode::MalformedImageMetadata);
    }
}

TEST_CASE("Opaque MakerNotes are not relocated without a native semantic interpretation", "[jpeg][metadata]")
{
    Exiv2::ExifData exif;
    exif["Exif.Image.Orientation"] = static_cast<std::uint16_t>(6);
    Exiv2::DataValue opaque;
    REQUIRE(opaque.read("1 2 3 4 5 6 7 8 9 10 11 12") == 0);
    exif.add(Exiv2::ExifKey{"Exif.Photo.MakerNote"}, &opaque);
    const auto thumbnail = createTransformFixture(8, TJSAMP_444, TJCS_YCbCr);
    Exiv2::ExifThumb{exif}.setJpegThumbnail(reinterpret_cast<const Exiv2::byte *>(thumbnail.data()), thumbnail.size());
    Exiv2::Blob encoded;
    Exiv2::ExifParser::encode(encoded, Exiv2::littleEndian, exif);
    Exiv2::ExifData decoded;
    REQUIRE(Exiv2::ExifParser::decode(decoded, encoded.data(), encoded.size()) == Exiv2::littleEndian);
    REQUIRE(decoded.findKey(Exiv2::ExifKey{"Exif.Photo.MakerNote"}) != decoded.end());
    REQUIRE(decoded.findKey(Exiv2::ExifKey{"Exif.MakerNote.Offset"}) == decoded.end());
    auto rewriteInput = encoded;
    Exiv2::ExifThumb{decoded}.erase();
    Exiv2::Blob rewritten;
    REQUIRE(Exiv2::ExifParser::encode(rewritten, rewriteInput.data(), rewriteInput.size(), Exiv2::littleEndian,
                                      decoded) == Exiv2::wmIntrusive);
    const auto result = MetadataReconciler::reconcileExifPayload(
        std::as_bytes(std::span{encoded.data(), encoded.size()}), clockwisePlan);
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == ImageProcessingErrorCode::MetadataPreservationFailed);
}

TEST_CASE("Recognized MakerNote facts survive thumbnail removal", "[jpeg][metadata]")
{
    Exiv2::ExifData exif;
    exif["Exif.Image.Make"] = "Canon";
    exif["Exif.Image.Orientation"] = static_cast<std::uint16_t>(6);
    // Minimal Canon IFD: SerialNumber (0x000c), LONG count 1, value 1234.
    // No camera file, personal metadata or undocumented payload is imported.
    Exiv2::DataValue makerNote;
    REQUIRE(makerNote.read("1 0 12 0 4 0 1 0 0 0 210 4 0 0 0 0 0 0") == 0);
    exif.add(Exiv2::ExifKey{"Exif.Photo.MakerNote"}, &makerNote);
    const auto thumbnail = createTransformFixture(8, TJSAMP_444, TJCS_YCbCr);
    Exiv2::ExifThumb{exif}.setJpegThumbnail(reinterpret_cast<const Exiv2::byte *>(thumbnail.data()), thumbnail.size());
    Exiv2::Blob encoded;
    Exiv2::ExifParser::encode(encoded, Exiv2::littleEndian, exif);
    Exiv2::ExifData decoded;
    REQUIRE(Exiv2::ExifParser::decode(decoded, encoded.data(), encoded.size()) == Exiv2::littleEndian);
    REQUIRE(decoded.findKey(Exiv2::ExifKey{"Exif.MakerNote.Offset"}) != decoded.end());
    REQUIRE(decoded["Exif.Canon.SerialNumber"].toInt64() == 1234);
    auto rewriteInput = encoded;
    Exiv2::ExifThumb{decoded}.erase();
    Exiv2::Blob rewritten;
    REQUIRE(Exiv2::ExifParser::encode(rewritten, rewriteInput.data(), rewriteInput.size(), Exiv2::littleEndian,
                                      decoded) == Exiv2::wmIntrusive);
    const auto result = MetadataReconciler::reconcileExifPayload(
        std::as_bytes(std::span{encoded.data(), encoded.size()}), clockwisePlan);
    REQUIRE(result.valueIfPresent() != nullptr);
    const auto &output = result.valueIfPresent()->encodedBytes;
    Exiv2::ExifData observed;
    REQUIRE(Exiv2::ExifParser::decode(observed, reinterpret_cast<const Exiv2::byte *>(output.data()), output.size()) ==
            Exiv2::littleEndian);
    CHECK(observed["Exif.Canon.SerialNumber"].toInt64() == 1234);
    CHECK(observed["Exif.Image.Orientation"].toInt64() == 1);
    CHECK(Exiv2::ExifThumbC{observed}.copy().size() == 0);
}

TEST_CASE("IPTC 2025.1 AI disclosures use the built-in namespace and survive reconciliation", "[jpeg][metadata]")
{
    // Exiv2 0.28.9 already registers this namespace. New property names do not
    // imply a missing namespace; do not unregister working native support just
    // to manufacture the obsolete plan's anticipated registration failure.
    CHECK(Exiv2::XmpProperties::ns("Iptc4xmpExt") == "http://iptc.org/std/Iptc4xmpExt/2008-02-29/");
    const std::array properties{"AIPromptInformation", "AIPromptWriterName", "AISystemUsed", "AISystemVersionUsed"};
    for (const auto *property : properties)
    {
        CAPTURE(property);
        Exiv2::XmpData source;
        const auto key = std::string{"Xmp.Iptc4xmpExt."} + property;
        source[key] = "Synthetic disclosure; unchanged";
        source["Xmp.xmpRights.WebStatement"] = "https://example.invalid/rights";
        source["Xmp.tiff.Orientation"] = "6";
        std::string packet;
        REQUIRE(Exiv2::XmpParser::encode(packet, source) == 0);
        const auto result = MetadataReconciler::reconcileStandardXmpPacket(
            std::as_bytes(std::span{packet.data(), packet.size()}), clockwisePlan);
        REQUIRE(result.valueIfPresent() != nullptr);
        Exiv2::XmpData output;
        REQUIRE(Exiv2::XmpParser::decode(output,
                                         {reinterpret_cast<const char *>(result.valueIfPresent()->encodedBytes.data()),
                                          result.valueIfPresent()->encodedBytes.size()}) == 0);
        // Exiv2's preferred alias for this URI is iptcExt, not its other accepted
        // spelling Iptc4xmpExt. Look up the actual namespace's native prefix.
        const auto outputKey =
            "Xmp." + Exiv2::XmpProperties::prefix("http://iptc.org/std/Iptc4xmpExt/2008-02-29/") + "." + property;
        CHECK(output[outputKey].toString() == "Synthetic disclosure; unchanged");
        CHECK(output["Xmp.xmpRights.WebStatement"].toString() == "https://example.invalid/rights");
        CHECK(output["Xmp.tiff.Orientation"].toString() == "1");
    }
}

TEST_CASE("Identity reconciliation retains exact metadata bytes including valid thumbnails", "[jpeg][metadata]")
{
    const JpegTransformPlan identity{LosslessTransform::None,
                                     {{48}, {32}},
                                     {{48}, {32}},
                                     EdgeHandlingPolicy::RequirePerfectCoefficientTransform,
                                     OutputScanOrganization::PreserveSource,
                                     {{16}, {16}},
                                     {{0}, {0}}};
    Exiv2::ExifData exif;
    exif["Exif.Image.Orientation"] = static_cast<std::uint16_t>(1);
    const auto thumbnail = createTransformFixture(8, TJSAMP_444, TJCS_YCbCr);
    Exiv2::ExifThumb{exif}.setJpegThumbnail(reinterpret_cast<const Exiv2::byte *>(thumbnail.data()), thumbnail.size());
    Exiv2::Blob encoded;
    Exiv2::ExifParser::encode(encoded, Exiv2::littleEndian, exif);
    const auto input = std::span{reinterpret_cast<const std::byte *>(encoded.data()), encoded.size()};
    const auto result = MetadataReconciler::reconcileExifPayload(input, identity);
    REQUIRE(result.valueIfPresent() != nullptr);
    CHECK(result.valueIfPresent()->encodedBytes == std::vector<std::byte>{input.begin(), input.end()});
    CHECK_FALSE(result.valueIfPresent()->removedEmbeddedThumbnail);
    const auto jpeg = orientationFixture(1, "1");
    const auto scan = JpegSegmentScanner::scan(jpeg);
    REQUIRE(scan.valueIfPresent() != nullptr);
    const auto range = scan.valueIfPresent()->standardXmpPacketRanges().front();
    const auto packet = std::span{jpeg}.subspan(static_cast<std::size_t>(range.offsetBytes),
                                                static_cast<std::size_t>(range.lengthBytes));
    const auto xmpResult = MetadataReconciler::reconcileStandardXmpPacket(packet, identity);
    REQUIRE(xmpResult.valueIfPresent() != nullptr);
    CHECK(xmpResult.valueIfPresent()->encodedBytes == std::vector<std::byte>{packet.begin(), packet.end()});
    CHECK_FALSE(xmpResult.valueIfPresent()->removedEmbeddedThumbnail);
}

TEST_CASE("Metadata analysis uses Exif authority and reports an XMP conflict without mutation", "[jpeg][metadata]")
{
    const auto source = orientationFixture();
    const auto original = source;
    const auto scan = JpegSegmentScanner::scan(source);
    REQUIRE(scan.valueIfPresent() != nullptr);
    REQUIRE(scan.valueIfPresent()->exifTiffDataRanges().size() == 1);
    REQUIRE(scan.valueIfPresent()->standardXmpPacketRanges().size() == 1);
    const auto result = MetadataReconciler::analyzeOrientation(source, *scan.valueIfPresent());
    REQUIRE(result.valueIfPresent() != nullptr);
    CHECK(result.valueIfPresent()->authoritativeOrientation == ExifOrientation::RightTop);
    CHECK(result.valueIfPresent()->exifOrientation == ExifOrientation::RightTop);
    CHECK(result.valueIfPresent()->xmpOrientation == ExifOrientation::BottomRight);
    CHECK(result.valueIfPresent()->hasOrientationConflict);
    CHECK(source == original);
}
