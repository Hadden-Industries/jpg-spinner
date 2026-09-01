#pragma once

#include "JpegResourceLimits.h"

#include <cstdint>
#include <system_error>
#include <variant>

namespace jpg_spinner::domain
{
/// Stable, presentation-independent failure identity. UI layers translate
/// these values to localized guidance and never display native messages.
enum class ImageProcessingErrorCode
{
    SourceAccessDenied,
    SourceChangedAfterAnalysis,
    DestinationAccessDenied,
    EncodedFileTooLarge,
    PixelCountLimitExceeded,
    MetadataLengthLimitExceeded,
    ProgressiveScanCountLimitExceeded,
    MalformedJpegStructure,
    MalformedImageMetadata,
    InvalidOrientationMetadata,
    UnsupportedJpegCodingProcess,
    UnsupportedJpegSamplePrecision,
    UnsupportedJpegComponentOrganization,
    MultiPictureJpegNotSupported,
    MotionPhotoNotSupported,
    UnsupportedTrailingPayload,
    ContentCredentialsWouldBeInvalidated,
    UnsupportedJumbfMetadata,
    UnsupportedEmbeddedPreviewMetadata,
    ExtendedXmpMutationNotSupported,
    PerfectCoefficientTransformUnavailable,
    InsufficientStorageSpace,
    OutputRelativePathCollision,
    StagingFileCreationFailed,
    StagingWriteFailed,
    StagingFlushFailed,
    StagedOutputHashMismatch,
    OutputValidationFailed,
    CorrectedCopyCommitFailed,
    BackupCreationFailed,
    BackupVerificationFailed,
    OriginalReplacementFailed,
    JournalPersistenceFailed,
    RecoveryConflict,
    Cancelled,
};

/// Identifies the operation that observed a failure. Stages describe
/// domain work rather than call-stack locations, so logs remain stable as
/// implementation details evolve.
enum class ImageProcessingStage
{
    CandidateDiscovery,
    SourceRevisionCapture,
    JpegStructureValidation,
    MetadataAnalysis,
    TransformPlanning,
    CoefficientTransformation,
    OutputValidation,
    StagingFileCreation,
    StagingWrite,
    StagingFlush,
    StagedOutputVerification,
    SourceRevisionRevalidation,
    CorrectedCopyCommit,
    BackupCreation,
    BackupVerification,
    OriginalReplacement,
    JournalPersistence,
    TransactionRecovery,
};

/// HRESULT is signed 32-bit state. A strong type prevents it from being
/// confused with a portable std::error_code or an arbitrary integer.
struct WindowsHResult final
{
    const std::int32_t signedValue;
};

using NativeErrorProjection = std::variant<std::monostate, std::error_code, WindowsHResult>;
using ImageProcessingDiagnosticContext = std::variant<std::monostate, JpegResourceLimitViolation>;

struct ImageProcessingError final
{
    const ImageProcessingErrorCode code;
    const ImageProcessingStage stage;
    const NativeErrorProjection nativeErrorProjection{};
    const ImageProcessingDiagnosticContext diagnosticContext{};
};
} // namespace jpg_spinner::domain
