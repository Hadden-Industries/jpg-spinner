#include "pch.h"
#include "Localization/ImageProcessingErrorText.h"

namespace jpg_spinner::presentation
{
namespace
{
const wchar_t *resourceStem(domain::ImageProcessingErrorCode code) noexcept
{
    using enum domain::ImageProcessingErrorCode;
    switch (code)
    {
    case SourceAccessDenied:
        return L"ImageProcessingError_SourceAccessDenied";
    case SourceSharingViolation:
        return L"ImageProcessingError_SourceSharingViolation";
    case SourceRevisionCaptureFailed:
        return L"ImageProcessingError_SourceRevisionCaptureFailed";
    case SourceDiscoveryFailed:
        return L"ImageProcessingError_SourceDiscoveryFailed";
    case InvalidBatchProcessingRequest:
        return L"ImageProcessingError_InvalidBatchProcessingRequest";
    case SourceChangedAfterAnalysis:
        return L"ImageProcessingError_SourceChangedAfterAnalysis";
    case DestinationAccessDenied:
        return L"ImageProcessingError_DestinationAccessDenied";
    case EncodedFileTooLarge:
        return L"ImageProcessingError_EncodedFileTooLarge";
    case PixelCountLimitExceeded:
        return L"ImageProcessingError_PixelCountLimitExceeded";
    case MetadataLengthLimitExceeded:
        return L"ImageProcessingError_MetadataLengthLimitExceeded";
    case ProgressiveScanCountLimitExceeded:
        return L"ImageProcessingError_ProgressiveScanCountLimitExceeded";
    case MalformedJpegStructure:
        return L"ImageProcessingError_MalformedJpegStructure";
    case MalformedImageMetadata:
        return L"ImageProcessingError_MalformedImageMetadata";
    case MetadataPreservationFailed:
        return L"ImageProcessingError_MetadataPreservationFailed";
    case InvalidOrientationMetadata:
        return L"ImageProcessingError_InvalidOrientationMetadata";
    case UnsupportedJpegCodingProcess:
        return L"ImageProcessingError_UnsupportedJpegCodingProcess";
    case UnsupportedJpegSamplePrecision:
        return L"ImageProcessingError_UnsupportedJpegSamplePrecision";
    case UnsupportedJpegComponentOrganization:
        return L"ImageProcessingError_UnsupportedJpegComponentOrganization";
    case UnsupportedJpegDeferredHeight:
        return L"ImageProcessingError_UnsupportedJpegDeferredHeight";
    case UnsupportedJpegRestartIntervalChanges:
        return L"ImageProcessingError_UnsupportedJpegRestartIntervalChanges";
    case MultiPictureJpegNotSupported:
        return L"ImageProcessingError_MultiPictureJpegNotSupported";
    case MotionPhotoNotSupported:
        return L"ImageProcessingError_MotionPhotoNotSupported";
    case UnsupportedTrailingPayload:
        return L"ImageProcessingError_UnsupportedTrailingPayload";
    case ContentCredentialsWouldBeInvalidated:
        return L"ImageProcessingError_ContentCredentialsWouldBeInvalidated";
    case UnsupportedJumbfMetadata:
        return L"ImageProcessingError_UnsupportedJumbfMetadata";
    case UnsupportedEmbeddedPreviewMetadata:
        return L"ImageProcessingError_UnsupportedEmbeddedPreviewMetadata";
    case ExtendedXmpMutationNotSupported:
        return L"ImageProcessingError_ExtendedXmpMutationNotSupported";
    case PerfectCoefficientTransformUnavailable:
        return L"ImageProcessingError_PerfectCoefficientTransformUnavailable";
    case CoefficientTransformationFailed:
        return L"ImageProcessingError_CoefficientTransformationFailed";
    case OutputBufferTooSmall:
        return L"ImageProcessingError_OutputBufferTooSmall";
    case WorkingMemoryAllocationFailed:
        return L"ImageProcessingError_WorkingMemoryAllocationFailed";
    case InsufficientStorageSpace:
        return L"ImageProcessingError_InsufficientStorageSpace";
    case OutputRelativePathCollision:
        return L"ImageProcessingError_OutputRelativePathCollision";
    case StagingFileCreationFailed:
        return L"ImageProcessingError_StagingFileCreationFailed";
    case StagingWriteFailed:
        return L"ImageProcessingError_StagingWriteFailed";
    case StagingFlushFailed:
        return L"ImageProcessingError_StagingFlushFailed";
    case StagingCloseFailed:
        return L"ImageProcessingError_StagingCloseFailed";
    case StagedOutputVerificationFailed:
        return L"ImageProcessingError_StagedOutputVerificationFailed";
    case StagedOutputHashMismatch:
        return L"ImageProcessingError_StagedOutputHashMismatch";
    case OutputValidationFailed:
        return L"ImageProcessingError_OutputValidationFailed";
    case CorrectedCopyDestinationCreationFailed:
        return L"ImageProcessingError_CorrectedCopyDestinationCreationFailed";
    case CorrectedCopyCommitFailed:
        return L"ImageProcessingError_CorrectedCopyCommitFailed";
    case BackupCreationFailed:
        return L"ImageProcessingError_BackupCreationFailed";
    case BackupVerificationFailed:
        return L"ImageProcessingError_BackupVerificationFailed";
    case OriginalReplacementFailed:
        return L"ImageProcessingError_OriginalReplacementFailed";
    case JournalPersistenceFailed:
        return L"ImageProcessingError_JournalPersistenceFailed";
    case StorageProviderRecoveryContractNotEstablished:
        return L"ImageProcessingError_StorageProviderRecoveryContractNotEstablished";
    case JournalStoreBusy:
        return L"ImageProcessingError_JournalStoreBusy";
    case RecoveryConflict:
        return L"ImageProcessingError_RecoveryConflict";
    case Cancelled:
        return L"ImageProcessingError_Cancelled";
    default:
        return L"ImageProcessingError_Unknown";
    }
}
} // namespace
namespace
{
ImageProcessingErrorText loadResourceTriplet(
    const std::wstring &stem, const winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceMap &resources,
    const winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceContext &context)
{
    // No caller-supplied diagnostic text or path enters a resource identity.
    const auto title = resources.GetValue(stem + L"_Title", context);
    const auto explanation = resources.GetValue(stem + L"_Explanation", context);
    const auto remedy = resources.GetValue(stem + L"_Remedy", context);
    return {title.ValueAsString(),
            explanation.ValueAsString(),
            remedy.ValueAsString(),
            {title.QualifierValues().Lookup(L"Language"), explanation.QualifierValues().Lookup(L"Language"),
             remedy.QualifierValues().Lookup(L"Language")}};
}
} // namespace
ImageProcessingErrorText loadImageProcessingErrorText(
    domain::ImageProcessingErrorCode code,
    const winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceMap &resources,
    const winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceContext &context)
{
    return loadResourceTriplet(resourceStem(code), resources, context);
}
ImageProcessingErrorText loadGenericImageProcessingErrorText(
    const winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceMap &resources,
    const winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceContext &context)
{
    return loadResourceTriplet(L"ImageProcessingError_Unknown", resources, context);
}
} // namespace jpg_spinner::presentation
