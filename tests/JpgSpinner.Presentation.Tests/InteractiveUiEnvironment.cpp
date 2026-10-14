#include "InteractiveUiEnvironment.h"
#include "TemporaryDirectory.h"
#include <Windows.h>
#include <WtsApi32.h>
#include <UIAutomation.h>
#include <AppxPackaging.h>
#include <Shobjidl.h>
#include <Shlwapi.h>
#include <WinInet.h>
#include <appmodel.h>
#include <roapi.h>
#include <wil/com.h>
#include <wil/resource.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.Management.Deployment.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Management.Core.h>
#include <winrt/Windows.Data.Json.h>
#include <array>
#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

namespace jpg_spinner::test_support
{
namespace
{
/// Timeout-capable native UIA client; Windows owns COM/provider semantics.
wil::com_ptr<IUIAutomation2> createAutomationClient()
{
    auto automation = wil::CoCreateInstance<IUIAutomation2>(CLSID_CUIAutomation8);
    THROW_IF_FAILED(automation->put_ConnectionTimeout(3000));
    THROW_IF_FAILED(automation->put_TransactionTimeout(3000));
    return automation;
}

/// Qualification owns only this process's short-lived window. Do not use
/// SwitchDesktop, AttachThreadInput, synthetic keystrokes, or foreground-lock
/// changes to manufacture permission the test user does not possess.
class QualificationWindow final
{
  public:
    QualificationWindow()
    {
        std::promise<HWND> ready;
        auto publishedWindow = ready.get_future();
        thread_ = std::thread{[promise = std::move(ready)]() mutable {
            try
            {
                const auto apartment = wil::CoInitializeEx(COINIT_APARTMENTTHREADED);
                WNDCLASSEXW windowClass{sizeof(WNDCLASSEXW)};
                windowClass.lpfnWndProc = [](HWND window, UINT message, WPARAM word, LPARAM value) -> LRESULT {
                    if (message == WM_DESTROY)
                    {
                        PostQuitMessage(0);
                        return 0;
                    }
                    return DefWindowProcW(window, message, word, value);
                };
                windowClass.hInstance = GetModuleHandleW(nullptr);
                windowClass.lpszClassName = L"JpgSpinner.InteractiveQualification";
                THROW_LAST_ERROR_IF(RegisterClassExW(&windowClass) == 0);
                const auto unregisterClass =
                    wil::scope_exit([&] { UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance); });
                wil::unique_hwnd window{CreateWindowExW(
                    0, windowClass.lpszClassName, L"JPG Spinner interactive qualification", WS_OVERLAPPEDWINDOW,
                    CW_USEDEFAULT, CW_USEDEFAULT, 480, 180, nullptr, nullptr, windowClass.hInstance, nullptr)};
                THROW_LAST_ERROR_IF_NULL(window);
                ShowWindow(window.get(), SW_SHOW);
                UpdateWindow(window.get());
                // The documented restrictions still apply. Observe the actual
                // foreground HWND below; a successful-looking API call alone
                // cannot qualify keyboard-driven product acceptance.
                SetForegroundWindow(window.get());
                promise.set_value(window.get());
                MSG message{};
                while (GetMessageW(&message, nullptr, 0, 0) > 0)
                {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
                // WM_CLOSE already destroyed it on its owning UI thread.
                window.release();
            }
            catch (...)
            {
                try
                {
                    promise.set_exception(std::current_exception());
                }
                catch (const std::future_error &)
                { /* A published window retains its original result. */
                }
            }
        }};
        try
        {
            window_ = publishedWindow.get();
        }
        catch (...)
        {
            thread_.join();
            throw;
        }
    }
    ~QualificationWindow()
    {
        PostMessageW(window_, WM_CLOSE, 0, 0);
        thread_.join();
    }
    QualificationWindow(const QualificationWindow &) = delete;
    QualificationWindow &operator=(const QualificationWindow &) = delete;
    [[nodiscard]] HWND handle() const noexcept
    {
        return window_;
    }

  private:
    HWND window_{};
    std::thread thread_;
};

/// Copy/register/remove a UUID-isolated development package using the native
/// PackageManager consumer. No Store identity, existing package or certificate is
/// changed. Reuse the containment-checked owned fixture directory implementation.
void withIsolatedPackageFixture(
    const std::filesystem::path &applicationDirectory,
    const std::function<void(const winrt::Windows::ApplicationModel::Package &)> &useRegisteredPackage,
    const std::function<void(const std::filesystem::path &, const winrt::Windows::Data::Xml::Dom::XmlDocument &)>
        &preparePayload = {})
{
    using namespace winrt::Windows;
    if (!std::filesystem::is_regular_file(applicationDirectory / L"AppxManifest.xml"))
        throw std::runtime_error{"The application build has no package manifest."};

    auto directory = std::make_unique<TemporaryDirectory>(std::filesystem::temp_directory_path());
    const auto copyRoot = directory->directoryPath();
    for (const auto &entry : std::filesystem::recursive_directory_iterator(applicationDirectory))
    {
        const auto attributes = GetFileAttributesW(entry.path().c_str());
        THROW_LAST_ERROR_IF(attributes == INVALID_FILE_ATTRIBUTES);
        if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
            throw std::runtime_error{"A test-package build entry is a reparse point."};
    }
    std::filesystem::copy(applicationDirectory, copyRoot, std::filesystem::copy_options::recursive);
    const auto manifestFile =
        Storage::StorageFile::GetFileFromPathAsync(winrt::hstring{(copyRoot / L"AppxManifest.xml").native()}).get();
    const auto manifest = Data::Xml::Dom::XmlDocument::LoadFromFileAsync(manifestFile).get();
    const auto identities = manifest.GetElementsByTagName(L"Identity");
    if (identities.Length() != 1)
        throw std::runtime_error{"The generated package requires exactly one native Identity element."};
    const auto identity = identities.Item(0).as<Data::Xml::Dom::XmlElement>();
    // Package identity must not depend on our fixture-directory naming convention.
    // StringFromGUID2 emits 36 UUID characters inside braces. This prefix plus
    // those characters fits the SDK's 50-character package-name maximum; the
    // native manifest reader below remains responsible for schema conformance.
    GUID packageGuid{};
    THROW_IF_FAILED(CoCreateGuid(&packageGuid));
    std::array<wchar_t, 39> guidText{};
    if (StringFromGUID2(packageGuid, guidText.data(), static_cast<int>(guidText.size())) == 0)
        throw std::runtime_error{"Could not format the isolated package UUID."};
    constexpr std::wstring_view packagePrefix{L"JpgSpinner.UI."};
    static_assert(packagePrefix.size() + 36 <= PACKAGE_NAME_MAX_LENGTH);
    const winrt::hstring packageName{std::wstring{packagePrefix} + std::wstring{guidText.data() + 1, 36}};
    identity.SetAttribute(L"Name", packageName);
    const auto publisher = identity.GetAttribute(L"Publisher");
    const auto phoneIdentities = manifest.GetElementsByTagName(L"mp:PhoneIdentity");
    if (phoneIdentities.Length() != 0)
        manifest.DocumentElement().RemoveChild(phoneIdentities.Item(0));
    if (preparePayload)
        preparePayload(copyRoot, manifest);
    manifest.SaveToFileAsync(manifestFile).get();

    // Validate the actual generated manifest with its Windows consumer before
    // deployment. Do not reproduce package-name, architecture or manifest schema
    // rules in a regex, or treat successful XML parsing as package conformance.
    const auto factory = wil::CoCreateInstance<IAppxFactory>(CLSID_AppxFactory);
    wil::com_ptr<IStream> manifestStream;
    THROW_IF_FAILED(SHCreateStreamOnFileEx(manifestFile.Path().c_str(), STGM_READ | STGM_SHARE_DENY_WRITE,
                                           FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, manifestStream.put()));
    wil::com_ptr<IAppxManifestReader> reader;
    THROW_IF_FAILED(factory->CreateManifestReader(manifestStream.get(), reader.put()));
    wil::com_ptr<IAppxManifestPackageId> packageId;
    THROW_IF_FAILED(reader->GetPackageId(packageId.put()));
    wil::unique_cotaskmem_string fullName;
    THROW_IF_FAILED(packageId->GetPackageFullName(fullName.put()));
    winrt::check_hresult(HRESULT_FROM_WIN32(VerifyPackageFullName(fullName.get())));
    reader.reset();
    manifestStream.reset();
    if (!useRegisteredPackage)
    {
        if (const auto error = directory->removeOwnedDirectory())
            throw std::system_error{error, "Could not remove the owned package-validation fixture"};
        return;
    }

    Management::Deployment::PackageManager manager;
    if (manager.FindPackagesForUser(L"", packageName, publisher).First().HasCurrent())
        throw std::runtime_error{"The unique test package identity already exists; nothing was registered."};

    bool registrationAttempted{};
    try
    {
        // Windows owns file-URI escaping (including spaces, '%' and '#'). A
        // slash replacement is not a URI encoder. Unsupported/oversized paths
        // fail before attempting registration rather than changing the target.
        std::array<wchar_t, INTERNET_MAX_URL_LENGTH> manifestUri{};
        auto uriLength = static_cast<DWORD>(manifestUri.size());
        THROW_IF_FAILED(UrlCreateFromPathW(manifestFile.Path().c_str(), manifestUri.data(), &uriLength, 0));
        // Flush the recovery locator before entering the OS deployment service.
        // A supervising timeout deliberately retains these generated files;
        // neither killing this client nor an empty inventory proves quiescence.
        std::cout << "OWNED_INTERACTIVE_TEST_PACKAGE: name=" << winrt::to_string(packageName)
                  << " root=" << winrt::to_string(copyRoot.native()) << std::endl;
        registrationAttempted = true;
        const auto result = manager
                                .RegisterPackageAsync(Foundation::Uri{manifestUri.data()}, nullptr,
                                                      Management::Deployment::DeploymentOptions::DevelopmentMode)
                                .get();
        winrt::check_hresult(result.ExtendedErrorCode());
        const auto packages = manager.FindPackagesForUser(L"", packageName, publisher);
        std::size_t packageCount{};
        ApplicationModel::Package registeredPackage{nullptr};
        for (const auto &package : packages)
        {
            ++packageCount;
            registeredPackage = package;
        }
        if (packageCount != 1 || registeredPackage.Id().FullName() != fullName.get())
            throw std::runtime_error{"Registration did not produce exactly the manifest's isolated package."};
        // Keep the native registration and owned working copy alive for the
        // consumer, including assertion exceptions. Existing package identities
        // are never supplied to the callback or removed by this fixture.
        useRegisteredPackage(registeredPackage);
        const auto removal = manager.RemovePackageAsync(registeredPackage.Id().FullName()).get();
        winrt::check_hresult(removal.ExtendedErrorCode());
        if (manager.FindPackagesForUser(L"", packageName, publisher).First().HasCurrent())
            throw std::runtime_error{"Test-package registration/removal did not reach the required observed state."};
    }
    catch (...)
    {
        // Registration can partially succeed before returning an error. Never
        // delete a registered package's working files; retain the owned path if
        // bounded native cleanup fails so the next operator can reconcile it.
        if (registrationAttempted)
        {
            try
            {
                for (const auto &package : manager.FindPackagesForUser(L"", packageName, publisher))
                    winrt::check_hresult(manager.RemovePackageAsync(package.Id().FullName()).get().ExtendedErrorCode());
                if (manager.FindPackagesForUser(L"", packageName, publisher).First().HasCurrent())
                    throw std::runtime_error{"Test-package registration remains present."};
            }
            catch (...)
            {
                std::cerr << "RETAINED_OWNED_TEST_PACKAGE: " << winrt::to_string(copyRoot.native()) << '\n';
                static_cast<void>(directory.release());
            }
        }
        throw;
    }
    if (const auto error = directory->removeOwnedDirectory())
        throw std::system_error{error, "Could not remove the owned test-package fixture"};
}
} // namespace

void requireApprovedInteractiveLane()
{
    std::array<wchar_t, 64> lane{};
    const auto length =
        GetEnvironmentVariableW(L"JPG_SPINNER_INTERACTIVE_TEST_LANE", lane.data(), static_cast<DWORD>(lane.size()));
    if (length == 0 || length >= lane.size() ||
        (std::wstring_view{lane.data()} != L"IsolatedVirtualMachine" &&
         std::wstring_view{lane.data()} != L"DedicatedSelfHosted" &&
         std::wstring_view{lane.data()} != L"OperatorApprovedLocalDesktop"))
        throw std::runtime_error{"No accepted interactive test lane was selected."};
}

void runPackagedSourceAccessTests(const std::filesystem::path &applicationDirectory,
                                  const std::filesystem::path &testExecutablePath, const bool requireRealFolderPicker)
{
    using namespace winrt::Windows;
    requireApprovedInteractiveLane();
    const auto apartment = wil::RoInitialize(RO_INIT_MULTITHREADED);
    if (requireRealFolderPicker)
    {
        // The controlling host, not the restricted worker, qualifies UIA and
        // the ordinary desktop. AppContainer need not automate other windows.
        const auto refusal = interactiveDesktopRefusal(observeInteractiveDesktop());
        if (!refusal.empty())
            throw std::runtime_error{refusal};
    }
    const TemporaryDirectory sourceFixture{std::filesystem::current_path() / L"artifacts/tests"};
    const auto siblingPath = sourceFixture.directoryPath() / L"Sibling";
    std::filesystem::create_directory(siblingPath);
    // These protected objects actually exist. A "not found" response cannot
    // stand in for a successful access-denial test in the restricted worker.
    const auto sibling = Storage::StorageFolder::GetFolderFromPathAsync(siblingPath.native()).get();
    Storage::FileIO::WriteTextAsync(sibling.CreateFileAsync(L"sentinel.txt").get(), L"owned qualification input").get();
    const auto selected = sibling.CreateFolderAsync(L"Selected").get();
    Storage::FileIO::WriteBytesAsync(selected.CreateFileAsync(L"sentinel.jpg").get(), {0x61, 0x62, 0x63}).get();
    if (requireRealFolderPicker)
    {
        // This real escaping link detects a provider that silently follows a
        // reparse point instead of returning its native item attributes.
        const auto escape = std::filesystem::path{selected.Path().c_str()} / L"escape";
        THROW_LAST_ERROR_IF(
            !CreateSymbolicLinkW(escape.c_str(), siblingPath.c_str(),
                                 SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE));
        if ((GetFileAttributesW(escape.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
            throw std::runtime_error{"The owned source qualification link is not a native reparse point."};
        std::cout << "OWNED_ACCESS_PICKER_SOURCE: " << winrt::to_string(selected.Path()) << std::endl;
    }
    withIsolatedPackageFixture(
        applicationDirectory,
        [&](const ApplicationModel::Package &package) {
            const auto local =
                Management::Core::ApplicationDataManager::CreateForPackageFamily(package.Id().FamilyName())
                    .LocalFolder();
            Data::Json::JsonObject request;
            request.Insert(L"parentFolderPath",
                           Data::Json::JsonValue::CreateStringValue(sourceFixture.directoryPath().native()));
            request.Insert(L"siblingFolderPath", Data::Json::JsonValue::CreateStringValue(siblingPath.native()));
            request.Insert(L"selectedFolderPath", Data::Json::JsonValue::CreateStringValue(selected.Path()));
            Storage::FileIO::WriteTextAsync(local.CreateFileAsync(L"SourceAccessRequest.json").get(),
                                            request.Stringify())
                .get();
            const auto entries = package.GetAppListEntriesAsync().get();
            if (entries.Size() != 1)
                throw std::runtime_error{"The native access-test package requires exactly one app entry."};
            const auto activation =
                wil::CoCreateInstance<IApplicationActivationManager>(CLSID_ApplicationActivationManager);
            DWORD processId{};
            THROW_IF_FAILED(activation->ActivateApplication(entries.GetAt(0).AppUserModelId().c_str(),
                                                            requireRealFolderPicker ? L"--run-packaged-picker-tests"
                                                                                    : L"--run-packaged-access-tests",
                                                            AO_NOERRORUI, &processId));
            std::cout << "OWNED_PACKAGED_ACCESS_ACTIVATION: package=" << winrt::to_string(package.Id().FullName())
                      << " pid=" << processId << std::endl;
            wil::unique_handle process{
                OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE | PROCESS_TERMINATE, FALSE, processId)};
            THROW_LAST_ERROR_IF_NULL(process);
            std::array<wchar_t, PACKAGE_FULL_NAME_MAX_LENGTH + 1> processPackage{};
            auto length = static_cast<UINT32>(processPackage.size());
            winrt::check_hresult(HRESULT_FROM_WIN32(GetPackageFullName(process.get(), &length, processPackage.data())));
            if (package.Id().FullName() != processPackage.data())
                throw std::runtime_error{"Activation returned a process outside the native access-test package."};
            const auto stopOwnedWorker = wil::scope_exit([&] {
                if (WaitForSingleObject(process.get(), 0) == WAIT_TIMEOUT)
                    if (!TerminateProcess(process.get(), 124) ||
                        WaitForSingleObject(process.get(), 5000) != WAIT_OBJECT_0)
                        std::cerr << "RETAINED_OWNED_ACCESS_WORKER: pid=" << processId << std::endl;
            });
            // Real keyboard entry has a separately bounded operator interval;
            // ordinary windowless controls retain their existing 60-second limit.
            if (WaitForSingleObject(process.get(), requireRealFolderPicker ? 180000 : 60000) != WAIT_OBJECT_0)
                throw std::runtime_error{"The native packaged access-test worker exceeded its bounded interval."};
            DWORD exitCode{};
            THROW_IF_WIN32_BOOL_FALSE(GetExitCodeProcess(process.get(), &exitCode));
            const auto report =
                Storage::FileIO::ReadTextAsync(local.GetFileAsync(L"SourceAccess.junit.xml").get()).get();
            std::cout << "PACKAGED_SOURCE_ACCESS_JUNIT_BEGIN\n"
                      << winrt::to_string(report) << "\nPACKAGED_SOURCE_ACCESS_JUNIT_END" << std::endl;
            Data::Xml::Dom::XmlDocument results;
            results.LoadXml(report); // Windows owns XML parsing, not a home-grown report grammar.
            // The noninteractive worker also qualifies startup restoration of
            // an actual retained grant. Its passing report must include all
            // four contracts; exit zero alone is not proof of discovery.
            const std::uint32_t expectedCases = requireRealFolderPicker ? 1 : 4;
            if (results.GetElementsByTagName(L"testcase").Length() != expectedCases || exitCode != 0)
                throw std::runtime_error{
                    "Packaged source-access tests failed or the expected contracts did not execute."};
        },
        [&](const std::filesystem::path &copyRoot, const Data::Xml::Dom::XmlDocument &manifest) {
            // Only the test package consumes this native test executable. The
            // shipping package never references Catch2 or bootstrap test assets.
            std::filesystem::copy_file(testExecutablePath, copyRoot / testExecutablePath.filename());
            const auto bootstrap = testExecutablePath.parent_path() / L"Microsoft.WindowsAppRuntime.Bootstrap.dll";
            if (std::filesystem::is_regular_file(bootstrap))
                std::filesystem::copy_file(bootstrap, copyRoot / bootstrap.filename());
            const auto applications = manifest.GetElementsByTagName(L"Application");
            if (applications.Length() != 1)
                throw std::runtime_error{"The access-test payload requires one application."};
            applications.Item(0).as<Data::Xml::Dom::XmlElement>().SetAttribute(L"Executable",
                                                                               testExecutablePath.filename().native());
        });
}

winrt::Windows::Storage::StorageFolder pickFolderForAccessQualification()
{
    QualificationWindow owner;
    winrt::Windows::Storage::Pickers::FolderPicker picker;
    picker.FileTypeFilter().Append(L"*");
    winrt::check_hresult(picker.as<IInitializeWithWindow>()->Initialize(owner.handle()));
    return picker.PickSingleFolderAsync().get();
}

void withIsolatedShellWindow(const std::filesystem::path &applicationDirectory,
                             const std::function<void(IUIAutomation2 *, IUIAutomationElement *)> &assertWindowContract)
{
    requireApprovedInteractiveLane();
    if (!assertWindowContract)
        throw std::invalid_argument{"An isolated shell fixture requires an assertion callback."};
    // UIA remains on this windowless MTA for its entire lifetime, including
    // close/teardown. Do not marshal its cross-bitness helpers to a UI thread.
    const auto apartment = wil::RoInitialize(RO_INIT_MULTITHREADED);
    const auto refusal = interactiveDesktopRefusal(observeInteractiveDesktop());
    if (!refusal.empty())
        throw std::runtime_error{"INTERACTIVE_ENVIRONMENT_NOT_QUALIFIED: " + refusal};
    withIsolatedPackageFixture(applicationDirectory, [&](const winrt::Windows::ApplicationModel::Package &package) {
        const auto entries = package.GetAppListEntriesAsync().get();
        if (entries.Size() != 1)
            throw std::runtime_error{"The isolated shell package must expose exactly one native app entry."};
        // Windows supplies the AUMID. No executable-path launch, synthesized
        // family-name string, debug-mode grant or NOSPLASH flag is involved.
        const auto activation =
            wil::CoCreateInstance<IApplicationActivationManager>(CLSID_ApplicationActivationManager);
        DWORD processId{};
        THROW_IF_FAILED(activation->ActivateApplication(entries.GetAt(0).AppUserModelId().c_str(), nullptr,
                                                        AO_NOERRORUI, &processId));
        std::cout << "OWNED_INTERACTIVE_TEST_ACTIVATION: package=" << winrt::to_string(package.Id().FullName())
                  << " pid=" << processId << std::endl;
        wil::unique_handle process{
            OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE | PROCESS_TERMINATE, FALSE, processId)};
        THROW_LAST_ERROR_IF_NULL(process);
        std::array<wchar_t, PACKAGE_FULL_NAME_MAX_LENGTH + 1> processPackage{};
        auto processPackageLength = static_cast<UINT32>(processPackage.size());
        winrt::check_hresult(
            HRESULT_FROM_WIN32(GetPackageFullName(process.get(), &processPackageLength, processPackage.data())));
        if (package.Id().FullName() != processPackage.data())
            throw std::runtime_error{
                "Activation returned a process outside the isolated package; refusing attachment."};
        // The retained native handle binds cleanup to the verified process
        // object, not a later process that happens to reuse its numeric PID.
        // Forced teardown is a test failure/recovery step, never clean shutdown.
        const auto stopOwnedProcessOnFailure = wil::scope_exit([&] {
            if (WaitForSingleObject(process.get(), 0) == WAIT_TIMEOUT)
            {
                if (!TerminateProcess(process.get(), 124) || WaitForSingleObject(process.get(), 5000) != WAIT_OBJECT_0)
                    std::cerr << "RETAINED_OWNED_TEST_PROCESS: pid=" << processId << std::endl;
            }
        });
        auto automation = createAutomationClient();
        wil::com_ptr<IUIAutomationElement> desktop;
        THROW_IF_FAILED(automation->GetRootElement(desktop.put()));
        VARIANT processValue{};
        processValue.vt = VT_I4;
        processValue.lVal = static_cast<LONG>(processId);
        wil::com_ptr<IUIAutomationCondition> processCondition;
        THROW_IF_FAILED(
            automation->CreatePropertyCondition(UIA_ProcessIdPropertyId, processValue, processCondition.put()));
        VARIANT windowValue{};
        windowValue.vt = VT_I4;
        windowValue.lVal = UIA_WindowControlTypeId;
        wil::com_ptr<IUIAutomationCondition> windowCondition;
        THROW_IF_FAILED(
            automation->CreatePropertyCondition(UIA_ControlTypePropertyId, windowValue, windowCondition.put()));
        wil::com_ptr<IUIAutomationCondition> ownedWindowCondition;
        THROW_IF_FAILED(
            automation->CreateAndCondition(processCondition.get(), windowCondition.get(), ownedWindowCondition.put()));
        wil::com_ptr<IUIAutomationElement> window;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{15};
        do
        {
            // Root children only: never walk unrelated users' desktop subtrees.
            THROW_IF_FAILED(desktop->FindFirst(TreeScope_Children, ownedWindowCondition.get(), window.put()));
            if (window)
                break;
            if (WaitForSingleObject(process.get(), 100) != WAIT_TIMEOUT)
                throw std::runtime_error{"The isolated shell exited before exposing its native window."};
        } while (std::chrono::steady_clock::now() < deadline);
        if (!window)
            throw std::runtime_error{"The isolated shell did not expose its window before the startup deadline."};
        assertWindowContract(automation.get(), window.get());
        wil::com_ptr<IUIAutomationWindowPattern> windowPattern;
        THROW_IF_FAILED(window->GetCurrentPatternAs(UIA_WindowPatternId, IID_PPV_ARGS(windowPattern.put())));
        THROW_IF_FAILED(windowPattern->Close());
        if (WaitForSingleObject(process.get(), 5000) != WAIT_OBJECT_0)
            throw std::runtime_error{"The isolated shell did not shut down cleanly after native WindowPattern.Close."};
    });
}

std::string interactiveDesktopRefusal(const InteractiveDesktopObservation &observation)
{
    if (!observation.isNonServiceSession)
        return "Service/session-zero execution is not interactive.";
    if (!observation.isSessionActive)
        return "The current session is not active.";
    if (!observation.isSessionUnlocked)
        return "The current session is locked or its lock state is unknown.";
    if (!observation.hasVisibleWindowStation)
        return "The process window station is not visible.";
    if (!observation.hasDefaultInputDesktop)
        return "The default interactive input desktop could not be confirmed.";
    if (!observation.hasUiAutomation)
        return "The SDK UI Automation client cannot read the desktop.";
    return {};
}

InteractiveDesktopObservation observeInteractiveDesktop()
{
    InteractiveDesktopObservation observation;
    DWORD sessionId{};
    observation.isNonServiceSession = ProcessIdToSessionId(GetCurrentProcessId(), &sessionId) && sessionId != 0;
    if (!observation.isNonServiceSession)
        return observation;
    LPWSTR sessionInformation{};
    DWORD informationBytes{};
    if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionId, WTSSessionInfoEx, &sessionInformation,
                                    &informationBytes))
    {
        wil::unique_wtsmem_ptr<WTSINFOEXW> information{reinterpret_cast<WTSINFOEXW *>(sessionInformation)};
        if (informationBytes >= sizeof(WTSINFOEXW) && information->Level == 1)
        {
            observation.isSessionActive = information->Data.WTSInfoExLevel1.SessionState == WTSActive;
            observation.isSessionUnlocked = information->Data.WTSInfoExLevel1.SessionFlags == WTS_SESSIONSTATE_UNLOCK;
        }
    }
    USEROBJECTFLAGS stationFlags{};
    observation.hasVisibleWindowStation =
        GetUserObjectInformationW(GetProcessWindowStation(), UOI_FLAGS, &stationFlags, sizeof(stationFlags), nullptr) &&
        (stationFlags.dwFlags & WSF_VISIBLE) != 0;
    wil::unique_hdesk inputDesktop{OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS)};
    std::array<wchar_t, 256> desktopName{};
    // False also covers denied/failed observation, not just a different name.
    // Admission needs positive evidence; its refusal must preserve uncertainty.
    observation.hasDefaultInputDesktop = inputDesktop &&
                                         GetUserObjectInformationW(inputDesktop.get(), UOI_NAME, desktopName.data(),
                                                                   static_cast<DWORD>(sizeof(desktopName)), nullptr) &&
                                         _wcsicmp(desktopName.data(), L"Default") == 0;
    try
    {
        const auto automation = createAutomationClient();
        wil::com_ptr<IUIAutomationElement> desktop;
        THROW_IF_FAILED(automation->GetRootElement(desktop.put()));
        observation.hasUiAutomation = static_cast<bool>(desktop);
    }
    catch (const wil::ResultException &)
    { /* Missing native capability stays an explicit refusal fact. */
    }
    return observation;
}

int qualifyInteractiveUiEnvironment(const std::filesystem::path &applicationDirectory)
{
    try
    {
        // Direct native invocations cannot evade the script's lane selection.
        // This declaration is not itself evidence of physical host isolation.
        requireApprovedInteractiveLane();
        const auto apartment = wil::RoInitialize(RO_INIT_MULTITHREADED);
        const auto refusal = interactiveDesktopRefusal(observeInteractiveDesktop());
        if (!refusal.empty())
            throw std::runtime_error{refusal};
        {
            QualificationWindow window;
            if (GetForegroundWindow() != window.handle())
                std::cout << "FOREGROUND_QUALIFICATION_WAITING: activate only the owned JPG Spinner interactive "
                             "qualification window within 30 seconds; pid="
                          << GetCurrentProcessId() << std::endl;
            // SetForegroundWindow may legitimately refuse a background process
            // while the operator is in another app. Give ordinary operator or
            // Computer Use activation a bounded opportunity, then require the
            // actual native foreground HWND. Never force an input queue, change
            // foreground-lock settings, or infer success from the API call alone.
            // https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-setforegroundwindow
            const auto foregroundDeadline = std::chrono::steady_clock::now() + std::chrono::seconds{30};
            while (GetForegroundWindow() != window.handle() && std::chrono::steady_clock::now() < foregroundDeadline)
                std::this_thread::sleep_for(std::chrono::milliseconds{100});
            if (GetForegroundWindow() != window.handle())
                throw std::runtime_error{
                    "The owned qualification window did not receive foreground focus within 30 seconds."};
            auto automation = createAutomationClient();
            wil::com_ptr<IUIAutomationElement> element;
            THROW_IF_FAILED(automation->ElementFromHandle(window.handle(), element.put()));
            int processId{};
            THROW_IF_FAILED(element->get_CurrentProcessId(&processId));
            if (static_cast<DWORD>(processId) != GetCurrentProcessId())
                throw std::runtime_error{"UI Automation did not attach to the owned qualification window."};
        }
        withIsolatedPackageFixture(applicationDirectory, [](const auto &) {});
        std::cout << "INTERACTIVE_ENVIRONMENT_QUALIFIED: native desktop, foreground, UIA and isolated registration "
                     "probes passed.\n";
        return 0;
    }
    catch (const winrt::hresult_error &error)
    {
        std::cerr << "INTERACTIVE_ENVIRONMENT_NOT_QUALIFIED: " << winrt::to_string(error.message()) << '\n';
    }
    catch (const std::exception &error)
    {
        std::cerr << "INTERACTIVE_ENVIRONMENT_NOT_QUALIFIED: " << error.what() << '\n';
    }
    return 78;
}

void validateIsolatedTestPackageManifest(const std::filesystem::path &applicationDirectory)
{
    withIsolatedPackageFixture(applicationDirectory, {});
}
} // namespace jpg_spinner::test_support
