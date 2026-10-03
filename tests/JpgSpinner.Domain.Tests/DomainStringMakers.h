#pragma once

#include <jpg_spinner/domain/EdgeHandlingPolicy.h>
#include <jpg_spinner/domain/ExifOrientation.h>
#include <jpg_spinner/domain/ImageProcessingError.h>
#include <jpg_spinner/domain/JpegAnalysisFinding.h>
#include <jpg_spinner/domain/LosslessTransform.h>
#include <jpg_spinner/domain/OutputScanOrganization.h>

#include <catch2/catch_tostring.hpp>

#include <string>

namespace Catch
{
// Catch2 uses these test-only projections in generated-section names and
// failed equality diagnostics. Returning the semantic enumerator names is
// deliberate: an integer would hide which orientation rule regressed.
template <> struct StringMaker<jpg_spinner::domain::ExifOrientation> final
{
    static std::string convert(jpg_spinner::domain::ExifOrientation orientation)
    {
        using jpg_spinner::domain::ExifOrientation;

        switch (orientation)
        {
        case ExifOrientation::TopLeft:
            return "TopLeft";
        case ExifOrientation::TopRight:
            return "TopRight";
        case ExifOrientation::BottomRight:
            return "BottomRight";
        case ExifOrientation::BottomLeft:
            return "BottomLeft";
        case ExifOrientation::LeftTop:
            return "LeftTop";
        case ExifOrientation::RightTop:
            return "RightTop";
        case ExifOrientation::RightBottom:
            return "RightBottom";
        case ExifOrientation::LeftBottom:
            return "LeftBottom";
        }

        return "InvalidExifOrientation";
    }
};

template <> struct StringMaker<jpg_spinner::domain::LosslessTransform> final
{
    static std::string convert(jpg_spinner::domain::LosslessTransform transform)
    {
        using jpg_spinner::domain::LosslessTransform;

        switch (transform)
        {
        case LosslessTransform::None:
            return "None";
        case LosslessTransform::FlipHorizontal:
            return "FlipHorizontal";
        case LosslessTransform::Rotate180:
            return "Rotate180";
        case LosslessTransform::FlipVertical:
            return "FlipVertical";
        case LosslessTransform::Transpose:
            return "Transpose";
        case LosslessTransform::Rotate90Clockwise:
            return "Rotate90Clockwise";
        case LosslessTransform::Transverse:
            return "Transverse";
        case LosslessTransform::Rotate270Clockwise:
            return "Rotate270Clockwise";
        }

        return "InvalidLosslessTransform";
    }
};

template <> struct StringMaker<jpg_spinner::domain::EdgeHandlingPolicy> final
{
    static std::string convert(jpg_spinner::domain::EdgeHandlingPolicy policy)
    {
        using jpg_spinner::domain::EdgeHandlingPolicy;

        switch (policy)
        {
        case EdgeHandlingPolicy::RequirePerfectCoefficientTransform:
            return "RequirePerfectCoefficientTransform";
        case EdgeHandlingPolicy::TrimPartialMinimumCodedUnits:
            return "TrimPartialMinimumCodedUnits";
        }

        return "InvalidEdgeHandlingPolicy";
    }
};

template <> struct StringMaker<jpg_spinner::domain::OutputScanOrganization> final
{
    static std::string convert(jpg_spinner::domain::OutputScanOrganization organization)
    {
        using jpg_spinner::domain::OutputScanOrganization;

        switch (organization)
        {
        case OutputScanOrganization::PreserveSource:
            return "PreserveSource";
        case OutputScanOrganization::SequentialDct:
            return "SequentialDct";
        case OutputScanOrganization::ProgressiveDct:
            return "ProgressiveDct";
        }

        return "InvalidOutputScanOrganization";
    }
};

template <> struct StringMaker<jpg_spinner::domain::ImageProcessingErrorCode> final
{
    static std::string convert(jpg_spinner::domain::ImageProcessingErrorCode code)
    {
        using jpg_spinner::domain::ImageProcessingErrorCode;

        switch (code)
        {
        case ImageProcessingErrorCode::SourceAccessDenied:
            return "SourceAccessDenied";
        case ImageProcessingErrorCode::SourceChangedAfterAnalysis:
            return "SourceChangedAfterAnalysis";
        case ImageProcessingErrorCode::DestinationAccessDenied:
            return "DestinationAccessDenied";
        case ImageProcessingErrorCode::EncodedFileTooLarge:
            return "EncodedFileTooLarge";
        case ImageProcessingErrorCode::PixelCountLimitExceeded:
            return "PixelCountLimitExceeded";
        case ImageProcessingErrorCode::MetadataLengthLimitExceeded:
            return "MetadataLengthLimitExceeded";
        case ImageProcessingErrorCode::ProgressiveScanCountLimitExceeded:
            return "ProgressiveScanCountLimitExceeded";
        case ImageProcessingErrorCode::MalformedJpegStructure:
            return "MalformedJpegStructure";
        case ImageProcessingErrorCode::MalformedImageMetadata:
            return "MalformedImageMetadata";
        case ImageProcessingErrorCode::MetadataPreservationFailed:
            return "MetadataPreservationFailed";
        case ImageProcessingErrorCode::InvalidOrientationMetadata:
            return "InvalidOrientationMetadata";
        case ImageProcessingErrorCode::UnsupportedJpegCodingProcess:
            return "UnsupportedJpegCodingProcess";
        case ImageProcessingErrorCode::UnsupportedJpegSamplePrecision:
            return "UnsupportedJpegSamplePrecision";
        case ImageProcessingErrorCode::UnsupportedJpegComponentOrganization:
            return "UnsupportedJpegComponentOrganization";
        case ImageProcessingErrorCode::MultiPictureJpegNotSupported:
            return "MultiPictureJpegNotSupported";
        case ImageProcessingErrorCode::MotionPhotoNotSupported:
            return "MotionPhotoNotSupported";
        case ImageProcessingErrorCode::UnsupportedTrailingPayload:
            return "UnsupportedTrailingPayload";
        case ImageProcessingErrorCode::ContentCredentialsWouldBeInvalidated:
            return "ContentCredentialsWouldBeInvalidated";
        case ImageProcessingErrorCode::UnsupportedJumbfMetadata:
            return "UnsupportedJumbfMetadata";
        case ImageProcessingErrorCode::UnsupportedEmbeddedPreviewMetadata:
            return "UnsupportedEmbeddedPreviewMetadata";
        case ImageProcessingErrorCode::ExtendedXmpMutationNotSupported:
            return "ExtendedXmpMutationNotSupported";
        case ImageProcessingErrorCode::PerfectCoefficientTransformUnavailable:
            return "PerfectCoefficientTransformUnavailable";
        case ImageProcessingErrorCode::InsufficientStorageSpace:
            return "InsufficientStorageSpace";
        case ImageProcessingErrorCode::OutputRelativePathCollision:
            return "OutputRelativePathCollision";
        case ImageProcessingErrorCode::StagingFileCreationFailed:
            return "StagingFileCreationFailed";
        case ImageProcessingErrorCode::StagingWriteFailed:
            return "StagingWriteFailed";
        case ImageProcessingErrorCode::StagingFlushFailed:
            return "StagingFlushFailed";
        case ImageProcessingErrorCode::StagedOutputHashMismatch:
            return "StagedOutputHashMismatch";
        case ImageProcessingErrorCode::OutputValidationFailed:
            return "OutputValidationFailed";
        case ImageProcessingErrorCode::CorrectedCopyCommitFailed:
            return "CorrectedCopyCommitFailed";
        case ImageProcessingErrorCode::BackupCreationFailed:
            return "BackupCreationFailed";
        case ImageProcessingErrorCode::BackupVerificationFailed:
            return "BackupVerificationFailed";
        case ImageProcessingErrorCode::OriginalReplacementFailed:
            return "OriginalReplacementFailed";
        case ImageProcessingErrorCode::JournalPersistenceFailed:
            return "JournalPersistenceFailed";
        case ImageProcessingErrorCode::RecoveryConflict:
            return "RecoveryConflict";
        case ImageProcessingErrorCode::Cancelled:
            return "Cancelled";
        }

        return "InvalidImageProcessingErrorCode";
    }
};

template <> struct StringMaker<jpg_spinner::domain::JpegAnalysisFindingCode> final
{
    static std::string convert(jpg_spinner::domain::JpegAnalysisFindingCode code)
    {
        using jpg_spinner::domain::JpegAnalysisFindingCode;

        switch (code)
        {
        case JpegAnalysisFindingCode::ExifXmpOrientationConflict:
            return "ExifXmpOrientationConflict";
        case JpegAnalysisFindingCode::EmbeddedThumbnailRemovalRequired:
            return "EmbeddedThumbnailRemovalRequired";
        case JpegAnalysisFindingCode::PartialMinimumCodedUnitTrimRequired:
            return "PartialMinimumCodedUnitTrimRequired";
        }

        return "InvalidJpegAnalysisFindingCode";
    }
};
} // namespace Catch
