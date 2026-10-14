#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <winrt/Windows.Storage.h>

struct IUIAutomation2;
struct IUIAutomationElement;

namespace jpg_spinner::test_support
{
/// Repository qualification decisions over observed SDK facts, not a Windows
/// struct/COM ABI substitute. Missing facts always refuse interactive acceptance.
struct InteractiveDesktopObservation
{
    bool isNonServiceSession{};
    bool isSessionActive{};
    bool isSessionUnlocked{};
    bool hasVisibleWindowStation{};
    bool hasDefaultInputDesktop{};
    bool hasUiAutomation{};
};

/// Returns an empty string only when every required read-only desktop fact holds.
/// Foreground access and real package registration are additional active probes.
[[nodiscard]] std::string interactiveDesktopRefusal(const InteractiveDesktopObservation &observation);

/// Reads the current process's session/desktop and activates SDK UI Automation
/// on the caller's windowless MTA. Does not switch desktops or inject input.
[[nodiscard]] InteractiveDesktopObservation observeInteractiveDesktop();

/// Rejects missing/unaccepted process-scoped lane declarations before any file
/// copy, deployment, activation or foreground operation. A declaration alone is
/// not proof of host isolation. OperatorApprovedLocalDesktop explicitly accepts
/// interaction with a personal desktop; it never represents an isolated host.
void requireApprovedInteractiveLane();

/// Registers one UUID-owned test package, activates its native app entry, and
/// supplies the verified process's window to a synchronous, windowless-MTA UIA
/// assertion callback. Native package/process teardown runs on callback failure
/// as well as success. Requires a qualified approved lane; never grants folders
/// to the product or invokes its processing pipeline on behalf of a test.
/// The interactive runner supplies the overall timeout; interrupted deployment
/// retains the flushed ownership locator for explicit reconciliation.
void withIsolatedShellWindow(const std::filesystem::path &applicationDirectory,
                             const std::function<void(IUIAutomation2 *, IUIAutomationElement *)> &assertWindowContract);

/// Creates and validates a UUID-owned package-manifest copy through the SDK
/// Appx manifest reader, then removes the generated fixture. No deployment,
/// desktop window, foreground action or host isolation assertion occurs.
void validateIsolatedTestPackageManifest(const std::filesystem::path &applicationDirectory);

/// Runs only the source-access tests in a UUID-owned AppContainer test package,
/// using the existing native deployment fixture and Catch2 runner. No product
/// test hook, picker bypass, UI input, capability broadening or user cache is
/// involved. Requires an approved interactive lane; bounds process completion
/// and preserves the native JUnit report in the parent test's captured output.
void runPackagedSourceAccessTests(const std::filesystem::path &applicationDirectory,
                                  const std::filesystem::path &testExecutablePath,
                                  bool requireRealFolderPicker = false);

/// Opens the real WinRT folder picker on an owned native qualification window.
/// Returns only the operator's brokered grant; cancellation returns null. The
/// supervising worker controller owns the bounded timeout and process teardown.
[[nodiscard]] winrt::Windows::Storage::StorageFolder pickFolderForAccessQualification();

/// Explicit qualifier entry point. Requires an operator-selected approved lane,
/// observes foreground permission on an owned window, and registers/removes a
/// UUID-owned copy of the application package. Returns 78 on any missing proof.
/// No host settings, trust certificates, or existing package identities change.
int qualifyInteractiveUiEnvironment(const std::filesystem::path &applicationDirectory);
} // namespace jpg_spinner::test_support
