#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <jpg_spinner/batch/BatchProcessingCoordinator.h>
#include <jpg_spinner/jpeg/LibJpegTurboTransformationEngine.h>
#include "DeterministicJpegFixtureFactory.h"
#include "DeterministicJpegTransformationEngine.h"
#include <catch2/catch_test_macros.hpp>
#include <clocale>
#include <atomic>
#include <catch2/generators/catch_generators.hpp>
#include <future>
#include <latch>
#include <chrono>
#include <iostream>
#include <locale>

namespace
{
using namespace jpg_spinner;
using namespace jpg_spinner::batch_processing;

/// This source double supplies real JPEGs but has no filesystem authority.
/// Native capture/identity/commit effects are qualified by Storage integration tests.
class RecordedImageSource final : public domain::ImageSource
{
  public:
    explicit RecordedImageSource(std::vector<std::byte> bytes) : bytes_(std::move(bytes))
    {
    }
    domain::ImageProcessingResult<domain::CapturedImageSource> capture(std::uint64_t maximum,
                                                                       std::stop_token cancellation) const override
    {
        ++captures;
        observedEncodedLimitBytes = maximum;
        if (captureObserver)
            captureObserver(captures.load());
        using Result = domain::ImageProcessingResult<domain::CapturedImageSource>;
        if (cancellation.stop_requested())
            return Result::failure(
                {domain::ImageProcessingErrorCode::Cancelled, domain::ImageProcessingStage::SourceRevisionCapture});
        if (bytes_.size() > maximum)
            return Result::failure({domain::ImageProcessingErrorCode::EncodedFileTooLarge,
                                    domain::ImageProcessingStage::SourceRevisionCapture});
        // Revision is a fixed scheduling observation, not a hash oracle. Native
        // tests independently validate the real digest and storage semantics.
        auto capturedBytes = bytes_;
        if (simulatedEncodedBufferCapacityBytes != 0)
        {
            // Real committed memory, not reserve-only virtual address space.
            // Keep a small valid JPEG length while simulating ownership of the
            // maximum input allocation; this is not a 512-MiB JPEG acceptance test.
            capturedBytes.resize(simulatedEncodedBufferCapacityBytes);
            capturedBytes.resize(bytes_.size());
        }
        return Result::success(
            {std::move(capturedBytes),
             {bytes_.size(),
              domain::SourceFileRevision{}.lastWriteTimeUtc + std::chrono::seconds{revisionGeneration},
              {}}});
    }
    domain::ImageProcessingResult<domain::CommittedImageSource> commit(const domain::SourceFileRevision &,
                                                                       const domain::ValidatedJpegOutput &output,
                                                                       domain::OutputDisposition disposition,
                                                                       std::stop_token cancellation) const override
    {
        ++commits;
        if (commitOperation)
            return commitOperation(output, disposition, cancellation);
        return domain::ImageProcessingResult<domain::CommittedImageSource>::success({L"observed.jpg"});
    }
    mutable std::atomic<unsigned> captures{};
    mutable std::atomic<unsigned> commits{};
    unsigned revisionGeneration{};
    std::size_t simulatedEncodedBufferCapacityBytes{};
    mutable std::uint64_t observedEncodedLimitBytes{};
    std::function<void(unsigned)> captureObserver;
    std::function<domain::ImageProcessingResult<domain::CommittedImageSource>(
        const domain::ValidatedJpegOutput &, domain::OutputDisposition, std::stop_token)>
        commitOperation;

  private:
    const std::vector<std::byte> bytes_;
};

class RecordedImageCollection final : public domain::ImageSourceCollection
{
  public:
    std::vector<domain::DiscoveredImageSource> candidates;
    mutable std::atomic<unsigned> discoveries{};
    domain::ImageProcessingResult<domain::ImageSourceDiscovery> discover(
        domain::TraversalScope, std::stop_token cancellation,
        const domain::ImageSourceDiscoveryObserver &observer) const override
    {
        domain::ImageSourceDiscovery result;
        ++discoveries;
        for (const auto &candidate : candidates)
        {
            if (cancellation.stop_requested())
                break;
            result.candidates.push_back(candidate);
            if (observer)
                observer({result.candidates.size(), result.candidates.size()});
        }
        result.cancellationRequested = cancellation.stop_requested();
        return domain::ImageProcessingResult<domain::ImageSourceDiscovery>::success(std::move(result));
    }
};
} // namespace

TEST_CASE("Batch analysis preserves original Unicode names in ordinal relative-path order",
          "[batch][discovery][ordering]")
{
    const auto jpegBytes = test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
        test_support::JpegChromaSubsampling::ycbcr444, 1);
    const auto source = std::make_shared<RecordedImageSource>(jpegBytes);
    const auto collection = std::make_shared<RecordedImageCollection>();
    collection->candidates = {{L"é.jpg", source}, {L"z.jpg", source}, {L"e\u0301.jpg", source}, {L"A.jpg", source}};
    const BatchProcessingCoordinator coordinator{std::make_shared<jpeg::LibJpegTurboTransformationEngine>()};
    const auto analyzed = coordinator.analyze({collection});
    REQUIRE(analyzed.valueIfPresent() != nullptr);
    const auto files = analyzed.valueIfPresent()->files();
    REQUIRE(files.size() == 4);
    REQUIRE(files[0].relativePath == L"A.jpg");
    REQUIRE(files[1].relativePath == L"e\u0301.jpg");
    REQUIRE(files[2].relativePath == L"z.jpg");
    REQUIRE(files[3].relativePath == L"é.jpg");
    for (const auto &file : files)
    {
        REQUIRE(file.jpegAnalysis != nullptr);
        REQUIRE(file.jpegAnalysis->authoritativeOrientation == domain::ExifOrientation::TopLeft);
        REQUIRE_FALSE(file.error.has_value());
    }
    REQUIRE(source->captures == 4);
    REQUIRE(source->commits == 0);
    REQUIRE(analyzed.valueIfPresent()->discoveryComplete());
}

TEST_CASE("Current cultural collation cannot change batch ordinal order", "[batch][discovery][locale]")
{
    const auto localeName = GENERATE("C", "de-DE");
    const std::locale selectedLocale{localeName};
    struct RestoreLocale final
    {
        std::locale previous;
        ~RestoreLocale()
        {
            std::locale::global(previous);
        }
    } restore{std::locale{}};
    std::locale::global(selectedLocale);
    REQUIRE(std::locale{} == selectedLocale);
    if (std::string_view{localeName} == "de-DE")
    {
        const auto &collation = std::use_facet<std::collate<wchar_t>>(selectedLocale);
        const std::wstring accented{L"é.jpg"};
        const std::wstring ascii{L"z.jpg"};
        REQUIRE(collation.compare(accented.data(), accented.data() + accented.size(), ascii.data(),
                                  ascii.data() + ascii.size()) < 0);
    }
    const auto source =
        std::make_shared<RecordedImageSource>(test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
            test_support::JpegChromaSubsampling::ycbcr444, 1));
    const auto collection = std::make_shared<RecordedImageCollection>();
    collection->candidates = {{L"é.jpg", source}, {L"z.jpg", source}};
    const BatchProcessingCoordinator coordinator{std::make_shared<jpeg::LibJpegTurboTransformationEngine>()};
    const auto analyzed = coordinator.analyze({collection});
    REQUIRE(analyzed.valueIfPresent() != nullptr);
    REQUIRE(analyzed.valueIfPresent()->files()[0].relativePath == L"z.jpg");
    REQUIRE(analyzed.valueIfPresent()->files()[1].relativePath == L"é.jpg");
}

TEST_CASE("Output collisions are refused before reading any candidate", "[batch][discovery][collision]")
{
    const auto source = std::make_shared<RecordedImageSource>(std::vector<std::byte>{std::byte{0}});
    const auto collection = std::make_shared<RecordedImageCollection>();
    // Z sorts between A and a case-sensitively; both separators designate the
    // same output-relative key. Unicode OS casing is exercised independently.
    const auto variant = GENERATE(0, 1, 2);
    if (variant == 0)
        collection->candidates = {{L"a.jpg", source}, {L"Z.jpg", source}, {L"A.jpg", source}};
    if (variant == 1)
        collection->candidates = {{L"nested/a.jpg", source}, {L"nested\\a.jpg", source}};
    if (variant == 2)
        collection->candidates = {{L"É.jpg", source}, {L"é.jpg", source}};
    const BatchProcessingCoordinator coordinator{std::make_shared<jpeg::LibJpegTurboTransformationEngine>()};
    const auto analyzed = coordinator.analyze({collection});
    REQUIRE(analyzed.errorIfPresent() != nullptr);
    REQUIRE(analyzed.errorIfPresent()->code == domain::ImageProcessingErrorCode::OutputRelativePathCollision);
    REQUIRE(source->captures == 0);
    REQUIRE(source->commits == 0);
}

TEST_CASE("Traversal-looking display names never manufacture source authority", "[batch][discovery][containment]")
{
    const auto name = GENERATE(L"../escape.jpg", L"nested/../escape.jpg", L"C:\\escape.jpg", L"/escape.jpg");
    const auto source = std::make_shared<RecordedImageSource>(std::vector<std::byte>{std::byte{0}});
    const auto collection = std::make_shared<RecordedImageCollection>();
    collection->candidates = {{name, source}};
    const BatchProcessingCoordinator coordinator{std::make_shared<jpeg::LibJpegTurboTransformationEngine>()};
    const auto analyzed = coordinator.analyze({collection});
    REQUIRE(analyzed.errorIfPresent() != nullptr);
    REQUIRE(analyzed.errorIfPresent()->code == domain::ImageProcessingErrorCode::SourceDiscoveryFailed);
    REQUIRE(source->captures == 0);
    REQUIRE(source->commits == 0);
}

TEST_CASE("Invalid batch policy is refused before discovery or source allocation", "[batch][policy]")
{
    const auto violation = GENERATE(0, 1, 2, 3, 4, 5, 6, 7);
    const auto source =
        std::make_shared<RecordedImageSource>(test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
            test_support::JpegChromaSubsampling::ycbcr444, 1));
    const auto collection = std::make_shared<RecordedImageCollection>();
    collection->candidates = {{L"valid.jpg", source}};
    const auto production = domain::JpegResourceLimits::production();
    const domain::JpegResourceLimits limits{production.maximumEncodedFileLengthBytes + (violation == 0 ? 1U : 0U),
                                            production.maximumPixelCount + (violation == 1 ? 1U : 0U),
                                            production.maximumMetadataLengthBytes + (violation == 2 ? 1U : 0U),
                                            production.maximumProgressiveScanCount + (violation == 3 ? 1U : 0U)};
    const BatchProcessingRequest request{
        collection,
        violation == 4 ? static_cast<domain::TraversalScope>(-1) : domain::TraversalScope::SelectedFolderOnly,
        violation == 5 ? static_cast<domain::OutputDisposition>(-1) : domain::OutputDisposition::CreateCorrectedCopy,
        violation == 6 ? static_cast<domain::EdgeHandlingPolicy>(-1)
                       : domain::EdgeHandlingPolicy::RequirePerfectCoefficientTransform,
        violation == 7 ? static_cast<domain::OutputScanOrganization>(-1)
                       : domain::OutputScanOrganization::PreserveSource,
        limits};
    const BatchProcessingCoordinator coordinator{std::make_shared<jpeg::LibJpegTurboTransformationEngine>()};
    const auto analyzed = coordinator.analyze(request);
    REQUIRE(analyzed.errorIfPresent() != nullptr);
    REQUIRE(analyzed.errorIfPresent()->code == domain::ImageProcessingErrorCode::InvalidBatchProcessingRequest);
    REQUIRE(collection->discoveries == 0);
    REQUIRE(source->captures == 0);
    REQUIRE(source->commits == 0);
}

TEST_CASE("Every analyzed candidate receives exactly one terminal outcome", "[batch][processing][outcomes]")
{
    const auto eligible =
        std::make_shared<RecordedImageSource>(test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
            test_support::JpegChromaSubsampling::ycbcr444, 3));
    const auto upright =
        std::make_shared<RecordedImageSource>(test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
            test_support::JpegChromaSubsampling::ycbcr444, 1));
    const auto malformed = std::make_shared<RecordedImageSource>(std::vector<std::byte>{std::byte{0}});
    const auto collection = std::make_shared<RecordedImageCollection>();
    collection->candidates = {
        {L"c-malformed.jpg", malformed}, {L"b-upright.jpg", upright}, {L"a-rotate.jpg", eligible}};
    const BatchProcessingCoordinator coordinator{std::make_shared<jpeg::LibJpegTurboTransformationEngine>()};
    BatchProcessingRequest request{collection};
    request.edgeHandlingPolicy = domain::EdgeHandlingPolicy::TrimPartialMinimumCodedUnits;
    auto analyzed = coordinator.analyze(request);
    REQUIRE(analyzed.valueIfPresent() != nullptr);
    REQUIRE(analyzed.valueIfPresent()->files().size() == 3);
    REQUIRE(analyzed.valueIfPresent()->files()[0].jpegAnalysis->authoritativeOrientation ==
            domain::ExifOrientation::BottomRight);
    REQUIRE(analyzed.valueIfPresent()->files()[2].error->code ==
            domain::ImageProcessingErrorCode::MalformedJpegStructure);
    std::vector<BatchProcessingProgress> observations;
    const auto completed = coordinator.execute(std::move(*analyzed.valueIfPresent()), {},
                                               [&](const auto &progress) { observations.push_back(progress); });
    REQUIRE(completed.valueIfPresent() != nullptr);
    const auto &summary = *completed.valueIfPresent();
    REQUIRE(summary.fileResults.size() == 3);
    REQUIRE(summary.fileResults[0].outcome == BatchFileOutcome::CorrectedCopyCreated);
    REQUIRE(summary.fileResults[1].outcome == BatchFileOutcome::NoOrientationNormalizationRequired);
    REQUIRE(summary.fileResults[2].outcome == BatchFileOutcome::UnsupportedSourceSkipped);
    REQUIRE_FALSE(summary.fileResults[0].error.has_value());
    REQUIRE_FALSE(summary.fileResults[1].error.has_value());
    REQUIRE(summary.fileResults[2].error->code == domain::ImageProcessingErrorCode::MalformedJpegStructure);
    REQUIRE(eligible->commits == 1);
    REQUIRE(upright->commits == 0);
    REQUIRE(malformed->commits == 0);
    REQUIRE(eligible->captures == 2);
    REQUIRE(upright->captures == 1);
    REQUIRE(malformed->captures == 1);
    REQUIRE(summary.finalProgress.discovered == 3);
    REQUIRE(summary.finalProgress.analyzed == 3);
    REQUIRE(summary.finalProgress.eligible == 1);
    REQUIRE(summary.finalProgress.completed == 3);
    REQUIRE(summary.finalProgress.skipped == 2);
    REQUIRE(summary.finalProgress.failed == 0);
    REQUIRE(summary.finalProgress.cancelled == 0);
    REQUIRE(summary.finalProgress.remaining == 0);
    REQUIRE(summary.finalProgress.discoveryComplete);
    REQUIRE_FALSE(summary.transactionIntegrityUncertain);
    REQUIRE(observations.size() >= 4);
    for (const auto &progress : observations)
    {
        REQUIRE(progress.remaining + progress.completed == progress.discovered);
        REQUIRE(progress.failed + progress.skipped + progress.cancelled <= progress.completed);
        REQUIRE(progress.discovered == 3);
        REQUIRE(progress.analyzed == 3);
        REQUIRE(progress.eligible == 1);
    }
}

TEST_CASE("Ordinary file failures continue but uncertain transactions stop later work",
          "[batch][processing][failure-isolation]")
{
    const auto uncertain = GENERATE(false, true);
    const auto bytes = test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
        test_support::JpegChromaSubsampling::ycbcr444, 3);
    const auto first = std::make_shared<RecordedImageSource>(bytes);
    const auto second = std::make_shared<RecordedImageSource>(bytes);
    first->commitOperation = [=](const auto &, auto, auto) {
        return domain::ImageProcessingResult<domain::CommittedImageSource>::failure(
            {uncertain ? domain::ImageProcessingErrorCode::RecoveryConflict
                       : domain::ImageProcessingErrorCode::CorrectedCopyCommitFailed,
             domain::ImageProcessingStage::CorrectedCopyCommit});
    };
    const auto collection = std::make_shared<RecordedImageCollection>();
    collection->candidates = {{L"b.jpg", second}, {L"a.jpg", first}};
    const BatchProcessingCoordinator coordinator{std::make_shared<jpeg::LibJpegTurboTransformationEngine>()};
    BatchProcessingRequest request{collection};
    request.edgeHandlingPolicy = domain::EdgeHandlingPolicy::TrimPartialMinimumCodedUnits;
    auto analyzed = coordinator.analyze(request);
    REQUIRE(analyzed.valueIfPresent() != nullptr);
    const auto completed = coordinator.execute(std::move(*analyzed.valueIfPresent()));
    REQUIRE(completed.valueIfPresent() != nullptr);
    const auto &summary = *completed.valueIfPresent();
    REQUIRE(summary.fileResults.size() == 2);
    REQUIRE(summary.fileResults[0].outcome == BatchFileOutcome::ProcessingFailed);
    REQUIRE(summary.fileResults[1].outcome ==
            (uncertain ? BatchFileOutcome::ProcessingFailed : BatchFileOutcome::CorrectedCopyCreated));
    REQUIRE(summary.transactionIntegrityUncertain == uncertain);
    REQUIRE(first->commits == 1);
    REQUIRE(second->commits == (uncertain ? 0U : 1U));
    REQUIRE(second->captures == (uncertain ? 1U : 2U));
    REQUIRE(summary.finalProgress.completed == 2);
    REQUIRE(summary.finalProgress.failed == (uncertain ? 2U : 1U));
    REQUIRE(summary.finalProgress.remaining == 0);
    if (uncertain)
        REQUIRE(summary.fileResults[1].error->code == domain::ImageProcessingErrorCode::RecoveryConflict);
}

TEST_CASE("Changed reviewed revisions are refused before transformation", "[batch][processing][revision]")
{
    const auto source =
        std::make_shared<RecordedImageSource>(test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
            test_support::JpegChromaSubsampling::ycbcr444, 3));
    const auto collection = std::make_shared<RecordedImageCollection>();
    collection->candidates = {{L"changed.jpg", source}};
    const auto engine = std::make_shared<test_support::DeterministicJpegTransformationEngine>(
        [](auto bytes, auto analysis, auto token) {
            return jpeg::LibJpegTurboTransformationEngine{}.createValidatedOutput(std::move(bytes), std::move(analysis),
                                                                                  token);
        });
    const BatchProcessingCoordinator coordinator{engine};
    BatchProcessingRequest request{collection};
    request.edgeHandlingPolicy = domain::EdgeHandlingPolicy::TrimPartialMinimumCodedUnits;
    auto analyzed = coordinator.analyze(request);
    REQUIRE(analyzed.valueIfPresent() != nullptr);
    source->revisionGeneration = 1;
    const auto completed = coordinator.execute(std::move(*analyzed.valueIfPresent()));
    REQUIRE(completed.valueIfPresent() != nullptr);
    REQUIRE(completed.valueIfPresent()->fileResults[0].outcome == BatchFileOutcome::ProcessingFailed);
    REQUIRE(completed.valueIfPresent()->fileResults[0].error->code ==
            domain::ImageProcessingErrorCode::SourceChangedAfterAnalysis);
    REQUIRE(source->commits == 0);
    REQUIRE(engine->invocationCount() == 0);
}

TEST_CASE("A blocked transformation prevents a second codec or transaction from starting",
          "[batch][processing][serialization]")
{
    const auto bytes = test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
        test_support::JpegChromaSubsampling::ycbcr444, 3);
    const auto first = std::make_shared<RecordedImageSource>(bytes);
    const auto second = std::make_shared<RecordedImageSource>(bytes);
    const auto collection = std::make_shared<RecordedImageCollection>();
    collection->candidates = {{L"a.jpg", first}, {L"b.jpg", second}};
    std::promise<void> entered;
    auto enteredObservation = entered.get_future();
    std::promise<void> release;
    auto releaseObservation = release.get_future().share();
    std::atomic<unsigned> calls{};
    const auto engine = std::make_shared<test_support::DeterministicJpegTransformationEngine>(
        [&](auto encoded, auto analysis, auto token) {
            if (++calls == 1)
            {
                entered.set_value();
                releaseObservation.wait();
            }
            return jpeg::LibJpegTurboTransformationEngine{}.createValidatedOutput(std::move(encoded),
                                                                                  std::move(analysis), token);
        });
    const BatchProcessingCoordinator coordinator{engine};
    BatchProcessingRequest request{collection};
    request.edgeHandlingPolicy = domain::EdgeHandlingPolicy::TrimPartialMinimumCodedUnits;
    auto analyzed = coordinator.analyze(request);
    REQUIRE(analyzed.valueIfPresent() != nullptr);
    auto completion =
        std::async(std::launch::async, [&] { return coordinator.execute(std::move(*analyzed.valueIfPresent())); });
    // Release even when an assertion unwinds; never leave a test-owned blocked
    // worker to deadlock std::future destruction on a failed precondition.
    struct ReleaseOnExit final
    {
        std::promise<void> &promise;
        bool released{false};
        ~ReleaseOnExit()
        {
            if (!released)
                promise.set_value();
        }
    } releaseGuard{release};
    REQUIRE(enteredObservation.wait_for(std::chrono::seconds{5}) == std::future_status::ready);
    REQUIRE(calls == 1);
    REQUIRE(second->captures == 1); // analysis only; no second execution read
    REQUIRE(first->commits == 0);
    REQUIRE(second->commits == 0);
    REQUIRE(engine->maximumConcurrentCalls() == 1);
    release.set_value();
    releaseGuard.released = true;
    const auto completed = completion.get();
    REQUIRE(completed.valueIfPresent() != nullptr);
    REQUIRE(completed.valueIfPresent()->finalProgress.completed == 2);
    REQUIRE(calls == 2);
    REQUIRE(engine->maximumConcurrentCalls() == 1);
    REQUIRE(first->commits == 1);
    REQUIRE(second->commits == 1);
}

TEST_CASE("Cancellation produces truthful terminal outcomes at public operation boundaries", "[batch][cancellation]")
{
    // Native codec/backup/commit-internal cancellation is covered by those
    // modules' real tests. This schedules cancellation only at the coordinator's
    // public seams, without claiming to interrupt native C or undo completed I/O.
    const auto boundary = GENERATE(0, 1, 2, 3, 4, 5);
    const auto bytes = test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
        test_support::JpegChromaSubsampling::ycbcr444, 3);
    const auto first = std::make_shared<RecordedImageSource>(bytes);
    const auto second = std::make_shared<RecordedImageSource>(bytes);
    const auto collection = std::make_shared<RecordedImageCollection>();
    collection->candidates = {{L"a.jpg", first}, {L"b.jpg", second}};
    std::stop_source cancellation;
    const auto engine = std::make_shared<test_support::DeterministicJpegTransformationEngine>(
        [&](auto encoded, auto analysis, auto token) {
            auto validated = jpeg::LibJpegTurboTransformationEngine{}.createValidatedOutput(std::move(encoded),
                                                                                            std::move(analysis), token);
            REQUIRE(validated.valueIfPresent() != nullptr);
            if (boundary == 3)
                cancellation.request_stop();
            return validated;
        });
    if (boundary == 2)
        first->captureObserver = [&](const auto invocation) {
            if (invocation == 2)
                cancellation.request_stop();
        };
    if (boundary == 4 || boundary == 5)
        first->commitOperation = [&](const auto &, auto, auto) {
            cancellation.request_stop();
            using Result = domain::ImageProcessingResult<domain::CommittedImageSource>;
            return boundary == 4 ? Result::failure({domain::ImageProcessingErrorCode::Cancelled,
                                                    domain::ImageProcessingStage::BackupCreation})
                                 : Result::success({L"observed.jpg"});
        };
    const BatchProcessingCoordinator coordinator{engine};
    BatchProcessingRequest request{collection};
    request.edgeHandlingPolicy = domain::EdgeHandlingPolicy::TrimPartialMinimumCodedUnits;
    auto analyzed = coordinator.analyze(request, cancellation.get_token(), [&](const auto &progress) {
        if ((boundary == 0 && !progress.discoveryComplete && progress.discovered == 1) ||
            (boundary == 1 && progress.analyzed == 1))
            cancellation.request_stop();
    });
    REQUIRE(analyzed.valueIfPresent() != nullptr);
    const auto completed = coordinator.execute(std::move(*analyzed.valueIfPresent()), cancellation.get_token());
    REQUIRE(completed.valueIfPresent() != nullptr);
    const auto &summary = *completed.valueIfPresent();
    REQUIRE(summary.fileResults.size() == (boundary == 0 ? 1U : 2U));
    REQUIRE(summary.finalProgress.completed == summary.fileResults.size());
    REQUIRE(summary.finalProgress.remaining == 0);
    REQUIRE(summary.finalProgress.discoveryComplete == (boundary != 0));
    REQUIRE(summary.finalProgress.cancelled == (boundary == 0 || boundary == 5 ? 1U : 2U));
    REQUIRE(summary.fileResults[0].outcome == (boundary == 5   ? BatchFileOutcome::CorrectedCopyCreated
                                               : boundary >= 3 ? BatchFileOutcome::CancelledBeforeCommit
                                                               : BatchFileOutcome::CancelledBeforeTransformation));
    if (boundary == 5)
        REQUIRE_FALSE(summary.fileResults[0].error.has_value());
    else
        REQUIRE(summary.fileResults[0].error->code == domain::ImageProcessingErrorCode::Cancelled);
    REQUIRE(second->commits == 0);
    REQUIRE(first->commits == (boundary >= 4 ? 1U : 0U));
    REQUIRE(engine->invocationCount() == (boundary >= 3 ? 1U : 0U));
}

TEST_CASE("Encoded-input and intermediate-codec budget simulators remain distinct and sequential", "[batch][memory]")
{
    constexpr std::size_t codecIntermediateBufferBudgetBytes = 512ULL * 1024ULL * 1024ULL;
    const auto encodedInputLimitBytes = domain::JpegResourceLimits::production().maximumEncodedFileLengthBytes;
    REQUIRE(encodedInputLimitBytes == 512ULL * 1024ULL * 1024ULL);
    const auto source =
        std::make_shared<RecordedImageSource>(test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
            test_support::JpegChromaSubsampling::ycbcr444, 3));
    source->simulatedEncodedBufferCapacityBytes = static_cast<std::size_t>(encodedInputLimitBytes);
    const auto collection = std::make_shared<RecordedImageCollection>();
    collection->candidates = {{L"a.jpg", source}, {L"b.jpg", source}};
    PROCESS_MEMORY_COUNTERS baseline{};
    REQUIRE(GetProcessMemoryInfo(GetCurrentProcess(), &baseline, sizeof(baseline)));
    std::size_t activeCodecBudgetBytes = 0;
    std::size_t peakCodecBudgetBytes = 0;
    std::size_t observedPeakWorkingSet = baseline.WorkingSetSize;
    const auto engine = std::make_shared<test_support::DeterministicJpegTransformationEngine>(
        [&](auto encoded, auto analysis, auto token) {
            REQUIRE(encoded.capacity() >= encodedInputLimitBytes);
            REQUIRE(encoded.size() < encodedInputLimitBytes);
            activeCodecBudgetBytes += codecIntermediateBufferBudgetBytes;
            peakCodecBudgetBytes = std::max(peakCodecBudgetBytes, activeCodecBudgetBytes);
            struct ReleaseBudget final
            {
                std::size_t &active;
                ~ReleaseBudget()
                {
                    active -= codecIntermediateBufferBudgetBytes;
                }
            } release{activeCodecBudgetBytes};
            const std::vector<std::byte> intermediateBuffer(codecIntermediateBufferBudgetBytes);
            PROCESS_MEMORY_COUNTERS current{};
            REQUIRE(GetProcessMemoryInfo(GetCurrentProcess(), &current, sizeof(current)));
            // Win32 SIZE_T and C++ size_t can be distinct unsigned types even
            // though both represent the native address-sized byte count.
            observedPeakWorkingSet = std::max(observedPeakWorkingSet, static_cast<std::size_t>(current.WorkingSetSize));
            REQUIRE(intermediateBuffer.size() == codecIntermediateBufferBudgetBytes);
            // Production TurboJPEG's TJPARAM_MAXMEMORY is separately verified by
            // its real resource-limit suite; this allocation is a named simulator
            // of one intermediate-buffer budget, not a total native-codec bound.
            return jpeg::LibJpegTurboTransformationEngine{}.createValidatedOutput(std::move(encoded),
                                                                                  std::move(analysis), token);
        });
    const BatchProcessingCoordinator coordinator{engine};
    BatchProcessingRequest request{collection};
    request.edgeHandlingPolicy = domain::EdgeHandlingPolicy::TrimPartialMinimumCodedUnits;
    auto analyzed = coordinator.analyze(request);
    REQUIRE(analyzed.valueIfPresent() != nullptr);
    const auto summary = coordinator.execute(std::move(*analyzed.valueIfPresent()));
    REQUIRE(summary.valueIfPresent() != nullptr);
    REQUIRE(summary.valueIfPresent()->finalProgress.completed == 2);
    REQUIRE(summary.valueIfPresent()->finalProgress.failed == 0);
    REQUIRE(source->observedEncodedLimitBytes == encodedInputLimitBytes);
    REQUIRE(engine->maximumConcurrentCalls() == 1);
    REQUIRE(peakCodecBudgetBytes == codecIntermediateBufferBudgetBytes);
    REQUIRE(activeCodecBudgetBytes == 0);
    REQUIRE(source->captures == 4);
    REQUIRE(source->commits == 2);
    std::cout << "batch_memory_simulators encoded_input_limit_bytes=" << encodedInputLimitBytes
              << " codec_intermediate_budget_bytes=" << codecIntermediateBufferBudgetBytes
              << " peak_active_codec_budget_bytes=" << peakCodecBudgetBytes
              << " baseline_working_set_bytes=" << baseline.WorkingSetSize
              << " observed_peak_working_set_bytes=" << observedPeakWorkingSet << '\n';
}

TEST_CASE("Cancellation inside the JPEG engine before coefficient work remains before transformation",
          "[batch][cancellation][engine-admission]")
{
    const auto source =
        std::make_shared<RecordedImageSource>(test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
            test_support::JpegChromaSubsampling::ycbcr444, 3));
    const auto collection = std::make_shared<RecordedImageCollection>();
    collection->candidates = {{L"source.jpg", source}};
    std::stop_source cancellation;
    const auto engine = std::make_shared<test_support::DeterministicJpegTransformationEngine>(
        [&](auto encoded, auto analysis, auto token) {
            // Cancel after the coordinator's pre-call check, then use the real
            // engine's entry check. No native transform can have started here.
            cancellation.request_stop();
            auto result = jpeg::LibJpegTurboTransformationEngine{}.createValidatedOutput(std::move(encoded),
                                                                                         std::move(analysis), token);
            REQUIRE(result.errorIfPresent() != nullptr);
            REQUIRE(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::Cancelled);
            REQUIRE(result.errorIfPresent()->stage == domain::ImageProcessingStage::TransformPlanning);
            return result;
        });
    const BatchProcessingCoordinator coordinator{engine};
    BatchProcessingRequest request{collection};
    request.edgeHandlingPolicy = domain::EdgeHandlingPolicy::TrimPartialMinimumCodedUnits;
    auto analyzed = coordinator.analyze(request, cancellation.get_token());
    REQUIRE(analyzed.valueIfPresent() != nullptr);
    const auto completed = coordinator.execute(std::move(*analyzed.valueIfPresent()), cancellation.get_token());
    REQUIRE(completed.valueIfPresent() != nullptr);
    REQUIRE(completed.valueIfPresent()->fileResults[0].outcome == BatchFileOutcome::CancelledBeforeTransformation);
    REQUIRE(completed.valueIfPresent()->finalProgress.cancelled == 1);
    REQUIRE(source->commits == 0);
}

TEST_CASE("Only an explicit native not-started observation permits pre-transformation cancellation",
          "[batch][cancellation][engine-admission]")
{
    using State = domain::CoefficientTransformationExecutionState;
    // An independently supplied engine failure exercises the coordinator's
    // diagnostic contract, including invalid observations from an extension.
    // The native engine's valid observations are covered by its own real tests.
    const auto observation = GENERATE(0, 1, 2, 3);
    const auto source =
        std::make_shared<RecordedImageSource>(test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
            test_support::JpegChromaSubsampling::ycbcr444, 3));
    const auto collection = std::make_shared<RecordedImageCollection>();
    collection->candidates = {{L"source.jpg", source}};
    const auto engine =
        std::make_shared<test_support::DeterministicJpegTransformationEngine>([observation](auto, auto, auto) {
            domain::ImageProcessingDiagnosticContext diagnostic;
            if (observation == 1)
                diagnostic = State::NotStarted;
            if (observation == 2)
                diagnostic = State::Started;
            if (observation == 3)
                diagnostic = static_cast<State>(2);
            return domain::ImageProcessingResult<domain::ValidatedJpegOutput>::failure(
                {domain::ImageProcessingErrorCode::Cancelled,
                 domain::ImageProcessingStage::CoefficientTransformation,
                 {},
                 diagnostic});
        });
    const BatchProcessingCoordinator coordinator{engine};
    BatchProcessingRequest request{collection};
    request.edgeHandlingPolicy = domain::EdgeHandlingPolicy::TrimPartialMinimumCodedUnits;
    auto analyzed = coordinator.analyze(request);
    REQUIRE(analyzed.valueIfPresent() != nullptr);
    const auto completed = coordinator.execute(std::move(*analyzed.valueIfPresent()));
    REQUIRE(completed.valueIfPresent() != nullptr);
    REQUIRE(
        completed.valueIfPresent()->fileResults[0].outcome ==
        (observation == 1 ? BatchFileOutcome::CancelledBeforeTransformation : BatchFileOutcome::CancelledBeforeCommit));
    REQUIRE(completed.valueIfPresent()->finalProgress.cancelled == 1);
    REQUIRE(source->commits == 0);
}
