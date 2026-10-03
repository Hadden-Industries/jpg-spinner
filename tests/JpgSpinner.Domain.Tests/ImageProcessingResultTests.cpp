#include "DomainStringMakers.h"

#include <jpg_spinner/domain/ImageProcessingResult.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

namespace
{
using jpg_spinner::domain::ImageProcessingError;
using jpg_spinner::domain::ImageProcessingErrorCode;
using jpg_spinner::domain::ImageProcessingResult;
using jpg_spinner::domain::ImageProcessingStage;

struct MoveOnlyValue final
{
    explicit MoveOnlyValue(const int value) : storedValue(std::make_unique<int>(value))
    {
    }

    MoveOnlyValue(const MoveOnlyValue &) = delete;
    MoveOnlyValue &operator=(const MoveOnlyValue &) = delete;
    MoveOnlyValue(MoveOnlyValue &&) noexcept = default;
    MoveOnlyValue &operator=(MoveOnlyValue &&) noexcept = default;

    std::unique_ptr<int> storedValue;
};

ImageProcessingError makePlanningError(const ImageProcessingErrorCode code)
{
    return ImageProcessingError{code, ImageProcessingStage::TransformPlanning};
}
} // namespace

TEST_CASE("a successful image-processing result exposes only its value", "[domain][result]")
{
    auto result = ImageProcessingResult<int>::success(42);

    REQUIRE(result.valueIfPresent() != nullptr);
    REQUIRE(*result.valueIfPresent() == 42);
    REQUIRE(result.errorIfPresent() == nullptr);

    const auto &constResult = result;
    REQUIRE(constResult.valueIfPresent() != nullptr);
    REQUIRE(*constResult.valueIfPresent() == 42);
    REQUIRE(constResult.errorIfPresent() == nullptr);
}

TEST_CASE("a failed image-processing result exposes only its structured error", "[domain][result]")
{
    auto result = ImageProcessingResult<int>::failure(
        makePlanningError(ImageProcessingErrorCode::PerfectCoefficientTransformUnavailable));

    REQUIRE(result.valueIfPresent() == nullptr);
    REQUIRE(result.errorIfPresent() != nullptr);
    REQUIRE(result.errorIfPresent()->code == ImageProcessingErrorCode::PerfectCoefficientTransformUnavailable);
    REQUIRE(result.errorIfPresent()->stage == ImageProcessingStage::TransformPlanning);
}

TEST_CASE("moving a result preserves an active alternative in both live objects", "[domain][result]")
{
    using Result = ImageProcessingResult<MoveOnlyValue>;

    STATIC_REQUIRE_FALSE(std::is_default_constructible_v<Result>);
    STATIC_REQUIRE_FALSE(std::is_copy_constructible_v<Result>);
    STATIC_REQUIRE(std::is_move_constructible_v<Result>);
    STATIC_REQUIRE_FALSE(std::is_move_assignable_v<Result>);

    auto source = Result::success(MoveOnlyValue{73});
    auto destination = std::move(source);

    REQUIRE(destination.valueIfPresent() != nullptr);
    REQUIRE(destination.valueIfPresent()->storedValue != nullptr);
    REQUIRE(*destination.valueIfPresent()->storedValue == 73);
    REQUIRE(destination.errorIfPresent() == nullptr);

    // A moved-from value may have moved-from contents, but the result object
    // itself must still identify the value alternative rather than becoming
    // an uninspectable third state.
    REQUIRE(source.valueIfPresent() != nullptr);
    REQUIRE(source.errorIfPresent() == nullptr);
}

TEST_CASE("every image-processing error code has a semantic diagnostic name", "[domain][result]")
{
    struct NamedErrorCode final
    {
        ImageProcessingErrorCode code;
        std::string_view expectedName;
    };

    constexpr std::array cases{
        NamedErrorCode{ImageProcessingErrorCode::SourceAccessDenied, "SourceAccessDenied"},
        NamedErrorCode{ImageProcessingErrorCode::SourceSharingViolation, "SourceSharingViolation"},
        NamedErrorCode{ImageProcessingErrorCode::SourceRevisionCaptureFailed, "SourceRevisionCaptureFailed"},
        NamedErrorCode{ImageProcessingErrorCode::SourceChangedAfterAnalysis, "SourceChangedAfterAnalysis"},
        NamedErrorCode{ImageProcessingErrorCode::DestinationAccessDenied, "DestinationAccessDenied"},
        NamedErrorCode{ImageProcessingErrorCode::EncodedFileTooLarge, "EncodedFileTooLarge"},
        NamedErrorCode{ImageProcessingErrorCode::PixelCountLimitExceeded, "PixelCountLimitExceeded"},
        NamedErrorCode{ImageProcessingErrorCode::MetadataLengthLimitExceeded, "MetadataLengthLimitExceeded"},
        NamedErrorCode{ImageProcessingErrorCode::ProgressiveScanCountLimitExceeded,
                       "ProgressiveScanCountLimitExceeded"},
        NamedErrorCode{ImageProcessingErrorCode::MalformedJpegStructure, "MalformedJpegStructure"},
        NamedErrorCode{ImageProcessingErrorCode::MalformedImageMetadata, "MalformedImageMetadata"},
        NamedErrorCode{ImageProcessingErrorCode::MetadataPreservationFailed, "MetadataPreservationFailed"},
        NamedErrorCode{ImageProcessingErrorCode::InvalidOrientationMetadata, "InvalidOrientationMetadata"},
        NamedErrorCode{ImageProcessingErrorCode::UnsupportedJpegCodingProcess, "UnsupportedJpegCodingProcess"},
        NamedErrorCode{ImageProcessingErrorCode::UnsupportedJpegSamplePrecision, "UnsupportedJpegSamplePrecision"},
        NamedErrorCode{ImageProcessingErrorCode::UnsupportedJpegComponentOrganization,
                       "UnsupportedJpegComponentOrganization"},
        NamedErrorCode{ImageProcessingErrorCode::MultiPictureJpegNotSupported, "MultiPictureJpegNotSupported"},
        NamedErrorCode{ImageProcessingErrorCode::MotionPhotoNotSupported, "MotionPhotoNotSupported"},
        NamedErrorCode{ImageProcessingErrorCode::UnsupportedTrailingPayload, "UnsupportedTrailingPayload"},
        NamedErrorCode{ImageProcessingErrorCode::ContentCredentialsWouldBeInvalidated,
                       "ContentCredentialsWouldBeInvalidated"},
        NamedErrorCode{ImageProcessingErrorCode::UnsupportedJumbfMetadata, "UnsupportedJumbfMetadata"},
        NamedErrorCode{ImageProcessingErrorCode::UnsupportedEmbeddedPreviewMetadata,
                       "UnsupportedEmbeddedPreviewMetadata"},
        NamedErrorCode{ImageProcessingErrorCode::ExtendedXmpMutationNotSupported, "ExtendedXmpMutationNotSupported"},
        NamedErrorCode{ImageProcessingErrorCode::PerfectCoefficientTransformUnavailable,
                       "PerfectCoefficientTransformUnavailable"},
        NamedErrorCode{ImageProcessingErrorCode::InsufficientStorageSpace, "InsufficientStorageSpace"},
        NamedErrorCode{ImageProcessingErrorCode::OutputRelativePathCollision, "OutputRelativePathCollision"},
        NamedErrorCode{ImageProcessingErrorCode::StagingFileCreationFailed, "StagingFileCreationFailed"},
        NamedErrorCode{ImageProcessingErrorCode::StagingWriteFailed, "StagingWriteFailed"},
        NamedErrorCode{ImageProcessingErrorCode::StagingFlushFailed, "StagingFlushFailed"},
        NamedErrorCode{ImageProcessingErrorCode::StagedOutputHashMismatch, "StagedOutputHashMismatch"},
        NamedErrorCode{ImageProcessingErrorCode::OutputValidationFailed, "OutputValidationFailed"},
        NamedErrorCode{ImageProcessingErrorCode::CorrectedCopyCommitFailed, "CorrectedCopyCommitFailed"},
        NamedErrorCode{ImageProcessingErrorCode::BackupCreationFailed, "BackupCreationFailed"},
        NamedErrorCode{ImageProcessingErrorCode::BackupVerificationFailed, "BackupVerificationFailed"},
        NamedErrorCode{ImageProcessingErrorCode::OriginalReplacementFailed, "OriginalReplacementFailed"},
        NamedErrorCode{ImageProcessingErrorCode::JournalPersistenceFailed, "JournalPersistenceFailed"},
        NamedErrorCode{ImageProcessingErrorCode::RecoveryConflict, "RecoveryConflict"},
        NamedErrorCode{ImageProcessingErrorCode::Cancelled, "Cancelled"},
    };

    for (const auto &testCase : cases)
    {
        CAPTURE(testCase.expectedName);
        REQUIRE(Catch::StringMaker<ImageProcessingErrorCode>::convert(testCase.code) == testCase.expectedName);
    }
}
