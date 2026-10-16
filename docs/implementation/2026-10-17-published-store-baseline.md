# Published Store baseline observation

Status: installed baseline identified, **not an upgrade pass or release approval**.
The owner installed the public Store version. Read-only Windows package commands
on 2026-10-17 distinguish it from the separate development registration.

| Registered component | Native version | Architecture / qualifier | Native signature kind / status |
| --- | --- | --- | --- |
| Main application | `1.1.2.0` | x64 | Store / Ok |
| Bundle | `2015.616.2316.3521` | neutral bundle | Store / Ok |
| Resource package | `1.1.2.0` | neutral, `split.scale-180` | Store / Ok |

The package name is `HaddenIndustriesLtd.JPGSpinner`; publisher is
`CN=42458E53-5B1F-4F49-97F4-ABE6B4A48BB3`; family is
`HaddenIndustriesLtd.JPGSpinner_mc2sj62c1fkda`; application ID is `App`.
Native installed-manifest inspection confirms the authored modern manifest uses
the same name, publisher and application ID. The source-tree version `1.1.3.0`
was not evidence that this version was published. The observed application
baseline is `1.1.2.0`; retain any subsequently discovered published variant as
an additional supported upgrade input rather than discarding this known one.

## Method and evidence

`Get-AppxPackage -Name HaddenIndustriesLtd.JPGSpinner` and
`Get-AppxPackageManifest` supplied main-package registration and manifest data.
Explicit `-PackageTypeFilter Bundle,Main,Resource` enumeration then exposed the
distinct bundle and installed resource registrations. Default main-package
enumeration alone would have omitted that distinction.

The original readable installed `AppxManifest.xml` was copied without decoding
or re-encoding and independently SHA-256-compared. Its hash is
`97eb72918b512a8aeea49a596035c61dd91b1d837c67e62cccf49b05426474bb`.
External verification-result inventory `cd8715293feb4b15970d77604eec6132`, execution
`352c2077-8e60-441f-ac25-4b13a115a082`, retains the inspection script, native
process streams, original manifest and component-registration observations.
Windows reports Store signature metadata and healthy registration; this is not
standalone cryptographic verification of a downloaded APPX container.

No old/new application was launched, no application settings or user files were
read or changed, and no update, removal, re-signing, trust-store change or build
was performed. The now-unneeded download-only WinGet request was stopped only
after its exact PID, native creation identity, parent and literal command were
checked; no Store app or authentication broker was targeted. Its unsuccessful
download logs remain historical, not package-acquisition success.

## Remaining qualification

The [bundle identity schema](https://learn.microsoft.com/en-us/uwp/schemas/bundlemanifestschema/element-identity)
defines a bundle version independently. The SDK's
[MakeAppx contract](https://learn.microsoft.com/en-us/windows/msix/package/create-app-package-with-makeappx-tool)
documents explicit `/bv` selection and date/time generation when it is omitted
or zero. Do not confuse the observed bundle version with the main version or
infer update continuity solely from `2.0.0.0 > 1.1.2.0`.
Microsoft's [Calculator versioning repair](https://github.com/microsoft/calculator/pull/586)
records a reset-related problem after lowering its bundle version; this is
relevant prior evidence to investigate, not proof that this exact JPG Spinner
update fails. The subsequent [native independent-version correction](2026-10-17-independent-bundle-version.md)
qualifies construction and inspection of bundle `2026.2.0.0` containing
application `2.0.0.0`. Bundle ordering/restore behaviour and the actual signed
update remain unqualified. Do not use a forced downgrade, change product identity,
uninstall/reinstall, discard the registered bundle or rewrite package bytes to
make the upgrade test appear successful.

Keep the Store baseline installed until the upgrade-test owner releases it.
The owner's main desktop is not thereby an isolated migration test user or VM.
The exact isolated-lane, authenticated candidate, activation/state/preservation,
architecture, uninstall/recovery and release gates remain in the approved plan.
This x64 observation supplies no x86 or physical ARM64 execution evidence, and
does not establish that an ARM64 legacy package ever existed. All source,
licensing, telemetry-credential and final full-verification obligations remain.
