#include "CoefficientTransformFixture.h"
#include "internal/JpegOutputValidator.h"
#include "internal/JpegSegmentScanner.h"
#include "internal/Sha256DigestCalculator.h"
#include "ValidatedOutputFixture.h"
#include "DeterministicJpegFixtureFactory.h"
#include <jpg_spinner/jpeg/JpegImageAnalyzer.h>
#include <jpg_spinner/jpeg/LibJpegTurboTransformationEngine.h>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <ranges>
#include <type_traits>

using namespace jpg_spinner::domain;
using namespace jpg_spinner::jpeg::internal;
using namespace jpg_spinner::test_support;

namespace
{
const JpegTransformPlan unchangedPlan{LosslessTransform::None,
                                      {{48}, {32}},
                                      {{48}, {32}},
                                      EdgeHandlingPolicy::RequirePerfectCoefficientTransform,
                                      OutputScanOrganization::PreserveSource,
                                      {{16}, {16}},
                                      {{0}, {0}}};
}

TEST_CASE("Validator rejects independently decodable quantization and restart policy changes", "[jpeg][validator]")
{
    for (const int precision : {8, 12})
        for (const bool progressive : {false, true})
        {
            CAPTURE(precision, progressive);
            const auto source = createTransformFixture(precision, TJSAMP_420, TJCS_YCbCr, progressive);
            const auto scan = JpegSegmentScanner::scan(source);
            REQUIRE(scan.valueIfPresent() != nullptr);
            REQUIRE(
                JpegOutputValidator::validate(source, *scan.valueIfPresent(), unchangedPlan, source).valueIfPresent() !=
                nullptr);
            const auto sourceCoefficients = readCoefficients(source);
            for (const bool changeQuantization : {true, false})
            {
                CAPTURE(changeQuantization);
                auto changed = source;
                if (changeQuantization)
                {
                    const auto table = *std::ranges::find_if(
                        scan.valueIfPresent()->markers(), [](const auto &marker) { return marker.markerCode == 0xdb; });
                    const auto offset = static_cast<std::size_t>(table.payloadRange.offsetBytes);
                    // Native-generated fixture has an eight-bit table zero. Change
                    // only its DC quantizer; the entropy stream remains valid.
                    REQUIRE(changed[offset] == std::byte{0});
                    changed[offset + 1] = static_cast<std::byte>(std::to_integer<unsigned>(changed[offset + 1]) + 1);
                    const auto observed = readCoefficients(changed);
                    CHECK(observed.components.front().quantization.front() ==
                          sourceCoefficients.components.front().quantization.front() + 1);
                    CHECK(observed.components.front().blocks == sourceCoefficients.components.front().blocks);
                }
                else
                {
                    // An interval larger than this image needs no restart marker,
                    // so full decoding alone cannot enforce the approved policy.
                    const std::array payload{std::byte{0xff}, std::byte{0xff}};
                    const auto dri = frameMetadataMarker(0xdd, payload);
                    changed.insert(changed.begin() + 2, dri.begin(), dri.end());
                    // A final reset must not mask the interval actually selected
                    // for this scan. Native decoding still succeeds in this case.
                    const std::array resetPayload{std::byte{0}, std::byte{0}};
                    const auto reset = frameMetadataMarker(0xdd, resetPayload);
                    changed.insert(changed.end() - 2, reset.begin(), reset.end());
                    CHECK(readCoefficients(changed).components.front().blocks ==
                          sourceCoefficients.components.front().blocks);
                }
                const auto changedInventory = JpegSegmentScanner::scan(changed);
                REQUIRE(changedInventory.valueIfPresent() != nullptr);
                if (!changeQuantization)
                {
                    const auto invalidSource = JpegOutputValidator::validate(
                        changed, *changedInventory.valueIfPresent(), unchangedPlan, source);
                    REQUIRE(invalidSource.errorIfPresent() != nullptr);
                    CHECK(std::get<JpegOutputValidationRule>(invalidSource.errorIfPresent()->diagnosticContext) ==
                          JpegOutputValidationRule::RestartIntervalPreservation);
                }
                const auto result =
                    JpegOutputValidator::validate(source, *scan.valueIfPresent(), unchangedPlan, std::move(changed));
                CHECK(result.errorIfPresent() != nullptr);
                if (const auto *error = result.errorIfPresent())
                    CHECK(std::get<JpegOutputValidationRule>(error->diagnosticContext) ==
                          (changeQuantization ? JpegOutputValidationRule::QuantizationTablePreservation
                                              : JpegOutputValidationRule::RestartIntervalPreservation));
            }
        }
}

TEST_CASE("Validator and engine reject independently observed native warnings and fatal failures",
          "[jpeg][validator][engine]")
{
    const auto source = createTransformFixture(8, TJSAMP_420, TJCS_YCbCr);
    const auto scan = JpegSegmentScanner::scan(source);
    REQUIRE(scan.valueIfPresent() != nullptr);
    for (const bool fatal : {false, true})
    {
        CAPTURE(fatal);
        auto corrupt = source;
        if (fatal)
        {
            const auto sos = *std::ranges::find_if(scan.valueIfPresent()->markers(),
                                                   [](const auto &marker) { return marker.markerCode == 0xda; });
            // Missing table three is syntactically legal, but the native
            // reader must report a fatal undefined-Huffman-table failure.
            corrupt[static_cast<std::size_t>(sos.payloadRange.offsetBytes) + 2] = std::byte{0x33};
        }
        else
            replaceFixtureRange(corrupt, scan.valueIfPresent()->entropyCodedDataRanges().front(),
                                std::array{std::byte{0}});
        REQUIRE(JpegSegmentScanner::scan(corrupt).valueIfPresent() != nullptr);
        const std::unique_ptr<void, decltype(&tj3Destroy)> observer(tj3Init(TJINIT_DECOMPRESS), &tj3Destroy);
        REQUIRE(observer != nullptr);
        REQUIRE(tj3Set(observer.get(), TJPARAM_STOPONWARNING, 1) == 0);
        std::array<unsigned char, 48 * 32 * 3> samples{};
        REQUIRE(tj3Decompress8(observer.get(), reinterpret_cast<const unsigned char *>(corrupt.data()), corrupt.size(),
                               samples.data(), 0, TJPF_RGB) != 0);
        REQUIRE(tj3GetErrorCode(observer.get()) == (fatal ? TJERR_FATAL : TJERR_WARNING));
        const auto validated = JpegOutputValidator::validate(source, *scan.valueIfPresent(), unchangedPlan, corrupt);
        REQUIRE(validated.errorIfPresent() != nullptr);
        CHECK(std::get<JpegOutputValidationRule>(validated.errorIfPresent()->diagnosticContext) ==
              JpegOutputValidationRule::FullDecode);
        const auto analyzed = jpg_spinner::jpeg::JpegImageAnalyzer::analyze(corrupt);
        REQUIRE(analyzed.valueIfPresent() != nullptr);
        const jpg_spinner::jpeg::LibJpegTurboTransformationEngine engine;
        const auto transformed = engine.createValidatedOutput(corrupt, *analyzed.valueIfPresent());
        REQUIRE(transformed.errorIfPresent() != nullptr);
        CHECK(transformed.errorIfPresent()->code == ImageProcessingErrorCode::CoefficientTransformationFailed);
        CHECK(transformed.errorIfPresent()->stage == ImageProcessingStage::CoefficientTransformation);
    }
}

TEST_CASE("Metadata-free JPEG approval hashes an empty marker sequence with the native provider",
          "[jpeg][validator][engine]")
{
    auto source = createTransformFixture(8, TJSAMP_420, TJCS_YCbCr);
    const auto original = JpegSegmentScanner::scan(source);
    REQUIRE(original.valueIfPresent() != nullptr);
    for (const auto &marker : std::views::reverse(original.valueIfPresent()->markers()))
        if ((marker.markerCode >= 0xe0 && marker.markerCode <= 0xef) || marker.markerCode == 0xfe)
            replaceFixtureRange(source, marker.encodedRange, {});
    const auto analysis = jpg_spinner::jpeg::JpegImageAnalyzer::analyze(source);
    REQUIRE(analysis.valueIfPresent() != nullptr);
    const jpg_spinner::jpeg::LibJpegTurboTransformationEngine engine;
    const auto result = engine.createValidatedOutput(source, *analysis.valueIfPresent());
    REQUIRE(result.valueIfPresent() != nullptr);
    CHECK(result.valueIfPresent()->preservedMarkerEvidence().metadataMarkerCount == 0);
    // FIPS SHA-256 empty-message known answer, not a second call to the helper.
    constexpr std::string_view expected = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    constexpr std::string_view hex = "0123456789abcdef";
    std::string observed;
    for (const auto byte : result.valueIfPresent()->preservedMarkerEvidence().encodedMetadataSha256)
    {
        observed += hex[std::to_integer<unsigned>(byte) >> 4];
        observed += hex[std::to_integer<unsigned>(byte) & 15];
    }
    CHECK(observed == expected);
    CHECK_FALSE(result.valueIfPresent()->metadataEvidence().hasExifOrientation);
    CHECK_FALSE(result.valueIfPresent()->metadataEvidence().hasStandardXmpOrientation);
}

TEST_CASE("Validator rejects stale canonical metadata and modified preserved markers", "[jpeg][validator]")
{
    const auto source = createMetadataSource();
    const auto scan = JpegSegmentScanner::scan(source);
    REQUIRE(scan.valueIfPresent() != nullptr);
    const JpegTransformPlan plan{LosslessTransform::Rotate90Clockwise,
                                 {{48}, {32}},
                                 {{32}, {48}},
                                 EdgeHandlingPolicy::RequirePerfectCoefficientTransform,
                                 OutputScanOrganization::PreserveSource,
                                 {{16}, {16}},
                                 {{0}, {0}}};
    const auto good = createReconciledOutput(source, plan);
    REQUIRE(JpegOutputValidator::validate(source, *scan.valueIfPresent(), plan, good).valueIfPresent() != nullptr);
    const auto outputScan = JpegSegmentScanner::scan(good);
    REQUIRE(outputScan.valueIfPresent() != nullptr);
    REQUIRE(outputScan.valueIfPresent()->exifTiffDataRanges().size() == 1);
    REQUIRE(outputScan.valueIfPresent()->standardXmpPacketRanges().size() == 1);
    for (const auto rule :
         {JpegOutputValidationRule::CanonicalExifOrientation, JpegOutputValidationRule::CanonicalStandardXmpOrientation,
          JpegOutputValidationRule::DerivedDimensions, JpegOutputValidationRule::IccProfilePreservation,
          JpegOutputValidationRule::PreservedMarkerSequence})
    {
        CAPTURE(static_cast<int>(rule));
        auto corrupt = good;
        if (rule == JpegOutputValidationRule::CanonicalExifOrientation ||
            rule == JpegOutputValidationRule::DerivedDimensions)
        {
            const auto range = outputScan.valueIfPresent()->exifTiffDataRanges().front();
            std::vector<Exiv2::byte> tiff;
            for (const auto byte : std::span{good}.subspan(static_cast<std::size_t>(range.offsetBytes),
                                                           static_cast<std::size_t>(range.lengthBytes)))
                tiff.push_back(std::to_integer<Exiv2::byte>(byte));
            Exiv2::ExifData metadata;
            REQUIRE(Exiv2::ExifParser::decode(metadata, tiff.data(), tiff.size()) == Exiv2::littleEndian);
            REQUIRE(metadata["Exif.Image.Orientation"].toInt64() == 1);
            REQUIRE(metadata["Exif.Image.ImageWidth"].toInt64() == 32);
            // Mutate one literal little-endian IFD entry, retaining every
            // surrounding byte. Native observations above and below establish
            // its identity; no serializer can change a second property.
            const std::array<unsigned char, 12> pattern =
                rule == JpegOutputValidationRule::CanonicalExifOrientation
                    ? std::array<unsigned char, 12>{0x12, 1, 3, 0, 1, 0, 0, 0, 1, 0, 0, 0}
                    : std::array<unsigned char, 12>{0, 1, 4, 0, 1, 0, 0, 0, 32, 0, 0, 0};
            const auto position = std::search(tiff.begin(), tiff.end(), pattern.begin(), pattern.end());
            REQUIRE(position != tiff.end());
            REQUIRE(std::search(position + 1, tiff.end(), pattern.begin(), pattern.end()) == tiff.end());
            const auto offset = static_cast<std::size_t>(position - tiff.begin()) + 8;
            tiff[offset] = rule == JpegOutputValidationRule::CanonicalExifOrientation ? 6 : 48;
            Exiv2::ExifData mutatedMetadata;
            REQUIRE(Exiv2::ExifParser::decode(mutatedMetadata, tiff.data(), tiff.size()) == Exiv2::littleEndian);
            CHECK(mutatedMetadata["Exif.Image.Orientation"].toInt64() ==
                  (rule == JpegOutputValidationRule::CanonicalExifOrientation ? 6 : 1));
            CHECK(mutatedMetadata["Exif.Image.ImageWidth"].toInt64() ==
                  (rule == JpegOutputValidationRule::DerivedDimensions ? 48 : 32));
            corrupt[static_cast<std::size_t>(range.offsetBytes) + offset] = static_cast<std::byte>(tiff[offset]);
        }
        else if (rule == JpegOutputValidationRule::CanonicalStandardXmpOrientation)
        {
            const auto range = outputScan.valueIfPresent()->standardXmpPacketRanges().front();
            Exiv2::XmpData metadata;
            REQUIRE(Exiv2::XmpParser::decode(metadata, {reinterpret_cast<const char *>(good.data() + range.offsetBytes),
                                                        static_cast<std::size_t>(range.lengthBytes)}) == 0);
            REQUIRE(metadata["Xmp.tiff.Orientation"].toString() == "1");
            metadata["Xmp.tiff.Orientation"] = "6";
            std::string packet;
            REQUIRE(Exiv2::XmpParser::encode(packet, metadata) == 0);
            std::string payload{"http://ns.adobe.com/xap/1.0/\0", 29};
            payload += packet;
            const auto marker = *std::ranges::find_if(outputScan.valueIfPresent()->markers(), [&](const auto &entry) {
                return entry.payloadRange.offsetBytes + 29 == range.offsetBytes;
            });
            replaceFixtureRange(corrupt, marker.encodedRange, frameMetadataMarker(0xe1, textBytes(payload)));
        }
        else
        {
            const auto code = rule == JpegOutputValidationRule::IccProfilePreservation ? 0xe2 : 0xfe;
            const auto marker = *std::ranges::find_if(outputScan.valueIfPresent()->markers(),
                                                      [=](const auto &entry) { return entry.markerCode == code; });
            corrupt[static_cast<std::size_t>(marker.payloadRange.offsetBytes + marker.payloadRange.lengthBytes - 1)] ^=
                std::byte{1};
        }
        REQUIRE(JpegSegmentScanner::scan(corrupt).valueIfPresent() != nullptr);
        const auto invalid = JpegOutputValidator::validate(source, *scan.valueIfPresent(), plan, std::move(corrupt));
        CHECK(invalid.errorIfPresent() != nullptr);
        if (const auto *error = invalid.errorIfPresent())
        {
            CHECK(error->code == ImageProcessingErrorCode::OutputValidationFailed);
            CHECK(std::get<JpegOutputValidationRule>(error->diagnosticContext) == rule);
        }
    }
}

TEST_CASE("Validator approves a real complete JPEG and rejects its truncated copy", "[jpeg][validator]")
{
    const auto source = createTransformFixture(8, TJSAMP_420, TJCS_YCbCr);
    const auto scan = JpegSegmentScanner::scan(source);
    REQUIRE(scan.valueIfPresent() != nullptr);
    static_assert(!std::is_copy_constructible_v<ValidatedJpegOutput>);
    static_assert(std::is_nothrow_move_constructible_v<ValidatedJpegOutput>);
    const auto valid = JpegOutputValidator::validate(source, *scan.valueIfPresent(), unchangedPlan, source);
    REQUIRE(valid.valueIfPresent() != nullptr);
    CHECK(valid.valueIfPresent()->outputProperties().dimensions == ImageDimensions{{48}, {32}});
    CHECK(std::ranges::equal(valid.valueIfPresent()->encodedBytes(), source));

    auto truncated = source;
    truncated.pop_back();
    const auto invalid =
        JpegOutputValidator::validate(source, *scan.valueIfPresent(), unchangedPlan, std::move(truncated));
    REQUIRE(invalid.errorIfPresent() != nullptr);
    CHECK(invalid.errorIfPresent()->code == ImageProcessingErrorCode::OutputValidationFailed);
    CHECK(std::get<JpegOutputValidationRule>(invalid.errorIfPresent()->diagnosticContext) ==
          JpegOutputValidationRule::CompleteJpegStructure);
}

TEST_CASE("Validator independently rejects restored Photoshop thumbnails and changed retained resources",
          "[jpeg][validator]")
{
    // Independently authored 8BIM records: empty Pascal name, big-endian ID
    // and four-byte body length. Neither production writer builds this oracle.
    const auto retained = textBytes(std::string_view{"8BIM\x04\x0a\0\0\0\0\0\2ok", 14});
    const auto preview = textBytes(std::string_view{"8BIM\x04\x0c\0\0\0\0\0\4tiny", 16});
    std::string payload{"Photoshop 3.0\0", 14};
    payload.append(reinterpret_cast<const char *>(retained.data()), retained.size());
    payload.append(reinterpret_cast<const char *>(preview.data()), preview.size());
    auto source = createMetadataSource(1);
    const auto marker = frameMetadataMarker(0xed, textBytes(payload));
    source.insert(source.begin() + 2, marker.begin(), marker.end());
    const auto scan = JpegSegmentScanner::scan(source);
    REQUIRE(scan.valueIfPresent() != nullptr);
    const JpegTransformPlan plan{LosslessTransform::FlipHorizontal,
                                 {{48}, {32}},
                                 {{48}, {32}},
                                 EdgeHandlingPolicy::RequirePerfectCoefficientTransform,
                                 OutputScanOrganization::PreserveSource,
                                 {{16}, {16}},
                                 {{0}, {0}}};
    const auto good = createReconciledOutput(source, plan);
    const auto valid = JpegOutputValidator::validate(source, *scan.valueIfPresent(), plan, good);
    REQUIRE(valid.valueIfPresent() != nullptr);
    CHECK(valid.valueIfPresent()->metadataEvidence().removedEmbeddedThumbnail);
    const auto outputScan = JpegSegmentScanner::scan(good);
    REQUIRE(outputScan.valueIfPresent() != nullptr);
    const auto outputMarker = *std::ranges::find_if(outputScan.valueIfPresent()->markers(),
                                                    [](const auto &entry) { return entry.markerCode == 0xed; });
    REQUIRE(outputMarker.payloadRange.lengthBytes == 28);
    for (const bool restorePreview : {true, false})
    {
        CAPTURE(restorePreview);
        auto corrupt = good;
        if (restorePreview)
            replaceFixtureRange(corrupt, outputMarker.encodedRange, marker);
        else
            corrupt[static_cast<std::size_t>(outputMarker.payloadRange.offsetBytes) + 26] = std::byte{'X'};
        REQUIRE(JpegSegmentScanner::scan(corrupt).valueIfPresent() != nullptr);
        const auto invalid = JpegOutputValidator::validate(source, *scan.valueIfPresent(), plan, std::move(corrupt));
        REQUIRE(invalid.errorIfPresent() != nullptr);
        CHECK(std::get<JpegOutputValidationRule>(invalid.errorIfPresent()->diagnosticContext) ==
              (restorePreview ? JpegOutputValidationRule::RemovedEmbeddedThumbnails
                              : JpegOutputValidationRule::MetadataPreservation));
    }
}

TEST_CASE("Identity validation rejects stale Exif and XMP dimensions independently of the writer", "[jpeg][validator]")
{
    const auto source = createMetadataSource(1, true);
    const auto scan = JpegSegmentScanner::scan(source);
    REQUIRE(scan.valueIfPresent() != nullptr);
    REQUIRE(JpegOutputValidator::validate(source, *scan.valueIfPresent(), unchangedPlan, source).valueIfPresent() !=
            nullptr);
    for (const bool exif : {true, false})
    {
        CAPTURE(exif);
        auto corrupt = source;
        if (exif)
        {
            const std::array<unsigned char, 12> widthEntry{0, 1, 4, 0, 1, 0, 0, 0, 48, 0, 0, 0};
            const auto found =
                std::search(corrupt.begin(), corrupt.end(), widthEntry.begin(), widthEntry.end(),
                            [](std::byte byte, unsigned char value) { return byte == static_cast<std::byte>(value); });
            REQUIRE(found != corrupt.end());
            REQUIRE(std::search(found + 1, corrupt.end(), widthEntry.begin(), widthEntry.end(),
                                [](std::byte byte, unsigned char value) {
                                    return byte == static_cast<std::byte>(value);
                                }) == corrupt.end());
            *(found + 8) = std::byte{32};
        }
        else
        {
            const auto width = textBytes("tiff:ImageWidth='48'");
            const auto found = std::search(corrupt.begin(), corrupt.end(), width.begin(), width.end());
            REQUIRE(found != corrupt.end());
            *(found + 17) = std::byte{'2'};
        }
        REQUIRE(JpegSegmentScanner::scan(corrupt).valueIfPresent() != nullptr);
        const auto invalid =
            JpegOutputValidator::validate(source, *scan.valueIfPresent(), unchangedPlan, std::move(corrupt));
        REQUIRE(invalid.errorIfPresent() != nullptr);
        CHECK(std::get<JpegOutputValidationRule>(invalid.errorIfPresent()->diagnosticContext) ==
              JpegOutputValidationRule::DerivedDimensions);
    }
}

TEST_CASE("Validator independently checks geometry coding and complete entropy decode", "[jpeg][validator]")
{
    const auto source = createTransformFixture(8, TJSAMP_420, TJCS_YCbCr);
    const auto scan = JpegSegmentScanner::scan(source);
    REQUIRE(scan.valueIfPresent() != nullptr);
    for (const auto rule : {JpegOutputValidationRule::PlannedDimensions, JpegOutputValidationRule::SamplePrecision,
                            JpegOutputValidationRule::ScanOrganization, JpegOutputValidationRule::EntropyCodingMode,
                            JpegOutputValidationRule::ComponentOrganization, JpegOutputValidationRule::FullDecode})
    {
        CAPTURE(static_cast<int>(rule));
        auto corrupt = source;
        if (rule == JpegOutputValidationRule::PlannedDimensions)
            corrupt = createTransformFixture(8, TJSAMP_420, TJCS_YCbCr, false, false, 56, 32);
        else if (rule == JpegOutputValidationRule::SamplePrecision)
            corrupt = createTransformFixture(12, TJSAMP_420, TJCS_YCbCr);
        else if (rule == JpegOutputValidationRule::ScanOrganization)
            corrupt = createTransformFixture(8, TJSAMP_420, TJCS_YCbCr, true);
        else if (rule == JpegOutputValidationRule::EntropyCodingMode)
            corrupt = createTransformFixture(8, TJSAMP_420, TJCS_YCbCr, false, true);
        else if (rule == JpegOutputValidationRule::ComponentOrganization)
        {
            // SOF component quantization-table selector remains legal, but
            // differs from the source. Do not mutate entropy or scan framing.
            const auto offset = static_cast<std::size_t>(scan.valueIfPresent()->frameHeader().encodedRange.offsetBytes);
            corrupt[offset + 12] = std::byte{1};
        }
        else
        {
            const auto entropy = scan.valueIfPresent()->entropyCodedDataRanges().front();
            REQUIRE(entropy.lengthBytes > 16);
            replaceFixtureRange(corrupt, entropy, std::array{std::byte{0}});
        }
        REQUIRE(JpegSegmentScanner::scan(corrupt).valueIfPresent() != nullptr);
        const auto invalid =
            JpegOutputValidator::validate(source, *scan.valueIfPresent(), unchangedPlan, std::move(corrupt));
        REQUIRE(invalid.errorIfPresent() != nullptr);
        CHECK(invalid.errorIfPresent()->code == ImageProcessingErrorCode::OutputValidationFailed);
        CHECK(std::get<JpegOutputValidationRule>(invalid.errorIfPresent()->diagnosticContext) == rule);
    }
}

TEST_CASE("Validator rejects removed reordered and changed metadata facts", "[jpeg][validator]")
{
    const auto source = createMetadataSource();
    const auto scan = JpegSegmentScanner::scan(source);
    REQUIRE(scan.valueIfPresent() != nullptr);
    const JpegTransformPlan plan{LosslessTransform::Rotate90Clockwise,
                                 {{48}, {32}},
                                 {{32}, {48}},
                                 EdgeHandlingPolicy::RequirePerfectCoefficientTransform,
                                 OutputScanOrganization::PreserveSource,
                                 {{16}, {16}},
                                 {{0}, {0}}};
    const auto good = createReconciledOutput(source, plan);
    const auto outputScan = JpegSegmentScanner::scan(good);
    REQUIRE(outputScan.valueIfPresent() != nullptr);
    for (const auto fault : {"missing", "reordered", "camera-model"})
    {
        CAPTURE(fault);
        auto corrupt = good;
        const auto &markers = outputScan.valueIfPresent()->markers();
        if (std::string_view{fault} == "camera-model")
        {
            const auto model = textBytes("Preservation camera");
            const auto found = std::search(corrupt.begin(), corrupt.end(), model.begin(), model.end());
            REQUIRE(found != corrupt.end());
            *found = std::byte{'X'};
        }
        else
        {
            const auto first =
                std::ranges::find_if(markers, [](const auto &marker) { return marker.markerCode == 0xfe; });
            REQUIRE(first != markers.end());
            const auto next =
                std::find_if(first + 1, markers.end(), [](const auto &marker) { return marker.markerCode == 0xfe; });
            REQUIRE(next != markers.end());
            if (std::string_view{fault} == "missing")
                replaceFixtureRange(corrupt, first->encodedRange, {});
            else
            {
                const auto firstBytes =
                    std::span{good}.subspan(static_cast<std::size_t>(first->encodedRange.offsetBytes),
                                            static_cast<std::size_t>(first->encodedRange.lengthBytes));
                const auto nextBytes =
                    std::span{good}.subspan(static_cast<std::size_t>(next->encodedRange.offsetBytes),
                                            static_cast<std::size_t>(next->encodedRange.lengthBytes));
                replaceFixtureRange(corrupt, next->encodedRange, firstBytes);
                replaceFixtureRange(corrupt, first->encodedRange, nextBytes);
            }
        }
        REQUIRE(JpegSegmentScanner::scan(corrupt).valueIfPresent() != nullptr);
        const auto result = JpegOutputValidator::validate(source, *scan.valueIfPresent(), plan, std::move(corrupt));
        REQUIRE(result.errorIfPresent() != nullptr);
        CHECK(std::get<JpegOutputValidationRule>(result.errorIfPresent()->diagnosticContext) ==
              (std::string_view{fault} == "camera-model" ? JpegOutputValidationRule::MetadataPreservation
                                                         : JpegOutputValidationRule::PreservedMarkerSequence));
    }
}

TEST_CASE("Validator rejects an otherwise canonical Exif thumbnail restored after transformation", "[jpeg][validator]")
{
    const auto source = createMetadataSource(1, true);
    const auto scan = JpegSegmentScanner::scan(source);
    REQUIRE(scan.valueIfPresent() != nullptr);
    const JpegTransformPlan plan{LosslessTransform::FlipHorizontal,
                                 {{48}, {32}},
                                 {{48}, {32}},
                                 EdgeHandlingPolicy::RequirePerfectCoefficientTransform,
                                 OutputScanOrganization::PreserveSource,
                                 {{16}, {16}},
                                 {{0}, {0}}};
    auto good = createReconciledOutput(source, plan);
    const auto valid = JpegOutputValidator::validate(source, *scan.valueIfPresent(), plan, good);
    REQUIRE(valid.valueIfPresent() != nullptr);
    CHECK(valid.valueIfPresent()->metadataEvidence().removedEmbeddedThumbnail);
    const auto out = JpegSegmentScanner::scan(good);
    REQUIRE(out.valueIfPresent() != nullptr);
    const auto findExif = [](const auto &inventory) {
        const auto offset = inventory.exifTiffDataRanges().front().offsetBytes;
        return *std::ranges::find_if(inventory.markers(),
                                     [=](const auto &marker) { return marker.payloadRange.offsetBytes + 6 == offset; });
    };
    const auto originalExif = findExif(*scan.valueIfPresent());
    replaceFixtureRange(good, findExif(*out.valueIfPresent()).encodedRange,
                        std::span{source}.subspan(static_cast<std::size_t>(originalExif.encodedRange.offsetBytes),
                                                  static_cast<std::size_t>(originalExif.encodedRange.lengthBytes)));
    const auto restored = JpegOutputValidator::validate(source, *scan.valueIfPresent(), plan, std::move(good));
    REQUIRE(restored.errorIfPresent() != nullptr);
    CHECK(std::get<JpegOutputValidationRule>(restored.errorIfPresent()->diagnosticContext) ==
          JpegOutputValidationRule::RemovedEmbeddedThumbnails);
}

TEST_CASE("SHA256 calculation matches a retained independently checked fixture", "[jpeg][validator]")
{
    const auto source = DeterministicJpegFixtureFactory::createEncodedJpeg(JpegChromaSubsampling::ycbcr444, 6);
    const auto scan = JpegSegmentScanner::scan(source);
    REQUIRE(scan.valueIfPresent() != nullptr);
    // This is the pre-existing independently checked encoded-byte vector in
    // TestSupportContracts.cpp. Its orientation is deliberately not upright,
    // so calculating its digest must not grant an output approval capability.
    const auto result = calculateSha256Digest(source);
    REQUIRE(result.has_value());
    constexpr std::string_view expected = "d588b223e94ccbe8adb4b5beaba86c15768526d39ea4048cccd9c23bf536ba12";
    constexpr std::string_view hex = "0123456789abcdef";
    std::string observed;
    for (auto byte : *result)
    {
        observed += hex[std::to_integer<unsigned>(byte) >> 4];
        observed += hex[std::to_integer<unsigned>(byte) & 15];
    }
    CHECK(observed == expected);
}

TEST_CASE("Validator rejects residual MPF and cancellation", "[jpeg][validator]")
{
    const auto source = createTransformFixture(8, TJSAMP_420, TJCS_YCbCr);
    const auto scan = JpegSegmentScanner::scan(source);
    REQUIRE(scan.valueIfPresent() != nullptr);
    auto corrupt = source;
    const auto mpf = frameMetadataMarker(0xe2, textBytes({"MPF\0", 4}));
    corrupt.insert(corrupt.begin() + 2, mpf.begin(), mpf.end());
    const auto detected = JpegSegmentScanner::scan(corrupt);
    REQUIRE(detected.errorIfPresent() != nullptr);
    REQUIRE(detected.errorIfPresent()->code == ImageProcessingErrorCode::MultiPictureJpegNotSupported);
    const auto result =
        JpegOutputValidator::validate(source, *scan.valueIfPresent(), unchangedPlan, std::move(corrupt));
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(std::get<JpegOutputValidationRule>(result.errorIfPresent()->diagnosticContext) ==
          JpegOutputValidationRule::NoUnsupportedAssetBindings);
    std::stop_source cancellation;
    cancellation.request_stop();
    const auto cancelled =
        JpegOutputValidator::validate(source, *scan.valueIfPresent(), unchangedPlan, source, cancellation.get_token());
    REQUIRE(cancelled.errorIfPresent() != nullptr);
    CHECK(cancelled.errorIfPresent()->code == ImageProcessingErrorCode::Cancelled);
}

TEST_CASE("Validator preserves complete Extended XMP bytes and rejects changed chunk facts", "[jpeg][validator]")
{
    const std::string guid = "0123456789ABCDEF0123456789ABCDEF";
    const std::string extension =
        "<x:xmpmeta xmlns:x='adobe:ns:meta/'><rdf:RDF xmlns:rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#'>"
        "<rdf:Description rdf:about='' xmlns:dc='http://purl.org/dc/elements/1.1/' "
        "dc:source='unchanged'/></rdf:RDF></x:xmpmeta>";
    std::string standard{"http://ns.adobe.com/xap/1.0/\0", 29};
    standard += "<x:xmpmeta xmlns:x='adobe:ns:meta/'><rdf:RDF xmlns:rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#'>"
                "<rdf:Description rdf:about='' xmlns:note='http://ns.adobe.com/xmp/note/' note:HasExtendedXMP='" +
                guid + "'/></rdf:RDF></x:xmpmeta>";
    std::string chunk{"http://ns.adobe.com/xmp/extension/\0", 35};
    chunk += guid;
    for (const auto value : {extension.size(), std::size_t{0}})
        for (int shift = 24; shift >= 0; shift -= 8)
            chunk += static_cast<char>((value >> shift) & 255);
    chunk += extension;
    auto source = createTransformFixture(8, TJSAMP_420, TJCS_YCbCr);
    for (const auto &payload : {standard, chunk})
    {
        const auto marker = frameMetadataMarker(0xe1, textBytes(payload));
        source.insert(source.begin() + 2, marker.begin(), marker.end());
    }
    const auto scan = JpegSegmentScanner::scan(source);
    REQUIRE(scan.valueIfPresent() != nullptr);
    REQUIRE(scan.valueIfPresent()->extendedXmpChunks().size() == 1);
    REQUIRE(JpegOutputValidator::validate(source, *scan.valueIfPresent(), unchangedPlan, source).valueIfPresent() !=
            nullptr);
    const auto &reference = scan.valueIfPresent()->extendedXmpChunks().front();
    for (const auto fault : {"bytes", "guid", "offset"})
    {
        CAPTURE(fault);
        auto corrupt = source;
        const auto dataOffset = static_cast<std::size_t>(reference.packetDataRange.offsetBytes);
        if (std::string_view{fault} == "bytes")
            corrupt[dataOffset + 1] ^= std::byte{1};
        else if (std::string_view{fault} == "guid")
            corrupt[dataOffset - 40] = std::byte{'F'};
        else
            corrupt[dataOffset - 1] = std::byte{1};
        const auto result =
            JpegOutputValidator::validate(source, *scan.valueIfPresent(), unchangedPlan, std::move(corrupt));
        REQUIRE(result.errorIfPresent() != nullptr);
        CHECK(std::get<JpegOutputValidationRule>(result.errorIfPresent()->diagnosticContext) ==
              (std::string_view{fault} == "offset" ? JpegOutputValidationRule::CompleteJpegStructure
                                                   : JpegOutputValidationRule::ExtendedXmpPreservation));
    }
}
