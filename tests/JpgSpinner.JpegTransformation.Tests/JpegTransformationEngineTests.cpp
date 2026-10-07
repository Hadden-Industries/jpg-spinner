#include "ValidatedOutputFixture.h"
#include <jpg_spinner/jpeg/JpegImageAnalyzer.h>
#include <jpg_spinner/jpeg/LibJpegTurboTransformationEngine.h>
#include <catch2/catch_test_macros.hpp>
#include "DeterministicJpegFixtureFactory.h"
#include "DeterministicJpegTransformationEngine.h"
#include "internal/JpegOutputValidator.h"
#include <barrier>
#include <future>

using namespace jpg_spinner::domain;
using namespace jpg_spinner::jpeg;
using namespace jpg_spinner::test_support;

TEST_CASE("Public engine owns an approved output for every Exif orientation", "[jpeg][engine]")
{
    const LibJpegTurboTransformationEngine nativeEngine;
    const JpegTransformationEngine &engine = nativeEngine;
    for (unsigned short orientation = 1; orientation <= 8; ++orientation)
    {
        CAPTURE(orientation);
        const auto source = createMetadataSource(orientation);
        const auto analysis = JpegImageAnalyzer::analyze(source);
        REQUIRE(analysis.valueIfPresent() != nullptr);
        REQUIRE(static_cast<unsigned>(analysis.valueIfPresent()->authoritativeOrientation) == orientation);
        const auto result = engine.createValidatedOutput(source, *analysis.valueIfPresent());
        CHECK(result.valueIfPresent() != nullptr);
        if (const auto *output = result.valueIfPresent())
        {
            CHECK(output->outputProperties().dimensions ==
                  (orientation >= 5 ? ImageDimensions{{32}, {48}} : ImageDimensions{{48}, {32}}));
            CHECK(output->metadataEvidence().hasExifOrientation);
            CHECK(output->metadataEvidence().hasStandardXmpOrientation);
            CHECK(output->preservedMarkerEvidence().iccProfileSha256.has_value());
            CHECK(!output->encodedBytes().empty());
            // A second native reader observes upright orientation and planned
            // dimensions; this is not the engine's evidence getter as oracle.
            const auto observed = JpegImageAnalyzer::analyze(output->encodedBytes());
            REQUIRE(observed.valueIfPresent() != nullptr);
            CHECK(observed.valueIfPresent()->authoritativeOrientation == ExifOrientation::TopLeft);
            CHECK(observed.valueIfPresent()->sourceProperties.dimensions == output->outputProperties().dimensions);
        }
    }
}

TEST_CASE("Deterministic engine records concurrency and rejects cancellation after controlled completion",
          "[jpeg][engine]")
{
    const auto source = createMetadataSource();
    const auto analysis = JpegImageAnalyzer::analyze(source);
    REQUIRE(analysis.valueIfPresent() != nullptr);
    std::barrier concurrentFactories{2};
    DeterministicJpegTransformationEngine engine{[&](auto bytes, auto approved, auto cancellation) {
        concurrentFactories.arrive_and_wait();
        return LibJpegTurboTransformationEngine{}.createValidatedOutput(std::move(bytes), std::move(approved),
                                                                        cancellation);
    }};
    auto first = std::async(std::launch::async,
                            [&] { return engine.createValidatedOutput(source, *analysis.valueIfPresent()); });
    auto second = std::async(std::launch::async,
                             [&] { return engine.createValidatedOutput(source, *analysis.valueIfPresent()); });
    REQUIRE(first.get().valueIfPresent() != nullptr);
    REQUIRE(second.get().valueIfPresent() != nullptr);
    CHECK(engine.invocationCount() == 2);
    CHECK(engine.maximumConcurrentCalls() == 2);

    std::stop_source cancellation;
    DeterministicJpegTransformationEngine cancelledEngine{[&](auto bytes, auto approved, auto token) {
        auto result =
            LibJpegTurboTransformationEngine{}.createValidatedOutput(std::move(bytes), std::move(approved), token);
        REQUIRE(result.valueIfPresent() != nullptr);
        cancellation.request_stop();
        return result;
    }};
    const auto cancelled =
        cancelledEngine.createValidatedOutput(source, *analysis.valueIfPresent(), cancellation.get_token());
    REQUIRE(cancelled.errorIfPresent() != nullptr);
    CHECK(cancelled.errorIfPresent()->code == ImageProcessingErrorCode::Cancelled);
    CHECK(std::get<CoefficientTransformationExecutionState>(cancelled.errorIfPresent()->diagnosticContext) ==
          CoefficientTransformationExecutionState::Started);
}

TEST_CASE("Public engine contract retains a real validator rejection from controlled completion", "[jpeg][engine]")
{
    const auto source = createMetadataSource(1);
    const auto analysis = JpegImageAnalyzer::analyze(source);
    REQUIRE(analysis.valueIfPresent() != nullptr);
    // Exercise the public contract through the test adapter, not a phase hook
    // in the production engine. The failure itself comes from real validation
    // of one corrupted retained marker, rather than a fabricated domain error.
    DeterministicJpegTransformationEngine controlled{[](auto bytes, auto approved, auto cancellation) {
        const auto inventory = jpg_spinner::jpeg::internal::JpegSegmentScanner::scan(bytes);
        REQUIRE(inventory.valueIfPresent() != nullptr);
        auto output = createReconciledOutput(bytes, approved.transformPlan);
        const auto out = jpg_spinner::jpeg::internal::JpegSegmentScanner::scan(output);
        REQUIRE(out.valueIfPresent() != nullptr);
        const auto &icc = out.valueIfPresent()->iccProfileChunks().front().profileDataRange;
        output[static_cast<std::size_t>(icc.offsetBytes)] ^= std::byte{1};
        return jpg_spinner::jpeg::internal::JpegOutputValidator::validate(
            bytes, *inventory.valueIfPresent(), approved.transformPlan, std::move(output), cancellation);
    }};
    const JpegTransformationEngine &engine = controlled;
    const auto result = engine.createValidatedOutput(source, *analysis.valueIfPresent());
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == ImageProcessingErrorCode::OutputValidationFailed);
    CHECK(result.errorIfPresent()->stage == ImageProcessingStage::OutputValidation);
    CHECK(std::get<JpegOutputValidationRule>(result.errorIfPresent()->diagnosticContext) ==
          JpegOutputValidationRule::IccProfilePreservation);
    CHECK(controlled.invocationCount() == 1);
}

TEST_CASE("Engine rechecks approval and applies trim and cancellation policies", "[jpeg][engine]")
{
    const auto source = DeterministicJpegFixtureFactory::createEncodedJpeg(JpegChromaSubsampling::ycbcr420, 6);
    const auto perfect = JpegImageAnalyzer::analyze(source);
    REQUIRE(perfect.errorIfPresent() != nullptr);
    CHECK(perfect.errorIfPresent()->code == ImageProcessingErrorCode::PerfectCoefficientTransformUnavailable);
    const auto analysis = JpegImageAnalyzer::analyze(source, EdgeHandlingPolicy::TrimPartialMinimumCodedUnits);
    REQUIRE(analysis.valueIfPresent() != nullptr);
    const LibJpegTurboTransformationEngine engine;
    const auto trimmed = engine.createValidatedOutput(source, *analysis.valueIfPresent());
    REQUIRE(trimmed.valueIfPresent() != nullptr);
    CHECK(trimmed.valueIfPresent()->outputProperties().dimensions == ImageDimensions{{16}, {31}});
    std::stop_source cancellation;
    cancellation.request_stop();
    const auto cancelled = engine.createValidatedOutput(source, *analysis.valueIfPresent(), cancellation.get_token());
    REQUIRE(cancelled.errorIfPresent() != nullptr);
    CHECK(cancelled.errorIfPresent()->code == ImageProcessingErrorCode::Cancelled);
    CHECK(std::get<CoefficientTransformationExecutionState>(cancelled.errorIfPresent()->diagnosticContext) ==
          CoefficientTransformationExecutionState::NotStarted);
    const auto changed = createMetadataSource(3);
    const auto original = createMetadataSource(6);
    const auto originalAnalysis = JpegImageAnalyzer::analyze(original);
    REQUIRE(originalAnalysis.valueIfPresent() != nullptr);
    const auto rejected = engine.createValidatedOutput(changed, *originalAnalysis.valueIfPresent());
    REQUIRE(rejected.errorIfPresent() != nullptr);
    CHECK(rejected.errorIfPresent()->code == ImageProcessingErrorCode::SourceChangedAfterAnalysis);
}

TEST_CASE("Identity pixel transform resolves a reviewed Exif XMP orientation conflict", "[jpeg][engine]")
{
    auto source = createMetadataSource(1);
    const auto attribute = textBytes("tiff:Orientation='1'");
    const auto found = std::search(source.begin(), source.end(), attribute.begin(), attribute.end());
    REQUIRE(found != source.end());
    *(found + 18) = std::byte{'6'};
    const auto analysis = JpegImageAnalyzer::analyze(source);
    REQUIRE(analysis.valueIfPresent() != nullptr);
    REQUIRE(analysis.valueIfPresent()->transformPlan.transform == LosslessTransform::None);
    REQUIRE(analysis.valueIfPresent()->findings.size() == 1);
    REQUIRE(jpegAnalysisFindingCode(analysis.valueIfPresent()->findings.front()) ==
            JpegAnalysisFindingCode::ExifXmpOrientationConflict);
    const auto result = LibJpegTurboTransformationEngine{}.createValidatedOutput(source, *analysis.valueIfPresent());
    REQUIRE(result.valueIfPresent() != nullptr);
    const auto outputAnalysis = JpegImageAnalyzer::analyze(result.valueIfPresent()->encodedBytes());
    REQUIRE(outputAnalysis.valueIfPresent() != nullptr);
    CHECK(outputAnalysis.valueIfPresent()->findings.empty());
}

TEST_CASE("Identity pixel transform corrects stale derived dimensions without removing valid thumbnails",
          "[jpeg][engine]")
{
    auto source = createMetadataSource(1, true);
    const std::array<unsigned char, 12> widthEntry{0, 1, 4, 0, 1, 0, 0, 0, 48, 0, 0, 0};
    const auto found =
        std::search(source.begin(), source.end(), widthEntry.begin(), widthEntry.end(),
                    [](std::byte byte, unsigned char value) { return byte == static_cast<std::byte>(value); });
    REQUIRE(found != source.end());
    *(found + 8) = std::byte{32};
    const auto xmpWidth = textBytes("tiff:ImageWidth='48'");
    const auto xmpFound = std::search(source.begin(), source.end(), xmpWidth.begin(), xmpWidth.end());
    REQUIRE(xmpFound != source.end());
    *(xmpFound + 17) = std::byte{'2'};
    const auto analysis = JpegImageAnalyzer::analyze(source);
    REQUIRE(analysis.valueIfPresent() != nullptr);
    REQUIRE(analysis.valueIfPresent()->transformPlan.transform == LosslessTransform::None);
    const auto result = LibJpegTurboTransformationEngine{}.createValidatedOutput(source, *analysis.valueIfPresent());
    REQUIRE(result.valueIfPresent() != nullptr);
    const auto scan = jpg_spinner::jpeg::internal::JpegSegmentScanner::scan(result.valueIfPresent()->encodedBytes());
    REQUIRE(scan.valueIfPresent() != nullptr);
    const auto range = scan.valueIfPresent()->exifTiffDataRanges().front();
    Exiv2::ExifData observed;
    const auto bytes = result.valueIfPresent()->encodedBytes().subspan(static_cast<std::size_t>(range.offsetBytes),
                                                                       static_cast<std::size_t>(range.lengthBytes));
    REQUIRE(Exiv2::ExifParser::decode(observed, reinterpret_cast<const Exiv2::byte *>(bytes.data()), bytes.size()) !=
            Exiv2::invalidByteOrder);
    CHECK(observed["Exif.Image.ImageWidth"].toInt64() == 48);
    CHECK(std::ranges::any_of(observed, [](const auto &entry) { return entry.groupName() == "Thumbnail"; }));
    const auto xmpRange = scan.valueIfPresent()->standardXmpPacketRanges().front();
    const auto xmpBytes = result.valueIfPresent()->encodedBytes().subspan(
        static_cast<std::size_t>(xmpRange.offsetBytes), static_cast<std::size_t>(xmpRange.lengthBytes));
    Exiv2::XmpData xmp;
    REQUIRE(Exiv2::XmpParser::decode(xmp, {reinterpret_cast<const char *>(xmpBytes.data()), xmpBytes.size()}) == 0);
    CHECK(xmp["Xmp.tiff.ImageWidth"].toString() == "48");
    CHECK_FALSE(result.valueIfPresent()->metadataEvidence().removedEmbeddedThumbnail);
}

TEST_CASE("Engine approves restart-coded axis exchange and progressive conversion", "[jpeg][engine]")
{
    for (const int precision : {8, 12})
        for (const auto scanOrganization :
             {OutputScanOrganization::PreserveSource, OutputScanOrganization::ProgressiveDct})
        {
            CAPTURE(precision, static_cast<int>(scanOrganization));
            auto source = createTransformFixture(precision, TJSAMP_420, TJCS_YCbCr, false, false, 48, 32, 3);
            std::string payload{"http://ns.adobe.com/xap/1.0/\0", 29};
            payload +=
                "<x:xmpmeta xmlns:x='adobe:ns:meta/'><rdf:RDF xmlns:rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#'>"
                "<rdf:Description rdf:about='' xmlns:tiff='http://ns.adobe.com/tiff/1.0/' tiff:Orientation='6'/>"
                "</rdf:RDF></x:xmpmeta>";
            const auto xmp = frameMetadataMarker(0xe1, textBytes(payload));
            source.insert(source.begin() + 2, xmp.begin(), xmp.end());
            const auto analysis = JpegImageAnalyzer::analyze(
                source, EdgeHandlingPolicy::RequirePerfectCoefficientTransform, scanOrganization);
            REQUIRE(analysis.valueIfPresent() != nullptr);
            REQUIRE(analysis.valueIfPresent()->transformPlan.transform == LosslessTransform::Rotate90Clockwise);
            const LibJpegTurboTransformationEngine native;
            const JpegTransformationEngine &engine = native;
            const auto result = engine.createValidatedOutput(source, *analysis.valueIfPresent());
            REQUIRE(result.valueIfPresent() != nullptr);
            CHECK(result.valueIfPresent()->outputProperties().dimensions == ImageDimensions{{32}, {48}});
            const auto observed =
                jpg_spinner::jpeg::internal::JpegSegmentScanner::scan(result.valueIfPresent()->encodedBytes());
            REQUIRE(observed.valueIfPresent() != nullptr);
            const auto dri = std::ranges::find_if(observed.valueIfPresent()->markers(),
                                                  [](const auto &marker) { return marker.markerCode == 0xdd; });
            REQUIRE(dri != observed.valueIfPresent()->markers().end());
            const auto bytes = result.valueIfPresent()->encodedBytes();
            CHECK(bytes[static_cast<std::size_t>(dri->payloadRange.offsetBytes)] == std::byte{0});
            CHECK(bytes[static_cast<std::size_t>(dri->payloadRange.offsetBytes) + 1] == std::byte{3});
            if (scanOrganization == OutputScanOrganization::ProgressiveDct)
                CHECK(observed.valueIfPresent()->scanCount() > 1U);
            else
                CHECK(observed.valueIfPresent()->scanCount() == 1U);
        }
}

TEST_CASE("Engine validation decodes supported precisions color spaces and entropy modes", "[jpeg][engine]")
{
    const LibJpegTurboTransformationEngine engine;
    for (const int precision : {8, 12})
        for (const int colorSpace : {TJCS_GRAY, TJCS_RGB, TJCS_YCbCr, TJCS_CMYK, TJCS_YCCK})
            for (const bool progressive : {false, true})
                for (const bool arithmetic : {false, true})
                {
                    // libjpeg-turbo documents arithmetic support only at 8 bits.
                    if (precision == 12 && arithmetic)
                        continue;
                    CAPTURE(precision, colorSpace, progressive, arithmetic);
                    const auto source =
                        createTransformFixture(precision, colorSpace == TJCS_GRAY ? TJSAMP_GRAY : TJSAMP_444,
                                               colorSpace, progressive, arithmetic);
                    const auto analysis = JpegImageAnalyzer::analyze(source);
                    REQUIRE(analysis.valueIfPresent() != nullptr);
                    const auto result = engine.createValidatedOutput(source, *analysis.valueIfPresent());
                    REQUIRE(result.valueIfPresent() != nullptr);
                    CHECK(result.valueIfPresent()->outputProperties().samplePrecisionBits == precision);
                    CHECK(result.valueIfPresent()->outputProperties().dimensions == ImageDimensions{{48}, {32}});
                    // TurboJPEG and libjpeg deliberately use different enum
                    // encodings. Compare their documented meanings, not integers.
                    const auto expectedColorSpace = colorSpace == TJCS_GRAY    ? JCS_GRAYSCALE
                                                    : colorSpace == TJCS_RGB   ? JCS_RGB
                                                    : colorSpace == TJCS_YCbCr ? JCS_YCbCr
                                                    : colorSpace == TJCS_CMYK  ? JCS_CMYK
                                                                               : JCS_YCCK;
                    REQUIRE(readCoefficients(source).colorSpace == expectedColorSpace);
                    CHECK(readCoefficients(result.valueIfPresent()->encodedBytes()).colorSpace == expectedColorSpace);
                }
}
