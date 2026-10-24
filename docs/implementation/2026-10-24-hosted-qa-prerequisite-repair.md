# Hosted QA prerequisite repair

The [manual QA run 40090734931](https://github.com/Hadden-Industries/jpg-spinner/actions/runs/40090734931)
at signed source `8efb4d541aa92399dc8630ebeff5af010919d622` failed. Its
jobs ran; this was not an Actions allowance refusal. All original artifact ZIPs
were hash-verified and retained through native HISEW checkpoint
`afffe48457cd46c6bb51e18a90a25535` before the owner-authorized cloud cleanup.
No additional remote run has been dispatched for this repair.

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

These are source-composition and read-only native prerequisite/refusal checks.
The missing-component installation branch still requires deliberate hosted
execution. The prior failed QA run remains failed. This repair supplies no
fresh application build matrix, new full verification, security/external review,
desktop acceptance, WACK, trusted upgrade, legal approval or Store release.
