#pragma once
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.Foundation.Collections.h>

namespace jpg_spinner::test_support
{
/// Read declared test-only copies of the owning source contracts beside this
/// executable. Windows parses XML; this fixture does not emulate XAML or MRT.
/// Missing/malformed files fail rather than falling back to checkout discovery.
inline winrt::Windows::Data::Xml::Dom::XmlDocument loadPresentationSourceContract(
    const std::filesystem::path &relativePath)
{
    std::array<wchar_t, 32768> executable{};
    const auto length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (length == 0 || length >= executable.size())
        throw std::runtime_error("Cannot locate the declared source-contract fixture.");
    std::ifstream file{std::filesystem::path{executable.data()}.parent_path() / L"SourceContracts" / relativePath,
                       std::ios::binary};
    if (!file)
        throw std::runtime_error("The source-contract fixture was not deployed by MSBuild.");
    const std::string content{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    winrt::Windows::Data::Xml::Dom::XmlLoadSettings settings;
    settings.ProhibitDtd(true);
    settings.ResolveExternals(false);
    settings.MaxElementDepth(32);
    winrt::Windows::Data::Xml::Dom::XmlDocument document;
    document.LoadXml(winrt::to_hstring(content), settings);
    return document;
}
} // namespace jpg_spinner::test_support
