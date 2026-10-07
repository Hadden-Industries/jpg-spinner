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
    switch (issue.kind)
    {
    case NativeKind::ReparsePointSkipped:
        kind_ = ImageSourceDiscoveryIssueKind::ReparsePointSkipped;
        break;
    case NativeKind::SourceAccessFailed:
        kind_ = ImageSourceDiscoveryIssueKind::SourceAccessFailed;
        break;
    case NativeKind::ApplicationOwnedFolderExcluded:
        kind_ = ImageSourceDiscoveryIssueKind::ApplicationOwnedFolderExcluded;
        break;
    default:
        throw hresult_invalid_argument();
    }
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
