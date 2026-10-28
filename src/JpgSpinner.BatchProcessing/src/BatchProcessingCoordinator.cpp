#define NOMINMAX
#include <windows.h>
#include <jpg_spinner/batch/BatchProcessingCoordinator.h>
#include <jpg_spinner/jpeg/JpegImageAnalyzer.h>
#include <algorithm>
#include <filesystem>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <condition_variable>
#include <latch>
#include <thread>
#include <atomic>
#include <wrl/wrappers/corewrappers.h>

#pragma comment(lib, "runtimeobject.lib")

namespace jpg_spinner::batch_processing
{
namespace
{
int compareRelativeNames(const std::wstring &left, const std::wstring &right, const bool ignoreCase)
{
    // Use the platform's ordinal UTF-16 and case semantics, not collation or a
    // local Unicode table. Lengths are checked before every native narrowing.
    if (left.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        right.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::invalid_argument("Relative names exceed the native comparison limit.");
    const auto order = CompareStringOrdinal(left.data(), static_cast<int>(left.size()), right.data(),
                                            static_cast<int>(right.size()), ignoreCase);
    if (order == 0)
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
    return order;
}

std::wstring normalizedRelativeName(const std::wstring &name)
{
    const std::filesystem::path relative{name};
    if (relative.empty() || relative.has_root_path())
        throw std::invalid_argument("A nonempty relative source name is required.");
    for (const auto &component : relative)
        if (component.empty() || component == "." || component == "..")
            throw std::invalid_argument("Source display names cannot contain traversal components.");
    // Standard-library separator conversion only. Do not normalize Unicode:
    // canonically equivalent spellings can identify distinct NTFS entries.
    return relative.generic_wstring();
}

void reportProgress(BatchProcessingProgress &progress, const BatchProcessingProgressObserver &observer)
{
    progress.remaining = progress.discovered - progress.completed;
    if (observer)
        observer(progress);
}

bool isUnsupportedSource(const domain::ImageProcessingErrorCode code) noexcept
{
    using enum domain::ImageProcessingErrorCode;
    // These are source-policy refusals, not operational failures. In particular,
    // allocation, access, codec, validator and transaction errors must not be
    // relabeled as unsupported input merely because they share a result type.
    switch (code)
    {
    case EncodedFileTooLarge:
    case PixelCountLimitExceeded:
    case MetadataLengthLimitExceeded:
    case ProgressiveScanCountLimitExceeded:
    case MalformedJpegStructure:
    case MalformedImageMetadata:
    case InvalidOrientationMetadata:
    case UnsupportedJpegCodingProcess:
    case UnsupportedJpegSamplePrecision:
    case UnsupportedJpegComponentOrganization:
    case UnsupportedJpegDeferredHeight:
    case UnsupportedJpegRestartIntervalChanges:
    case MultiPictureJpegNotSupported:
    case MotionPhotoNotSupported:
    case UnsupportedTrailingPayload:
    case ContentCredentialsWouldBeInvalidated:
    case UnsupportedJumbfMetadata:
    case UnsupportedEmbeddedPreviewMetadata:
    case ExtendedXmpMutationNotSupported:
    case PerfectCoefficientTransformUnavailable:
        return true;
    default:
        return false;
    }
}
} // namespace

BatchProcessingCoordinator::BatchProcessingCoordinator(
    std::shared_ptr<const jpeg::JpegTransformationEngine> transformationEngine)
    : transformationEngine_(std::move(transformationEngine))
{
    if (!transformationEngine_)
        throw std::invalid_argument("A JPEG transformation engine is required.");
}

domain::ImageProcessingResult<AnalyzedImageBatch> BatchProcessingCoordinator::analyze(
    const BatchProcessingRequest &request, const std::stop_token cancellation,
    const BatchProcessingProgressObserver &observer) const
{
    using Result = domain::ImageProcessingResult<AnalyzedImageBatch>;
    const std::lock_guard lock{operationMutex_};
    try
    {
        const auto production = domain::JpegResourceLimits::production();
        if (!request.sourceCollection || request.executionLimits.maximumConcurrentImageCount == 0 ||
            request.executionLimits.maximumConcurrentImageCount >
                BatchExecutionLimits::maximumSupportedConcurrentImageCount ||
            request.executionLimits.estimatedWorkingMemoryBudgetBytes == 0 ||
            (request.traversalScope != domain::TraversalScope::SelectedFolderOnly &&
             request.traversalScope != domain::TraversalScope::SelectedFolderAndDescendants) ||
            (request.outputDisposition != domain::OutputDisposition::CreateCorrectedCopy &&
             request.outputDisposition != domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup &&
             request.outputDisposition != domain::OutputDisposition::ReplaceOriginalWithoutRetainedBackup) ||
            (request.edgeHandlingPolicy != domain::EdgeHandlingPolicy::RequirePerfectCoefficientTransform &&
             request.edgeHandlingPolicy != domain::EdgeHandlingPolicy::TrimPartialMinimumCodedUnits) ||
            (request.outputScanOrganization != domain::OutputScanOrganization::PreserveSource &&
             request.outputScanOrganization != domain::OutputScanOrganization::SequentialDct &&
             request.outputScanOrganization != domain::OutputScanOrganization::ProgressiveDct) ||
            request.resourceLimits.maximumEncodedFileLengthBytes > production.maximumEncodedFileLengthBytes ||
            request.resourceLimits.maximumPixelCount > production.maximumPixelCount ||
            request.resourceLimits.maximumMetadataLengthBytes > production.maximumMetadataLengthBytes ||
            request.resourceLimits.maximumProgressiveScanCount > production.maximumProgressiveScanCount)
            return Result::failure({domain::ImageProcessingErrorCode::InvalidBatchProcessingRequest,
                                    domain::ImageProcessingStage::CandidateDiscovery});
        BatchProcessingProgress progress;
        reportProgress(progress, observer);
        auto discovered = request.sourceCollection->discover(
            request.traversalScope, cancellation, [&](const domain::ImageSourceDiscoveryProgress &observation) {
                progress.discovered = observation.discoveredCandidates;
                reportProgress(progress, observer);
            });
        if (const auto error = discovered.errorIfPresent())
            return Result::failure(*error);
        auto &discovery = *discovered.valueIfPresent();
        for (auto &candidate : discovery.candidates)
        {
            if (!candidate.source)
                throw std::invalid_argument("A source capability is required.");
            candidate.relativePath = normalizedRelativeName(candidate.relativePath);
        }
        // Sorting by case-sensitive ordinal alone would not put A.jpg and a.jpg
        // adjacent in the presence of Z.jpg. Check the conservative output key
        // in its own sorted index before any image is opened or analyzed.
        std::vector<std::size_t> collisionOrder(discovery.candidates.size());
        std::iota(collisionOrder.begin(), collisionOrder.end(), std::size_t{0});
        std::sort(collisionOrder.begin(), collisionOrder.end(), [&](const auto left, const auto right) {
            return compareRelativeNames(discovery.candidates[left].relativePath,
                                        discovery.candidates[right].relativePath, true) == CSTR_LESS_THAN;
        });
        for (std::size_t index = 1; index < collisionOrder.size(); ++index)
            if (compareRelativeNames(discovery.candidates[collisionOrder[index - 1]].relativePath,
                                     discovery.candidates[collisionOrder[index]].relativePath, true) == CSTR_EQUAL)
                return Result::failure({domain::ImageProcessingErrorCode::OutputRelativePathCollision,
                                        domain::ImageProcessingStage::CandidateDiscovery});
        std::sort(discovery.candidates.begin(), discovery.candidates.end(), [](const auto &left, const auto &right) {
            return compareRelativeNames(left.relativePath, right.relativePath, false) == CSTR_LESS_THAN;
        });
        progress.discovered = discovery.candidates.size();
        progress.discoveryComplete = !discovery.cancellationRequested;
        reportProgress(progress, observer);
        std::vector<BatchFileAnalysis> files;
        files.reserve(discovery.candidates.size());
        for (auto &candidate : discovery.candidates)
        {
            BatchFileAnalysis file{std::move(candidate.relativePath), std::move(candidate.source), {}, {}, {}};
            if (cancellation.stop_requested() || discovery.cancellationRequested)
                file.error.emplace(domain::ImageProcessingErrorCode::Cancelled,
                                   domain::ImageProcessingStage::SourceRevisionCapture);
            else
            {
                auto captured =
                    file.source->capture(request.resourceLimits.maximumEncodedFileLengthBytes, cancellation);
                if (const auto error = captured.errorIfPresent())
                    file.error.emplace(*error);
                else
                {
                    const auto &source = *captured.valueIfPresent();
                    file.sourceRevision.emplace(source.revision);
                    if (cancellation.stop_requested())
                        file.error.emplace(domain::ImageProcessingErrorCode::Cancelled,
                                           domain::ImageProcessingStage::JpegStructureValidation);
                    else
                    {
                        auto analysis =
                            jpeg::JpegImageAnalyzer::analyze(source.encodedBytes, request.edgeHandlingPolicy,
                                                             request.outputScanOrganization, request.resourceLimits);
                        ++progress.analyzed;
                        if (const auto analysisError = analysis.errorIfPresent())
                            file.error.emplace(*analysisError);
                        else
                        {
                            file.jpegAnalysis = std::make_shared<const domain::JpegImageAnalysis>(
                                std::move(*analysis.valueIfPresent()));
                            if (file.jpegAnalysis->transformPlan.transform != domain::LosslessTransform::None)
                                ++progress.eligible;
                        }
                    }
                }
                // captured's single encoded buffer dies before opening the next file.
            }
            files.push_back(std::move(file));
            reportProgress(progress, observer);
        }
        return Result::success(AnalyzedImageBatch{
            request.identifier, request.outputDisposition, request.resourceLimits, request.executionLimits,
            std::move(files), std::move(discovery.issues), progress.discoveryComplete, progress.analyzed});
    }
    catch (const std::bad_alloc &)
    {
        return Result::failure({domain::ImageProcessingErrorCode::WorkingMemoryAllocationFailed,
                                domain::ImageProcessingStage::CandidateDiscovery});
    }
    catch (const std::invalid_argument &)
    {
        return Result::failure({domain::ImageProcessingErrorCode::SourceDiscoveryFailed,
                                domain::ImageProcessingStage::CandidateDiscovery,
                                domain::WindowsHResult{E_INVALIDARG}});
    }
    catch (const std::system_error &error)
    {
        return Result::failure({domain::ImageProcessingErrorCode::SourceDiscoveryFailed,
                                domain::ImageProcessingStage::CandidateDiscovery, error.code()});
    }
}
domain::ImageProcessingResult<BatchProcessingSummary> BatchProcessingCoordinator::execute(
    AnalyzedImageBatch reviewedBatch, const std::stop_token cancellation,
    const BatchProcessingProgressObserver &observer) const
{
    using Result = domain::ImageProcessingResult<BatchProcessingSummary>;
    using enum BatchFileOutcome;
    using enum domain::ImageProcessingErrorCode;
    const std::lock_guard lock{operationMutex_};
    BatchProcessingSummary summary{reviewedBatch.identifier(), {}, std::move(reviewedBatch.issues_), {}};
    auto &progress = summary.finalProgress;
    progress.discovered = reviewedBatch.files_.size();
    progress.analyzed = reviewedBatch.analyzedCount_;
    progress.discoveryComplete = reviewedBatch.discoveryComplete_;
    for (const auto &file : reviewedBatch.files_)
        if (file.jpegAnalysis && file.jpegAnalysis->transformPlan.transform != domain::LosslessTransform::None)
            ++progress.eligible;
    try
    {
        // Preallocate every path and terminal slot before any mutating work.
        // A later allocation failure cannot erase observations of earlier commits.
        summary.fileResults.reserve(reviewedBatch.files_.size());
        for (const auto &file : reviewedBatch.files_)
            summary.fileResults.push_back({file.relativePath, CancelledBeforeTransformation, {}});
    }
    catch (const std::bad_alloc &)
    {
        return Result::failure(
            {WorkingMemoryAllocationFailed, domain::ImageProcessingStage::CoefficientTransformation});
    }
    reportProgress(progress, observer);
    std::mutex stateMutex;
    std::mutex commitMutex;
    std::condition_variable_any memoryAvailable;
    const auto memoryBudget = std::min(reviewedBatch.executionLimits_.estimatedWorkingMemoryBudgetBytes,
                                       BatchExecutionLimits::recommendedWorkingMemoryBudgetBytes());
    std::uint64_t reservedMemoryBytes{};
    std::stop_source workerCancellation;
    const auto workerToken = workerCancellation.get_token();
    std::stop_callback cancellationCallback{cancellation, [&] { workerCancellation.request_stop(); }};
    std::optional<domain::ImageProcessingError> integrityFailure;
    const auto observedIntegrityFailure = [&] {
        const std::lock_guard guard{stateMutex};
        return integrityFailure;
    };
    const auto processFileAt = [&](const std::size_t index) {
        const auto &file = reviewedBatch.files_[index];
        auto &terminal = summary.fileResults[index];
        struct MemoryReservation final
        {
            std::mutex &mutex;
            std::condition_variable_any &available;
            std::uint64_t &reserved;
            std::uint64_t bytes{};
            ~MemoryReservation()
            {
                if (bytes == 0)
                    return;
                {
                    const std::lock_guard guard{mutex};
                    reserved -= bytes;
                }
                available.notify_all();
            }
        } reservation{stateMutex, memoryAvailable, reservedMemoryBytes};
        const auto recordError = [&](const domain::ImageProcessingError &error, const bool transformationEntered) {
            terminal.error.emplace(error);
            terminal.outcome = error.code == Cancelled
                                   ? (transformationEntered ? CancelledBeforeCommit : CancelledBeforeTransformation)
                                   : (isUnsupportedSource(error.code) ? UnsupportedSourceSkipped : ProcessingFailed);
            if (error.code == RecoveryConflict)
            {
                {
                    const std::lock_guard guard{stateMutex};
                    if (!integrityFailure)
                        integrityFailure.emplace(error);
                    summary.transactionIntegrityUncertain = true;
                }
                // Stop pending work and commits; an already admitted native
                // save must still finish and report its actual result.
                workerCancellation.request_stop();
            }
        };
        const auto processFile = [&] {
            if (const auto conflict = observedIntegrityFailure())
            {
                recordError(*conflict, false);
                return;
            }
            if (file.error)
            {
                recordError(*file.error, false);
                return;
            }
            if (workerToken.stop_requested() || !reviewedBatch.discoveryComplete_)
            {
                recordError({Cancelled, domain::ImageProcessingStage::SourceRevisionCapture}, false);
                return;
            }
            if (file.jpegAnalysis->transformPlan.transform == domain::LosslessTransform::None)
            {
                terminal.outcome = NoOrientationNormalizationRequired;
                return;
            }
            {
                const auto budget = memoryBudget;
                const auto charge = std::min(
                    budget,
                    std::max<std::uint64_t>(1, transformationEngine_->estimatedWorkingMemoryBytes(
                                                   file.sourceRevision->encodedLengthBytes, *file.jpegAnalysis)));
                std::unique_lock guard{stateMutex};
                // Capping the charge at the whole budget admits an oversized
                // image exclusively. Existing per-image codec bounds remain
                // authoritative; this estimate is not an absolute RAM quota.
                if (!memoryAvailable.wait(guard, workerToken, [&] { return charge <= budget - reservedMemoryBytes; }))
                {
                    const auto conflict = integrityFailure;
                    guard.unlock();
                    recordError(conflict
                                    ? *conflict
                                    : domain::ImageProcessingError{Cancelled,
                                                                   domain::ImageProcessingStage::SourceRevisionCapture},
                                false);
                    return;
                }
                reservedMemoryBytes += charge;
                reservation.bytes = charge;
            }
            auto captured =
                file.source->capture(reviewedBatch.resourceLimits_.maximumEncodedFileLengthBytes, workerToken);
            if (const auto captureError = captured.errorIfPresent())
            {
                recordError(*captureError, false);
                return;
            }
            auto &source = *captured.valueIfPresent();
            if (source.revision != *file.sourceRevision)
            {
                recordError({SourceChangedAfterAnalysis, domain::ImageProcessingStage::SourceRevisionRevalidation},
                            false);
                return;
            }
            if (workerToken.stop_requested())
            {
                recordError({Cancelled, domain::ImageProcessingStage::CoefficientTransformation}, false);
                return;
            }
            auto output = transformationEngine_->createValidatedOutput(std::move(source.encodedBytes),
                                                                       *file.jpegAnalysis, workerToken);
            if (const auto transformError = output.errorIfPresent())
            {
                const auto executionState =
                    std::get_if<domain::CoefficientTransformationExecutionState>(&transformError->diagnosticContext);
                // Only an explicit native observation justifies a pre-transform
                // outcome. An engine violating its diagnostic contract remains
                // conservatively before-commit, never asserted not to have run.
                const auto transformationStarted =
                    !executionState || *executionState != domain::CoefficientTransformationExecutionState::NotStarted;
                recordError(*transformError, transformationStarted);
                return;
            }
            if (workerToken.stop_requested())
            {
                recordError({Cancelled, domain::ImageProcessingStage::OutputValidation}, true);
                return;
            }
            // One admission boundary serializes saves and their journal effects.
            // Never hold stateMutex while acquiring this mutex. A published
            // integrity conflict prohibits all later admissions, while a native
            // save already inside this boundary retains its completion semantics.
            const std::lock_guard commitGuard{commitMutex};
            if (const auto conflict = observedIntegrityFailure())
            {
                recordError(*conflict, true);
                return;
            }
            if (workerToken.stop_requested())
            {
                recordError({Cancelled, domain::ImageProcessingStage::OutputValidation}, true);
                return;
            }
            const auto committed = file.source->commit(*file.sourceRevision, *output.valueIfPresent(),
                                                       reviewedBatch.disposition_, workerToken);
            if (const auto commitError = committed.errorIfPresent())
            {
                recordError(*commitError, true);
                return;
            }
            // No token check here: a reported native commit is real, even if
            // cancellation arrived while finishing its journal transition.
            terminal.outcome =
                reviewedBatch.disposition_ == domain::OutputDisposition::CreateCorrectedCopy
                    ? CorrectedCopyCreated
                    : (reviewedBatch.disposition_ == domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup
                           ? OriginalReplacedWithVerifiedBackup
                           : OriginalReplacedWithoutRetainedBackup);
        };
        try
        {
            processFile();
        }
        catch (const std::bad_alloc &)
        {
            recordError({WorkingMemoryAllocationFailed, domain::ImageProcessingStage::CoefficientTransformation},
                        false);
        }
        const std::lock_guard progressGuard{stateMutex};
        ++progress.completed;
        switch (terminal.outcome)
        {
        case ProcessingFailed:
            ++progress.failed;
            break;
        case UnsupportedSourceSkipped:
        case NoOrientationNormalizationRequired:
            ++progress.skipped;
            break;
        case CancelledBeforeTransformation:
        case CancelledBeforeCommit:
            ++progress.cancelled;
            break;
        case CorrectedCopyCreated:
        case OriginalReplacedWithVerifiedBackup:
        case OriginalReplacedWithoutRetainedBackup:
            break;
        }
        reportProgress(progress, observer);
    };
    const auto workerCount =
        std::min<std::size_t>(reviewedBatch.executionLimits_.maximumConcurrentImageCount, reviewedBatch.files_.size());
    if (workerCount == 0)
        return Result::success(std::move(summary));
    if (workerCount == 1)
    {
        // Retain the serial caller/apartment contract and avoid constructing
        // a pool when the user requests one image or the batch has only one.
        for (std::size_t index = 0; index < reviewedBatch.files_.size(); ++index)
            processFileAt(index);
        return Result::success(std::move(summary));
    }
    std::latch workersReady{static_cast<std::ptrdiff_t>(workerCount)};
    std::latch startWorkers{1};
    std::atomic<HRESULT> initializationResult{S_OK};
    bool abortStartup{}; // Published before startWorkers releases its waiters.
    std::size_t nextIndex{};
    std::exception_ptr workerFailure;
    std::vector<std::jthread> workers;
    try
    {
        workers.reserve(workerCount);
        for (std::size_t worker = 0; worker < workerCount; ++worker)
            workers.emplace_back([&] {
                // Every native worker owns and balances its MTA initialization;
                // no UI apartment is captured or blocked by a codec operation.
                Microsoft::WRL::Wrappers::RoInitializeWrapper apartment{RO_INIT_MULTITHREADED};
                const HRESULT initialized = apartment;
                if (FAILED(initialized))
                    initializationResult.store(initialized);
                workersReady.count_down();
                startWorkers.wait();
                if (abortStartup)
                    return;
                try
                {
                    for (;;)
                    {
                        std::size_t index;
                        {
                            const std::lock_guard guard{stateMutex};
                            if (nextIndex == reviewedBatch.files_.size())
                                break;
                            index = nextIndex++;
                        }
                        processFileAt(index);
                    }
                }
                catch (...)
                {
                    {
                        const std::lock_guard guard{stateMutex};
                        if (!workerFailure)
                            workerFailure = std::current_exception();
                    }
                    workerCancellation.request_stop();
                }
            });
    }
    catch (const std::system_error &error)
    {
        abortStartup = true;
        startWorkers.count_down();
        workers.clear(); // Join before returning; no source work was admitted.
        return Result::failure({CoefficientTransformationFailed,
                                domain::ImageProcessingStage::CoefficientTransformation, error.code(),
                                domain::CoefficientTransformationExecutionState::NotStarted});
    }
    catch (const std::bad_alloc &)
    {
        abortStartup = true;
        startWorkers.count_down();
        workers.clear();
        return Result::failure(
            {WorkingMemoryAllocationFailed, domain::ImageProcessingStage::CoefficientTransformation});
    }
    workersReady.wait();
    abortStartup = FAILED(initializationResult.load());
    startWorkers.count_down();
    for (auto &worker : workers)
        worker.join();
    if (abortStartup)
        return Result::failure({CoefficientTransformationFailed,
                                domain::ImageProcessingStage::CoefficientTransformation,
                                domain::WindowsHResult{initializationResult.load()},
                                domain::CoefficientTransformationExecutionState::NotStarted});
    if (workerFailure)
        std::rethrow_exception(workerFailure); // Caller must reconcile any earlier admitted native commits.
    return Result::success(std::move(summary));
}
} // namespace jpg_spinner::batch_processing
