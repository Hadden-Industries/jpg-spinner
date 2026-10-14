#include "pch.h"
#include "ViewModels/ImageSourceDiscoveryIssueViewModel.h"
#include "Presentation.ImageSourceDiscoveryIssueViewModel.g.cpp"

namespace winrt::JpgSpinner::Presentation::implementation
{
ImageSourceDiscoveryIssueViewModel::ImageSourceDiscoveryIssueViewModel(
    Microsoft::UI::Dispatching::DispatcherQueue dispatcherQueue,
    jpg_spinner::domain::ImageSourceDiscoveryIssue const &issue,
    Microsoft::Windows::ApplicationModel::Resources::ResourceMap const &resources,
    Microsoft::Windows::ApplicationModel::Resources::ResourceContext const &context)
    : dispatcherQueue_(std::move(dispatcherQueue)), relativePath_(issue.relativePath)
{
    if (!dispatcherQueue_)
        throw hresult_invalid_argument();
    requireDispatcherThread();
    // Explicit semantic mapping must not assume the two enums share ordinals.
    using NativeKind = jpg_spinner::domain::ImageSourceDiscoveryIssueKind;
    const wchar_t *descriptionKey{};
    switch (issue.kind)
    {
    case NativeKind::ReparsePointSkipped:
        kind_ = ImageSourceDiscoveryIssueKind::ReparsePointSkipped;
        descriptionKey = L"ImageSourceDiscoveryObservation_ReparsePointSkipped";
        break;
    case NativeKind::SourceAccessFailed:
        kind_ = ImageSourceDiscoveryIssueKind::SourceAccessFailed;
        descriptionKey = L"ImageSourceDiscoveryObservation_SourceAccessFailed";
        break;
    case NativeKind::ApplicationOwnedFolderExcluded:
        kind_ = ImageSourceDiscoveryIssueKind::ApplicationOwnedFolderExcluded;
        descriptionKey = L"ImageSourceDiscoveryObservation_ApplicationOwnedFolderExcluded";
        break;
    default:
        throw hresult_invalid_argument();
    }
    // MRT owns language selection and fallback; no synthesized error is needed
    // to explain a deliberately excluded directory or skipped reparse point.
    kindDescription_ = resources.GetValue(descriptionKey, context).ValueAsString();
    if (issue.error)
        errorText_ = jpg_spinner::presentation::loadImageProcessingErrorText(issue.error->code, resources, context);
}
void ImageSourceDiscoveryIssueViewModel::requireDispatcherThread() const
{
    if (!dispatcherQueue_.HasThreadAccess())
        throw hresult_wrong_thread();
}
hstring ImageSourceDiscoveryIssueViewModel::RelativePath() const
{
    requireDispatcherThread();
    return relativePath_;
}
Presentation::ImageSourceDiscoveryIssueKind ImageSourceDiscoveryIssueViewModel::Kind() const
{
    requireDispatcherThread();
    return kind_;
}
hstring ImageSourceDiscoveryIssueViewModel::ErrorTitle() const
{
    requireDispatcherThread();
    return errorText_.title;
}
hstring ImageSourceDiscoveryIssueViewModel::KindDescription() const
{
    requireDispatcherThread();
    return kindDescription_;
}
hstring ImageSourceDiscoveryIssueViewModel::ErrorExplanation() const
{
    requireDispatcherThread();
    return errorText_.explanation;
}
hstring ImageSourceDiscoveryIssueViewModel::ErrorRemedy() const
{
    requireDispatcherThread();
    return errorText_.remedy;
}
} // namespace winrt::JpgSpinner::Presentation::implementation
