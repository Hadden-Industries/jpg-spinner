#include "pch.h"
#include "ViewModels/MainWindowViewModel.h"
#include "DeterministicJpegFixtureFactory.h"
#include "PresentationTestApartment.h"
#include <jpg_spinner/jpeg/LibJpegTurboTransformationEngine.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/catch_translate_exception.hpp>
#include <chrono>
#include <future>
#include <initializer_list>
#include <stop_token>
#include <string_view>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <wil/resource.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>

// Preserve the native diagnostic for failed fixture/API qualification; do not
// hide an HRESULT behind Catch2's unknown-exception text or call it test GREEN.
CATCH_TRANSLATE_EXCEPTION(winrt::hresult_error const &error)
{
    return winrt::to_string(error.message()) + " (HRESULT " + std::to_string(static_cast<std::int32_t>(error.code())) +
           ")";
}

namespace
{
namespace domain = jpg_spinner::domain;
namespace storage = jpg_spinner::storage;

// No transform is expected in the initial/empty-source scenarios. This double
// refuses an accidental call rather than fabricating validated JPEG output.
class RefusingTransformationEngine final : public jpg_spinner::jpeg::JpegTransformationEngine
{
  public:
    domain::ImageProcessingResult<domain::ValidatedJpegOutput> createValidatedOutput(std::vector<std::byte>,
                                                                                     domain::JpegImageAnalysis,
                                                                                     std::stop_token) const override
    {
        return domain::ImageProcessingResult<domain::ValidatedJpegOutput>::failure(
            {domain::ImageProcessingErrorCode::CoefficientTransformationFailed,
             domain::ImageProcessingStage::CoefficientTransformation});
    }
};

// Recovery is an injected storage capability. These initial tests do not invoke
// it; later recovery tests can inspect calls without touching user files.
class RecoveryEngine final : public storage::ImageFileTransactionEngine
{
  public:
    mutable std::atomic<unsigned> recoveries{};
    bool refuseRecovery{false};
    bool throwRecoveryException{false};
    bool waitForCancellation{};
    mutable std::promise<void> recoveryEntered;
    mutable std::stop_token observedRecoveryCancellation;
    domain::ImageProcessingResult<storage::CommittedImageFile> execute(const storage::ImageFileTransactionRequest &,
                                                                       const domain::ValidatedJpegOutput &,
                                                                       std::stop_token) const override
    {
        return domain::ImageProcessingResult<storage::CommittedImageFile>::failure(
            {domain::ImageProcessingErrorCode::RecoveryConflict, domain::ImageProcessingStage::TransactionRecovery});
    }
    domain::ImageProcessingResult<storage::ImageFileTransactionRecoverySummary> recoverIncompleteTransactions(
        std::stop_token cancellation) const override
    {
        ++recoveries;
        if (waitForCancellation)
        {
            // Publishing the promise makes this token observable by the test
            // driver without a race; it is not reassigned during this wait.
            observedRecoveryCancellation = cancellation;
            recoveryEntered.set_value();
            std::mutex mutex;
            std::condition_variable_any cancelled;
            std::unique_lock lock{mutex};
            cancelled.wait(lock, cancellation, [] { return false; });
            return domain::ImageProcessingResult<storage::ImageFileTransactionRecoverySummary>::failure(
                {domain::ImageProcessingErrorCode::Cancelled, domain::ImageProcessingStage::TransactionRecovery});
        }
        if (throwRecoveryException)
            throw winrt::hresult_error{E_FAIL, L"private native diagnostic C:\\private\\photo.jpg"};
        if (refuseRecovery)
            return domain::ImageProcessingResult<storage::ImageFileTransactionRecoverySummary>::failure(
                {domain::ImageProcessingErrorCode::RecoveryConflict,
                 domain::ImageProcessingStage::TransactionRecovery});
        return domain::ImageProcessingResult<storage::ImageFileTransactionRecoverySummary>::success({});
    }
};

// A read-only source capability feeds a real encoded JPEG to the real batch
// analyzer. Commit remains a spy; this test does not qualify Windows persistence.
class EncodedImageSource final : public domain::ImageSource
{
  public:
    explicit EncodedImageSource(std::uint16_t orientation)
        : bytes_(jpg_spinner::test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
              jpg_spinner::test_support::JpegChromaSubsampling::ycbcr444, orientation))
    {
    }
    domain::ImageProcessingResult<domain::CapturedImageSource> capture(std::uint64_t, std::stop_token) const override
    {
        ++captures;
        return domain::ImageProcessingResult<domain::CapturedImageSource>::success({bytes_, {bytes_.size(), {}, {}}});
    }
    domain::ImageProcessingResult<domain::CommittedImageSource> commit(const domain::SourceFileRevision &,
                                                                       const domain::ValidatedJpegOutput &,
                                                                       domain::OutputDisposition,
                                                                       std::stop_token) const override
    {
        ++commits;
        if (afterCommit)
            afterCommit();
        if (refuseCommitWithRecoveryConflict)
            return domain::ImageProcessingResult<domain::CommittedImageSource>::failure(
                {domain::ImageProcessingErrorCode::RecoveryConflict,
                 domain::ImageProcessingStage::TransactionRecovery});
        return domain::ImageProcessingResult<domain::CommittedImageSource>::success({L"photo.jpg"});
    }
    mutable std::atomic<unsigned> captures{};
    mutable std::atomic<unsigned> commits{};
    bool refuseCommitWithRecoveryConflict{};
    std::function<void()> afterCommit;

  private:
    std::vector<std::byte> bytes_;
};
class ImageCollection final : public domain::ImageSourceCollection
{
  public:
    std::vector<domain::DiscoveredImageSource> candidates;
    std::vector<domain::ImageSourceDiscoveryIssue> issues;
    bool waitForCancellation{};
    mutable std::promise<void> discoveryEntered;
    domain::ImageProcessingResult<domain::ImageSourceDiscovery> discover(
        domain::TraversalScope, std::stop_token cancellation,
        const domain::ImageSourceDiscoveryObserver &observer) const override
    {
        if (waitForCancellation)
        {
            discoveryEntered.set_value();
            std::mutex mutex;
            std::condition_variable_any cancelled;
            std::unique_lock lock{mutex};
            cancelled.wait(lock, cancellation, [] { return false; });
            return domain::ImageProcessingResult<domain::ImageSourceDiscovery>::failure(
                {domain::ImageProcessingErrorCode::Cancelled, domain::ImageProcessingStage::CandidateDiscovery});
        }
        if (observer)
            observer({candidates.size(), candidates.size()});
        return domain::ImageProcessingResult<domain::ImageSourceDiscovery>::success({candidates, issues, false});
    }
};

/// Runs real projected calls on a dedicated native dispatcher; only the MTA test
/// driver waits. This tests UI affinity without creating a XAML window or pumping
/// an invented event loop. The official test-host bootstrap supplies runtime identity.
class PresentationDispatcher final
{
  public:
    PresentationDispatcher()
    {
        jpg_spinner::test_support::ensurePresentationTestApartment();
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        auto ready = std::make_shared<std::promise<void>>();
        auto initialized = ready->get_future();
        thread_ = std::jthread([this, ready] {
            bool initializationPublished = false;
            try
            {
                // CreateOnDedicatedThread does not establish the STA fixture
                // contract. Own this STA and let WinUI run its native event loop.
                auto apartment = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
                controller_ = winrt::Microsoft::UI::Dispatching::DispatcherQueueController::CreateOnCurrentThread();
                // A console test host is not a XAML Application. Initialize its
                // native XAML lifetime before activating XAML event arguments.
                auto xaml = winrt::Microsoft::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
                ready->set_value();
                initializationPublished = true;
                controller_.DispatcherQueue().RunEventLoop();
                xaml.Close();
                controller_.ShutdownQueue();
            }
            catch (...)
            {
                // Publish setup failure once. A later event-loop failure belongs
                // to the joined thread, not the already fulfilled ready promise.
                if (!initializationPublished)
                    ready->set_exception(std::current_exception());
                else
                    // After publishing readiness, a failed native event loop
                    // invalidates the host. Do not silently return a passing
                    // Catch2 exit code or fulfill the ready promise a second time.
                    std::terminate();
            }
        });
        try
        {
            initialized.get();
        }
        catch (...)
        {
            thread_.join();
            winrt::uninit_apartment();
            throw;
        }
    }
    ~PresentationDispatcher()
    {
        controller_.DispatcherQueue().EnqueueEventLoopExit();
        thread_.join();
        winrt::uninit_apartment();
    }
    void invoke(const std::function<void()> &operation) const
    {
        auto completion = std::make_shared<std::promise<void>>();
        auto finished = completion->get_future();
        const bool queued = controller_.DispatcherQueue().TryEnqueue([completion, operation] {
            try
            {
                operation();
                completion->set_value();
            }
            catch (...)
            {
                completion->set_exception(std::current_exception());
            }
        });
        if (!queued)
            throw std::runtime_error("The native presentation queue refused the test operation.");
        if (finished.wait_for(std::chrono::seconds{30}) != std::future_status::ready)
            throw std::runtime_error("The native presentation queue did not complete within its test bound.");
        // No assertions run concurrently in the MTA driver and STA callback;
        // Catch2's reporter is not a multi-threaded assertion consumer.
        finished.get();
    }
    auto createModel(std::shared_ptr<RecoveryEngine> recovery = std::make_shared<RecoveryEngine>(),
                     std::shared_ptr<const jpg_spinner::jpeg::JpegTransformationEngine> transformation =
                         std::make_shared<RefusingTransformationEngine>()) const
    {
        return winrt::make_self<winrt::JpgSpinner::Presentation::implementation::MainWindowViewModel>(
            controller_.DispatcherQueue(),
            std::make_shared<jpg_spinner::batch_processing::BatchProcessingCoordinator>(std::move(transformation)),
            std::move(recovery));
    }

  private:
    winrt::Microsoft::UI::Dispatching::DispatcherQueueController controller_{nullptr};
    std::jthread thread_;
};

PresentationDispatcher &presentationDispatcher()
{
    // Like the app's UI thread, one XAML/dispatcher lifetime spans the host.
    // Individual tests create fresh models and capabilities, not another XAML
    // framework after shutting down the process's previous main STA.
    static PresentationDispatcher dispatcher;
    return dispatcher;
}

/// Register while the action is Started, before Cancel. Native Status becomes
/// Canceled immediately, so get()/GetResults alone are not a worker-completion
/// barrier. Only the MTA test driver waits on this bounded completion signal.
std::future<void> observeActionCompletion(winrt::Windows::Foundation::IAsyncAction const &action)
{
    auto completion = std::make_shared<std::promise<void>>();
    auto completed = completion->get_future();
    action.Completed([completion](auto const &, auto) { completion->set_value(); });
    return completed;
}
} // namespace

TEST_CASE("Review refuses replacement without a displayed backup and unacknowledged trimming",
          "[presentation][view-model][review-safety]")
{
    const bool replacement = GENERATE(true, false);
    auto &dispatcher = presentationDispatcher();
    auto source = std::make_shared<EncodedImageSource>(replacement ? std::uint16_t{5} : std::uint16_t{2});
    auto collection = std::make_shared<ImageCollection>();
    collection->candidates = {{L"photo.jpg", source}};
    jpg_spinner::batch_processing::BatchProcessingRequest request{collection};
    if (replacement)
        request.outputDisposition = domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup;
    else
        request.edgeHandlingPolicy = domain::EdgeHandlingPolicy::TrimPartialMinimumCodedUnits;
    winrt::Windows::Foundation::IAsyncAction analysis{nullptr};
    winrt::com_ptr<winrt::JpgSpinner::Presentation::implementation::MainWindowViewModel> implementation;
    dispatcher.invoke([&] {
        implementation = dispatcher.createModel();
        implementation->selectSource(request, L"selected", L"");
        analysis = implementation->AnalyzeAsync();
    });
    analysis.get();
    dispatcher.invoke([&] {
        auto model = implementation.as<winrt::JpgSpinner::Presentation::MainWindowViewModel>();
        REQUIRE(model.State() == winrt::JpgSpinner::Presentation::ImageProcessingExperienceState::Review);
        REQUIRE(model.Rows().Size() == 1);
        REQUIRE(model.Rows().GetAt(0).IsEligibleForProcessing()); // Not a blanket refusal of the valid JPEG.
        REQUIRE_FALSE(model.CanBeginProcessing());
        REQUIRE(source->commits == 0);
    });
}

// The accepted initial contract must exist in the generated projection before
// the shell can bind it. This is intentionally a compile-time contract RED,
// not a claim that the missing implementation executed.
TEST_CASE("Presentation exposes the six semantic workflow states", "[presentation][view-model]")
{
    using winrt::JpgSpinner::Presentation::ImageProcessingExperienceState;
    REQUIRE(ImageProcessingExperienceState::SourceSelection != ImageProcessingExperienceState::Processing);
    REQUIRE(ImageProcessingExperienceState::Analysis != ImageProcessingExperienceState::Review);
    REQUIRE(ImageProcessingExperienceState::Results != ImageProcessingExperienceState::RecoveryRequired);
}

TEST_CASE("Review exposes exact trim extents and requires explicit per-row acknowledgment",
          "[presentation][view-model][review-safety]")
{
    auto &dispatcher = presentationDispatcher();
    auto collection = std::make_shared<ImageCollection>();
    collection->candidates = {{L"photo.jpg", std::make_shared<EncodedImageSource>(std::uint16_t{2})}};
    jpg_spinner::batch_processing::BatchProcessingRequest request{collection};
    request.edgeHandlingPolicy = domain::EdgeHandlingPolicy::TrimPartialMinimumCodedUnits;
    winrt::com_ptr<winrt::JpgSpinner::Presentation::implementation::MainWindowViewModel> implementation;
    winrt::Windows::Foundation::IAsyncAction analysis{nullptr};
    dispatcher.invoke([&] {
        implementation = dispatcher.createModel();
        implementation->selectSource(request, L"selected", L"");
        analysis = implementation->AnalyzeAsync();
    });
    analysis.get();
    dispatcher.invoke([&] {
        auto model = implementation.as<winrt::JpgSpinner::Presentation::MainWindowViewModel>();
        auto row = model.Rows().GetAt(0);
        // Independent fixture geometry: 31x19, 8x8 iMCU, horizontal mirror.
        REQUIRE(row.DiscardedSourceRightEdgeWidthInPixels() == 7);
        REQUIRE(row.DiscardedSourceBottomEdgeHeightInPixels() == 0);
        REQUIRE_FALSE(row.IsEdgeTrimmingAcknowledged());
        REQUIRE_FALSE(model.CanBeginProcessing());
        model.AcknowledgeEdgeTrimming(0);
        REQUIRE(row.IsEdgeTrimmingAcknowledged());
        REQUIRE(model.CanBeginProcessing());
        REQUIRE_FALSE(model.Rows()
                          .try_as<winrt::Windows::Foundation::Collections::IVector<
                              winrt::JpgSpinner::Presentation::ImageProcessingRowViewModel>>());
        model.ReturnToSourceSelection();
        REQUIRE_THROWS_AS(model.AcknowledgeEdgeTrimming(0), winrt::hresult_illegal_method_call);
    });
}

TEST_CASE("Replacement review exposes its supplied computed backup destination",
          "[presentation][view-model][review-safety]")
{
    auto &dispatcher = presentationDispatcher();
    auto collection = std::make_shared<ImageCollection>();
    collection->candidates = {{L"photo.jpg", std::make_shared<EncodedImageSource>(std::uint16_t{5})}};
    jpg_spinner::batch_processing::BatchProcessingRequest request{collection};
    request.outputDisposition = domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup;
    winrt::com_ptr<winrt::JpgSpinner::Presentation::implementation::MainWindowViewModel> implementation;
    winrt::Windows::Foundation::IAsyncAction analysis{nullptr};
    dispatcher.invoke([&] {
        implementation = dispatcher.createModel();
        implementation->selectSource(request, L"selected", L"selected/JPG Spinner Backups/batch-identity");
        analysis = implementation->AnalyzeAsync();
    });
    analysis.get();
    dispatcher.invoke([&] {
        auto model = implementation.as<winrt::JpgSpinner::Presentation::MainWindowViewModel>();
        REQUIRE(model.BackupRootDisplayPath() == L"selected/JPG Spinner Backups/batch-identity");
        REQUIRE(model.CanBeginProcessing());
    });
}

TEST_CASE("Processing consumes review once and reports a transformation refusal honestly", "[presentation][view-model]")
{
    auto &dispatcher = presentationDispatcher();
    auto source = std::make_shared<EncodedImageSource>(std::uint16_t{5});
    auto collection = std::make_shared<ImageCollection>();
    collection->candidates = {{L"photo.jpg", source}};
    winrt::com_ptr<winrt::JpgSpinner::Presentation::implementation::MainWindowViewModel> implementation;
    winrt::Windows::Foundation::IAsyncAction action{nullptr};
    dispatcher.invoke([&] {
        implementation = dispatcher.createModel();
        implementation->selectSource({collection}, L"selected", L"");
        action = implementation->AnalyzeAsync();
    });
    action.get();
    dispatcher.invoke([&] {
        action = implementation->BeginProcessingAsync();
        REQUIRE(implementation->State() == winrt::JpgSpinner::Presentation::ImageProcessingExperienceState::Processing);
        REQUIRE(implementation->CanCancelCurrentOperation());
        REQUIRE_FALSE(implementation->CanSelectSourceFolder());
    });
    action.get();
    dispatcher.invoke([&] {
        REQUIRE(implementation->State() == winrt::JpgSpinner::Presentation::ImageProcessingExperienceState::Results);
        REQUIRE(implementation->CompletedFileCount() == 1);
        REQUIRE(implementation->FailedFileCount() == 1);
        REQUIRE_FALSE(implementation->Rows().GetAt(0).ErrorTitle().empty());
        REQUIRE_FALSE(implementation->Rows().GetAt(0).ErrorExplanation().empty());
        REQUIRE_FALSE(implementation->Rows().GetAt(0).ErrorRemedy().empty());
        REQUIRE(implementation->Rows().GetAt(0).Outcome() ==
                winrt::JpgSpinner::Presentation::ImageProcessingRowOutcome::ProcessingFailed);
        REQUIRE(implementation->ProgressPercentage().Value() == 100.0);
        REQUIRE(source->commits == 0);
        REQUIRE_THROWS_AS(implementation->BeginProcessingAsync(), winrt::hresult_illegal_method_call);
    });
}

TEST_CASE("Discovery issues remain visible without corrupting image-result ordinals",
          "[presentation][view-model][discovery-issues]")
{
    auto &dispatcher = presentationDispatcher();
    auto collection = std::make_shared<ImageCollection>();
    collection->candidates = {{L"photo.jpg", std::make_shared<EncodedImageSource>(std::uint16_t{5})}};
    collection->issues = std::vector<domain::ImageSourceDiscoveryIssue>{
        {L"unreadable/photo.jpg", domain::ImageSourceDiscoveryIssueKind::SourceAccessFailed,
         domain::ImageProcessingError{domain::ImageProcessingErrorCode::SourceAccessDenied,
                                      domain::ImageProcessingStage::CandidateDiscovery}},
        {L"linked-folder", domain::ImageSourceDiscoveryIssueKind::ReparsePointSkipped, {}},
        {L"owned-output", domain::ImageSourceDiscoveryIssueKind::ApplicationOwnedFolderExcluded, {}}};
    winrt::com_ptr<winrt::JpgSpinner::Presentation::implementation::MainWindowViewModel> implementation;
    winrt::Windows::Foundation::IAsyncAction action{nullptr};
    auto notifications = std::make_shared<unsigned>(0);
    dispatcher.invoke([&] {
        implementation = dispatcher.createModel();
        implementation->PropertyChanged([notifications](auto const &, auto const &args) {
            if (args.PropertyName() == L"DiscoveryIssues")
                ++*notifications;
        });
        implementation->selectSource({collection}, L"selected", L"");
        action = implementation->AnalyzeAsync();
    });
    action.get();
    dispatcher.invoke([&] {
        auto issues = implementation->DiscoveryIssues();
        REQUIRE(implementation->Rows().Size() == 1);
        REQUIRE(issues.Size() == 3);
        REQUIRE(issues.GetAt(0).RelativePath() == L"unreadable/photo.jpg");
        REQUIRE(issues.GetAt(0).Kind() ==
                winrt::JpgSpinner::Presentation::ImageSourceDiscoveryIssueKind::SourceAccessFailed);
        REQUIRE_FALSE(issues.GetAt(0).ErrorTitle().empty());
        REQUIRE_FALSE(issues.GetAt(0).ErrorExplanation().empty());
        REQUIRE_FALSE(issues.GetAt(0).ErrorRemedy().empty());
        REQUIRE(issues.GetAt(1).Kind() ==
                winrt::JpgSpinner::Presentation::ImageSourceDiscoveryIssueKind::ReparsePointSkipped);
        REQUIRE(issues.GetAt(1).ErrorTitle().empty()); // A deliberate exclusion is not a fabricated domain error.
        REQUIRE(issues.GetAt(2).Kind() ==
                winrt::JpgSpinner::Presentation::ImageSourceDiscoveryIssueKind::ApplicationOwnedFolderExcluded);
        REQUIRE_FALSE(issues.try_as<winrt::Windows::Foundation::Collections::IVector<
                          winrt::JpgSpinner::Presentation::ImageSourceDiscoveryIssueViewModel>>());
        REQUIRE(implementation->CanBeginProcessing()); // Warnings do not deny the valid image's plan.
        action = implementation->BeginProcessingAsync();
    });
    action.get();
    dispatcher.invoke([&] {
        REQUIRE(implementation->DiscoveryIssues().Size() == 3);
        REQUIRE(implementation->Rows().Size() == 1);
        REQUIRE(implementation->Rows().GetAt(0).Outcome() ==
                winrt::JpgSpinner::Presentation::ImageProcessingRowOutcome::ProcessingFailed);
        REQUIRE(*notifications >= 2);
        implementation->ReturnToSourceSelection();
        REQUIRE(implementation->DiscoveryIssues().Size() == 0);
        implementation->selectSource({collection}, L"new selection", L"");
        REQUIRE(implementation->DiscoveryIssues().Size() == 0);
    });
}

TEST_CASE("Analysis cancellation retains no reviewed artifact", "[presentation][view-model][cancellation]")
{
    const bool cancelReturnedAction = GENERATE(false, true);
    auto &dispatcher = presentationDispatcher();
    auto collection = std::make_shared<ImageCollection>();
    collection->waitForCancellation = true;
    auto entered = collection->discoveryEntered.get_future();
    winrt::com_ptr<winrt::JpgSpinner::Presentation::implementation::MainWindowViewModel> implementation;
    winrt::Windows::Foundation::IAsyncAction action{nullptr};
    std::future<void> completed;
    dispatcher.invoke([&] {
        implementation = dispatcher.createModel();
        implementation->selectSource({collection}, L"selected", L"");
        action = implementation->AnalyzeAsync();
        if (cancelReturnedAction)
            completed = observeActionCompletion(action);
    });
    const bool started = entered.wait_for(std::chrono::seconds{30}) == std::future_status::ready;
    dispatcher.invoke([&] {
        if (cancelReturnedAction)
            action.Cancel();
        else
            implementation->CancelCurrentOperation();
    });
    if (cancelReturnedAction)
    {
        REQUIRE(completed.wait_for(std::chrono::seconds{30}) == std::future_status::ready);
        completed.get();
        REQUIRE_THROWS_AS(action.GetResults(), winrt::hresult_canceled);
    }
    else
        action.get();
    REQUIRE(started);
    dispatcher.invoke([&] {
        REQUIRE(implementation->State() ==
                winrt::JpgSpinner::Presentation::ImageProcessingExperienceState::SourceSelection);
        REQUIRE_FALSE(implementation->CanBeginProcessing());
        REQUIRE_FALSE(implementation->ProgressPercentage());
        REQUIRE(implementation->Rows().Size() == 0);
    });
}

TEST_CASE("Unresolved recovery disables selection and successful retry releases it",
          "[presentation][view-model][recovery]")
{
    auto &dispatcher = presentationDispatcher();
    auto recovery = std::make_shared<RecoveryEngine>();
    recovery->refuseRecovery = true;
    winrt::com_ptr<winrt::JpgSpinner::Presentation::implementation::MainWindowViewModel> implementation;
    winrt::Windows::Foundation::IAsyncAction action{nullptr};
    dispatcher.invoke([&] {
        implementation = dispatcher.createModel(recovery);
        action = implementation->RecoverAsync();
    });
    action.get();
    dispatcher.invoke([&] {
        REQUIRE(implementation->State() ==
                winrt::JpgSpinner::Presentation::ImageProcessingExperienceState::RecoveryRequired);
        REQUIRE_FALSE(implementation->CanSelectSourceFolder());
        REQUIRE_FALSE(implementation->CanBeginProcessing());
        REQUIRE_FALSE(implementation->CanReturnToSourceSelection());
        REQUIRE_FALSE(implementation->ErrorTitle().empty());
        REQUIRE_THROWS_AS(implementation->ReturnToSourceSelection(), winrt::hresult_illegal_method_call);
        recovery->refuseRecovery = false; // Previous operation has completed; no concurrent fake mutation.
        action = implementation->RecoverAsync();
    });
    action.get();
    dispatcher.invoke([&] {
        REQUIRE(implementation->State() ==
                winrt::JpgSpinner::Presentation::ImageProcessingExperienceState::SourceSelection);
        REQUIRE(implementation->CanSelectSourceFolder());
        REQUIRE(implementation->ErrorTitle().empty());
        REQUIRE(recovery->recoveries == 2);
    });
}

TEST_CASE("Committed output is not relabelled as cancelled and uncertain commit requires recovery",
          "[presentation][view-model][results]")
{
    const bool uncertain = GENERATE(false, true);
    const bool cancelReturnedAction = GENERATE(false, true);
    auto &dispatcher = presentationDispatcher();
    auto source = std::make_shared<EncodedImageSource>(std::uint16_t{5});
    auto collection = std::make_shared<ImageCollection>();
    collection->candidates = {{L"photo.jpg", source}};
    winrt::com_ptr<winrt::JpgSpinner::Presentation::implementation::MainWindowViewModel> implementation;
    winrt::Windows::Foundation::IAsyncAction action{nullptr};
    std::future<void> completed;
    dispatcher.invoke([&] {
        implementation =
            dispatcher.createModel(std::make_shared<RecoveryEngine>(),
                                   std::make_shared<jpg_spinner::jpeg::LibJpegTurboTransformationEngine>());
        implementation->selectSource({collection}, L"selected", L"");
        action = implementation->AnalyzeAsync();
    });
    action.get();
    source->refuseCommitWithRecoveryConflict = uncertain;
    if (!uncertain)
        source->afterCommit = [&] {
            dispatcher.invoke([&] {
                if (cancelReturnedAction)
                    action.Cancel();
                else
                    implementation->CancelCurrentOperation();
            });
        };
    dispatcher.invoke([&] {
        action = implementation->BeginProcessingAsync();
        if (!uncertain && cancelReturnedAction)
            completed = observeActionCompletion(action);
    });
    if (!uncertain && cancelReturnedAction)
    {
        REQUIRE(completed.wait_for(std::chrono::seconds{30}) == std::future_status::ready);
        completed.get();
        REQUIRE_THROWS_AS(action.GetResults(), winrt::hresult_canceled);
    }
    else
        action.get();
    dispatcher.invoke([&] {
        REQUIRE(source->commits == 1); // Validated JPEG reached the capability; no Windows write is claimed.
        REQUIRE(implementation->CompletedFileCount() == 1);
        REQUIRE(implementation->CancelledFileCount() == 0);
        if (uncertain)
        {
            REQUIRE(implementation->State() ==
                    winrt::JpgSpinner::Presentation::ImageProcessingExperienceState::RecoveryRequired);
            REQUIRE_FALSE(implementation->CanSelectSourceFolder());
            REQUIRE(implementation->Rows().GetAt(0).Outcome() ==
                    winrt::JpgSpinner::Presentation::ImageProcessingRowOutcome::ProcessingFailed);
        }
        else
        {
            REQUIRE(implementation->State() ==
                    winrt::JpgSpinner::Presentation::ImageProcessingExperienceState::Results);
            REQUIRE(implementation->Rows().GetAt(0).Outcome() ==
                    winrt::JpgSpinner::Presentation::ImageProcessingRowOutcome::CorrectedCopyCreated);
            REQUIRE(implementation->FailedFileCount() == 0);
        }
    });
}

TEST_CASE("Cancelling the returned recovery action preserves fail-closed state and releases the retry gate",
          "[presentation][view-model][cancellation][recovery]")
{
    auto &dispatcher = presentationDispatcher();
    auto recovery = std::make_shared<RecoveryEngine>();
    recovery->waitForCancellation = true;
    auto entered = recovery->recoveryEntered.get_future();
    winrt::com_ptr<winrt::JpgSpinner::Presentation::implementation::MainWindowViewModel> implementation;
    winrt::Windows::Foundation::IAsyncAction action{nullptr};
    std::future<void> completed;
    dispatcher.invoke([&] {
        implementation = dispatcher.createModel(recovery);
        action = implementation->RecoverAsync();
        completed = observeActionCompletion(action);
    });
    const bool started = entered.wait_for(std::chrono::seconds{30}) == std::future_status::ready;
    bool canCancelRecovery{};
    bool batchCancelRefused{};
    dispatcher.invoke([&] {
        // The batch Cancel command is deliberately unavailable during recovery.
        // Its refusal must not request stop; only this action's Cancel below
        // admits cooperative recovery cancellation and terminal publication.
        canCancelRecovery = implementation->CanCancelCurrentOperation();
        try
        {
            implementation->CancelCurrentOperation();
        }
        catch (winrt::hresult_illegal_method_call const &)
        {
            batchCancelRefused = true;
        }
    });
    const bool refusalRequestedStop = started && recovery->observedRecoveryCancellation.stop_requested();
    // Always release the worker before asserting the refusal observations: a
    // regression in the command predicate must not strand the test fixture.
    action.Cancel();
    REQUIRE(completed.wait_for(std::chrono::seconds{30}) == std::future_status::ready);
    completed.get();
    REQUIRE_THROWS_AS(action.GetResults(), winrt::hresult_canceled);
    REQUIRE(started);
    REQUIRE_FALSE(canCancelRecovery);
    REQUIRE(batchCancelRefused);
    REQUIRE_FALSE(refusalRequestedStop);
    dispatcher.invoke([&] {
        REQUIRE(implementation->State() ==
                winrt::JpgSpinner::Presentation::ImageProcessingExperienceState::RecoveryRequired);
        REQUIRE_FALSE(implementation->ErrorTitle().empty());
        recovery->waitForCancellation = false; // Native operation has returned; no fixture data race.
        action = implementation->RecoverAsync();
    });
    action.get();
    dispatcher.invoke([&] {
        REQUIRE(implementation->CanSelectSourceFolder());
        REQUIRE(recovery->recoveries == 2);
    });
}

TEST_CASE("Unexpected recovery exception becomes safe text and does not leave recovery permanently busy",
          "[presentation][view-model][recovery]")
{
    auto &dispatcher = presentationDispatcher();
    auto recovery = std::make_shared<RecoveryEngine>();
    recovery->throwRecoveryException = true;
    winrt::com_ptr<winrt::JpgSpinner::Presentation::implementation::MainWindowViewModel> implementation;
    winrt::Windows::Foundation::IAsyncAction action{nullptr};
    dispatcher.invoke([&] {
        implementation = dispatcher.createModel(recovery);
        action = implementation->RecoverAsync();
    });
    action.get();
    dispatcher.invoke([&] {
        REQUIRE(implementation->State() ==
                winrt::JpgSpinner::Presentation::ImageProcessingExperienceState::RecoveryRequired);
        REQUIRE_FALSE(implementation->ErrorTitle().empty());
        // The injected exception contains both native text and a private path.
        // Every visible field must use the symbolic resource seam instead.
        for (auto const &component :
             {implementation->ErrorTitle(), implementation->ErrorExplanation(), implementation->ErrorRemedy()})
        {
            REQUIRE_FALSE(component.empty());
            const std::wstring_view displayedText{component.c_str(), component.size()};
            REQUIRE(displayedText.find(L"private") == std::wstring_view::npos);
            REQUIRE(displayedText.find(L"C:\\") == std::wstring_view::npos);
            REQUIRE(displayedText.find(L"HRESULT") == std::wstring_view::npos);
        }
        recovery->throwRecoveryException = false;
        action = implementation->RecoverAsync();
    });
    action.get();
    dispatcher.invoke([&] {
        REQUIRE(implementation->CanSelectSourceFolder());
        REQUIRE(implementation->ErrorTitle().empty());
    });
}

TEST_CASE("Presentation dispatcher qualifies native observable event arguments", "[presentation][runtime]")
{
    auto &dispatcher = presentationDispatcher();
    dispatcher.invoke([] {
        APTTYPE apartmentType{};
        APTTYPEQUALIFIER apartmentQualifier{};
        winrt::check_hresult(CoGetApartmentType(&apartmentType, &apartmentQualifier));
        // An agile WinRT delegate can enter the neutral apartment from the STA.
        // Its qualifier, not APTTYPE alone, identifies the underlying UI thread.
        INFO("COM apartment type=" << apartmentType << ", qualifier=" << apartmentQualifier);
        REQUIRE((apartmentType == APTTYPE_STA || apartmentType == APTTYPE_MAINSTA ||
                 (apartmentType == APTTYPE_NA && (apartmentQualifier == APTTYPEQUALIFIER_NA_ON_STA ||
                                                  apartmentQualifier == APTTYPEQUALIFIER_NA_ON_MAINSTA))));
        auto arguments = winrt::Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{L"State"};
        REQUIRE(arguments.PropertyName() == L"State");
    });
}

TEST_CASE("Source selection cannot bypass reviewed analysis", "[presentation][view-model]")
{
    auto &dispatcher = presentationDispatcher();
    dispatcher.invoke([&] {
        auto implementation = dispatcher.createModel();
        auto model = implementation.as<winrt::JpgSpinner::Presentation::MainWindowViewModel>();
        REQUIRE(model.State() == winrt::JpgSpinner::Presentation::ImageProcessingExperienceState::SourceSelection);
        REQUIRE(model.CanSelectSourceFolder());
        REQUIRE_FALSE(model.CanBeginProcessing());
        REQUIRE_FALSE(model.CanCancelCurrentOperation());
        REQUIRE_FALSE(model.CanReturnToSourceSelection());
        REQUIRE_FALSE(model.ProgressPercentage());
        REQUIRE(model.OutputDisposition() == winrt::JpgSpinner::Presentation::OutputDisposition::CreateCorrectedCopy);
        REQUIRE(model.TraversalScope() == winrt::JpgSpinner::Presentation::TraversalScope::SelectedFolderOnly);
        REQUIRE(model.EdgeHandlingPolicy() ==
                winrt::JpgSpinner::Presentation::EdgeHandlingPolicy::RequirePerfectCoefficientTransform);
        REQUIRE(model.OutputScanOrganization() ==
                winrt::JpgSpinner::Presentation::OutputScanOrganization::PreserveSource);
        REQUIRE_THROWS_AS(model.BeginProcessingAsync(), winrt::hresult_illegal_method_call);
        REQUIRE(model.State() == winrt::JpgSpinner::Presentation::ImageProcessingExperienceState::SourceSelection);
    });
}

TEST_CASE("Real analysis produces observable review without transforming or committing", "[presentation][view-model]")
{
    auto &dispatcher = presentationDispatcher();
    auto source = std::make_shared<EncodedImageSource>(std::uint16_t{5}); // Transpose is perfect for partial iMCUs.
    auto collection = std::make_shared<ImageCollection>();
    collection->candidates = {{L"selected/photo.jpg", source}};
    winrt::Windows::Foundation::IAsyncAction analysis{nullptr};
    winrt::com_ptr<winrt::JpgSpinner::Presentation::implementation::MainWindowViewModel> implementation;
    auto states = std::make_shared<std::vector<winrt::JpgSpinner::Presentation::ImageProcessingExperienceState>>();
    winrt::event_token stateSubscription;
    dispatcher.invoke([&] {
        implementation = dispatcher.createModel();
        auto model = implementation.as<winrt::JpgSpinner::Presentation::MainWindowViewModel>();
        stateSubscription =
            model.PropertyChanged([states, weak = implementation->get_weak()](auto const &, auto const &change) {
                if (change.PropertyName() == L"State")
                    if (auto observed = weak.get())
                        states->push_back(observed->State());
            });
        implementation->selectSource({collection}, L"selected", L"");
        analysis = model.AnalyzeAsync();
        REQUIRE(model.State() == winrt::JpgSpinner::Presentation::ImageProcessingExperienceState::Analysis);
        REQUIRE(model.CanCancelCurrentOperation());
        REQUIRE_FALSE(model.CanSelectSourceFolder());
        REQUIRE_FALSE(model.CanBeginProcessing());
        REQUIRE_FALSE(model.ProgressPercentage());
    });
    analysis.get(); // MTA driver only: the UI queue must remain free to publish completion.
    dispatcher.invoke([&] {
        auto model = implementation.as<winrt::JpgSpinner::Presentation::MainWindowViewModel>();
        REQUIRE(model.State() == winrt::JpgSpinner::Presentation::ImageProcessingExperienceState::Review);
        REQUIRE(model.CanBeginProcessing());
        REQUIRE(model.CanReturnToSourceSelection());
        REQUIRE_FALSE(model.CanCancelCurrentOperation());
        REQUIRE(model.Rows().Size() == 1);
        REQUIRE(model.Rows().GetAt(0).DisplayName() == L"photo.jpg");
        REQUIRE(model.Rows().GetAt(0).IsEligibleForProcessing());
        REQUIRE(source->captures == 1);
        REQUIRE(source->commits == 0);
        REQUIRE(states->size() == 2);
        REQUIRE((*states)[0] == winrt::JpgSpinner::Presentation::ImageProcessingExperienceState::Analysis);
        REQUIRE((*states)[1] == winrt::JpgSpinner::Presentation::ImageProcessingExperienceState::Review);
        model.ReturnToSourceSelection();
        REQUIRE(model.State() == winrt::JpgSpinner::Presentation::ImageProcessingExperienceState::SourceSelection);
        REQUIRE(model.Rows().Size() == 0);
        REQUIRE_FALSE(model.CanBeginProcessing());
        model.PropertyChanged(stateSubscription);
    });
}
