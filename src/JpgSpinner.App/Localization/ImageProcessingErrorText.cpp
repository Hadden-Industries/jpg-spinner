#include "pch.h"
#include "Localization/ImageProcessingErrorText.h"
#include <jpg_spinner/domain/ImageProcessingSymbolicNames.h>

namespace jpg_spinner::presentation
{
namespace
{
std::wstring resourceStem(domain::ImageProcessingErrorCode code)
{
    // Resource keys and diagnostic symbols share the same closed vocabulary.
    // Unknown values retain the existing localized generic-error fallback.
    return L"ImageProcessingError_" + std::wstring{domain::symbolicName(code)};
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
winrt::hstring loadStartupRecoveryErrorMessage(
    domain::ImageProcessingErrorCode code,
    const winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceMap &resources,
    const winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceContext &context)
{
    const auto text = loadImageProcessingErrorText(code, resources, context);
    // Preserve the recovery-specific artifact warning without substituting a
    // source-access instruction for a different refusal's actual remedy.
    return resources.GetValue(L"StartupRecovery_PreservationNotice", context).ValueAsString() + L"\n" +
           text.explanation + L"\n" + text.remedy;
}
} // namespace jpg_spinner::presentation
