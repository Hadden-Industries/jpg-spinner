# Resolved NuGet notice inputs

Status: development input preservation, **not distribution or legal clearance**.
The [release register](../../release-qualification.md) retains the outstanding
exact-payload, corresponding-source, licensing and Store obligations.

## Exact resolved packages

The current [application lock](../../../src/JpgSpinner.App/packages.lock.json)
contains these 17 external package identities across its targets. Project
references are owned source, not extra NuGet packages. The table records package
contents, **not a claim that all installed components or their third parties
ship in or execute inside JPG Spinner**. Build tools, generated/header inputs,
framework dependencies and self-contained redistributables have different roles.

| Package | Resolved version | Retained license / notice input |
| --- | --- | --- |
| Microsoft.Web.WebView2 | 1.0.3719.77 | [LICENSE.txt](Microsoft.Web.WebView2.LICENSE.txt), [NOTICE.txt](Microsoft.Web.WebView2.NOTICE.txt) |
| Microsoft.Windows.AI.MachineLearning | 2.1.74 | [license.txt](Microsoft.Windows.AI.MachineLearning.LICENSE.txt), [ThirdPartyNotices.txt](Microsoft.Windows.AI.MachineLearning.ThirdPartyNotices.txt) |
| Microsoft.Windows.CppWinRT | 3.0.260818.1 | [LICENSE](Microsoft.Windows.CppWinRT.LICENSE.txt) |
| Microsoft.Windows.ImplementationLibrary | 1.0.260126.7 | [LICENSE](../Microsoft.Windows.ImplementationLibrary.LICENSE.txt), [ThirdPartyNotices.txt](Microsoft.Windows.ImplementationLibrary.ThirdPartyNotices.txt) |
| Microsoft.Windows.SDK.BuildTools | 10.0.28000.2705 | [External SDK terms snapshot obtained; applicability unresolved](https://aka.ms/WinSDKLicenseURL) |
| Microsoft.Windows.SDK.BuildTools.MSIX | 1.7.251221100 | [NOTICE.txt](Microsoft.Windows.SDK.BuildTools.MSIX.NOTICE.txt), [sdk_license.txt](Microsoft.Windows.SDK.BuildTools.MSIX.LICENSE.txt) |
| Microsoft.WindowsAppSDK | 2.5.1 | [license.txt](Microsoft.WindowsAppSDK.LICENSE.txt), [NOTICE.txt](Microsoft.WindowsAppSDK.NOTICE.txt) |
| Microsoft.WindowsAppSDK.AI | 2.5.5 | [license.txt](Microsoft.WindowsAppSDK.LICENSE.txt) |
| Microsoft.WindowsAppSDK.Base | 2.0.4 | [NOTICE.txt](Microsoft.WindowsAppSDK.NOTICE.txt), [license.txt](Microsoft.WindowsAppSDK.LICENSE.txt) |
| Microsoft.WindowsAppSDK.DWrite | 2.1.0 | [license.txt](Microsoft.WindowsAppSDK.LICENSE.txt) |
| Microsoft.WindowsAppSDK.Foundation | 2.3.12 | [license.txt](Microsoft.WindowsAppSDK.LICENSE.txt) |
| Microsoft.WindowsAppSDK.InteractiveExperiences | 2.1.9 | [license.txt](Microsoft.WindowsAppSDK.LICENSE.txt) |
| Microsoft.WindowsAppSDK.ML | 2.1.94 | [license.txt](Microsoft.WindowsAppSDK.ML.LICENSE.txt) |
| Microsoft.WindowsAppSDK.Runtime | 2.5.1 | [license.txt](Microsoft.WindowsAppSDK.LICENSE.txt), [NOTICE.txt](Microsoft.WindowsAppSDK.NOTICE.txt) |
| Microsoft.WindowsAppSDK.Search | 2.5.5 | [license.txt](Microsoft.WindowsAppSDK.LICENSE.txt) |
| Microsoft.WindowsAppSDK.Widgets | 2.0.5 | [license.txt](Microsoft.WindowsAppSDK.LICENSE.txt) |
| Microsoft.WindowsAppSDK.WinUI | 2.3.9 | [license.txt](Microsoft.WindowsAppSDK.LICENSE.txt), [NOTICE.txt](Microsoft.WindowsAppSDK.WinUI.NOTICE.txt), [tools/NOTICE.txt](Microsoft.WindowsAppSDK.WinUI.Tools.NOTICE.txt) |

The shared WindowsAppSDK license above is byte-identical in the resolved parent,
AI, Base, DWrite, Foundation, InteractiveExperiences, Runtime, Search, Widgets and
WinUI packages. The parent, Base and Runtime also contain identical NOTICE bytes.
They therefore reference one retained copy per exact content, not independently
maintained duplicates. ML's terms differ and are retained separately.
WinUI's and SDK BuildTools.MSIX's root NOTICE files are both 2,061 bytes but have
different hashes: matching names or lengths are not grounds for deduplication.
WIL's existing notice is reused and now checked by the same hash policy.

SDK BuildTools 10.0.28000.2705 has no packaged license/notice file or modern
license element; its nuspec supplies only the external SDK terms URL. Its bytes
must not inherit another package's license merely because Microsoft publishes
both. This inventory deliberately has no fabricated license file.

On 16 October 2026 native HTTPS retrieval of that exact declared URL returned
HTTP 200 and redirected to Microsoft's
[original RTF](https://download.microsoft.com/download/0/F/F/0FF2B061-47DD-4F55-89B6-FD1D8C44F14D/sdk_license.rtf).
The original is 246,945 bytes with SHA-256
`dd07eb178e00c6bba4148457fc00ff77cd4887eb521d504186fe59c9ec8bbe62`.
Windows RichTextBox's native RTF reader identifies it as Windows 10 SDK terms,
EULA ID `WIN10SDK.RTM.AUG_2018_en-US`; no custom RTF parser or text replacement
was used. The original bytes are retained externally under reproduction-input
inventory `cb4d88ac3718453e8a7f9529c779d30f`, owning execution
`352c2077-8e60-441f-ac25-4b13a115a082`. Fetching or reading this document does
not establish its applicability to every 2026 SDK component, accept an agreement,
establish GPL compatibility or authorize redistributing SDK tools. The qualified
rights reviewer must settle exact applicability, additional component terms and
any redistribution requirements before this gate can close. The selected
package's declared URL, observed final URL and snapshot identity are separate
facts; a later changed redirect must not overwrite this historical observation.

## Native method and evidence boundary

On 16 October 2026 the installed NuGet.Packaging 7.10.0.40 reader supplied
package identities, nuspec license metadata, original package member names and
streams. It reported no license metadata warnings/errors for these selected
inputs. Every native content hash matched the original lock, and native
signature-integrity checks passed. The installed .NET SDK's
[dotnet nuget verify --all](https://learn.microsoft.com/en-us/dotnet/core/tools/dotnet-nuget-verify)
also returned zero for all 17 original cached archives using the repository's
NuGet.config, without restoring packages or changing trust configuration.

For a signed package, NuGet's content hash is **not** SHA512 of the complete
signed archive. Its [native PackageArchiveReader](https://github.com/NuGet/NuGet.Client/blob/dev/src/NuGet.Core/NuGet.Packaging/PackageArchiveReader.cs)
owns that distinction. Reading a declared hash alone is not signature-integrity
or signer-trust verification. A draft whole-container hash comparison refused a
valid signed WebView2 package; it was corrected by using the native API and CLI,
not by changing a lock, weakening an assertion or rewriting package bytes.

NuGet's [license-file/expressions contract](https://learn.microsoft.com/en-us/nuget/reference/nuspec#license)
and [native NuspecReader](https://github.com/NuGet/NuGet.Client/blob/dev/src/NuGet.Core/NuGet.Packaging/NuspecReader.cs)
own parsing. An absent license element is recorded as absent, not interpreted as
MIT, acceptance or permission to redistribute. Native diagnostics must still be
checked: this API can report bad metadata without throwing.

Fourteen distinct notice files total 762,909 original bytes. Thirteen are newly
retained here; the exact WIL license already exists one directory above.
Do not normalize vendor BOMs, line endings, final-newline absence or whitespace.
The package-specific hashes are enforced by
[Test-DocumentationPolicy.ps1](../../../scripts/Test-DocumentationPolicy.ps1);
its owned-copy controls require missing/edited NuGet notices to be rejected,
alongside the native-port controls. Hash equality protects selected bytes, not
notice sufficiency or license compatibility.

No new NuGet dependency, parser, resolver, source rewriter or application runtime
feature is introduced by this inventory. Future dependency updates must refresh
these inputs from their newly qualified original packages, preserve provenance,
and repeat the exact binary/source/legal comparison. Header-generated content,
SDK files, framework packages and tool redistribution still need their applicable
rights analysis; package membership alone cannot decide that analysis.
