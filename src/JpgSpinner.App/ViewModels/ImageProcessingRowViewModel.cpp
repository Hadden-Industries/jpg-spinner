#include "pch.h"
#include "ViewModels/ImageProcessingRowViewModel.h"
#include "Localization/UnsignedIntegerText.h"
#include "Presentation.ImageProcessingRowViewModel.g.cpp"
#include <filesystem>

namespace winrt::JpgSpinner::Presentation::implementation
{
ImageProcessingRowViewModel::ImageProcessingRowViewModel(
    Microsoft::UI::Dispatching::DispatcherQueue dispatcherQueue,
    const jpg_spinner::batch_processing::BatchFileAnalysis &analysis,
    Microsoft::Windows::ApplicationModel::Resources::ResourceMap resources,
    Microsoft::Windows::ApplicationModel::Resources::ResourceContext resourceContext)
    : dispatcherQueue_(std::move(dispatcherQueue)),
      displayName_(std::filesystem::path{analysis.relativePath}.filename().wstring()),
      isEligibleForProcessing_(analysis.jpegAnalysis && !analysis.error && analysis.sourceRevision &&
                               analysis.jpegAnalysis->transformPlan.transform !=
                                   jpg_spinner::domain::LosslessTransform::None),
      resources_(std::move(resources)), resourceContext_(std::move(resourceContext))
{
    if (!dispatcherQueue_)
        throw hresult_invalid_argument();
    requireDispatcherThread();
    if (analysis.error)
        errorText_ =
            jpg_spinner::presentation::loadImageProcessingErrorText(analysis.error->code, resources_, resourceContext_);
    if (analysis.jpegAnalysis)
    {
        plannedTransform_ = analysis.jpegAnalysis->transformPlan.transform;
        plannedOutputDimensions_ = analysis.jpegAnalysis->transformPlan.outputDimensions;
        auto const &discarded = analysis.jpegAnalysis->transformPlan.discardedSourceEdgePixels;
        discardedSourceRightEdgeWidthInPixels_ = discarded.rightEdgeWidth.pixels;
        discardedSourceBottomEdgeHeightInPixels_ = discarded.bottomEdgeHeight.pixels;
    }
}
void ImageProcessingRowViewModel::requireDispatcherThread() const
{
    if (!dispatcherQueue_.HasThreadAccess())
        throw hresult_wrong_thread();
}
hstring ImageProcessingRowViewModel::DisplayName() const
{
    requireDispatcherThread();
    return displayName_;
}
bool ImageProcessingRowViewModel::IsEligibleForProcessing() const
{
    requireDispatcherThread();
    return isEligibleForProcessing_;
}
std::uint64_t ImageProcessingRowViewModel::DiscardedSourceRightEdgeWidthInPixels() const
{
    requireDispatcherThread();
    return discardedSourceRightEdgeWidthInPixels_;
}
std::uint64_t ImageProcessingRowViewModel::DiscardedSourceBottomEdgeHeightInPixels() const
{
    requireDispatcherThread();
    return discardedSourceBottomEdgeHeightInPixels_;
}
bool ImageProcessingRowViewModel::IsEdgeTrimmingAcknowledged() const
{
    requireDispatcherThread();
    return isEdgeTrimmingAcknowledged_;
}
hstring ImageProcessingRowViewModel::DiscardedSourceRightEdgeWidthInPixelsText() const
{
    return jpg_spinner::presentation::formatUnsignedInteger(DiscardedSourceRightEdgeWidthInPixels());
}
hstring ImageProcessingRowViewModel::DiscardedSourceBottomEdgeHeightInPixelsText() const
{
    return jpg_spinner::presentation::formatUnsignedInteger(DiscardedSourceBottomEdgeHeightInPixels());
}
bool ImageProcessingRowViewModel::RequiresEdgeTrimmingAcknowledgment() const
{
    requireDispatcherThread();
    return HasEdgeTrimming() && !isEdgeTrimmingAcknowledged_;
}
bool ImageProcessingRowViewModel::HasEdgeTrimming() const
{
    requireDispatcherThread();
    return isEligibleForProcessing_ &&
           (discardedSourceRightEdgeWidthInPixels_ != 0 || discardedSourceBottomEdgeHeightInPixels_ != 0);
}
hstring ImageProcessingRowViewModel::EdgeTrimmingAcknowledgmentDescription() const
{
    requireDispatcherThread();
    return resources_
        .GetValue(isEdgeTrimmingAcknowledged_ ? L"ImageReview_TrimmingApproved" : L"ImageReview_ApproveTrimming",
                  resourceContext_)
        .ValueAsString();
}
hstring ImageProcessingRowViewModel::StatusDescription() const
{
    requireDispatcherThread();
    // A plan is not a success claim. Until the batch publishes its actual
    // per-file result, report only the immutable analysis fact.
    if (outcome_ != ImageProcessingRowOutcome::Pending)
        return OutcomeDescription();
    if (HasError() || !isEligibleForProcessing_)
        return resources_
            .GetValue(HasError() ? L"ImageReview_NeedsAttention" : L"ImageReview_NoCorrectionPlanned", resourceContext_)
            .ValueAsString();
    const wchar_t *key{};
    using enum jpg_spinner::domain::LosslessTransform;
    switch (plannedTransform_)
    {
    case FlipHorizontal:
        key = L"ImageReview_FlipHorizontal";
        break;
    case FlipVertical:
        key = L"ImageReview_FlipVertical";
        break;
    case Rotate180:
        key = L"ImageReview_Rotate180";
        break;
    case Rotate90Clockwise:
        key = L"ImageReview_Rotate90Clockwise";
        break;
    case Rotate270Clockwise:
        key = L"ImageReview_Rotate90Counterclockwise";
        break;
    case Transpose:
        key = L"ImageReview_MirrorThenRotate90Counterclockwise";
        break;
    case Transverse:
        key = L"ImageReview_MirrorThenRotate90Clockwise";
        break;
    default:
        throw hresult_invalid_argument();
    }
    // These are exact planner output dimensions, not an image decode or a
    // fabricated preview. Pixel counts use the user's native number formatting.
    return resources_.GetValue(key, resourceContext_).ValueAsString() + L" · " +
           jpg_spinner::presentation::formatUnsignedInteger(plannedOutputDimensions_.width.pixels) + L" × " +
           jpg_spinner::presentation::formatUnsignedInteger(plannedOutputDimensions_.height.pixels) + L" px";
}
void ImageProcessingRowViewModel::acknowledgeEdgeTrimming()
{
    requireDispatcherThread();
    if (!HasEdgeTrimming())
        throw hresult_illegal_method_call();
    if (!isEdgeTrimmingAcknowledged_)
    {
        isEdgeTrimmingAcknowledged_ = true;
        propertyChanged_(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{L"IsEdgeTrimmingAcknowledged"});
        propertyChanged_(*this,
                         Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{L"RequiresEdgeTrimmingAcknowledgment"});
        propertyChanged_(*this,
                         Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{L"EdgeTrimmingAcknowledgmentDescription"});
    }
}
Presentation::ImageProcessingRowOutcome ImageProcessingRowViewModel::Outcome() const
{
    requireDispatcherThread();
    return outcome_;
}
hstring ImageProcessingRowViewModel::ErrorTitle() const
{
    requireDispatcherThread();
    return errorText_.title;
}
bool ImageProcessingRowViewModel::HasError() const
{
    requireDispatcherThread();
    return !errorText_.title.empty();
}
hstring ImageProcessingRowViewModel::OutcomeDescription() const
{
    requireDispatcherThread();
    const wchar_t *key{};
    switch (outcome_)
    {
    case ImageProcessingRowOutcome::Pending:
        key = L"ImageProcessingOutcome_Pending";
        break;
    case ImageProcessingRowOutcome::CorrectedCopyCreated:
        key = L"ImageProcessingOutcome_CorrectedCopyCreated";
        break;
    case ImageProcessingRowOutcome::OriginalReplacedWithVerifiedBackup:
        key = L"ImageProcessingOutcome_OriginalReplacedWithVerifiedBackup";
        break;
    case ImageProcessingRowOutcome::OriginalReplacedWithoutRetainedBackup:
        key = L"ImageProcessingOutcome_OriginalReplacedWithoutRetainedBackup";
        break;
    case ImageProcessingRowOutcome::NoOrientationNormalizationRequired:
        key = L"ImageProcessingOutcome_NoOrientationNormalizationRequired";
        break;
    case ImageProcessingRowOutcome::UnsupportedSourceSkipped:
        key = L"ImageProcessingOutcome_UnsupportedSourceSkipped";
        break;
    case ImageProcessingRowOutcome::ProcessingFailed:
        key = L"ImageProcessingOutcome_ProcessingFailed";
        break;
    case ImageProcessingRowOutcome::CancelledBeforeTransformation:
        key = L"ImageProcessingOutcome_CancelledBeforeTransformation";
        break;
    case ImageProcessingRowOutcome::CancelledBeforeCommit:
        key = L"ImageProcessingOutcome_CancelledBeforeCommit";
        break;
    default:
        throw hresult_invalid_argument();
    }
    return resources_.GetValue(key, resourceContext_).ValueAsString();
}
hstring ImageProcessingRowViewModel::ErrorExplanation() const
{
    requireDispatcherThread();
    return errorText_.explanation;
}
hstring ImageProcessingRowViewModel::ErrorRemedy() const
{
    requireDispatcherThread();
    return errorText_.remedy;
}
void ImageProcessingRowViewModel::applyResult(const jpg_spinner::batch_processing::BatchFileProcessingResult &result)
{
    requireDispatcherThread();
    using NativeOutcome = jpg_spinner::batch_processing::BatchFileOutcome;
    switch (result.outcome)
    {
    case NativeOutcome::CorrectedCopyCreated:
        outcome_ = ImageProcessingRowOutcome::CorrectedCopyCreated;
        break;
    case NativeOutcome::OriginalReplacedWithVerifiedBackup:
        outcome_ = ImageProcessingRowOutcome::OriginalReplacedWithVerifiedBackup;
        break;
    case NativeOutcome::OriginalReplacedWithoutRetainedBackup:
        outcome_ = ImageProcessingRowOutcome::OriginalReplacedWithoutRetainedBackup;
        break;
    case NativeOutcome::NoOrientationNormalizationRequired:
        outcome_ = ImageProcessingRowOutcome::NoOrientationNormalizationRequired;
        break;
    case NativeOutcome::UnsupportedSourceSkipped:
        outcome_ = ImageProcessingRowOutcome::UnsupportedSourceSkipped;
        break;
    case NativeOutcome::ProcessingFailed:
        outcome_ = ImageProcessingRowOutcome::ProcessingFailed;
        break;
    case NativeOutcome::CancelledBeforeTransformation:
        outcome_ = ImageProcessingRowOutcome::CancelledBeforeTransformation;
        break;
    case NativeOutcome::CancelledBeforeCommit:
        outcome_ = ImageProcessingRowOutcome::CancelledBeforeCommit;
        break;
    }
    errorText_ = result.error ? jpg_spinner::presentation::loadImageProcessingErrorText(result.error->code, resources_,
                                                                                        resourceContext_)
                              : jpg_spinner::presentation::ImageProcessingErrorText{};
    // Publish a coherent terminal observation before notifying any binding.
    propertyChanged_(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{L"Outcome"});
    for (auto const *property :
         {L"OutcomeDescription", L"StatusDescription", L"ErrorTitle", L"ErrorExplanation", L"ErrorRemedy", L"HasError"})
        propertyChanged_(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{property});
}
event_token ImageProcessingRowViewModel::PropertyChanged(
    Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const &handler)
{
    requireDispatcherThread();
    return propertyChanged_.add(handler);
}
void ImageProcessingRowViewModel::PropertyChanged(event_token const &token)
{
    requireDispatcherThread();
    propertyChanged_.remove(token);
}
} // namespace winrt::JpgSpinner::Presentation::implementation
