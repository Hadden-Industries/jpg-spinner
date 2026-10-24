# Hosted QA prerequisite repair

The [manual QA run 40090734931](https://github.com/Hadden-Industries/jpg-spinner/actions/runs/40090734931)
at signed source `8efb4d541aa92399dc8630ebeff5af010919d622` failed. Its
jobs ran; this was not an Actions allowance refusal. All original artifact ZIPs
were hash-verified and retained through native HISEW checkpoint
`afffe48457cd46c6bb51e18a90a25535` before the owner-authorized cloud cleanup.
The owner subsequently authorized publication of signed repair `9e7ac782` and
exactly one [manual QA attempt, 40107330714/1](https://github.com/Hadden-Industries/jpg-spinner/actions/runs/40107330714).
That attempt finished with a failure; it has not been rerun.

## Provenance fixture

The policy job's positive fixture contained six actions, while the adopted pin
inventory contained eight. `setup-go` and `setup-java` had been added for Store
evidence production, but their literal fixture entries were missing. The
production validator correctly rejected the incomplete inventory.

The existing fixture command reproduced that same failure locally before the
repair. Adding the two exact adopted action identities makes the positive record
and all nine existing negative cases pass. The fixture remains independent test
data: it is not generated from the validator's expected pins, and no fixture
tool observation is claimed as actual execution.

## Hosted SDK prerequisite

The build and analysis jobs failed on the missing selected native input
`Windows Kits/10/lib/10.0.28000.0/ucrt/x64`. The project intentionally selects SDK
28000; GitHub's [published runner inventory](https://github.com/actions/runner-images/blob/main/images/windows/Windows2025-VS2026-Readme.md)
lists SDK 26100. A runner label therefore did not establish the required
installation. The existing missing-library gate remains correct and unchanged.

`Install-HostedWindowsSdk.ps1` evaluates the real project with native MSBuild
`-getProperty`, then checks the selected x86/x64/ARM64 UCRT and Windows import
libraries, headers and native packaging executable. The Universal CRT authority
is Microsoft's `UCRTContentRoot` and `TargetUniversalCRTVersion`, as defined by
the installed `ucrt.props`; it does not infer that selection from an unrelated
environment variable. A complete installation needs no installer call.

For missing inputs, installation is restricted to an elevated ephemeral
GitHub-hosted Windows runner. The existing `.vsconfig` must adopt the selected
SDK component. Microsoft's installed Visual Studio Installer performs a bounded
workflow step that adds only that component to the already selected stable
instance. It requests no product update, forced close or automatic restart.
An installation error, reboot requirement, changed locked toolchain or still
missing input stops qualification. There is no SDK retargeting, library filtering,
installation repair loop or workstation installation fallback.

Microsoft documents [component modification and installer parameters](https://learn.microsoft.com/en-us/visualstudio/install/use-command-line-parameters-to-install-visual-studio).
`setup.exe` does not support the bootstrapper's `--wait` switch; the script uses
[PowerShell's process-tree wait](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.management/start-process)
and retains the native exit code. Each workflow setup step has a 15-minute
limit. Existing manual-only triggers and one-day compressed artifact retention
remain unchanged. SDK diagnostics stay within the existing uploaded evidence
roots; no installed SDK tree or package cache is uploaded.

## Focused proof and remaining scope

The CI source contract first fails against the previous workflow because SDK
setup is absent. Native YAML/actionlint controls then reject omitted,
conditionally skipped and unbounded setup. The actual installed local SDK check
passes without installation. A separate owned project, changed through the
native XML DOM, makes Microsoft's own UCRT macros select an absent test root;
the actual script refuses it in a non-hosted child before launching an installer.
All original native streams and owned inputs are retained under inventory
`c494a27086ca4f83a35545e691cc90be`.

Those initial checks are source-composition and read-only native
prerequisite/refusal proof. The later authorized hosted attempt supplies the
additional observations below, without changing either failed attempt's result.

## Observed hosted attempt and remaining prerequisites

All five selected application configurations compiled successfully on hosted
runners: x86 Debug/Release, x64 Debug/Release and ARM64 Release. SDK component
installation and native inline-task environment checks passed. The ARM64 lane,
the instrumented JPEG ASan job and the MSVC analysis job completed successfully.
This exercises the formerly missing SDK installation branch on actual ephemeral
runners without retargeting the project or changing the local workstation.

The overall attempt still failed at two later boundaries:

- The source-policy job eventually reached the real COMDAT compiler/linker
  probe. That job has no SDK setup. The probe now belongs to the existing
  SDK-prepared x64 Release lane; its original streams and generated inputs are
  placed under the always-uploaded build-log root rather than lost in runner
  temporary storage. The native probe and its assertions are unchanged.
- The unpackaged presentation test host could not bootstrap before `main`.
  The selected x64 Release shutdown check returned `0x80670016`, and the other
  executable matrix lanes stopped before a presentation JUnit report appeared.
  x64 Debug and x86 Debug/Release each retained 316 passing testcase elements
  across Domain, BatchProcessing, JPEG transformation and Windows Storage,
  with no failure, error or skipped elements. This does not claim that the
  presentation tests ran, or that x64 Release reached those same suites.

Microsoft distinguishes [runtime deployment for unpackaged hosts](https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/deploy-unpackaged-apps)
from SDK restoration and compilation. `Install-HostedWindowsAppRuntime.ps1`
therefore runs only in executable x86/x64 hosted test lanes, after compilation
and before either presentation-host entry point. Native MSBuild evaluates the
unique adopted `Microsoft.WindowsAppSDK` package version. The associated
[official 2.5.1 runtime installers](https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/downloads)
have pinned resolved Microsoft download URLs, SHA-256 identities and valid
Microsoft Corporation Authenticode signatures. Their actual PE file version is
`2.5`; the associated SDK package version is `2.5.1`.

The native installer owns package ordering, architecture applicability and
existing-version handling. It receives only the documented `--quiet` option;
there is no custom deployment solver, forced shutdown, bootstrap bypass,
runtime downgrade, workstation installation, signing/trust modification or
self-contained test-host replacement. Acquisition and native execution have
bounded deadlines, and the whole workflow step is limited to ten minutes.
The reconstructible executable stays outside uploaded diagnostics and is
removed after successful native execution. A failed hosted setup retains its
native streams; an unresolved installer remains within the disposable runner's
lifetime, without a cleanup claim.

Focused source checks first reject the absent runtime prerequisite, then pass
the corrected placement. Negative controls reject omitted, unbounded and
wrong-lane deployment, and incorrect COMDAT lane/evidence placement. Both actual
Microsoft runtime binaries pass read-only `-WhatIf` identity qualification;
non-hosted installation, local acquisition and an incorrect binary are refused.
These checks install nothing locally. Actual hosted runtime deployment and the
following tests still require a separately authorized attempt of the resulting
checkpoint; no second workflow dispatch is implied by the first authorization.

All seven original archives from attempt `40107330714/1` were downloaded and
checked against GitHub's exact artifact sizes and native SHA-256 digests before
cloud reclamation. Original archives, run/attempt identity, native reports and
failure logs are retained through inventory
`c494a27086ca4f83a35545e691cc90be`. A retained failed run is diagnostic evidence,
not successful full QA, desktop acceptance, WACK, trusted upgrade, legal
approval or Store release.
