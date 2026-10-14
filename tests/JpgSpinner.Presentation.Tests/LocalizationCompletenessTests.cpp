#include "pch.h"
#include "Localization/ImageProcessingErrorText.h"
#include "PresentationTestApartment.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <array>
#include <initializer_list>
#include <string_view>
#include <wil/resource.h>
#include <winrt/Microsoft.Windows.ApplicationModel.Resources.h>

namespace
{
using ErrorCode = jpg_spinner::domain::ImageProcessingErrorCode;
// Independently enumerated domain contract: a missing resource cannot be hidden
// by iterating the production mapper's own list of supported errors.
constexpr std::array errorCodes{ErrorCode::SourceAccessDenied,
                                ErrorCode::SourceSharingViolation,
                                ErrorCode::SourceRevisionCaptureFailed,
                                ErrorCode::SourceDiscoveryFailed,
                                ErrorCode::InvalidBatchProcessingRequest,
                                ErrorCode::SourceChangedAfterAnalysis,
                                ErrorCode::DestinationAccessDenied,
                                ErrorCode::EncodedFileTooLarge,
                                ErrorCode::PixelCountLimitExceeded,
                                ErrorCode::MetadataLengthLimitExceeded,
                                ErrorCode::ProgressiveScanCountLimitExceeded,
                                ErrorCode::MalformedJpegStructure,
                                ErrorCode::MalformedImageMetadata,
                                ErrorCode::MetadataPreservationFailed,
                                ErrorCode::InvalidOrientationMetadata,
                                ErrorCode::UnsupportedJpegCodingProcess,
                                ErrorCode::UnsupportedJpegSamplePrecision,
                                ErrorCode::UnsupportedJpegComponentOrganization,
                                ErrorCode::UnsupportedJpegDeferredHeight,
                                ErrorCode::UnsupportedJpegRestartIntervalChanges,
                                ErrorCode::MultiPictureJpegNotSupported,
                                ErrorCode::MotionPhotoNotSupported,
                                ErrorCode::UnsupportedTrailingPayload,
                                ErrorCode::ContentCredentialsWouldBeInvalidated,
                                ErrorCode::UnsupportedJumbfMetadata,
                                ErrorCode::UnsupportedEmbeddedPreviewMetadata,
                                ErrorCode::ExtendedXmpMutationNotSupported,
                                ErrorCode::PerfectCoefficientTransformUnavailable,
                                ErrorCode::CoefficientTransformationFailed,
                                ErrorCode::OutputBufferTooSmall,
                                ErrorCode::WorkingMemoryAllocationFailed,
                                ErrorCode::InsufficientStorageSpace,
                                ErrorCode::OutputRelativePathCollision,
                                ErrorCode::StagingFileCreationFailed,
                                ErrorCode::StagingWriteFailed,
                                ErrorCode::StagingFlushFailed,
                                ErrorCode::StagingCloseFailed,
                                ErrorCode::StagedOutputVerificationFailed,
                                ErrorCode::StagedOutputHashMismatch,
                                ErrorCode::OutputValidationFailed,
                                ErrorCode::CorrectedCopyDestinationCreationFailed,
                                ErrorCode::CorrectedCopyCommitFailed,
                                ErrorCode::BackupCreationFailed,
                                ErrorCode::BackupVerificationFailed,
                                ErrorCode::OriginalReplacementFailed,
                                ErrorCode::JournalPersistenceFailed,
                                ErrorCode::StorageProviderRecoveryContractNotEstablished,
                                ErrorCode::JournalStoreBusy,
                                ErrorCode::RecoveryConflict,
                                ErrorCode::WindowsLongPathSupportUnavailable,
                                ErrorCode::Cancelled};
static_assert(errorCodes.size() == 51);
static_assert(static_cast<unsigned>(ErrorCode::Cancelled) + 1 == errorCodes.size());
} // namespace

TEST_CASE("Every domain error has native resources in each supported language", "[presentation][localization]")
{
    jpg_spinner::test_support::ensurePresentationTestApartment();
    const auto language = GENERATE(L"en-US", L"en-GB", L"ru");
    using namespace winrt::Microsoft::Windows::ApplicationModel::Resources;
    ResourceManager manager{ResourceLoader::GetDefaultResourceFilePath()};
    auto context = manager.CreateResourceContext();
    context.QualifierValues().Insert(L"Language", language);
    auto resources = manager.MainResourceMap().GetSubtree(L"Resources");
    for (auto code : errorCodes)
    {
        INFO("Error code=" << static_cast<unsigned>(code));
        auto text = jpg_spinner::presentation::loadImageProcessingErrorText(code, resources, context);
        REQUIRE_FALSE(text.title.empty());
        REQUIRE_FALSE(text.explanation.empty());
        REQUIRE_FALSE(text.remedy.empty());
        // Check the whole visible triplet, not only the explanation. A future
        // resource edit must not move native diagnostics into a title/remedy.
        for (auto const &component : {text.title, text.explanation, text.remedy})
        {
            const std::wstring_view displayedText{component.c_str(), component.size()};
            REQUIRE(displayedText.find(L"C:\\") == std::wstring_view::npos);
            REQUIRE(displayedText.find(L"HRESULT") == std::wstring_view::npos);
        }
        // Native PRI exposes the selected qualifier: matching English fallback
        // text is not evidence that the requested locale has its own candidate.
        for (auto const &selectedLanguage : text.selectedResourceLanguages)
        {
            INFO("Selected language=" << winrt::to_string(selectedLanguage));
            // BCP 47 language tags are case-insensitive (RFC 5646 section 2.1.1).
            // Preserve the full tag: en-US is not proof of an en-GB candidate.
            REQUIRE(CompareStringOrdinal(selectedLanguage.c_str(), -1, language, -1, TRUE) == CSTR_EQUAL);
        }
    }
}

TEST_CASE("Unavailable Windows long paths have specific localized guidance rather than unknown-error fallback",
          "[presentation][localization][long-path-policy]")
{
    jpg_spinner::test_support::ensurePresentationTestApartment();
    const auto language = GENERATE(L"en-US", L"en-GB", L"ru");
    using namespace winrt::Microsoft::Windows::ApplicationModel::Resources;
    ResourceManager manager{ResourceLoader::GetDefaultResourceFilePath()};
    const auto context = manager.CreateResourceContext();
    context.QualifierValues().Insert(L"Language", language);
    const auto resources = manager.MainResourceMap().GetSubtree(L"Resources");
    const auto text = jpg_spinner::presentation::loadImageProcessingErrorText(
        ErrorCode::WindowsLongPathSupportUnavailable, resources, context);
    // Independent resource identities prevent a nonempty generic fallback from
    // masquerading as guidance for the specific host prerequisite.
    CHECK(text.title ==
          resources.GetValue(L"ImageProcessingError_WindowsLongPathSupportUnavailable_Title", context).ValueAsString());
    CHECK(text.explanation ==
          resources.GetValue(L"ImageProcessingError_WindowsLongPathSupportUnavailable_Explanation", context)
              .ValueAsString());
    CHECK(
        text.remedy ==
        resources.GetValue(L"ImageProcessingError_WindowsLongPathSupportUnavailable_Remedy", context).ValueAsString());
}

TEST_CASE("Startup recovery preserves the specific localized explanation and remedy",
          "[presentation][localization][startup-recovery-guidance]")
{
    jpg_spinner::test_support::ensurePresentationTestApartment();
    const auto language = GENERATE(L"en-US", L"en-GB", L"ru");
    const auto code = GENERATE(ErrorCode::WindowsLongPathSupportUnavailable, ErrorCode::RecoveryConflict);
    using namespace winrt::Microsoft::Windows::ApplicationModel::Resources;
    ResourceManager manager{ResourceLoader::GetDefaultResourceFilePath()};
    const auto context = manager.CreateResourceContext();
    context.QualifierValues().Insert(L"Language", language);
    const auto resources = manager.MainResourceMap().GetSubtree(L"Resources");
    // Independent resource identities distinguish the administrator/restart
    // remedy from the source-access remedy without activating a XAML Window.
    const auto stem = code == ErrorCode::WindowsLongPathSupportUnavailable
                          ? L"ImageProcessingError_WindowsLongPathSupportUnavailable"
                          : L"ImageProcessingError_RecoveryConflict";
    const auto expected = resources.GetValue(L"StartupRecovery_PreservationNotice", context).ValueAsString() + L"\n" +
                          resources.GetValue(std::wstring{stem} + L"_Explanation", context).ValueAsString() + L"\n" +
                          resources.GetValue(std::wstring{stem} + L"_Remedy", context).ValueAsString();
    CHECK(jpg_spinner::presentation::loadStartupRecoveryErrorMessage(code, resources, context) == expected);
}

TEST_CASE("Unknown errors use safe generic resources without diagnostic interpolation", "[presentation][localization]")
{
    jpg_spinner::test_support::ensurePresentationTestApartment();
    using namespace winrt::Microsoft::Windows::ApplicationModel::Resources;
    ResourceManager manager{ResourceLoader::GetDefaultResourceFilePath()};
    auto resources = manager.MainResourceMap().GetSubtree(L"Resources");
    auto context = manager.CreateResourceContext();
    const auto language = GENERATE(L"en-US", L"en-GB", L"ru");
    context.QualifierValues().Insert(L"Language", language);
    auto unknown =
        jpg_spinner::presentation::loadImageProcessingErrorText(static_cast<ErrorCode>(-1), resources, context);
    if (std::wstring_view{language} == L"en-US")
        REQUIRE(unknown.title == L"Processing could not be completed");
    for (auto const &component : {unknown.title, unknown.explanation, unknown.remedy})
    {
        REQUIRE_FALSE(component.empty());
        const std::wstring_view displayedText{component.c_str(), component.size()};
        REQUIRE(displayedText.find(L"C:\\") == std::wstring_view::npos);
        REQUIRE(displayedText.find(L"HRESULT") == std::wstring_view::npos);
    }
}

TEST_CASE("Every shell label has its own native resource candidate in every supported language",
          "[presentation][localization][shell]")
{
    jpg_spinner::test_support::ensurePresentationTestApartment();
    using namespace winrt::Microsoft::Windows::ApplicationModel::Resources;
    ResourceManager manager{ResourceLoader::GetDefaultResourceFilePath()};
    auto resources = manager.MainResourceMap().GetSubtree(L"Resources");
    auto context = manager.CreateResourceContext();
    const auto language = GENERATE(L"en-US", L"en-GB", L"ru");
    context.QualifierValues().Insert(L"Language", language);
    // MRT indexes property identifiers as URI path segments. Independently list
    // the shell's contract rather than reusing a production mapper or treating
    // successful English fallback as a translated Russian/British candidate.
    constexpr const wchar_t *keys[]{
        L"Shell_SourceSelectionHeading/Text",
        L"Shell_AnalysisHeading/Text",
        L"Shell_ReviewHeading/Text",
        L"Shell_ProcessingHeading/Text",
        L"Shell_ResultsHeading/Text",
        L"Shell_RecoveryRequiredHeading/Text",
        L"Shell_SourcePicker/Content",
        L"Shell_Analyze/Content",
        L"Shell_TraversalScope/Header",
        L"Shell_OutputDisposition/Header",
        L"Shell_EdgeHandling/Header",
        L"Shell_ScanOrganization/Header",
        L"Shell_SelectedFolderOnly/Content",
        L"Shell_SelectedFolderAndDescendants/Content",
        L"Shell_CreateCorrectedCopy/Content",
        L"Shell_ReplaceWithVerifiedBackup/Content",
        L"Shell_RequirePerfectTransform/Content",
        L"Shell_TrimPartialMinimumCodedUnits/Content",
        L"Shell_PreserveSourceScans/Content",
        L"Shell_SequentialDct/Content",
        L"Shell_ProgressiveDct/Content",
        L"Shell_ReviewInstruction/Text",
        L"Shell_RecursiveExclusionNotice/Text",
        L"Shell_BackupDestinationLabel/Text",
        L"Shell_TrimConsent/Content",
        L"Shell_DiscardedRightEdgeLabel/Text",
        L"Shell_DiscardedBottomEdgeLabel/Text",
        L"Shell_StartProcessing/Content",
        L"Shell_CancelOperation/Content",
        L"Shell_ReturnToSourceSelection/Content",
        L"Shell_RetryRecovery/Content",
        L"Shell_ProcessingRowsLabel/Text",
        L"Shell_EligibleFilesLabel/Text",
        L"Shell_CompletedRowsLabel/Text",
        L"Shell_FailedFilesLabel/Text",
        L"Shell_CancelledFilesLabel/Text",
        L"Shell_InitialInstruction/Text",
        L"Shell_StartupRecoveryProgress/[using:Microsoft.UI.Xaml.Automation]AutomationProperties/Name",
        L"Shell_StartupRecoveryError/Title",
        L"StartupRecovery_PreservationNotice",
        L"Shell_StartupRecoveredCount/Text",
        L"Shell_RestoreRecoverySource/Content",
        L"Shell_ExportDiagnostics/Content",
        L"Shell_DiagnosticFileType",
        L"Shell_DiagnosticLogError/Title",
        L"Shell_DiagnosticLogError/Message"};
    for (const auto *key : keys)
    {
        INFO("Shell resource=" << winrt::to_string(key));
        const auto candidate = resources.GetValue(key, context);
        REQUIRE_FALSE(candidate.ValueAsString().empty());
        const auto selectedLanguage = candidate.QualifierValues().Lookup(L"Language");
        REQUIRE(CompareStringOrdinal(selectedLanguage.c_str(), -1, language, -1, TRUE) == CSTR_EQUAL);
    }
    // Attached-property namespace dots remain inside the qualified segment;
    // this is the native MakePRI resource shape, not a home-grown URI parser.
    for (const auto *control :
         {L"SourcePicker", L"Analyze", L"TraversalScope", L"OutputDisposition", L"EdgeHandling", L"ScanOrganization",
          L"StartProcessing", L"CancelOperation", L"RetryRecovery", L"RestoreRecoverySource", L"ExportDiagnostics",
          L"ReturnToSourceSelection", L"TrimConsent", L"DiscoveryObservations"})
        for (const auto *property : {L"Name", L"HelpText"})
        {
            const auto key = std::wstring{L"Shell_"} + control +
                             L"/[using:Microsoft.UI.Xaml.Automation]AutomationProperties/" + property;
            INFO("Accessible resource=" << winrt::to_string(key));
            const auto candidate = resources.GetValue(key, context);
            REQUIRE_FALSE(candidate.ValueAsString().empty());
            REQUIRE(CompareStringOrdinal(candidate.QualifierValues().Lookup(L"Language").c_str(), -1, language, -1,
                                         TRUE) == CSTR_EQUAL);
        }
    for (const auto *outcome : {L"Pending", L"CorrectedCopyCreated", L"OriginalReplacedWithVerifiedBackup",
                                L"NoOrientationNormalizationRequired", L"UnsupportedSourceSkipped", L"ProcessingFailed",
                                L"CancelledBeforeTransformation", L"CancelledBeforeCommit"})
    {
        const auto candidate = resources.GetValue(std::wstring{L"ImageProcessingOutcome_"} + outcome, context);
        REQUIRE_FALSE(candidate.ValueAsString().empty());
        REQUIRE(CompareStringOrdinal(candidate.QualifierValues().Lookup(L"Language").c_str(), -1, language, -1, TRUE) ==
                CSTR_EQUAL);
    }
}

TEST_CASE("Every discovery observation kind has a localized explanation", "[presentation][localization][discovery]")
{
    jpg_spinner::test_support::ensurePresentationTestApartment();
    using namespace winrt::Microsoft::Windows::ApplicationModel::Resources;
    ResourceManager manager{ResourceLoader::GetDefaultResourceFilePath()};
    const auto resources = manager.MainResourceMap().GetSubtree(L"Resources");
    const auto context = manager.CreateResourceContext();
    const auto language = GENERATE(L"en-US", L"en-GB", L"ru");
    context.QualifierValues().Insert(L"Language", language);
    // Explicitly cover deliberate exclusions as well as failed access. A blank
    // error title must not turn a skipped folder into an unexplained pathname.
    for (const auto *kind : {L"ReparsePointSkipped", L"SourceAccessFailed", L"ApplicationOwnedFolderExcluded"})
    {
        INFO("Discovery kind=" << winrt::to_string(kind));
        const auto candidate = resources.GetValue(std::wstring{L"ImageSourceDiscoveryObservation_"} + kind, context);
        REQUIRE_FALSE(candidate.ValueAsString().empty());
        REQUIRE(CompareStringOrdinal(candidate.QualifierValues().Lookup(L"Language").c_str(), -1, language, -1, TRUE) ==
                CSTR_EQUAL);
    }
}

TEST_CASE("Every top-level access key has its own native PRI candidate in each supported locale",
          "[presentation][localization][access-keys][compiled-resources]")
{
    jpg_spinner::test_support::ensurePresentationTestApartment();
    using namespace winrt::Microsoft::Windows::ApplicationModel::Resources;
    ResourceManager manager{ResourceLoader::GetDefaultResourceFilePath()};
    const auto resources = manager.MainResourceMap().GetSubtree(L"Resources");
    const auto context = manager.CreateResourceContext();
    const auto language = GENERATE(L"en-US", L"en-GB", L"ru");
    context.QualifierValues().Insert(L"Language", language);
    for (const auto *control :
         {L"SourcePicker", L"Analyze", L"StartProcessing", L"CancelOperation", L"ReturnToSourceSelection",
          L"RetryRecovery", L"RestoreRecoverySource", L"ExportDiagnostics", L"TraversalScope", L"OutputDisposition",
          L"EdgeHandling", L"ScanOrganization"})
    {
        INFO("Language=" << winrt::to_string(language) << ", control=" << winrt::to_string(control));
        const auto candidate = resources.GetValue(std::wstring{L"Shell_"} + control + L"/AccessKey", context);
        CHECK(candidate.ValueAsString().size() == 1);
        CHECK(CompareStringOrdinal(candidate.QualifierValues().Lookup(L"Language").c_str(), -1, language, -1, TRUE) ==
              CSTR_EQUAL);
    }
}
