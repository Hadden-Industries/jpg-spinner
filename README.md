# JPG Spinner

JPG Spinner corrects JPEG orientation through coefficient-domain transforms and
reconciles metadata to describe the result. It does not decode and re-encode
photographs as a way to rotate them.

## Status

Version 2.0 is under development, not release-qualified. The modern solution is
`JpgSpinner.sln`; `JPG Spinner.sln` and `JPG Spinner/` are quarantined historical
reference material pending an explicitly qualified cutover. Building a package
does not establish Store acceptance or update continuity. See the
[qualification register](docs/release-qualification.md) and
[approved plan](docs/plans/2026-08-30-jpg-spinner-modernization.md).

The declared target is Windows 10 build 19045 or later, including Windows 11,
on x86, x64 and ARM64. Declared support is not physical-device qualification:
minimum-OS, physical ARM64, assistive-technology and trusted-upgrade checks remain
release gates. Source and package identity remain the existing Store product.

## Safe defaults

- Pick a folder, analyze it, review eligibility and consent, then start processing.
- The default creates corrected copies under `JPG Spinner Output` in the selected
  root, in an exclusively created timestamp/batch directory with relative source
  structure. Existing destinations are not overwritten.
- Replacement is opt-in and requires an independently verified retained backup
  under `JPG Spinner Backups` in that root, followed by the journal commit gate.
  Backups are not automatically deleted. Keep a separate independent backup too.
- Perfect transforms are the default. An image requiring incomplete edge-MCU
  trimming needs explicit review of the exact loss and acknowledgment; trimming
  is never silently enabled.
- A changed source revision, unsupported provider or ambiguous recovery outcome
  refuses the operation. Do not rename, edit or delete in-progress transaction
  artifacts to try to make recovery succeed.

Transactions require the documented Windows long-path opt-ins: the executable's
`longPathAware` manifest and machine `LongPathsEnabled=1`. The app does not change
that Windows setting. Provider qualification is deliberately conservative; FAT32
is refused before modification. See [guarantee limits](docs/architecture.md#guarantee-limits).

## Supported inputs

The bounded scanner accepts supported single-image DCT JPEG coding processes
only when the codec reports them transformable. The policy includes 8-bit
baseline and 8-/12-bit extended sequential, progressive and arithmetic DCT forms; grayscale,
RGB, YCbCr, CMYK and YCCK are preserved without color conversion. Predictive
lossless/hierarchical JPEG, unsupported precisions, MPO/MPF, C2PA/other JUMBF and
payload-bearing trailing data such as Motion Photos are refused, not flattened.

Resource limits are defined by
[`JpegResourceLimits::production`](src/JpgSpinner.Domain/include/jpg_spinner/domain/JpegResourceLimits.h):
512 MiB encoded input, 268,435,456 pixels, 32 MiB aggregate metadata and 100
progressive scans. The codec's separate 512 MiB intermediate-memory setting is
not an encoded-file-size guarantee or a bound on total process memory.

Exif/XMP orientation and derived dimensions are reconciled; ICC and descriptive,
rights and IPTC metadata follow the explicit preservation policy. Stale derived
previews are not passed off as current images. Extended XMP is preserved only
when no required mutation belongs to the extension; otherwise it is refused with
`ExtendedXmpMutationNotSupported`. See the
[metadata contract](docs/specs/2026-08-30-jpg-spinner-modernization-design.md#8-metadata-reconciliation)
and tests for the exact preservation/removal/refusal cases.

## Build and test

Use Windows, PowerShell 7, Visual Studio 2026 C++ Build Tools with the declared
platform components, and the SDK/toolset selected by
[`eng/toolchain-lock.json`](eng/toolchain-lock.json) and root build policy.
ARM64 needs the UWP ARM64 C++ platform component as well as the cross compiler.
NuGet versions and vcpkg ports/baseline are locked; do not retarget to whatever a
machine happens to have installed. No Visual Studio project edits are required.

```powershell
./scripts/Test-RepositoryPolicy.ps1
./scripts/Invoke-Build.ps1 -Configuration Release -Architecture x64 -MaximumBuildProcessCount 2
./scripts/Invoke-TestSuite.ps1 -Project HeadlessAll -Configuration Release -Architecture x64
./scripts/Invoke-Build.ps1 -Configuration Release -Architecture ARM64 -MaximumBuildProcessCount 2
./scripts/Invoke-Build.ps1 -Configuration Release -Architecture x64 -BuildEntryPoint PackagedApplication -UnsignedPackageOutputDirectory C:/OwnedFreshDrop/x64
```

Choose a fresh output directory you own. ARM64 cross-compilation on x64 is not
ARM64 execution. Interactive tests require their explicit environment preflight
and a bounded dedicated session; do not run them as headless service tests.
Two MSBuild workers is a conservative default, not a measured performance optimum.
Build concurrency can be chosen explicitly for available memory.

Remote workflows are **manual-only**: pushes, pull requests and schedules do not
start CI. Once these workflows are present on the default branch, use GitHub
Actions **Run workflow** (select the candidate ref), or an explicit CLI call:

```powershell
gh workflow run continuous-integration.yml --ref main
gh workflow run codeql.yml --ref main
gh workflow run scanner-fuzz.yml --ref main
```

These are operator commands, not automatic release authorization. See GitHub's
[manual workflow guidance](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/manually-run-a-workflow).

See [architecture](docs/architecture.md), [privacy](docs/privacy.md),
[security reporting](SECURITY.md) and [third-party qualification](THIRD_PARTY_NOTICES.md).

## License

The repository carries the [GNU GPL version 3 license](LICENSE). Distribution
also requires the exact applicable third-party notices and corresponding source;
the existence of this README does not constitute legal or Store approval.
