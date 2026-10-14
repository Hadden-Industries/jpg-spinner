#include "InteractiveUiEnvironment.h"
#include "PresentationTestApartment.h"
#include <Windows.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <wil/resource.h>
#include <array>
#include <vector>

TEST_CASE("The presentation test host uses per-monitor-v2 physical coordinates", "[presentation][environment][dpi]")
{
    // UIA rectangles are physical pixels. A DPI-virtualized host can request
    // a different effective window width while its GetWindowRect still appears
    // to confirm the request. The native manifest must establish PMv2 before
    // any test window or XAML island is created; no runtime fallback is used.
    CHECK(AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(), DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2));
}

TEST_CASE("Native interactive entry refuses unaccepted lane declarations before side effects",
          "[presentation][environment][lane-guard]")
{
    // Only this test process's environment changes. Restore a pre-existing
    // operator declaration exactly; no desktop/package observation occurs here.
    constexpr auto variable = L"JPG_SPINNER_INTERACTIVE_TEST_LANE";
    SetLastError(ERROR_SUCCESS);
    const auto required = GetEnvironmentVariableW(variable, nullptr, 0);
    const auto originalError = GetLastError();
    REQUIRE((required != 0 || originalError == ERROR_ENVVAR_NOT_FOUND || originalError == ERROR_SUCCESS));
    std::vector<wchar_t> original(required);
    if (required != 0)
        REQUIRE(GetEnvironmentVariableW(variable, original.data(), required) < required);
    const auto restore =
        wil::scope_exit([&] { SetEnvironmentVariableW(variable, required == 0 ? nullptr : original.data()); });
    REQUIRE(SetEnvironmentVariableW(variable, nullptr));
    CHECK_THROWS_WITH(jpg_spinner::test_support::requireApprovedInteractiveLane(),
                      "No accepted interactive test lane was selected.");
    for (const auto declaration : {L"isolatedvirtualmachine", L"LocalDesktop"})
    {
        REQUIRE(SetEnvironmentVariableW(variable, declaration));
        CHECK_THROWS_WITH(jpg_spinner::test_support::requireApprovedInteractiveLane(),
                          "No accepted interactive test lane was selected.");
    }
    // These positive controls exercise declaration syntax only, not qualification
    // or permission to activate the app on the current desktop.
    for (const auto declaration : {L"IsolatedVirtualMachine", L"DedicatedSelfHosted", L"OperatorApprovedLocalDesktop"})
    {
        REQUIRE(SetEnvironmentVariableW(variable, declaration));
        CHECK_NOTHROW(jpg_spinner::test_support::requireApprovedInteractiveLane());
    }
}

TEST_CASE("Interactive qualification refuses each absent desktop fact", "[presentation][environment]")
{
    using namespace jpg_spinner::test_support;
    // Expectations come from Task 13.1 and the documented Windows session/UIA
    // contracts. This pure decision test does not pretend to create a locked
    // desktop, disconnect a user, or qualify the current execution environment.
    const InteractiveDesktopObservation qualified{true, true, true, true, true, true};
    REQUIRE(interactiveDesktopRefusal(qualified).empty());
    auto unconfirmedDesktop = qualified;
    unconfirmedDesktop.hasDefaultInputDesktop = false;
    // A failed OpenInputDesktop/name query and an observed different desktop
    // both withhold this fact. Neither proves a lock screen or secure prompt.
    CHECK(interactiveDesktopRefusal(unconfirmedDesktop) ==
          "The default interactive input desktop could not be confirmed.");
    bool InteractiveDesktopObservation::*const facts[]{
        &InteractiveDesktopObservation::isNonServiceSession,    &InteractiveDesktopObservation::isSessionActive,
        &InteractiveDesktopObservation::isSessionUnlocked,      &InteractiveDesktopObservation::hasVisibleWindowStation,
        &InteractiveDesktopObservation::hasDefaultInputDesktop, &InteractiveDesktopObservation::hasUiAutomation};
    for (const auto fact : facts)
    {
        auto missingFact = qualified;
        missingFact.*fact = false;
        CHECK_FALSE(interactiveDesktopRefusal(missingFact).empty());
    }
    CHECK_FALSE(interactiveDesktopRefusal({}).empty());
}

TEST_CASE("The isolated test-package manifest conforms to its native Windows consumer",
          "[presentation][package-fixture]")
{
    jpg_spinner::test_support::ensurePresentationTestApartment();
    std::array<wchar_t, 32768> executablePath{};
    const auto length = GetModuleFileNameW(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
    REQUIRE(length != 0);
    REQUIRE(length < executablePath.size());
    const auto appDirectory =
        std::filesystem::path{executablePath.data()}.parent_path().parent_path() / L"JpgSpinner.App";
    REQUIRE(std::filesystem::is_regular_file(appDirectory / L"AppxManifest.xml"));
    // Exercise the generated UUID identity through AppxFactory/VerifyPackageFullName,
    // not a second schema implementation or a live package registration.
    CHECK_NOTHROW(jpg_spinner::test_support::validateIsolatedTestPackageManifest(appDirectory));
}

TEST_CASE("A native packaged worker qualifies source-token lifetime and ungranted path denial",
          "[presentation][packaged-runtime][source-access-controller]")
{
    std::array<wchar_t, 32768> executablePath{};
    const auto length = GetModuleFileNameW(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
    REQUIRE(length != 0);
    REQUIRE(length < executablePath.size());
    const auto applicationDirectory =
        std::filesystem::path{executablePath.data()}.parent_path().parent_path() / L"JpgSpinner.App";
    CHECK_NOTHROW(jpg_spinner::test_support::runPackagedSourceAccessTests(applicationDirectory, executablePath.data()));
}

TEST_CASE("A native packaged worker qualifies real picked-folder discovery",
          "[presentation][packaged-runtime][source-access-picker-controller][operator-assisted][ui-automation]")
{
    std::array<wchar_t, 32768> executablePath{};
    const auto length = GetModuleFileNameW(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
    REQUIRE(length != 0);
    REQUIRE(length < executablePath.size());
    const auto applicationDirectory =
        std::filesystem::path{executablePath.data()}.parent_path().parent_path() / L"JpgSpinner.App";
    // The worker obtains its own real picker grant. The request carries only
    // the owned expected location; the controller cannot manufacture access.
    CHECK_NOTHROW(
        jpg_spinner::test_support::runPackagedSourceAccessTests(applicationDirectory, executablePath.data(), true));
}
