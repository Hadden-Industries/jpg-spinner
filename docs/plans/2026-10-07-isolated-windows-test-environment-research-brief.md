# Research brief: isolated Windows testing environment for JPG Spinner

Date: 2026-10-07

Purpose: research handoff for a deployment-only Windows UI test environment

Status: requirements brief; not authorization to provision infrastructure or evidence of Task 13 completion

## Objective and scope

Research the smallest reliable environment for deploying and testing the modernized JPG Spinner Windows application
in an isolated Windows virtual machine, using the owner's existing Oracle VirtualBox 7.2.20 installation.

The immediate purpose is Task 13 qualification: application activation, native UI Automation, folder picking,
keyboard accessibility, presentation behavior, and shutdown. All application and test binaries will be built
outside the VM. The guest is a deployment-and-test environment, not a development environment.

Research and propose the setup. Do not write application or infrastructure code, provision the VM, install
software, or change the host as part of this assignment. Runtime installation belongs in the proposed setup,
but executing that setup requires separate authorization.

## Public repository and source references

The public repository is [Hadden-Industries/jpg-spinner](https://github.com/Hadden-Industries/jpg-spinner).
Its default branch is [main](https://github.com/Hadden-Industries/jpg-spinner/tree/main).

The links below are pinned to the published commit
[`7ddad0bd4e83cc39998069f9f2259a4d2c16d4dc`](https://github.com/Hadden-Industries/jpg-spinner/commit/7ddad0bd4e83cc39998069f9f2259a4d2c16d4dc),
verified on the document date. They remain reproducible if `main` advances. They are published context,
not a claim that the uncommitted Task 13 candidate is available on GitHub.

| Reference | What to inspect |
| --- | --- |
| [Modernization plan](https://github.com/Hadden-Industries/jpg-spinner/blob/7ddad0bd4e83cc39998069f9f2259a4d2c16d4dc/docs/plans/2026-08-30-jpg-spinner-modernization.md#task-13-implement-the-winui-3-shell-and-folder-review-workflow) | Task 13 requirements and the required isolated interactive lane. |
| [Modernization design](https://github.com/Hadden-Industries/jpg-spinner/blob/7ddad0bd4e83cc39998069f9f2259a4d2c16d4dc/docs/specs/2026-08-30-jpg-spinner-modernization-design.md) | Application architecture, processing policy, storage boundaries, and acceptance intent. |
| [Application source](https://github.com/Hadden-Industries/jpg-spinner/tree/7ddad0bd4e83cc39998069f9f2259a4d2c16d4dc/src/JpgSpinner.App) | WinUI application structure. |
| [Main window XAML](https://github.com/Hadden-Industries/jpg-spinner/blob/7ddad0bd4e83cc39998069f9f2259a4d2c16d4dc/src/JpgSpinner.App/MainWindow.xaml) | Published shell context; local Task 13 changes are not included. |
| [Source package manifest](https://github.com/Hadden-Industries/jpg-spinner/blob/7ddad0bd4e83cc39998069f9f2259a4d2c16d4dc/src/JpgSpinner.App/Package.appxmanifest) | Package identity and capabilities; use the supplied generated manifest for actual runtime dependencies. |
| [Central package versions](https://github.com/Hadden-Industries/jpg-spinner/blob/7ddad0bd4e83cc39998069f9f2259a4d2c16d4dc/Directory.Packages.props) | Build dependency context, not a guest software installation checklist. |
| [Test-suite launcher](https://github.com/Hadden-Industries/jpg-spinner/blob/7ddad0bd4e83cc39998069f9f2259a4d2c16d4dc/scripts/Invoke-TestSuite.ps1) | Published launcher baseline; the local candidate adds interactive preflight and bounded execution. |
| [Presentation test project](https://github.com/Hadden-Industries/jpg-spinner/tree/7ddad0bd4e83cc39998069f9f2259a4d2c16d4dc/tests/JpgSpinner.Presentation.Tests) | Published project context, not the complete local interactive tests. |
| [Windows Storage module](https://github.com/Hadden-Industries/jpg-spinner/tree/7ddad0bd4e83cc39998069f9f2259a4d2c16d4dc/src/JpgSpinner.WindowsStorage) | Filesystem and transaction behavior that requires guest-local storage. |

The following candidate files are local and unpublished at this checkpoint. They must be supplied separately
or linked after publication; do not assume the published launcher contains their behavior:

- `scripts/Test-InteractiveUiEnvironment.ps1`
- The modified `scripts/Invoke-TestSuite.ps1`
- `tests/JpgSpinner.Presentation.Tests/InteractiveUiEnvironment.cpp` and `.h`
- `tests/JpgSpinner.Presentation.Tests/PresentationTestMain.cpp`
- `tests/JpgSpinner.Presentation.Tests/WindowStateAutomationTests.cpp`

The generated `AppxManifest.xml` and complete binary outputs must likewise come from the selected build candidate,
not be reconstructed from a published source snapshot. Source references are for research; the VM needs no checkout.

## 1. Build-host and test-guest boundary

The guest must receive a complete, immutable, versioned binary test bundle containing:

- The application's deployment output: executable, generated manifest, assets, resources, and dependencies.
- The prebuilt native presentation-test executable and its runtime dependencies and resources.
- The existing test-launch and environment-preflight scripts.
- Synthetic test fixtures and any consumer-required auxiliary files.
- Artifact hashes and provenance identifying the exact source candidate, architecture, and configuration.

A commit hash alone does not identify an uncommitted candidate. Define how its exact content identity will be retained.
Keep diagnostic symbols available outside the guest or include them if needed; symbol storage does not require a compiler.

The guest must not require Visual Studio, MSBuild, compiler toolchains, Windows SDK build tools, CMake, vcpkg,
NuGet restoration, Git, or the source tree. Installing approved runtime or testing utilities is not compilation.
Deploying only the app is insufficient: automated qualification also requires the prebuilt test host.

Preserve the relative directory structure expected by the current candidate harness:

```text
<bundle-root>/
    scripts/
        Invoke-TestSuite.ps1
        Test-InteractiveUiEnvironment.ps1
    artifacts/
        bin/
            <platform>/<configuration>/
                JpgSpinner.App/                 complete application output
                JpgSpinner.Presentation.Tests/  complete native test output
        TestResults/
            <architecture>/<configuration>/    retained test results
```

The platform directory is `x64`, `Win32`, or `ARM64`; the corresponding architecture parameter is `x64`, `x86`, or
`ARM64`. The script's existing `RepositoryRoot` parameter can identify this bundle root; it does not require a Git
checkout. Preserve the application/test sibling relationship used by the native fixture.

## 2. VM platform and initial sizing

Research a supported, licensed, patched Windows 11 x64 guest. Qualify x64 artifacts first. Later x86 qualification
can use the same x64 guest with architecture-appropriate dependencies. ARM64 qualification is a separate lane.

These are initial recommendations, not measured minimums:

| Component | Initial recommendation |
| --- | --- |
| CPU | 4 virtual CPUs |
| Memory | 8 GB |
| Storage | 100 GB dynamically allocated virtual disk |
| Firmware | UEFI, Secure Boot, and virtual TPM 2.0 |
| Display | One stable virtual display; initially 1920 x 1080 at 100% scaling |

Confirm host capacity and adjust from observed deployment/test behavior. A build-sized VM is unnecessary.
Use VirtualBox terminology, not Hyper-V generation terminology.

Investigate the appropriate Windows graphics controller, matching Guest Additions, and reliable WinUI rendering.
GPU passthrough is not an established requirement. VM graphics acceptance does not establish physical-GPU acceptance.
Keep resolution stable during each run rather than allowing automatic resizing to change the test conditions.
See Oracle's [VM settings](https://docs.oracle.com/en/virtualization/virtualbox/7.2/user/working-with-vms.html) and
[Guest Additions guidance](https://docs.oracle.com/en/virtualization/virtualbox/7.2/user/guestadditions.html).

## 3. Guest storage, isolation, and reset

Application fixtures, output images, journals, and staging files must reside on a guest-local NTFS volume.
Do not substitute VirtualBox shared folders when qualifying transactional filesystem behavior: Oracle implements
Windows shared folders through a pseudo-network redirector.
See [Oracle's shared-folder documentation](https://docs.oracle.com/en/virtualization/virtualbox/7.2/user/guestadditions.html#sharedfolders).

Research a controlled artifact-transfer method. Use synthetic data only and avoid exposing personal host folders,
photographs, credentials, or production application state. Disable unnecessary shared clipboard, drag-and-drop,
and folder sharing unless a specific approved transfer workflow needs them.

Provide a clean baseline snapshot and a repeatable reset procedure. Export evidence before reverting snapshots.
Cleanup must affect only owned guest fixtures, packages, processes, and the designated VM, never unrelated host data.

## 4. Runtime dependencies without build tools

Determine the supported deployment method from the supplied generated manifests and executable dependencies.
The current local Debug candidate includes:

- `Microsoft.WindowsAppRuntime.2`, with manifest minimum version `2.5.1.0`.
- `Microsoft.VCLibs.140.00.Debug.UWPDesktop`, with manifest minimum version `14.0.33728.0`.
- An unpackaged native test host that imports Debug MSVC and Universal CRT libraries, including
  `MSVCP140D.dll`, `VCRUNTIME140D.dll`, and `ucrtbased.dll`.

The ordinary Visual C++ Redistributable does not supply those Debug libraries. Research Microsoft's supported
private testing deployment route and its licensing restrictions. Do not install Visual Studio to obtain a runnable
test guest, or copy arbitrary libraries into system directories. Microsoft documents testing Debug executables
without Visual Studio, subject to Debug-library deployment restrictions.
See [preparing a machine for Debug executables](https://learn.microsoft.com/en-us/cpp/windows/preparing-a-test-machine-to-run-a-debug-executable?view=msvc-170).

Distinguish packaged-app dependency resolution from the unpackaged test host's resolution; satisfying the app's
framework dependency does not by itself prove the test host can load its DLLs. Research Windows App SDK deployment
against [Microsoft's unpackaged-app guidance](https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/deploy-unpackaged-apps).

Also propose a clean-machine Release lane using normal runtime deployment. Do not silently substitute an
unqualified Release artifact for the currently build-checked Debug candidate. Both configurations must be built elsewhere.
Select supported compatible runtimes, not the newest unrelated SDK or major release. Confirm optional dependencies
from actual consumers rather than installing every tool associated with a transitive build package.

## 5. Interactive desktop and execution

Require a dedicated ordinary non-administrator test account, an active unlocked interactive Windows session,
the normal interactive desktop, foreground-window access, and functioning native UI Automation.
Reserve exclusive use of the guest desktop during a suite. Administrative access may be needed for provisioning,
but product acceptance must not rely on elevating the test account.

A running VM, service account, SSH session, or successful remote command does not establish these desktop conditions.
Microsoft distinguishes interactive desktop testing from service execution and warns about remote-session
disconnection effects. See [Microsoft's UI-testing guidance](https://learn.microsoft.com/en-us/azure/devops/pipelines/test/ui-testing-considerations?view=azure-devops).

Research the simplest native launch mechanism first: an attended terminal inside the logged-in guest is acceptable.
Investigate VirtualBox-native transfer and process execution, but verify whether any proposed remote launch reaches
the required desktop. Guest-process execution alone is not proof. Hyper-V PowerShell Direct is not a VirtualBox mechanism.

Include a supported PowerShell 7 runtime for the existing scripts, with x64 PowerShell as the initial launcher.
Persistent autologon, scheduled tasks, services, agents, or additional automation require separate approval.

## 6. Deployment and product qualification

Research guest-only Developer Mode for the existing loose-package Development Mode registration. Do not enable
Device Portal or Device Discovery without justification. See
[Microsoft's developer settings](https://learn.microsoft.com/en-us/windows/advanced-settings/developer-mode).
Do not change production Store identity, personal certificates, or host security settings to make deployment pass.

The existing harness uses native Windows UI Automation. Appium, WinAppDriver, Selenium, custom agents, and compatibility
shims are not required merely to provision this lane. Additional accessibility tools must have a concrete purpose.

The environment must support qualification of:

- Package registration, activation, identity, and cleanup.
- Folder-picker window ownership, cancellation, and the selected-folder storage capability.
- Initial control states and default processing policies.
- Review and confirmation before starting work, including trimming acknowledgment and backup visibility.
- Keyboard navigation, control availability, and stable automation identifiers.
- Resizing, minimum size, and the planned 640/1008 effective-pixel layout breakpoints.
- Display scaling, themes, and localization without clipping or unexplained state changes.
- Clean shutdown, preservation of source images, and exclusion of generated output/backup roots.

The local positive UI test currently covers initial window state only. It has been authored and build-checked,
not executed successfully in an interactive lane. Distinguish existing test coverage from acceptance cases still
requiring implementation or manual qualification. Infrastructure readiness is not full Task 13 acceptance.

### Packaging boundary

The current candidate declares `Windows.FullTrustApplication` and `runFullTrust`. VM isolation therefore does not
demonstrate AppContainer confinement. Keep product-packaging qualification separate from VM setup. Do not modify
the manifest, add capabilities, or claim AppContainer-denial acceptance to make this environment pass.

## 7. Evidence and bounded execution

Retain exact artifact hashes and candidate identity; OS build and patch level; runtime and Guest Additions versions;
architecture/configuration; account integrity and session conditions; resolution, scaling, locale, and theme;
raw stdout/stderr, exit codes, JUnit results, screenshots, and owned package/process cleanup results.

Use the existing bounded harness. The current candidate gives its native qualification child 120 seconds and its
interactive test host 300 seconds. Environment refusal is distinct from product failure; the wrapper uses exit 78
for unavailable qualification, while a test-host timeout uses exit 124. Do not turn failed qualification into a skip.

Setting `JPG_SPINNER_INTERACTIVE_TEST_LANE=IsolatedVirtualMachine` is an operator declaration, not proof of isolation.
An inspection command returning zero likewise does not prove successful qualification. Require actual preflight
and test execution, nonzero test counts, and explicit results; do not count excluded or skipped tests as passes.

After a timeout, reconcile printed ownership locators for packages and broker-launched processes. Killing the test
process does not establish complete cleanup. Bound retries and preserve failure evidence before resetting the VM.

## 8. Research deliverables and decision method

Return a source-grounded proposal containing:

1. The smallest recommended VM configuration, host resource assumptions, and Windows licensing/support position.
2. A required-versus-optional software inventory with official sources, versions, architectures, and integrity checks.
3. Supported Debug and Release runtime deployment routes, explicitly excluding build tooling.
4. The binary-bundle contract and deployment, execution, evidence export, cleanup, and reset runbooks.
5. A verified interactive execution approach, starting with an attended option; approval effects of unattended options.
6. Acceptance criteria separating infrastructure qualification from product qualification and outstanding coverage.
7. Remaining uncertainties, approval-requiring actions, and resource estimates clearly separated from measurements.

Resolve choices through first principles, modern best practice, authoritative Microsoft/Oracle specifications and
guidance, and then established community practice. Prefer existing native capabilities over custom infrastructure.
Check current supported versions at research time without changing the application's dependency contract implicitly.

Do not write code, install build tooling, provision persistent automation, conduct an unrelated broad security
assessment, or waive isolation or product acceptance requirements as part of this research assignment.
