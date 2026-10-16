# Independent bundle and application versions

Status: native packaging/inspection qualification passed on 2026-10-17;
**trusted upgrade, final clean builds, certification and release remain pending**.

The [installed Store observation](2026-10-17-published-store-baseline.md) exposed
main application `1.1.2.0` inside bundle `2015.616.2316.3521`. The earlier
standalone producer authored bundle `2.0.0.0`, and the inspector incorrectly
required the outer bundle version to equal every application version. Neither
the schema nor the installed product supports that equality invariant.

## Native contract and implementation

[Microsoft's bundle Identity schema](https://learn.microsoft.com/en-us/uwp/schemas/bundlemanifestschema/element-identity)
and [contained Package schema](https://learn.microsoft.com/en-us/uwp/schemas/bundlemanifestschema/element-package)
define separate Version attributes. [MakeAppx](https://learn.microsoft.com/en-us/windows/msix/package/create-app-package-with-makeappx-tool)
accepts an explicit `/bv`; omitting it or supplying zero permits date/time
generation. Microsoft's historical [Calculator repair](https://github.com/microsoft/calculator/pull/586)
also documents reset-related consequences from lowering a bundle version. That
precedent motivates qualification, not a claim that this app's upgrade failed.

The authored owning-project property is now `AppxBundleManifestVersion`, fixed
at `2026.2.0.0`. This exceeds the observed legacy bundle without changing main
application `2.0.0.0`, product identity or publisher. It is a fixed version epoch,
not a clock-derived value or an automatic application-to-bundle mapping.
Installed `Microsoft.AppxPackage.Targets` and the selected
`Microsoft.Windows.SDK.BuildTools.MSIX` 1.7.251221100 packaging targets consume
this exact property through their native MakeAppx bundle tasks. No guessed
property, vendor edit or package-byte rewrite is used.

`Resolve-MsixToolchain.ps1` evaluates the owning project once for both native
MakeAppx selection and bundle version. The shared strict JSON converter checks
MSBuild's native multi-property output before PowerShell conversion; .NET's
Version parser rejects missing, zero or incomplete versions before they can
activate MakeAppx's clock default. Native MakeAppx remains responsible for the
full MSIX rules. The standalone producer and inspector reuse this resolution.
The inspector requires the exact selected outer version, matching Name and
Publisher, and each inner manifest's Version/Architecture agreement with its
own bundle declaration. Independence does not weaken those checks.

The SBOM producer already reads the actual outer Identity.Version. Thus a new
bundle's artifact version is `2026.2.0.0`; application declarations remain
`2.0.0.0`. Historical bundle/SBOM reports are not rewritten or reinterpreted as
evidence for the new container. Exact new final-container SBOM/provenance still
requires its own successful run.

## Retained qualification

The new XML policy regression first failed against unchanged production with
`Bundle/package identity differs: Version` (native child exit 1). After the
correction, independent versions pass. Unexpected lower/higher outer versions,
inner declaration mismatch, identity/architecture mismatch and existing closed
membership/path controls still refuse.

The real native qualification ran the following maintained tests against the
existing Release architecture MSIX files, with fresh owned destinations:

- `Test-MsixBundleInputControls.ps1`: missing/duplicate selections refuse before effects.
- `Test-MsixBundleContract.ps1`: native bundle/unbundle confirms outer
  `2026.2.0.0`, inner `2.0.0.0`, exact Store identity and x86/x64/ARM64 membership.
- `Test-BundleInspection.ps1`: native unpack, existing AppContainer/manifest
  policy and payload inventories pass for all three architectures.
- `Test-MsixBundleNativeRefusalControls.ps1`: repeated architecture refuses
  with native `0x80080204`, despite three different paths.
- `Test-BundleInspectionRefusalControls.ps1`: existing output, real ancestor
  junction and a truncated container refuse without input mutation or promotion.

The repository policy, all six Debug/Release and Win32/x64/ARM64 native
owning-target version selections, application manifest policy and SBOM binding
schema controls also pass. The real new bundle still triggers the exact
Store-submission wrong-digest refusal and native signed-distribution trust
refusal before extraction/cataloging/publication. These negative controls do
not establish positive new-bundle SBOM generation or Windows signature trust.

The wrapper exited 0 with complete native diagnostic streams. The selected SDK
MakeAppx reports `10.0.28000.2705`. No compilation, signing, installation,
desktop input, Windows setting change, trust change or hosted workflow ran.

| Original retained input | SHA-256 before and after |
| --- | --- |
| Release Win32 MSIX | `0e5974cef752cd7a8a4cc6d6ecddcfb0c004667942409b66196eb5b6086edf18` |
| Release x64 MSIX | `5f7b115191b3bdc50f02e31df4d7a0022bc958d1183a52b2c4f8f308170f722c` |
| Release ARM64 MSIX | `91a196999889ffedfe2b4a27507a512b6f05825a5c9b3aacfe144144e2c480fe` |

The resulting unsigned development bundle SHA-256 is
`f28b688b5da3f83f7cbcf47ecb2b542f0e9a564bdcce8b7ba69c387b1ec6aa9c`.
External inventory `42ba6e97c2214c48b6be5edc57f1d472`, execution
`352c2077-8e60-441f-ac25-4b13a115a082`, retains the qualification wrapper,
RED/GREEN/native streams, native manifest, inventories and exact new bundle.
Spent staging/extraction/control copies are eligible for recoverable removal
after those required records are independently preserved; original inputs and
the installed baseline are not cleanup targets.

This is not a new application build or proof of native ARM64 execution. The
isolated trusted legacy-to-modern update, bundle ordering/restore, local-state
preservation, launch, uninstall, final clean-build comparison, exact final SBOM,
WACK and release/legal approval gates remain unchanged. Do not use forced
downgrade, uninstall/reinstall or identity changes to substitute for them.
