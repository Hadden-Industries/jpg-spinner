#include "CoefficientTransformFixture.h"
#include "internal/JpegSegmentScanner.h"
#include "internal/MetadataReconciler.h"

#include <catch2/catch_test_macros.hpp>
#include <exiv2/exiv2.hpp>
#include <string>

using namespace jpg_spinner::domain;
using namespace jpg_spinner::jpeg::internal;
using namespace jpg_spinner::test_support;

namespace
{
std::vector<std::byte> assetFixture(const std::string &properties, std::size_t trailingLength)
{
    const std::string packet =
        "<x:xmpmeta xmlns:x='adobe:ns:meta/'><rdf:RDF xmlns:rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#'>"
        "<rdf:Description rdf:about='' xmlns:provenance='http://purl.org/dc/terms/' "
        "xmlns:camera='http://ns.google.com/photos/1.0/camera/' "
        "xmlns:container='http://ns.google.com/photos/1.0/container/' "
        "xmlns:item='http://ns.google.com/photos/1.0/container/item/'>" +
        properties + "</rdf:Description></rdf:RDF></x:xmpmeta>";
    std::string payload{"http://ns.adobe.com/xap/1.0/\0", 29};
    payload += packet;
    const auto length = payload.size() + 2;
    std::vector<std::byte> marker{std::byte{0xff}, std::byte{0xe1}, static_cast<std::byte>(length >> 8),
                                  static_cast<std::byte>(length & 255)};
    for (unsigned char value : payload)
        marker.push_back(static_cast<std::byte>(value));
    auto jpeg = createTransformFixture(8, TJSAMP_420, TJCS_YCbCr);
    jpeg.insert(jpeg.begin() + 2, marker.begin(), marker.end());
    jpeg.resize(jpeg.size() + trailingLength, std::byte{0});
    return jpeg;
}

std::string motionPhotoProperties(const std::string &videoLength, const std::string &padding = "4")
{
    return "<camera:MotionPhoto>1</camera:MotionPhoto><camera:MotionPhotoVersion>1</camera:MotionPhotoVersion>"
           "<container:Directory><rdf:Seq><rdf:li rdf:parseType='Resource'>"
           "<container:Item item:Mime='image/jpeg' item:Semantic='Primary' item:Padding='" +
           padding +
           "'/>"
           "</rdf:li><rdf:li rdf:parseType='Resource'>"
           "<container:Item item:Mime='video/mp4' item:Semantic='MotionPhoto' item:Length='" +
           videoLength +
           "'/>"
           "</rdf:li></rdf:Seq></container:Directory>";
}
} // namespace

TEST_CASE("External Content Credentials are rejected without fetching their URI", "[jpeg][metadata][asset-policy]")
{
    const auto jpeg =
        assetFixture("<provenance:provenance>https://example.invalid/manifest.c2pa</provenance:provenance>", 0);
    const auto scan = JpegSegmentScanner::scan(jpeg);
    REQUIRE(scan.valueIfPresent() != nullptr);
    const auto result = MetadataReconciler::validateSourceAssetBindings(jpeg, *scan.valueIfPresent());
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == ImageProcessingErrorCode::ContentCredentialsWouldBeInvalidated);
}

TEST_CASE("Motion Photo declarations must account for the complete appended asset", "[jpeg][metadata][asset-policy]")
{
    const auto jpeg = assetFixture(motionPhotoProperties("32"), 36);
    const auto scan = JpegSegmentScanner::scan(jpeg);
    REQUIRE(scan.valueIfPresent() != nullptr);
    REQUIRE(scan.valueIfPresent()->trailingDataRange().lengthBytes == 36);
    const auto range = scan.valueIfPresent()->standardXmpPacketRanges().front();
    Exiv2::XmpData decoded;
    REQUIRE(Exiv2::XmpParser::decode(decoded, {reinterpret_cast<const char *>(jpeg.data() + range.offsetBytes),
                                               static_cast<std::size_t>(range.lengthBytes)}) == 0);
    const auto prefix = Exiv2::XmpProperties::prefix("http://ns.google.com/photos/1.0/container/");
    REQUIRE(dynamic_cast<const Exiv2::XmpValue &>(decoded["Xmp." + prefix + ".Directory"].value()).xmpArrayType() ==
            Exiv2::XmpValue::xaSeq);
    Exiv2::Dictionary namespaces;
    REQUIRE_NOTHROW(Exiv2::XmpProperties::registeredNamespaces(namespaces));
    const auto result = MetadataReconciler::validateSourceAssetBindings(jpeg, *scan.valueIfPresent());
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == ImageProcessingErrorCode::MotionPhotoNotSupported);
}

TEST_CASE("A residual flag is not a Motion Photo and unexplained zero bytes are not padding",
          "[jpeg][metadata][asset-policy]")
{
    const auto still = assetFixture("<camera:MotionPhoto>1</camera:MotionPhoto>", 0);
    const auto scan = JpegSegmentScanner::scan(still);
    REQUIRE(scan.valueIfPresent() != nullptr);
    CHECK(MetadataReconciler::validateSourceAssetBindings(still, *scan.valueIfPresent()).valueIfPresent() != nullptr);
    for (const auto &properties : {std::string{}, std::string{"<camera:MotionPhoto>1</camera:MotionPhoto>"},
                                   motionPhotoProperties("0"), motionPhotoProperties("31"), motionPhotoProperties("33"),
                                   motionPhotoProperties("18446744073709551615"), motionPhotoProperties("32.0")})
    {
        const auto jpeg = assetFixture(properties, 36);
        const auto trailingScan = JpegSegmentScanner::scan(jpeg);
        REQUIRE(trailingScan.valueIfPresent() != nullptr);
        const auto result = MetadataReconciler::validateSourceAssetBindings(jpeg, *trailingScan.valueIfPresent());
        REQUIRE(result.errorIfPresent() != nullptr);
        CHECK(result.errorIfPresent()->code == ImageProcessingErrorCode::UnsupportedTrailingPayload);
    }
}
