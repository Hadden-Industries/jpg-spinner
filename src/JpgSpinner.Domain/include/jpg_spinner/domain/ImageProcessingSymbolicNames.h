#pragma once

#include "ImageProcessingError.h"
#include <string_view>

namespace jpg_spinner::domain
{
/// Stable, nonlocalized symbols for diagnostics and resource identities. These
/// closed mappings admit no user prose or native exception text. Unknown numeric
/// values remain the fixed Unknown symbol; enum renames require wire review.
[[nodiscard]] constexpr std::wstring_view symbolicName(const ImageProcessingErrorCode value) noexcept
{
    switch (value)
    {
    case ImageProcessingErrorCode::SourceAccessDenied:
        return L"SourceAccessDenied";
    case ImageProcessingErrorCode::SourceSharingViolation:
        return L"SourceSharingViolation";
    case ImageProcessingErrorCode::SourceRevisionCaptureFailed:
        return L"SourceRevisionCaptureFailed";
    case ImageProcessingErrorCode::SourceDiscoveryFailed:
        return L"SourceDiscoveryFailed";
    case ImageProcessingErrorCode::InvalidBatchProcessingRequest:
        return L"InvalidBatchProcessingRequest";
    case ImageProcessingErrorCode::SourceChangedAfterAnalysis:
        return L"SourceChangedAfterAnalysis";
    case ImageProcessingErrorCode::DestinationAccessDenied:
        return L"DestinationAccessDenied";
    case ImageProcessingErrorCode::EncodedFileTooLarge:
        return L"EncodedFileTooLarge";
    case ImageProcessingErrorCode::PixelCountLimitExceeded:
        return L"PixelCountLimitExceeded";
    case ImageProcessingErrorCode::MetadataLengthLimitExceeded:
        return L"MetadataLengthLimitExceeded";
    case ImageProcessingErrorCode::ProgressiveScanCountLimitExceeded:
        return L"ProgressiveScanCountLimitExceeded";
    case ImageProcessingErrorCode::MalformedJpegStructure:
        return L"MalformedJpegStructure";
    case ImageProcessingErrorCode::MalformedImageMetadata:
        return L"MalformedImageMetadata";
    case ImageProcessingErrorCode::MetadataPreservationFailed:
        return L"MetadataPreservationFailed";
    case ImageProcessingErrorCode::InvalidOrientationMetadata:
        return L"InvalidOrientationMetadata";
    case ImageProcessingErrorCode::UnsupportedJpegCodingProcess:
        return L"UnsupportedJpegCodingProcess";
    case ImageProcessingErrorCode::UnsupportedJpegSamplePrecision:
        return L"UnsupportedJpegSamplePrecision";
    case ImageProcessingErrorCode::UnsupportedJpegComponentOrganization:
        return L"UnsupportedJpegComponentOrganization";
    case ImageProcessingErrorCode::UnsupportedJpegDeferredHeight:
        return L"UnsupportedJpegDeferredHeight";
    case ImageProcessingErrorCode::UnsupportedJpegRestartIntervalChanges:
        return L"UnsupportedJpegRestartIntervalChanges";
    case ImageProcessingErrorCode::MultiPictureJpegNotSupported:
        return L"MultiPictureJpegNotSupported";
    case ImageProcessingErrorCode::MotionPhotoNotSupported:
        return L"MotionPhotoNotSupported";
    case ImageProcessingErrorCode::UnsupportedTrailingPayload:
        return L"UnsupportedTrailingPayload";
    case ImageProcessingErrorCode::ContentCredentialsWouldBeInvalidated:
        return L"ContentCredentialsWouldBeInvalidated";
    case ImageProcessingErrorCode::UnsupportedJumbfMetadata:
        return L"UnsupportedJumbfMetadata";
    case ImageProcessingErrorCode::UnsupportedEmbeddedPreviewMetadata:
        return L"UnsupportedEmbeddedPreviewMetadata";
    case ImageProcessingErrorCode::ExtendedXmpMutationNotSupported:
        return L"ExtendedXmpMutationNotSupported";
    case ImageProcessingErrorCode::PerfectCoefficientTransformUnavailable:
        return L"PerfectCoefficientTransformUnavailable";
    case ImageProcessingErrorCode::CoefficientTransformationFailed:
        return L"CoefficientTransformationFailed";
    case ImageProcessingErrorCode::OutputBufferTooSmall:
        return L"OutputBufferTooSmall";
    case ImageProcessingErrorCode::WorkingMemoryAllocationFailed:
        return L"WorkingMemoryAllocationFailed";
    case ImageProcessingErrorCode::InsufficientStorageSpace:
        return L"InsufficientStorageSpace";
    case ImageProcessingErrorCode::OutputRelativePathCollision:
        return L"OutputRelativePathCollision";
    case ImageProcessingErrorCode::StagingFileCreationFailed:
        return L"StagingFileCreationFailed";
    case ImageProcessingErrorCode::StagingWriteFailed:
        return L"StagingWriteFailed";
    case ImageProcessingErrorCode::StagingFlushFailed:
        return L"StagingFlushFailed";
    case ImageProcessingErrorCode::StagingCloseFailed:
        return L"StagingCloseFailed";
    case ImageProcessingErrorCode::StagedOutputVerificationFailed:
        return L"StagedOutputVerificationFailed";
    case ImageProcessingErrorCode::StagedOutputHashMismatch:
        return L"StagedOutputHashMismatch";
    case ImageProcessingErrorCode::OutputValidationFailed:
        return L"OutputValidationFailed";
    case ImageProcessingErrorCode::CorrectedCopyDestinationCreationFailed:
        return L"CorrectedCopyDestinationCreationFailed";
    case ImageProcessingErrorCode::CorrectedCopyCommitFailed:
        return L"CorrectedCopyCommitFailed";
    case ImageProcessingErrorCode::BackupCreationFailed:
        return L"BackupCreationFailed";
    case ImageProcessingErrorCode::BackupVerificationFailed:
        return L"BackupVerificationFailed";
    case ImageProcessingErrorCode::OriginalReplacementFailed:
        return L"OriginalReplacementFailed";
    case ImageProcessingErrorCode::JournalPersistenceFailed:
        return L"JournalPersistenceFailed";
    case ImageProcessingErrorCode::StorageProviderRecoveryContractNotEstablished:
        return L"StorageProviderRecoveryContractNotEstablished";
    case ImageProcessingErrorCode::JournalStoreBusy:
        return L"JournalStoreBusy";
    case ImageProcessingErrorCode::RecoveryConflict:
        return L"RecoveryConflict";
    case ImageProcessingErrorCode::WindowsLongPathSupportUnavailable:
        return L"WindowsLongPathSupportUnavailable";
    case ImageProcessingErrorCode::Cancelled:
        return L"Cancelled";
    default:
        return L"Unknown";
    }
}

[[nodiscard]] constexpr std::wstring_view symbolicName(const ImageProcessingStage value) noexcept
{
    switch (value)
    {
    case ImageProcessingStage::CandidateDiscovery:
        return L"CandidateDiscovery";
    case ImageProcessingStage::SourceRevisionCapture:
        return L"SourceRevisionCapture";
    case ImageProcessingStage::JpegStructureValidation:
        return L"JpegStructureValidation";
    case ImageProcessingStage::MetadataAnalysis:
        return L"MetadataAnalysis";
    case ImageProcessingStage::MetadataReconciliation:
        return L"MetadataReconciliation";
    case ImageProcessingStage::TransformPlanning:
        return L"TransformPlanning";
    case ImageProcessingStage::CoefficientTransformation:
        return L"CoefficientTransformation";
    case ImageProcessingStage::OutputValidation:
        return L"OutputValidation";
    case ImageProcessingStage::StagingFileCreation:
        return L"StagingFileCreation";
    case ImageProcessingStage::StagingWrite:
        return L"StagingWrite";
    case ImageProcessingStage::StagingFlush:
        return L"StagingFlush";
    case ImageProcessingStage::StagingClose:
        return L"StagingClose";
    case ImageProcessingStage::StagedOutputVerification:
        return L"StagedOutputVerification";
    case ImageProcessingStage::SourceRevisionRevalidation:
        return L"SourceRevisionRevalidation";
    case ImageProcessingStage::CorrectedCopyDestinationCreation:
        return L"CorrectedCopyDestinationCreation";
    case ImageProcessingStage::CorrectedCopyCommit:
        return L"CorrectedCopyCommit";
    case ImageProcessingStage::BackupCreation:
        return L"BackupCreation";
    case ImageProcessingStage::BackupVerification:
        return L"BackupVerification";
    case ImageProcessingStage::OriginalReplacement:
        return L"OriginalReplacement";
    case ImageProcessingStage::JournalPersistence:
        return L"JournalPersistence";
    case ImageProcessingStage::TransactionRecovery:
        return L"TransactionRecovery";
    case ImageProcessingStage::TransactionRecoverabilityPreflight:
        return L"TransactionRecoverabilityPreflight";
    case ImageProcessingStage::JournalStoreLeaseAcquisition:
        return L"JournalStoreLeaseAcquisition";
    default:
        return L"Unknown";
    }
}

[[nodiscard]] constexpr std::wstring_view symbolicName(const JpegResourceLimit value) noexcept
{
    switch (value)
    {
    case JpegResourceLimit::EncodedFileLengthBytes:
        return L"EncodedFileLengthBytes";
    case JpegResourceLimit::PixelCount:
        return L"PixelCount";
    case JpegResourceLimit::MetadataLengthBytes:
        return L"MetadataLengthBytes";
    case JpegResourceLimit::ProgressiveScanCount:
        return L"ProgressiveScanCount";
    default:
        return L"Unknown";
    }
}

} // namespace jpg_spinner::domain
