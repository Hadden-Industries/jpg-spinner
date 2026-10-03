#include "CoefficientTransformFixture.h"
#include "internal/JpegSegmentScanner.h"
#include "internal/MetadataReconciler.h"
#include <jpg_spinner/jpeg/JpegImageAnalyzer.h>

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <string_view>

using namespace jpg_spinner::domain;
using namespace jpg_spinner::jpeg::internal;
using namespace jpg_spinner::test_support;

namespace
{
constexpr std::string_view packetGuid = "0123456789ABCDEF0123456789ABCDEF";

void insertApp1(std::vector<std::byte> &jpeg, const std::string &payload)
{
    REQUIRE(payload.size() <= 65533);
    const auto length = payload.size() + 2;
    std::vector<std::byte> marker{std::byte{0xff}, std::byte{0xe1}, static_cast<std::byte>(length >> 8),
                                  static_cast<std::byte>(length & 255)};
    for (unsigned char value : payload)
        marker.push_back(static_cast<std::byte>(value));
    jpeg.insert(jpeg.begin() + 2, marker.begin(), marker.end());
}

std::string rdfPacket(const std::string &properties)
{
    return "<x:xmpmeta xmlns:x='adobe:ns:meta/'><rdf:RDF xmlns:rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#'>"
           "<rdf:Description rdf:about='' xmlns:note='http://ns.adobe.com/xmp/note/' "
           "xmlns:tiff='http://ns.adobe.com/tiff/1.0/' xmlns:exif='http://ns.adobe.com/exif/1.0/' "
           "xmlns:xmp='http://ns.adobe.com/xap/1.0/' xmlns:dc='http://purl.org/dc/elements/1.1/'>" +
           properties + "</rdf:Description></rdf:RDF></x:xmpmeta>";
}

std::vector<std::byte> extendedFixture(const std::string &extension, const std::string &declaredGuid)
{
    auto jpeg = createTransformFixture(8, TJSAMP_420, TJCS_YCbCr);
    std::string standard{"http://ns.adobe.com/xap/1.0/\0", 29};
    standard +=
        rdfPacket(declaredGuid.empty() ? "" : "<note:HasExtendedXMP>" + declaredGuid + "</note:HasExtendedXMP>");
    insertApp1(jpeg, standard);
    const auto split = extension.size() / 2;
    for (const auto offset : {std::size_t{0}, split})
    {
        std::string payload{"http://ns.adobe.com/xmp/extension/\0", 35};
        payload += packetGuid;
        for (const auto value : {extension.size(), offset})
            for (int shift = 24; shift >= 0; shift -= 8)
                payload.push_back(static_cast<char>((value >> shift) & 255));
        payload += extension.substr(offset, offset == 0 ? split : extension.size() - split);
        // Inserting both at SOI deliberately reverses their physical order.
        insertApp1(jpeg, payload);
    }
    return jpeg;
}
} // namespace

TEST_CASE("Extended XMP requires native standard-packet GUID correlation", "[jpeg][metadata][extended-xmp]")
{
    for (const std::string declaration : {"", "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF", "0123456789abcdef0123456789abcdef"})
    {
        CAPTURE(declaration);
        const auto jpeg = extendedFixture(rdfPacket("<dc:source>Unchanged</dc:source>"), declaration);
        const auto scan = JpegSegmentScanner::scan(jpeg);
        REQUIRE(scan.valueIfPresent() != nullptr);
        REQUIRE(scan.valueIfPresent()->extendedXmpChunks().size() == 2);
        const auto result = MetadataReconciler::validateExtendedXmpPreservation(jpeg, *scan.valueIfPresent(),
                                                                                LosslessTransform::Rotate90Clockwise);
        REQUIRE(result.errorIfPresent() != nullptr);
        CHECK(result.errorIfPresent()->code == ImageProcessingErrorCode::MalformedImageMetadata);
    }
}

TEST_CASE("Complete reordered extension is preserved only if its properties need no mutation",
          "[jpeg][metadata][extended-xmp]")
{
    const auto jpeg = extendedFixture(rdfPacket("<dc:source>Unchanged</dc:source>"), std::string{packetGuid});
    const auto original = jpeg;
    const auto scan = JpegSegmentScanner::scan(jpeg);
    REQUIRE(scan.valueIfPresent() != nullptr);
    const auto &chunks = scan.valueIfPresent()->extendedXmpChunks();
    REQUIRE(chunks.size() == 2);
    REQUIRE(chunks[0].packetDataRange.offsetBytes > chunks[1].packetDataRange.offsetBytes);
    const auto result = MetadataReconciler::validateExtendedXmpPreservation(jpeg, *scan.valueIfPresent(),
                                                                            LosslessTransform::Rotate90Clockwise);
    REQUIRE(result.valueIfPresent() != nullptr);
    CHECK(jpeg == original);
}

TEST_CASE("Unparseable or affected extension cannot be partially rewritten", "[jpeg][metadata][extended-xmp]")
{
    for (const auto &extension : {std::string{"not RDF"}, rdfPacket("<tiff:Orientation>6</tiff:Orientation>"),
                                  rdfPacket("<tiff:ImageWidth>48</tiff:ImageWidth>"),
                                  rdfPacket("<exif:PixelYDimension>32</exif:PixelYDimension>"),
                                  rdfPacket("<xmp:Thumbnails><rdf:Alt/></xmp:Thumbnails>")})
    {
        const auto jpeg = extendedFixture(extension, std::string{packetGuid});
        const auto scan = JpegSegmentScanner::scan(jpeg);
        REQUIRE(scan.valueIfPresent() != nullptr);
        const auto result = MetadataReconciler::validateExtendedXmpPreservation(jpeg, *scan.valueIfPresent(),
                                                                                LosslessTransform::Rotate90Clockwise);
        REQUIRE(result.errorIfPresent() != nullptr);
        CHECK(result.errorIfPresent()->code == ImageProcessingErrorCode::ExtendedXmpMutationNotSupported);
    }
}

TEST_CASE("Extension-held orientation cannot masquerade as an identity image", "[jpeg][metadata][extended-xmp]")
{
    // Neither Exif nor standard XMP contains an orientation. The analyzer must
    // not infer TopLeft and bypass the no-partial-extension-rewrite contract.
    const auto jpeg = extendedFixture(rdfPacket("<tiff:Orientation>6</tiff:Orientation>"), std::string{packetGuid});
    const auto result = jpg_spinner::jpeg::JpegImageAnalyzer::analyze(jpeg);
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == ImageProcessingErrorCode::ExtendedXmpMutationNotSupported);
}
