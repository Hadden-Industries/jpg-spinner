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
                                ErrorCode::Cancelled};
static_assert(errorCodes.size() == 50);
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
