#include "pch.h"
#include "PresentationSourceContracts.h"
#include "PresentationTestApartment.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <map>
#include <set>
#include <string_view>
#include <vector>

namespace
{
using Document = winrt::Windows::Data::Xml::Dom::XmlDocument;
using Resources = std::map<std::wstring, std::wstring>;

// XML declarations are a separate authoring contract; compiled PRI candidate
// provenance and real UIA behavior retain their existing independent tests.
Resources resourceValues(const Document &document)
{
    Resources result;
    for (const auto &entry : document.GetElementsByTagName(L"data"))
    {
        const auto name = entry.Attributes().GetNamedItem(L"name");
        REQUIRE(name);
        const auto value = entry.SelectSingleNode(L"value");
        REQUIRE(value);
        REQUIRE_FALSE(value.InnerText().empty());
        REQUIRE(result.emplace(std::wstring{name.InnerText()}, std::wstring{value.InnerText()}).second);
    }
    REQUIRE_FALSE(result.empty());
    return result;
}
} // namespace

TEST_CASE("All locale declarations have equal unique nonempty semantic resource sets",
          "[presentation][localization][source-contract]")
{
    jpg_spinner::test_support::ensurePresentationTestApartment();
    const auto reference = resourceValues(jpg_spinner::test_support::loadPresentationSourceContract(L"en-US.resw"));
    const auto locale = GENERATE(L"en-GB", L"ru");
    const auto translated =
        resourceValues(jpg_spinner::test_support::loadPresentationSourceContract(std::wstring{locale} + L".resw"));
    REQUIRE(translated.size() == reference.size());
    for (const auto &[key, value] : reference)
    {
        INFO("Locale=" << winrt::to_string(locale) << ", resource=" << winrt::to_string(key));
        REQUIRE(translated.contains(key));
        // Current UI uses separate label/value elements, not format templates
        // or plural interpolation. A future template requires a native formatter
        // and explicit placeholder/plural tests before changing this contract.
        CHECK(value.find_first_of(L"{}") == std::wstring::npos);
        CHECK(translated.at(key).find_first_of(L"{}") == std::wstring::npos);
    }
}

TEST_CASE("Lossless and preservation terminology carries entry-specific translator guidance",
          "[presentation][localization][translator-guidance]")
{
    jpg_spinner::test_support::ensurePresentationTestApartment();
    const auto locale = GENERATE(L"en-US", L"en-GB", L"ru");
    const auto document = jpg_spinner::test_support::loadPresentationSourceContract(std::wstring{locale} + L".resw");
    for (const auto *key : {L"Shell_RequirePerfectTransform.Content", L"Shell_TrimPartialMinimumCodedUnits.Content",
                            L"Shell_ReplaceWithVerifiedBackup.Content", L"Shell_ProgressiveDct.Content",
                            L"ImageProcessingError_UnsupportedEmbeddedPreviewMetadata_Explanation"})
    {
        INFO("Locale=" << winrt::to_string(locale) << ", resource=" << winrt::to_string(key));
        const auto entry = document.SelectSingleNode(std::wstring{L"/root/data[@name='"} + key + L"']");
        REQUIRE(entry);
        const auto comment = entry.SelectSingleNode(L"comment");
        REQUIRE(comment);
        CHECK_FALSE(comment.InnerText().empty());
    }
}

TEST_CASE("Authored shell keeps live announcements restrained and integer displays localized",
          "[presentation][accessibility][source-contract]")
{
    jpg_spinner::test_support::ensurePresentationTestApartment();
    const auto document = jpg_spinner::test_support::loadPresentationSourceContract(L"MainWindow.xaml");
    std::set<std::wstring> automationIds;
    for (const auto &element : document.SelectNodes(L"//*[@AutomationProperties.AutomationId]"))
    {
        const auto id = element.Attributes().GetNamedItem(L"AutomationProperties.AutomationId").InnerText();
        REQUIRE(automationIds.emplace(id).second);
        if (id == L"ProcessingRows")
            CHECK_FALSE(element.Attributes().GetNamedItem(L"AutomationProperties.LiveSetting"));
    }
    for (const auto *id : {L"AnalysisStatus", L"ProcessingRegion", L"ResultsSummary"})
    {
        const auto node =
            document.SelectSingleNode(std::wstring{L"//*[@AutomationProperties.AutomationId='"} + id + L"']");
        REQUIRE(node);
        CHECK(node.Attributes().GetNamedItem(L"AutomationProperties.LiveSetting").InnerText() == L"Polite");
    }
    for (const auto &element : document.GetElementsByTagName(L"TextBlock"))
        if (const auto text = element.Attributes().GetNamedItem(L"Text"))
        {
            const std::wstring binding{text.InnerText()};
            for (const auto *count :
                 {L"EligibleFileCount", L"CompletedFileCount", L"FailedFileCount", L"CancelledFileCount",
                  L"DiscardedSourceRightEdgeWidthInPixels", L"DiscardedSourceBottomEdgeHeightInPixels"})
                if (binding.find(count) != std::wstring::npos)
                    CHECK(binding.find(std::wstring{count} + L"Text") != std::wstring::npos);
        }
}

TEST_CASE("Top-level commands and choices have collision-free localized native access keys",
          "[presentation][localization][accessibility][access-keys]")
{
    jpg_spinner::test_support::ensurePresentationTestApartment();
    const auto locale = GENERATE(L"en-US", L"en-GB", L"ru");
    const auto resources =
        resourceValues(jpg_spinner::test_support::loadPresentationSourceContract(std::wstring{locale} + L".resw"));
    std::vector<std::wstring> keys;
    for (const auto *control :
         {L"SourcePicker", L"Analyze", L"StartProcessing", L"CancelOperation", L"ReturnToSourceSelection",
          L"RetryRecovery", L"RestoreRecoverySource", L"ExportDiagnostics", L"TraversalScope", L"OutputDisposition",
          L"EdgeHandling", L"ScanOrganization"})
    {
        const auto resource = std::wstring{L"Shell_"} + control + L".AccessKey";
        INFO("Locale=" << winrt::to_string(locale) << ", resource=" << winrt::to_string(resource));
        REQUIRE(resources.contains(resource));
        const auto &key = resources.at(resource);
        REQUIRE(key.size() == 1); // Product chooses single-character native keytips.
        for (const auto &prior : keys)
            CHECK(CompareStringOrdinal(prior.c_str(), -1, key.c_str(), -1, TRUE) != CSTR_EQUAL);
        keys.push_back(key);
    }
}

TEST_CASE("Policy declaration order follows row-major visual workflow without tab overrides",
          "[presentation][accessibility][focus-order]")
{
    jpg_spinner::test_support::ensurePresentationTestApartment();
    const auto document = jpg_spinner::test_support::loadPresentationSourceContract(L"MainWindow.xaml");
    const auto choices = document.GetElementsByTagName(L"ComboBox");
    REQUIRE(choices.Size() == 4);
    constexpr const wchar_t *order[]{L"TraversalScope", L"EdgeHandling", L"OutputDisposition", L"ScanOrganization"};
    for (std::uint32_t index = 0; index != choices.Size(); ++index)
    {
        const auto attributes = choices.GetAt(index).Attributes();
        CHECK(attributes.GetNamedItem(L"AutomationProperties.AutomationId").InnerText() == order[index]);
        CHECK_FALSE(attributes.GetNamedItem(L"TabIndex"));
    }
}
