#include "InteractiveUiEnvironment.h"
#include "DeterministicJpegFixtureFactory.h"
#include "TemporaryDirectory.h"
#include <catch2/catch_session.hpp>
#include <catch2/catch_test_case_info.hpp>
#include <catch2/interfaces/catch_interfaces_registry_hub.hpp>
#include <catch2/interfaces/catch_interfaces_testcase.hpp>
#include <roapi.h>
#include <wil/com.h>
#include <wil/resource.h>
#include <winrt/base.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Foundation.h>
#include <iostream>
#include <chrono>
#include <fstream>
#include <thread>
#include <string_view>
#include <vector>
#include <algorithm>

namespace
{
/// Checks the selection that Catch2 actually parsed, without splitting its
/// specification grammar or changing the user's selected cases. A headless
/// declaration is a restriction, never authority to activate another desktop.
/// Ordinary invocations may not select the package-only worker entry at all.
int runAdmittedPresentationTests(int argumentCount, const char *const arguments[])
{
    Catch::Session session;
    if (const auto parseResult = session.applyCommandLine(argumentCount, arguments); parseResult != 0)
        return parseResult;
    std::array<wchar_t, 32> environment{};
    const auto length = GetEnvironmentVariableW(L"JPG_SPINNER_TEST_EXECUTION_ENVIRONMENT", environment.data(),
                                                static_cast<DWORD>(environment.size()));
    const std::wstring_view declaredEnvironment{environment.data(), length < environment.size() ? length : 0};
    if (length >= environment.size() ||
        (length != 0 && declaredEnvironment != L"Headless" && declaredEnvironment != L"Interactive"))
    {
        std::cerr << "PRESENTATION_TEST_SELECTION_NOT_ADMITTED: unknown declared execution environment.\n";
        return 78;
    }
    const auto &configuration = session.config();
    const auto &specification = configuration.testSpec();
    for (const auto &test : Catch::getRegistryHub().getTestCaseRegistry().getAllTests())
    {
        const auto &information = test.getTestCaseInfo();
        const bool isSelected =
            specification.hasFilters() ? specification.matches(information) : !information.isHidden();
        if (!isSelected)
            continue;
        const auto hasTag = [&](const char *tag) {
            return std::find(information.tags.begin(), information.tags.end(), Catch::Tag{tag}) !=
                   information.tags.end();
        };
        // Catch2 3.16 applies forbidden patterns within each comma alternative.
        // Appending exclusions alone does not constrain earlier alternatives.
        // Refuse the whole unsafe selection before executing any test rather
        // than reconstructing Catch2's grammar or silently dropping cases.
        if (hasTag("packaged-access-worker") ||
            (declaredEnvironment == L"Headless" &&
             (hasTag("ui-automation") || hasTag("assistive-technology") || hasTag("packaged-runtime"))))
        {
            std::cerr << "PRESENTATION_TEST_SELECTION_NOT_ADMITTED: selected case requires another execution context: "
                      << information.name << '\n';
            return 78;
        }
    }
    return session.run();
}
} // namespace

/// Preserve the standard Catch2 runner; the exact qualification-only entry point
/// is separate so environment refusal cannot look like a failed product test.
int wmain(int argumentCount, wchar_t *arguments[])
{
    // Keep native Windows path arguments Unicode, independently of Catch2's
    // optional wide-command-line build feature.
    if (argumentCount == 2 && std::wstring_view{arguments[1]} == L"--inspect-interactive-desktop")
    {
        // Read-only diagnostics intentionally cannot qualify an isolated lane,
        // foreground capability or package deployment. No window is created.
        const auto apartment = wil::CoInitializeEx(COINIT_MULTITHREADED);
        const auto observation = jpg_spinner::test_support::observeInteractiveDesktop();
        std::cout << "nonServiceSession=" << observation.isNonServiceSession
                  << " active=" << observation.isSessionActive << " unlocked=" << observation.isSessionUnlocked
                  << " visibleWindowStation=" << observation.hasVisibleWindowStation
                  << " defaultInputDesktop=" << observation.hasDefaultInputDesktop
                  << " uiAutomation=" << observation.hasUiAutomation << '\n';
        const auto refusal = jpg_spinner::test_support::interactiveDesktopRefusal(observation);
        if (!refusal.empty())
            std::cout << "Desktop refusal: " << refusal << '\n';
        return 0;
    }
    if (argumentCount == 3 && std::wstring_view{arguments[1]} == L"--qualify-interactive-ui")
        return jpg_spinner::test_support::qualifyInteractiveUiEnvironment(arguments[2]);
    if (argumentCount == 2 && (std::wstring_view{arguments[1]} == L"--run-packaged-access-tests" ||
                               std::wstring_view{arguments[1]} == L"--run-packaged-picker-tests"))
    {
        try
        {
            const auto apartment = wil::RoInitialize(RO_INIT_MULTITHREADED);
            const auto local = winrt::Windows::Storage::ApplicationData::Current().LocalFolder();
            const auto reportPath = std::filesystem::path{local.Path().c_str()} / L"SourceAccess.junit.xml";
            const auto reporter = "JUnit::out=" + winrt::to_string(reportPath.native());
            const auto pickerQualification = std::wstring_view{arguments[1]} == L"--run-packaged-picker-tests";
            const char *testArguments[]{"packaged-source-access",
                                        pickerQualification ? "[packaged-access-worker][native-picker]"
                                                            : "[packaged-access-worker]~[operator-assisted]",
                                        "--reporter", reporter.c_str()};
            return Catch::Session().run(static_cast<int>(std::size(testArguments)), testArguments);
        }
        catch (const winrt::hresult_error &error)
        {
            std::cerr << "PACKAGED_ACCESS_SETUP_FAILED: HRESULT=" << static_cast<std::int32_t>(error.code()) << '\n';
            return 78; // Native setup refusal is not a behavioural RED or a passing product test.
        }
    }
    if (argumentCount == 3 && std::wstring_view{arguments[1]} == L"--exercise-interactive-shell")
    {
        try
        {
            // This is a bounded operator exercise, NOT an automated passing test.
            // Reuse native package activation/teardown: launching a packaged WinUI
            // executable directly does not establish its package dependency graph.
            // No production test hook supplies the folder; the operator must use
            // the real HWND-owned picker, with these exclusively owned JPEGs.
            jpg_spinner::test_support::requireApprovedInteractiveLane();
            const auto apartment = wil::RoInitialize(RO_INIT_MULTITHREADED);
            const auto refusal = jpg_spinner::test_support::interactiveDesktopRefusal(
                jpg_spinner::test_support::observeInteractiveDesktop());
            if (!refusal.empty())
                throw std::runtime_error{refusal};
            // Keep this positive fixture outside application data so access
            // depends on a real picker grant, not the app's default local rights.
            const jpg_spinner::test_support::TemporaryDirectory fixture{std::filesystem::current_path() /
                                                                        L"artifacts/tests"};
            const auto selected = fixture.directoryPath() / L"Selected";
            std::filesystem::create_directory(selected);
            const auto bytes = jpg_spinner::test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
                jpg_spinner::test_support::JpegChromaSubsampling::ycbcr444, 5);
            {
                std::ofstream file{selected / L"orientation-5.jpg", std::ios::binary};
                file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                file.close();
                if (!file.good())
                    throw std::runtime_error{"Could not write the owned interactive JPEG fixture."};
            }
            std::cout << "OPERATOR_EXERCISE_SOURCE: " << winrt::to_string(selected.native()) << std::endl;
            jpg_spinner::test_support::withIsolatedShellWindow(arguments[2], [](auto *, auto *) {
                std::cout << "OPERATOR_EXERCISE_READY: 180 seconds; no automated workflow verdict." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds{180});
            });
            return 0;
        }
        catch (const winrt::hresult_error &error)
        {
            std::cerr << "OPERATOR_EXERCISE_FAILED: " << winrt::to_string(error.message())
                      << " HRESULT=" << static_cast<std::int32_t>(error.code()) << '\n';
        }
        catch (const std::exception &error)
        {
            std::cerr << "OPERATOR_EXERCISE_FAILED: " << error.what() << '\n';
        }
        return 1;
    }
    // The locked Catch2 library exposes its narrow UTF-8 command-line interface.
    // Use C++/WinRT's conversion and let Catch2 own argument parsing; do not
    // reproduce its optional wide overload or change a dependency's ABI flags.
    std::vector<std::string> utf8Arguments;
    utf8Arguments.reserve(static_cast<std::size_t>(argumentCount));
    for (int index = 0; index != argumentCount; ++index)
        utf8Arguments.push_back(winrt::to_string(std::wstring_view{arguments[index]}));
    // Only the explicit packaged entry above may select worker contracts. The
    // ordinary host can run the controller, never accidentally qualify itself.
    utf8Arguments.emplace_back("~[packaged-access-worker]");
    std::vector<const char *> argumentPointers;
    argumentPointers.reserve(utf8Arguments.size());
    for (const auto &argument : utf8Arguments)
        argumentPointers.push_back(argument.c_str());
    return runAdmittedPresentationTests(static_cast<int>(argumentPointers.size()), argumentPointers.data());
}
