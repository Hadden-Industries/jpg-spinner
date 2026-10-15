#include "InteractiveUiEnvironment.h"
#include "DeterministicJpegFixtureFactory.h"
#include "TemporaryDirectory.h"
#include <Windows.h>
#include <dlgs.h>
#include <UIAutomation.h>
#include <wil/com.h>
#include <wil/resource.h>
#include <winrt/base.h>
#include <winrt/Microsoft.Windows.ApplicationModel.Resources.h>
#include <winrt/Windows.Management.Core.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Search.h>
// Win32's printing macro collides with the projected runtime-class name.
// Scope Microsoft's documented C++/WinRT macro-collision remedy to this header.
#pragma push_macro("DocumentProperties")
#undef DocumentProperties
#include <winrt/Windows.Storage.FileProperties.h>
#pragma pop_macro("DocumentProperties")
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <appmodel.h>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <filesystem>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <cstring>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
#include <algorithm>

namespace
{
/// Finds an authored automation ID only inside the verified app window. Native
/// UIA owns traversal/property semantics; no XAML/OS-picker tree is imitated.
wil::com_ptr<IUIAutomationElement> findControl(IUIAutomation2 *automation, IUIAutomationElement *window,
                                               const wchar_t *automationId, CONTROLTYPEID controlType = 0)
{
    wil::unique_variant value;
    value.vt = VT_BSTR;
    value.bstrVal = SysAllocString(automationId);
    THROW_IF_NULL_ALLOC(value.bstrVal);
    wil::com_ptr<IUIAutomationCondition> condition;
    THROW_IF_FAILED(automation->CreatePropertyCondition(UIA_AutomationIdPropertyId, value, condition.put()));
    if (controlType != 0)
    {
        // OS dialog numeric IDs can recur in unrelated descendant containers.
        // Combine the SDK ID with the required control contract using UIA's
        // own condition evaluator, rather than accepting the first ID match.
        wil::unique_variant typeValue;
        typeValue.vt = VT_I4;
        typeValue.lVal = controlType;
        wil::com_ptr<IUIAutomationCondition> typeCondition;
        THROW_IF_FAILED(automation->CreatePropertyCondition(UIA_ControlTypePropertyId, typeValue, typeCondition.put()));
        wil::com_ptr<IUIAutomationCondition> combined;
        THROW_IF_FAILED(automation->CreateAndCondition(condition.get(), typeCondition.get(), combined.put()));
        condition = std::move(combined);
    }
    wil::com_ptr<IUIAutomationElement> control;
    THROW_IF_FAILED(window->FindFirst(TreeScope_Descendants, condition.get(), control.put()));
    return control;
}

/// Opens the production picker and admits only an enabled popup owned by the
/// verified product HWND. A missing broker relationship fails qualification;
/// never search unrelated desktop dialogs by title or guessed process identity.
wil::com_ptr<IUIAutomationElement> openOwnedStoragePicker(IUIAutomation2 *automation, IUIAutomationElement *window,
                                                          const wchar_t *pickerButtonIdentifier = L"SourceFolderPicker")
{
    UIA_HWND nativeWindow{};
    THROW_IF_FAILED(window->get_CurrentNativeWindowHandle(&nativeWindow));
    const auto owner = reinterpret_cast<HWND>(nativeWindow);
    REQUIRE(IsWindowEnabled(owner));
    const auto existingPopup = GetWindow(owner, GW_ENABLEDPOPUP);
    // GetWindow's general no-relationship result can be null. Neither that
    // result nor the owner itself admits a separate dialog as an input target.
    REQUIRE((existingPopup == nullptr || existingPopup == owner));
    const auto sourcePicker = findControl(automation, window, pickerButtonIdentifier);
    REQUIRE(sourcePicker);
    wil::com_ptr<IUIAutomationInvokePattern> invoke;
    THROW_IF_FAILED(sourcePicker->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(invoke.put())));
    THROW_IF_FAILED(invoke->Invoke());

    HWND popup{};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    do
    {
        popup = GetWindow(owner, GW_ENABLEDPOPUP);
        if (popup && popup != owner && !IsWindowEnabled(owner))
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
    } while (std::chrono::steady_clock::now() < deadline);
    REQUIRE(popup);
    REQUIRE(popup != owner);
    REQUIRE_FALSE(IsWindowEnabled(owner));
    REQUIRE(GetWindow(popup, GW_OWNER) == owner);
    wil::com_ptr<IUIAutomationElement> picker;
    THROW_IF_FAILED(automation->ElementFromHandle(reinterpret_cast<UIA_HWND>(popup), picker.put()));
    REQUIRE(picker);
    return picker;
}

/// Invoke the provider's supported operation; never synthesize application
/// events, assign a production folder, or bypass the OS capability grant.
void invokeControl(IUIAutomationElement *control)
{
    REQUIRE(control);
    wil::com_ptr<IUIAutomationInvokePattern> invoke;
    THROW_IF_FAILED(control->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(invoke.put())));
    THROW_IF_FAILED(invoke->Invoke());
}

/// Focus the actual native picker edit and await a real keyboard-mediated grant.
/// The qualified OS provider exposes the SDK common-dialog edt1/IDOK controls.
/// If that provider changes, fail qualification rather than guess another edit.
/// ValuePattern assignment did not commit this broker's typed-input state in
/// the retained reproducer; do not simulate private change notifications or
/// substitute a production folder hook to make the acceptance case pass.
void awaitKeyboardSourceFolderGrant(IUIAutomation2 *automation, IUIAutomationElement *window,
                                    const std::filesystem::path &source)
{
    REQUIRE(source.is_absolute());
    REQUIRE(std::filesystem::is_directory(source));
    const auto picker = openOwnedStoragePicker(automation, window);
    const auto folder = findControl(automation, picker.get(), std::to_wstring(edt1).c_str());
    REQUIRE(folder);
    CONTROLTYPEID type{};
    THROW_IF_FAILED(folder->get_CurrentControlType(&type));
    REQUIRE(type == UIA_EditControlTypeId);
    BOOL enabled{};
    THROW_IF_FAILED(folder->get_CurrentIsEnabled(&enabled));
    REQUIRE(enabled);
    wil::com_ptr<IUIAutomationValuePattern> value;
    THROW_IF_FAILED(folder->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(value.put())));
    BOOL readOnly{};
    THROW_IF_FAILED(value->get_CurrentIsReadOnly(&readOnly));
    REQUIRE_FALSE(readOnly);
    THROW_IF_FAILED(folder->SetFocus());
    BOOL focused{};
    THROW_IF_FAILED(folder->get_CurrentHasKeyboardFocus(&focused));
    REQUIRE(focused);
    std::cout << "NATIVE_PICKER_KEYBOARD_READY: enter only " << winrt::to_string(source.native())
              << "; confirm the folder; do not invoke Start. Native UIA drives the remaining workflow." << std::endl;
    UIA_HWND nativeWindow{};
    THROW_IF_FAILED(window->get_CurrentNativeWindowHandle(&nativeWindow));
    const auto owner = reinterpret_cast<HWND>(nativeWindow);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{180};
    do
    {
        if (IsWindowEnabled(owner))
            return;
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
    } while (std::chrono::steady_clock::now() < deadline);
    INFO("No real keyboard-mediated folder grant arrived within the bounded operator interval; "
         "this is an input-qualification gap, not a product regression.");
    REQUIRE(IsWindowEnabled(owner));
}

/// Wait for the real asynchronous picker handler to publish its enabled gate;
/// native owner re-enabling alone can precede that application state transition.
wil::com_ptr<IUIAutomationElement> waitForEnabledControl(IUIAutomation2 *automation, IUIAutomationElement *window,
                                                         const wchar_t *identifier)
{
    INFO("Waiting for authored control: " << winrt::to_string(identifier));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    UIA_HWND nativeWindow{};
    THROW_IF_FAILED(window->get_CurrentNativeWindowHandle(&nativeWindow));
    wil::com_ptr<IUIAutomationElement> control;
    BOOL enabled{};
    do
    {
        if (IsWindowEnabled(reinterpret_cast<HWND>(nativeWindow)))
            control = findControl(automation, window, identifier);
        if (control)
            THROW_IF_FAILED(control->get_CurrentIsEnabled(&enabled));
        if (enabled)
            return control;
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
    } while (std::chrono::steady_clock::now() < deadline);
    FAIL("The asynchronous shell did not enable the requested control within five seconds.");
}

/// Select an authored option by its localized public label through native UIA.
/// This exercises the ComboBox event path instead of assigning model options.
void selectLocalizedOption(IUIAutomation2 *automation, IUIAutomationElement *window, const wchar_t *identifier,
                           const winrt::hstring &name)
{
    const auto control = waitForEnabledControl(automation, window, identifier);
    wil::com_ptr<IUIAutomationExpandCollapsePattern> expansion;
    THROW_IF_FAILED(control->GetCurrentPatternAs(UIA_ExpandCollapsePatternId, IID_PPV_ARGS(expansion.put())));
    THROW_IF_FAILED(expansion->Expand());
    wil::unique_variant expected;
    expected.vt = VT_BSTR;
    expected.bstrVal = SysAllocString(name.c_str());
    THROW_IF_NULL_ALLOC(expected.bstrVal);
    wil::com_ptr<IUIAutomationCondition> condition;
    THROW_IF_FAILED(automation->CreatePropertyCondition(UIA_NamePropertyId, expected, condition.put()));
    wil::com_ptr<IUIAutomationElement> option;
    THROW_IF_FAILED(window->FindFirst(TreeScope_Descendants, condition.get(), option.put()));
    REQUIRE(option);
    wil::com_ptr<IUIAutomationSelectionItemPattern> selection;
    THROW_IF_FAILED(option->GetCurrentPatternAs(UIA_SelectionItemPatternId, IID_PPV_ARGS(selection.put())));
    THROW_IF_FAILED(selection->Select());
    THROW_IF_FAILED(expansion->Collapse());
}

/// Scrolls only the caller's verified native viewport, returning whether the
/// control's complete physical bounds fit within five seconds. Native UIA
/// failures propagate; a hidden or horizontally overflowing control stays false.
bool scrollControlFullyIntoViewport(IUIAutomationElement *control, IUIAutomationElement *viewport,
                                    IUIAutomationScrollPattern *scrolling)
{
    BOOL canScrollInitially{};
    THROW_IF_FAILED(scrolling->get_CurrentVerticallyScrollable(&canScrollInitially));
    if (canScrollInitially)
        // Focus/consent can scroll below a previously visible review fact. Start
        // this bounded reachability observation at the native vertical origin;
        // searching downward from an arbitrary offset can never find that fact.
        THROW_IF_FAILED(scrolling->SetScrollPercent(UIA_ScrollPatternNoScroll, 0));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    do
    {
        RECT viewportBounds{};
        RECT controlBounds{};
        THROW_IF_FAILED(viewport->get_CurrentBoundingRectangle(&viewportBounds));
        THROW_IF_FAILED(control->get_CurrentBoundingRectangle(&controlBounds));
        if (controlBounds.right > controlBounds.left && controlBounds.bottom > controlBounds.top &&
            controlBounds.left >= viewportBounds.left && controlBounds.right <= viewportBounds.right &&
            controlBounds.top >= viewportBounds.top && controlBounds.bottom <= viewportBounds.bottom)
            return true;
        BOOL canScroll{};
        THROW_IF_FAILED(scrolling->get_CurrentVerticallyScrollable(&canScroll));
        double scrollPercent{};
        THROW_IF_FAILED(scrolling->get_CurrentVerticalScrollPercent(&scrollPercent));
        if (canScroll && scrollPercent < 100)
            THROW_IF_FAILED(scrolling->Scroll(ScrollAmount_NoAmount, ScrollAmount_SmallIncrement));
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
}

TEST_CASE("The minimum viewport keeps source and policy controls reachable through native scrolling",
          "[presentation][automation][reachability][ui-automation]")
{
    REQUIRE(AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(), DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2));
    // Preserve long-path coverage without a 64 KiB stack allocation.
    std::vector<wchar_t> executablePath(32768);
    const auto length = GetModuleFileNameW(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
    REQUIRE(length != 0);
    REQUIRE(length < executablePath.size());
    const auto applicationDirectory =
        std::filesystem::path{executablePath.data()}.parent_path().parent_path() / L"JpgSpinner.App";
    jpg_spinner::test_support::withIsolatedShellWindow(
        applicationDirectory, [](IUIAutomation2 *automation, IUIAutomationElement *window) {
            UIA_HWND nativeWindow{};
            THROW_IF_FAILED(window->get_CurrentNativeWindowHandle(&nativeWindow));
            const auto dpi = GetDpiForWindow(reinterpret_cast<HWND>(nativeWindow));
            REQUIRE(dpi != 0);
            wil::com_ptr<IUIAutomationTransformPattern> transform;
            THROW_IF_FAILED(window->GetCurrentPatternAs(UIA_TransformPatternId, IID_PPV_ARGS(transform.put())));
            THROW_IF_FAILED(
                transform->Resize(MulDiv(360, static_cast<int>(dpi), 96), MulDiv(480, static_cast<int>(dpi), 96)));
            const auto viewport = findControl(automation, window, L"WorkflowContextViewport");
            REQUIRE(viewport);
            wil::com_ptr<IUIAutomationScrollPattern> scrolling;
            THROW_IF_FAILED(viewport->GetCurrentPatternAs(UIA_ScrollPatternId, IID_PPV_ARGS(scrolling.put())));
            for (const auto automationId :
                 {L"SourceFolderPicker", L"TraversalScope", L"OutputDisposition", L"EdgeHandling", L"ScanOrganization"})
            {
                INFO("Minimum-height reachability: " << winrt::to_string(automationId));
                const auto control = findControl(automation, window, automationId);
                REQUIRE(control);
                // Observe actual clipping after native scrolling, not merely a
                // control's presence in the accessibility tree. The initial
                // disabled policy controls still need readable, reachable text.
                CHECK(scrollControlFullyIntoViewport(control.get(), viewport.get(), scrolling.get()));
            }
        });
}

/// Independent initial-state contract: before a real folder grant only the
/// picker can begin this workflow. A present but enabled analysis/start button
/// would falsify the shell's null-model binding fallback.
void requireControl(IUIAutomation2 *automation, IUIAutomationElement *window, const wchar_t *automationId,
                    CONTROLTYPEID expectedType, bool expectedEnabled)
{
    INFO("Automation ID: " << winrt::to_string(automationId));
    const auto control = findControl(automation, window, automationId);
    REQUIRE(control);
    CONTROLTYPEID controlType{};
    THROW_IF_FAILED(control->get_CurrentControlType(&controlType));
    CHECK(controlType == expectedType);
    BOOL isEnabled{};
    THROW_IF_FAILED(control->get_CurrentIsEnabled(&isEnabled));
    CHECK(static_cast<bool>(isEnabled) == expectedEnabled);
    BOOL isFocusable{};
    THROW_IF_FAILED(control->get_CurrentIsKeyboardFocusable(&isFocusable));
    CHECK(static_cast<bool>(isFocusable) == expectedEnabled);
    wil::unique_bstr name;
    THROW_IF_FAILED(control->get_CurrentName(name.put()));
    CHECK((name && SysStringLen(name.get()) != 0));
    wil::unique_bstr help;
    THROW_IF_FAILED(control->get_CurrentHelpText(help.put()));
    CHECK((help && SysStringLen(help.get()) != 0));
}

/// Query the native selection provider even while initial controls are disabled.
/// Compare against the independently selected localized resource, not a guessed
/// enum ordinal, control position, or hard-coded OS display language.
void requireDefaultSelection(IUIAutomation2 *automation, IUIAutomationElement *window, const wchar_t *automationId,
                             const winrt::hstring &expectedName)
{
    INFO("Default selection: " << winrt::to_string(automationId));
    const auto control = findControl(automation, window, automationId);
    REQUIRE(control);
    wil::com_ptr<IUIAutomationSelectionPattern> selection;
    THROW_IF_FAILED(control->GetCurrentPatternAs(UIA_SelectionPatternId, IID_PPV_ARGS(selection.put())));
    wil::com_ptr<IUIAutomationElementArray> selected;
    THROW_IF_FAILED(selection->GetCurrentSelection(selected.put()));
    int count{};
    THROW_IF_FAILED(selected->get_Length(&count));
    REQUIRE(count == 1);
    wil::com_ptr<IUIAutomationElement> item;
    THROW_IF_FAILED(selected->GetElement(0, item.put()));
    wil::unique_bstr name;
    THROW_IF_FAILED(item->get_CurrentName(name.put()));
    CHECK(std::wstring_view{name.get(), SysStringLen(name.get())} ==
          std::wstring_view{expectedName.c_str(), expectedName.size()});
}
} // namespace

TEST_CASE("The real source picker exposes an owned native automation provider",
          "[presentation][automation][picker-provider][ui-automation]")
{
    // Characterize the actual OS provider before relying on a dialog locator.
    // Log only control types/automation IDs, never directory items or values.
    // Preserve long-path coverage without a 64 KiB stack allocation.
    std::vector<wchar_t> executablePath(32768);
    const auto length = GetModuleFileNameW(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
    REQUIRE(length != 0);
    REQUIRE(length < executablePath.size());
    const auto applicationDirectory =
        std::filesystem::path{executablePath.data()}.parent_path().parent_path() / L"JpgSpinner.App";
    jpg_spinner::test_support::withIsolatedShellWindow(
        applicationDirectory, [](IUIAutomation2 *automation, IUIAutomationElement *window) {
            const auto picker = openOwnedStoragePicker(automation, window);
            // These SDK identifiers were independently observed from the real
            // native provider. Qualify its operations, not directory contents.
            const auto folder = findControl(automation, picker.get(), std::to_wstring(edt1).c_str());
            REQUIRE(folder);
            CONTROLTYPEID type{};
            THROW_IF_FAILED(folder->get_CurrentControlType(&type));
            REQUIRE(type == UIA_EditControlTypeId);
            wil::com_ptr<IUIAutomationValuePattern> value;
            THROW_IF_FAILED(folder->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(value.put())));
            REQUIRE(value);
            for (const auto identifier : {IDOK, IDCANCEL})
            {
                const auto control =
                    findControl(automation, picker.get(), std::to_wstring(identifier).c_str(), UIA_ButtonControlTypeId);
                REQUIRE(control);
                THROW_IF_FAILED(control->get_CurrentControlType(&type));
                REQUIRE(type == UIA_ButtonControlTypeId);
                wil::com_ptr<IUIAutomationInvokePattern> invoke;
                THROW_IF_FAILED(control->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(invoke.put())));
                REQUIRE(invoke);
            }
            wil::com_ptr<IUIAutomationWindowPattern> pickerWindow;
            THROW_IF_FAILED(picker->GetCurrentPatternAs(UIA_WindowPatternId, IID_PPV_ARGS(pickerWindow.put())));
            THROW_IF_FAILED(pickerWindow->Close()); // Cancel; do not grant any folder.
        });
}

TEST_CASE("The isolated shell starts with only source selection available",
          "[presentation][automation][state][ui-automation]")
{
    // This is a black-box initial-state contract, NOT folder-picker/review,
    // sibling access denial, Narrator, or rendered-layout acceptance. The test
    // executes only after a declared lane passes actual desktop qualification.
    // Headless runs exclude this tag; direct native calls still fail the lane
    // guard before copying files, registration or desktop activation.
    // Preserve long-path coverage without a 64 KiB stack allocation.
    std::vector<wchar_t> executablePath(32768);
    const auto length = GetModuleFileNameW(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
    REQUIRE(length != 0);
    REQUIRE(length < executablePath.size());
    const auto applicationDirectory =
        std::filesystem::path{executablePath.data()}.parent_path().parent_path() / L"JpgSpinner.App";
    jpg_spinner::test_support::withIsolatedShellWindow(applicationDirectory, [](IUIAutomation2 *automation,
                                                                                IUIAutomationElement *window) {
        int processId{};
        THROW_IF_FAILED(window->get_CurrentProcessId(&processId));
        const wil::unique_handle process{
            OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(processId))};
        REQUIRE(process);
        wil::unique_handle token;
        REQUIRE(OpenProcessToken(process.get(), TOKEN_QUERY, token.put()));
        DWORD isAppContainer{};
        DWORD returnedLength{};
        REQUIRE(GetTokenInformation(token.get(), TokenIsAppContainer, &isAppContainer, sizeof(isAppContainer),
                                    &returnedLength));
        CHECK(isAppContainer == 1); // Effective product trust, not merely manifest spelling.
        const auto heading = findControl(automation, window, L"SourceSelectionRegion");
        REQUIRE(heading);
        BOOL isOffscreen{};
        THROW_IF_FAILED(heading->get_CurrentIsOffscreen(&isOffscreen));
        CHECK_FALSE(isOffscreen);
        requireControl(automation, window, L"SourceFolderPicker", UIA_ButtonControlTypeId, true);
        for (const auto automationId : {L"TraversalScope", L"OutputDisposition", L"EdgeHandling", L"ScanOrganization"})
            requireControl(automation, window, automationId, UIA_ComboBoxControlTypeId, false);
        for (const auto automationId : {L"AnalyzeSelectedSource", L"StartProcessing", L"CancelOperation"})
            requireControl(automation, window, automationId, UIA_ButtonControlTypeId, false);
        using namespace winrt::Microsoft::Windows::ApplicationModel::Resources;
        ResourceManager resources{ResourceLoader::GetDefaultResourceFilePath()};
        const auto text = resources.MainResourceMap().GetSubtree(L"Resources");
        const auto context = resources.CreateResourceContext();
        const std::pair<const wchar_t *, const wchar_t *> defaults[]{
            {L"TraversalScope", L"Shell_SelectedFolderOnly/Content"},
            {L"OutputDisposition", L"Shell_CreateCorrectedCopy/Content"},
            {L"EdgeHandling", L"Shell_RequirePerfectTransform/Content"},
            {L"ScanOrganization", L"Shell_PreserveSourceScans/Content"}};
        for (const auto &[automationId, resource] : defaults)
            requireDefaultSelection(automation, window, automationId, text.GetValue(resource, context).ValueAsString());
        // This test deliberately does not invoke Start/Analyze or supply a
        // path through a production test hook. Actual grants require the
        // HWND-owned picker and a separately qualified keyboard workflow.
    });
}

TEST_CASE("Cancelling the HWND-owned diagnostic export picker leaves every local log byte unchanged",
          "[presentation][automation][diagnostic-export][native-picker][ui-automation]")
{
    // Preserve long-path coverage without a 64 KiB stack allocation.
    std::vector<wchar_t> executablePath(32768);
    const auto length = GetModuleFileNameW(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
    REQUIRE(length != 0);
    REQUIRE(length < executablePath.size());
    const auto applicationDirectory =
        std::filesystem::path{executablePath.data()}.parent_path().parent_path() / L"JpgSpinner.App";
    jpg_spinner::test_support::withIsolatedShellWindow(applicationDirectory, [](IUIAutomation2 *automation,
                                                                                IUIAutomationElement *window) {
        const auto exportButton = waitForEnabledControl(automation, window, L"ExportDiagnostics");
        REQUIRE(exportButton);
        THROW_IF_FAILED(exportButton->SetFocus());
        int processId{};
        THROW_IF_FAILED(window->get_CurrentProcessId(&processId));
        const wil::unique_handle process{
            OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(processId))};
        REQUIRE(process);
        UINT32 familyLength{};
        REQUIRE(GetPackageFamilyName(process.get(), &familyLength, nullptr) == ERROR_INSUFFICIENT_BUFFER);
        std::vector<wchar_t> family(familyLength);
        REQUIRE(GetPackageFamilyName(process.get(), &familyLength, family.data()) == ERROR_SUCCESS);
        INFO("Verified owned family=" << winrt::to_string(winrt::hstring{family.data()}));
        const auto local =
            winrt::Windows::Management::Core::ApplicationDataManager::CreateForPackageFamily(family.data())
                .LocalFolder();
        const auto folder = local.GetFolderAsync(L"LocalDiagnostics").get();
        // The controller owns this exclusively created test package. Physical
        // fixture inspection is not a product source grant or picker bypass.
        const auto physicalFolder = winrt::Windows::Storage::StorageFolder::GetFolderFromPathAsync(folder.Path()).get();
        const auto files =
            physicalFolder.GetFilesAsync(winrt::Windows::Storage::Search::CommonFileQuery::DefaultQuery, 0, 130).get();
        REQUIRE(files.Size() >= 2); // A startup record and its native empty lease.
        std::vector<std::pair<winrt::Windows::Storage::StorageFile, winrt::Windows::Storage::Streams::IBuffer>> before;
        for (const auto &file : files)
        {
            // Empty lease files have no payload to compare. Prove their size
            // independently and byte-compare the nonempty published records.
            const auto size = file.GetBasicPropertiesAsync().get().Size();
            if (size == 0)
                before.emplace_back(file, nullptr);
            else
                before.emplace_back(file, winrt::Windows::Storage::FileIO::ReadBufferAsync(file).get());
        }
        const auto picker = openOwnedStoragePicker(automation, window, L"ExportDiagnostics");
        const auto cancel =
            findControl(automation, picker.get(), std::to_wstring(IDCANCEL).c_str(), UIA_ButtonControlTypeId);
        REQUIRE(cancel);
        invokeControl(cancel.get());
        REQUIRE(waitForEnabledControl(automation, window, L"ExportDiagnostics"));
        // Characterize native focus restoration before adding app focus code.
        // A declaration cannot substitute for the actual post-picker peer state.
        BOOL restoredFocus{};
        THROW_IF_FAILED(exportButton->get_CurrentHasKeyboardFocus(&restoredFocus));
        CHECK(restoredFocus);
        CHECK(physicalFolder.GetFilesAsync().get().Size() == files.Size());
        for (const auto &[file, bytes] : before)
        {
            if (bytes)
                CHECK(winrt::Windows::Security::Cryptography::CryptographicBuffer::Compare(
                    bytes, winrt::Windows::Storage::FileIO::ReadBufferAsync(file).get()));
            else
                CHECK(file.GetBasicPropertiesAsync().get().Size() == 0);
        }
    });
}

TEST_CASE("A real save-picker grant exports exact sanitized diagnostics and completes its provider",
          "[presentation][automation][diagnostic-save][native-picker][ui-automation]")
{
    jpg_spinner::test_support::requireApprovedInteractiveLane();
    const jpg_spinner::test_support::TemporaryDirectory directory{std::filesystem::temp_directory_path() /
                                                                  L"jpg-spinner-diagnostic-save-tests"};
    const auto destination = directory.directoryPath() / L"diagnostics.jsonl";
    REQUIRE_FALSE(std::filesystem::exists(destination));
    // Preserve long-path coverage without a 64 KiB stack allocation.
    std::vector<wchar_t> executablePath(32768);
    const auto length = GetModuleFileNameW(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
    REQUIRE(length != 0);
    REQUIRE(length < executablePath.size());
    const auto applicationDirectory =
        std::filesystem::path{executablePath.data()}.parent_path().parent_path() / L"JpgSpinner.App";
    jpg_spinner::test_support::withIsolatedShellWindow(applicationDirectory, [&](IUIAutomation2 *automation,
                                                                                 IUIAutomationElement *window) {
        REQUIRE(waitForEnabledControl(automation, window, L"ExportDiagnostics"));
        int processId{};
        THROW_IF_FAILED(window->get_CurrentProcessId(&processId));
        const wil::unique_handle process{
            OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(processId))};
        REQUIRE(process);
        UINT32 familyLength{};
        REQUIRE(GetPackageFamilyName(process.get(), &familyLength, nullptr) == ERROR_INSUFFICIENT_BUFFER);
        std::vector<wchar_t> family(familyLength);
        REQUIRE(GetPackageFamilyName(process.get(), &familyLength, family.data()) == ERROR_SUCCESS);
        const auto local =
            winrt::Windows::Management::Core::ApplicationDataManager::CreateForPackageFamily(family.data())
                .LocalFolder();
        const auto logFolder = local.GetFolderAsync(L"LocalDiagnostics").get();
        const auto physicalLogFolder =
            winrt::Windows::Storage::StorageFolder::GetFolderFromPathAsync(logFolder.Path()).get();
        std::vector<std::pair<std::wstring, std::string>> records;
        for (const auto &file : physicalLogFolder.GetFilesAsync().get())
            if (std::wstring_view{file.Name()}.starts_with(L"diagnostic-"))
                records.emplace_back(std::wstring{file.Name()},
                                     winrt::to_string(winrt::Windows::Storage::FileIO::ReadTextAsync(file).get()));
        REQUIRE_FALSE(records.empty());
        std::sort(records.begin(), records.end()); // Persisted publication ordinal is part of the filename.
        std::string expected;
        for (const auto &[name, bytes] : records)
            expected += bytes;
        const auto picker = openOwnedStoragePicker(automation, window, L"ExportDiagnostics");
        // Observed native Save-provider identity, not the Folder provider's edt1
        // contract. Exercise supported Value/Invoke patterns and fail if their
        // contract changes; no private notification or product grant injection.
        const auto fileName = findControl(automation, picker.get(), L"1001", UIA_EditControlTypeId);
        REQUIRE(fileName);
        wil::com_ptr<IUIAutomationValuePattern> value;
        THROW_IF_FAILED(fileName->GetCurrentPatternAs(UIA_ValuePatternId, IID_PPV_ARGS(value.put())));
        BOOL readOnly{};
        THROW_IF_FAILED(value->get_CurrentIsReadOnly(&readOnly));
        REQUIRE_FALSE(readOnly);
        const wil::unique_bstr fileNameValue{SysAllocString(destination.c_str())};
        REQUIRE(fileNameValue);
        THROW_IF_FAILED(value->SetValue(fileNameValue.get()));
        wil::unique_bstr selectedName;
        THROW_IF_FAILED(value->get_CurrentValue(selectedName.put()));
        REQUIRE(std::wstring_view{selectedName.get()} == destination.native());
        const auto save = findControl(automation, picker.get(), std::to_wstring(IDOK).c_str(), UIA_ButtonControlTypeId);
        REQUIRE(save);
        invokeControl(save.get());
        UIA_HWND nativeWindow{};
        THROW_IF_FAILED(window->get_CurrentNativeWindowHandle(&nativeWindow));
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
        while (!IsWindowEnabled(reinterpret_cast<HWND>(nativeWindow)) && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds{20});
        REQUIRE(IsWindowEnabled(reinterpret_cast<HWND>(nativeWindow)));
        REQUIRE(waitForEnabledControl(automation, window, L"ExportDiagnostics"));
        REQUIRE(std::filesystem::is_regular_file(destination));
        std::ifstream exported{destination, std::ios::binary};
        const std::string actual{std::istreambuf_iterator<char>{exported}, std::istreambuf_iterator<char>{}};
        REQUIRE_FALSE(actual.empty());
        CHECK(actual == expected);
        const auto warning = findControl(automation, window, L"DiagnosticLogError");
        if (warning)
        {
            BOOL offscreen{};
            THROW_IF_FAILED(warning->get_CurrentIsOffscreen(&offscreen));
            CHECK(offscreen); // The write alone is not a completed provider export.
        }
        for (const auto &file : physicalLogFolder.GetFilesAsync().get())
            if (std::wstring_view{file.Name()}.starts_with(L"diagnostic-"))
            {
                const auto before = std::find_if(records.begin(), records.end(), [&](const auto &record) {
                    return record.first == std::wstring{file.Name()};
                });
                REQUIRE(before != records.end());
                CHECK(winrt::to_string(winrt::Windows::Storage::FileIO::ReadTextAsync(file).get()) == before->second);
            }
    });
}

TEST_CASE("A real picked folder reaches a non-destructive eligible review",
          "[presentation][automation][folder-review][native-picker][operator-assisted][ui-automation]")
{
    // Native UIA opens/focuses the actual picker; real keyboard input supplies
    // only the owned folder. It then drives Analyze without a production hook.
    // This mixed-input case does not claim an entirely keyboard-only workflow.
    // The same independent review and unchanged-source oracle is preserved.
    jpg_spinner::test_support::requireApprovedInteractiveLane();
    // Qualify ordinary user files separately from application-data locations.
    // Keep this exclusively owned JPEG outside %LOCALAPPDATA%/Temp so an
    // app-data/provider restriction cannot invalidate the positive fixture.
    const jpg_spinner::test_support::TemporaryDirectory fixture{std::filesystem::current_path() / L"artifacts/tests"};
    const auto selected = fixture.directoryPath() / L"Selected";
    std::filesystem::create_directory(selected);
    const auto source = selected / L"orientation-5.jpg";
    const auto bytes = jpg_spinner::test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
        jpg_spinner::test_support::JpegChromaSubsampling::ycbcr444, 5);
    {
        std::ofstream file{source, std::ios::binary};
        file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        file.close();
        REQUIRE(file.good());
    }
    // Preserve long-path coverage without a 64 KiB stack allocation.
    std::vector<wchar_t> executablePath(32768);
    const auto length = GetModuleFileNameW(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
    REQUIRE(length != 0);
    REQUIRE(length < executablePath.size());
    const auto applicationDirectory =
        std::filesystem::path{executablePath.data()}.parent_path().parent_path() / L"JpgSpinner.App";
    std::cout << "OWNED_FOLDER_REVIEW_SOURCE: " << winrt::to_string(selected.native()) << std::endl;
    jpg_spinner::test_support::withIsolatedShellWindow(applicationDirectory, [&](IUIAutomation2 *automation,
                                                                                 IUIAutomationElement *window) {
        const auto picker = findControl(automation, window, L"SourceFolderPicker");
        REQUIRE(picker);
        THROW_IF_FAILED(picker->SetFocus()); // Establish the keyboard scenario's initial focus, not a folder grant.
        awaitKeyboardSourceFolderGrant(automation, window, selected);
        const auto analyze = waitForEnabledControl(automation, window, L"AnalyzeSelectedSource");
        invokeControl(analyze.get());
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{15};
        UIA_HWND nativeWindow{};
        THROW_IF_FAILED(window->get_CurrentNativeWindowHandle(&nativeWindow));
        bool reviewVisible{};
        do
        {
            // The HWND-owned system picker disables its owner. Do not walk the
            // inactive XAML provider while that modal is serving user input:
            // a provider timeout there is a harness failure, not failed analysis.
            if (IsWindowEnabled(reinterpret_cast<HWND>(nativeWindow)))
            {
                if (const auto review = findControl(automation, window, L"ReviewRegion"))
                {
                    BOOL offscreen{};
                    THROW_IF_FAILED(review->get_CurrentIsOffscreen(&offscreen));
                    reviewVisible = !offscreen;
                }
            }
            if (!reviewVisible)
                std::this_thread::sleep_for(std::chrono::milliseconds{100});
        } while (!reviewVisible && std::chrono::steady_clock::now() < deadline);
        INFO("The native picker and Analyze must reach the observable review; no folder test hook is supplied.");
        REQUIRE(reviewVisible);
        const auto count = findControl(automation, window, L"EligibleFileCount");
        REQUIRE(count);
        wil::unique_bstr value;
        THROW_IF_FAILED(count->get_CurrentName(value.put()));
        REQUIRE(std::wstring_view{value.get(), SysStringLen(value.get())} == L"1");
        const auto rows = findControl(automation, window, L"ProcessingRows");
        REQUIRE(rows);
        wil::unique_variant itemType;
        itemType.vt = VT_I4;
        itemType.lVal = UIA_ListItemControlTypeId;
        wil::com_ptr<IUIAutomationCondition> itemCondition;
        THROW_IF_FAILED(automation->CreatePropertyCondition(UIA_ControlTypePropertyId, itemType, itemCondition.put()));
        wil::com_ptr<IUIAutomationElementArray> items;
        THROW_IF_FAILED(rows->FindAll(TreeScope_Descendants, itemCondition.get(), items.put()));
        int itemCount{};
        THROW_IF_FAILED(items->get_Length(&itemCount));
        REQUIRE(itemCount == 1); // A correct total alone does not prove that review rows rendered.
        requireControl(automation, window, L"StartProcessing", UIA_ButtonControlTypeId, true);
        THROW_IF_FAILED(picker->SetFocus());
        const auto reopenedPicker = openOwnedStoragePicker(automation, window);
        const bool pickerWasOpened = !IsWindowEnabled(reinterpret_cast<HWND>(nativeWindow));
        const auto cancel = findControl(automation, reopenedPicker.get(), std::to_wstring(IDCANCEL).c_str());
        invokeControl(cancel.get());
        const auto cancellationDeadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
        bool pickerWasClosed{};
        do
        {
            const bool ownerEnabled = IsWindowEnabled(reinterpret_cast<HWND>(nativeWindow));
            pickerWasClosed = pickerWasOpened && ownerEnabled;
            if (!pickerWasClosed)
                std::this_thread::sleep_for(std::chrono::milliseconds{100});
        } while (!pickerWasClosed && std::chrono::steady_clock::now() < cancellationDeadline);
        REQUIRE(pickerWasOpened);
        REQUIRE(pickerWasClosed);
        // Native owner re-enabling precedes completion of the awaiting handler.
        // Wait for its observable stable gate, not an arbitrary settling delay.
        const auto resumeDeadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
        BOOL startEnabled{};
        const auto start = findControl(automation, window, L"StartProcessing");
        REQUIRE(start);
        do
        {
            THROW_IF_FAILED(start->get_CurrentIsEnabled(&startEnabled));
            if (!startEnabled)
                std::this_thread::sleep_for(std::chrono::milliseconds{100});
        } while (!startEnabled && std::chrono::steady_clock::now() < resumeDeadline);
        REQUIRE(startEnabled);
        // Cancellation must keep the existing review, not merely its folder label.
        const auto retainedReview = findControl(automation, window, L"ReviewRegion");
        REQUIRE(retainedReview);
        BOOL reviewOffscreen{};
        THROW_IF_FAILED(retainedReview->get_CurrentIsOffscreen(&reviewOffscreen));
        CHECK_FALSE(reviewOffscreen);
        THROW_IF_FAILED(count->get_CurrentName(value.put()));
        CHECK(std::wstring_view{value.get(), SysStringLen(value.get())} == L"1");
        const auto sourceLabel = findControl(automation, window, L"SelectedSourceFolder");
        REQUIRE(sourceLabel);
        THROW_IF_FAILED(sourceLabel->get_CurrentName(value.put()));
        CHECK(std::wstring_view{value.get(), SysStringLen(value.get())} == L"Selected");
        requireControl(automation, window, L"StartProcessing", UIA_ButtonControlTypeId, true);
    });
    std::ifstream unchanged{source, std::ios::binary};
    const std::vector<char> observed{std::istreambuf_iterator<char>{unchanged}, std::istreambuf_iterator<char>{}};
    REQUIRE(observed.size() == bytes.size());
    CHECK(std::memcmp(observed.data(), bytes.data(), bytes.size()) == 0);
}

TEST_CASE("Replacement trimming review keeps its backup readable and requires exact edge consent",
          "[presentation][automation][review-policies][native-picker][operator-assisted][ui-automation]")
{
    jpg_spinner::test_support::requireApprovedInteractiveLane();
    REQUIRE(AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(), DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2));
    const jpg_spinner::test_support::TemporaryDirectory fixture{std::filesystem::current_path() / L"artifacts/tests"};
    const auto selected = fixture.directoryPath() / L"Selected";
    std::filesystem::create_directory(selected);
    const auto source = selected / L"horizontal-mirror.jpg";
    // Independent fixture contract: 31x19, 8x8 iMCUs, Exif horizontal mirror.
    // Trimming loses seven source-right pixels and no source-bottom pixels.
    const auto bytes = jpg_spinner::test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
        jpg_spinner::test_support::JpegChromaSubsampling::ycbcr444, 2);
    {
        std::ofstream file{source, std::ios::binary};
        file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        file.close();
        REQUIRE(file.good());
    }
    // Preserve long-path coverage without a 64 KiB stack allocation.
    std::vector<wchar_t> executablePath(32768);
    const auto length = GetModuleFileNameW(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
    REQUIRE(length != 0);
    REQUIRE(length < executablePath.size());
    const auto applicationDirectory =
        std::filesystem::path{executablePath.data()}.parent_path().parent_path() / L"JpgSpinner.App";
    std::cout << "OWNED_POLICY_REVIEW_SOURCE: " << winrt::to_string(selected.native()) << std::endl;
    jpg_spinner::test_support::withIsolatedShellWindow(applicationDirectory, [&](IUIAutomation2 *automation,
                                                                                 IUIAutomationElement *window) {
        // Establish a genuinely wide, but bounded, viewport. The owned long
        // backup path must wrap without pushing other review prose offscreen.
        UIA_HWND windowHandle{};
        THROW_IF_FAILED(window->get_CurrentNativeWindowHandle(&windowHandle));
        const auto windowDpi = GetDpiForWindow(reinterpret_cast<HWND>(windowHandle));
        REQUIRE(windowDpi != 0);
        wil::com_ptr<IUIAutomationTransformPattern> transform;
        THROW_IF_FAILED(window->GetCurrentPatternAs(UIA_TransformPatternId, IID_PPV_ARGS(transform.put())));
        // Package activation can publish its HWND before the native
        // provider admits resizing. Wait for that supported operation;
        // do not retry a refused Resize or mistake it for product RED.
        const auto resizeDeadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
        BOOL canResize{};
        do
        {
            THROW_IF_FAILED(transform->get_CurrentCanResize(&canResize));
            if (!canResize)
                std::this_thread::sleep_for(std::chrono::milliseconds{10});
        } while (!canResize && std::chrono::steady_clock::now() < resizeDeadline);
        REQUIRE(canResize);
        THROW_IF_FAILED(transform->Resize(MulDiv(1100, static_cast<int>(windowDpi), 96),
                                          MulDiv(800, static_cast<int>(windowDpi), 96)));
        const auto picker = findControl(automation, window, L"SourceFolderPicker");
        REQUIRE(picker);
        THROW_IF_FAILED(picker->SetFocus());
        awaitKeyboardSourceFolderGrant(automation, window, selected);
        using namespace winrt::Microsoft::Windows::ApplicationModel::Resources;
        ResourceManager resources{ResourceLoader::GetDefaultResourceFilePath()};
        const auto text = resources.MainResourceMap().GetSubtree(L"Resources");
        const auto context = resources.CreateResourceContext();
        selectLocalizedOption(automation, window, L"OutputDisposition",
                              text.GetValue(L"Shell_ReplaceWithVerifiedBackup/Content", context).ValueAsString());
        selectLocalizedOption(automation, window, L"EdgeHandling",
                              text.GetValue(L"Shell_TrimPartialMinimumCodedUnits/Content", context).ValueAsString());
        const auto analyze = waitForEnabledControl(automation, window, L"AnalyzeSelectedSource");
        invokeControl(analyze.get());
        UIA_HWND nativeWindow{};
        THROW_IF_FAILED(window->get_CurrentNativeWindowHandle(&nativeWindow));
        const auto analysisDeadline = std::chrono::steady_clock::now() + std::chrono::seconds{15};
        bool reviewVisible{};
        do
        {
            // The native picker owns input while its app HWND is disabled.
            if (IsWindowEnabled(reinterpret_cast<HWND>(nativeWindow)))
            {
                if (const auto review = findControl(automation, window, L"ReviewRegion"))
                {
                    BOOL offscreen{};
                    THROW_IF_FAILED(review->get_CurrentIsOffscreen(&offscreen));
                    reviewVisible = !offscreen;
                }
            }
            if (!reviewVisible)
                std::this_thread::sleep_for(std::chrono::milliseconds{100});
        } while (!reviewVisible && std::chrono::steady_clock::now() < analysisDeadline);
        REQUIRE(reviewVisible);
        requireDefaultSelection(automation, window, L"OutputDisposition",
                                text.GetValue(L"Shell_ReplaceWithVerifiedBackup/Content", context).ValueAsString());
        requireDefaultSelection(automation, window, L"EdgeHandling",
                                text.GetValue(L"Shell_TrimPartialMinimumCodedUnits/Content", context).ValueAsString());
        for (const auto &[automationId, expected] :
             {std::pair{L"EligibleFileCount", L"1"}, std::pair{L"DiscardedRightEdgePixels", L"7"},
              std::pair{L"DiscardedBottomEdgePixels", L"0"}})
        {
            const auto control = findControl(automation, window, automationId);
            REQUIRE(control);
            wil::unique_bstr name;
            THROW_IF_FAILED(control->get_CurrentName(name.put()));
            CHECK(std::wstring_view{name.get(), SysStringLen(name.get())} == expected);
        }
        const auto backup = findControl(automation, window, L"BackupDestination");
        REQUIRE(backup);
        BOOL backupOffscreen{};
        THROW_IF_FAILED(backup->get_CurrentIsOffscreen(&backupOffscreen));
        CHECK_FALSE(backupOffscreen);
        wil::unique_bstr backupName;
        THROW_IF_FAILED(backup->get_CurrentName(backupName.put()));
        const std::wstring backupPath{backupName.get(), SysStringLen(backupName.get())};
        const auto viewport = findControl(automation, window, L"WorkflowContextViewport");
        REQUIRE(viewport);
        RECT viewportBounds{};
        RECT backupBounds{};
        THROW_IF_FAILED(viewport->get_CurrentBoundingRectangle(&viewportBounds));
        THROW_IF_FAILED(backup->get_CurrentBoundingRectangle(&backupBounds));
        CHECK(backupBounds.left >= viewportBounds.left);
        CHECK(backupBounds.right <= viewportBounds.right);
        // Query by localized public text, not a guessed framework fallback
        // for x:Name. Native UIA still owns text/property semantics.
        wil::unique_variant instructionName;
        instructionName.vt = VT_BSTR;
        instructionName.bstrVal =
            SysAllocString(text.GetValue(L"Shell_ReviewInstruction/Text", context).ValueAsString().c_str());
        THROW_IF_NULL_ALLOC(instructionName.bstrVal);
        wil::com_ptr<IUIAutomationCondition> instructionCondition;
        THROW_IF_FAILED(
            automation->CreatePropertyCondition(UIA_NamePropertyId, instructionName, instructionCondition.put()));
        wil::com_ptr<IUIAutomationElement> instruction;
        THROW_IF_FAILED(window->FindFirst(TreeScope_Descendants, instructionCondition.get(), instruction.put()));
        REQUIRE(instruction);
        BOOL instructionOffscreen{};
        THROW_IF_FAILED(instruction->get_CurrentIsOffscreen(&instructionOffscreen));
        CHECK_FALSE(instructionOffscreen);
        RECT instructionBounds{};
        THROW_IF_FAILED(instruction->get_CurrentBoundingRectangle(&instructionBounds));
        CHECK(instructionBounds.right > instructionBounds.left);
        CHECK(instructionBounds.bottom > instructionBounds.top);
        CHECK(instructionBounds.left >= viewportBounds.left);
        CHECK(instructionBounds.right <= viewportBounds.right);
        const auto expectedBackupParent = selected / L"JPG Spinner Backups";
        CHECK(std::filesystem::path{backupPath}.parent_path() == expectedBackupParent);
        CHECK_FALSE(std::filesystem::path{backupPath}.filename().empty());
        CHECK_FALSE(std::filesystem::exists(expectedBackupParent)); // Review must not begin a transaction.
        const auto start = findControl(automation, window, L"StartProcessing");
        REQUIRE(start);
        BOOL startEnabled{};
        THROW_IF_FAILED(start->get_CurrentIsEnabled(&startEnabled));
        REQUIRE_FALSE(startEnabled);
        requireControl(automation, window, L"AcknowledgeEdgeTrimming", UIA_ButtonControlTypeId, true);
        const auto acknowledgment = findControl(automation, window, L"AcknowledgeEdgeTrimming");
        THROW_IF_FAILED(acknowledgment->SetFocus());
        // Consent is invoked only after independently asserting the displayed
        // 7-right/0-bottom loss and disabled Start. Never invoke Start itself.
        invokeControl(acknowledgment.get());
        const auto consentDeadline = std::chrono::steady_clock::now() + std::chrono::seconds{15};
        do
        {
            THROW_IF_FAILED(start->get_CurrentIsEnabled(&startEnabled));
            if (!startEnabled)
                std::this_thread::sleep_for(std::chrono::milliseconds{100});
        } while (!startEnabled && std::chrono::steady_clock::now() < consentDeadline);
        REQUIRE(startEnabled);
        BOOL acknowledgmentEnabled{};
        THROW_IF_FAILED(acknowledgment->get_CurrentIsEnabled(&acknowledgmentEnabled));
        CHECK_FALSE(acknowledgmentEnabled);
        CHECK_FALSE(std::filesystem::exists(expectedBackupParent));
        // Consent must not make the backup destination disappear at the
        // minimum size. Exercise the same native scroller as source setup;
        // no Start invocation or transaction is permitted in this scenario.
        THROW_IF_FAILED(transform->Resize(MulDiv(360, static_cast<int>(windowDpi), 96),
                                          MulDiv(480, static_cast<int>(windowDpi), 96)));
        wil::com_ptr<IUIAutomationScrollPattern> scrolling;
        THROW_IF_FAILED(viewport->GetCurrentPatternAs(UIA_ScrollPatternId, IID_PPV_ARGS(scrolling.put())));
        CHECK(scrollControlFullyIntoViewport(backup.get(), viewport.get(), scrolling.get()));
    });
    std::ifstream unchanged{source, std::ios::binary};
    const std::vector<char> observed{std::istreambuf_iterator<char>{unchanged}, std::istreambuf_iterator<char>{}};
    REQUIRE(observed.size() == bytes.size());
    CHECK(std::memcmp(observed.data(), bytes.data(), bytes.size()) == 0);
}

TEST_CASE("The shell preserves a usable minimum size and adaptive policy layout",
          "[presentation][automation][layout][ui-automation]")
{
    // This contract mixes HWND and UIA measurements. Require physical-pixel
    // semantics before resizing, even when only this test tag is selected.
    REQUIRE(AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(), DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2));
    // Preserve long-path coverage without a 64 KiB stack allocation.
    std::vector<wchar_t> executablePath(32768);
    const auto length = GetModuleFileNameW(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
    REQUIRE(length != 0);
    REQUIRE(length < executablePath.size());
    const auto applicationDirectory =
        std::filesystem::path{executablePath.data()}.parent_path().parent_path() / L"JpgSpinner.App";
    jpg_spinner::test_support::withIsolatedShellWindow(
        applicationDirectory, [](IUIAutomation2 *automation, IUIAutomationElement *window) {
            UIA_HWND nativeWindow{};
            THROW_IF_FAILED(window->get_CurrentNativeWindowHandle(&nativeWindow));
            const auto handle = reinterpret_cast<HWND>(nativeWindow);
            const auto dpi = GetDpiForWindow(handle);
            REQUIRE(dpi != 0);
            wil::com_ptr<IUIAutomationTransformPattern> transform;
            THROW_IF_FAILED(window->GetCurrentPatternAs(UIA_TransformPatternId, IID_PPV_ARGS(transform.put())));
            BOOL canResize{};
            THROW_IF_FAILED(transform->get_CurrentCanResize(&canResize));
            REQUIRE(canResize);
            // UX contract: at least 360 by 480 effective pixels. Content
            // scrolling/reachability needs separate interaction checks.
            // Do not mistake physical UIA pixels
            // for XAML's effective-pixel adaptive thresholds on a scaled monitor.
            THROW_IF_FAILED(transform->Resize(1, 1));
            RECT minimumBounds{};
            REQUIRE(GetWindowRect(handle, &minimumBounds));
            CHECK(minimumBounds.right - minimumBounds.left >= MulDiv(360, static_cast<int>(dpi), 96));
            CHECK(minimumBounds.bottom - minimumBounds.top >= MulDiv(480, static_cast<int>(dpi), 96));
            for (const auto width : {600, 800, 1100})
            {
                INFO("Requested effective-pixel width: " << width);
                THROW_IF_FAILED(transform->Resize(MulDiv(width, static_cast<int>(dpi), 96),
                                                  MulDiv(800, static_cast<int>(dpi), 96)));
                RECT resizedBounds{};
                REQUIRE(GetWindowRect(handle, &resizedBounds));
                REQUIRE(resizedBounds.right - resizedBounds.left == MulDiv(width, static_cast<int>(dpi), 96));
                const auto traversal = findControl(automation, window, L"TraversalScope");
                const auto edge = findControl(automation, window, L"EdgeHandling");
                REQUIRE(traversal);
                REQUIRE(edge);
                RECT traversalBounds{};
                RECT edgeBounds{};
                // A completed native window resize does not flush XAML's
                // deferred arrange pass. Observe the independent UX geometry
                // within the same five-second interaction budget used above,
                // rather than asserting stale bounds or sleeping unconditionally.
                // Do not force UpdateLayout through a production test hook:
                // https://learn.microsoft.com/windows/windows-app-sdk/api/winrt/microsoft.ui.xaml.uielement.updatelayout
                const auto layoutDeadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
                bool policyLayoutObserved{};
                do
                {
                    THROW_IF_FAILED(traversal->get_CurrentBoundingRectangle(&traversalBounds));
                    THROW_IF_FAILED(edge->get_CurrentBoundingRectangle(&edgeBounds));
                    policyLayoutObserved =
                        width < 640 ? edgeBounds.left == traversalBounds.left && edgeBounds.top > traversalBounds.bottom
                                    : edgeBounds.left > traversalBounds.right;
                    if (!policyLayoutObserved)
                        std::this_thread::sleep_for(std::chrono::milliseconds{10});
                } while (!policyLayoutObserved && std::chrono::steady_clock::now() < layoutDeadline);
                if (width < 640)
                {
                    CHECK(edgeBounds.left == traversalBounds.left);
                    CHECK(edgeBounds.top > traversalBounds.bottom);
                }
                else
                    CHECK(edgeBounds.left > traversalBounds.right);
            }
        });
}
