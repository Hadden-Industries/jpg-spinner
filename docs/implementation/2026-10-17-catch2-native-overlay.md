# Catch2 3.16.1 native-overlay qualification

## Decision and boundaries

On 2026-10-17 the owner approved bounded qualification of exactly one additional
native overlay, `vcpkg-ports/catch2`. The existing metadata overlays and immutable
registry baseline remain. This is a test-framework patch, not an application
runtime dependency, new registry, global installation or permission to publish.
Final release, device, security, clean-build and hosted qualification remain open.

The native qualification passes the focused checks below. The root manifest and
closed policy now select 3.16.1. Historical 3.16.0 tests and SBOMs retain their
original candidate identities; they are not relabelled as 3.16.1 evidence.

## Authoritative inputs and implementation

[Catch2's upstream release](https://github.com/catchorg/Catch2/releases/tag/v3.16.1)
fixes the 3.16.0 MSVC benchmarking-header optimization-state regression. No
repository performance improvement is inferred from upstream benchmark claims.
The release tag object is `5010924c4d8e912e64fd64a11819eecacf572c06`, targeting
commit `08092139210881f01a34e60c5f2fd6afa0ea4024`. GitHub's recorded verification
is retained; its web page also reports the key as expired. This is not a new
independent signature-trust qualification.

The original `v3.16.1.tar.gz` has 1,228,880 bytes:

- SHA-256: `d16bd1b8b2364918bd472cb216394acb93bde6784890df0d91bccfb3f28d777d`.
- Native vcpkg SHA-512: `a6b0f1d1bd9178382f06de9773b31e3efdcdfb20fc3b65b23d9675e0838da6f64c417a75e062bda918dd28781e5d11a78e7130cef2394e676c7bc2da9e49106b`.

Reuse [Microsoft's exact baseline recipe](https://github.com/microsoft/vcpkg/tree/c748cb44f2a435fcf015c35225c9d5545fe0021c/ports/catch2)
with its unchanged installation-layout patch. That patch changes native CMake
installation paths, not Catch2's library implementation. Update the upstream
version/hash and omit the generated legacy `catch.hpp` wrapper. Keep the native
CMake targets, pkg-config fixups, static Windows linkage and upstream feature
definition; consumers continue using `<catch2/...>` headers and the existing
reviewed `Catch::Session` entry point. No new runner, argument parser or shim is
introduced. Microsoft's recipe/patch attribution is preserved in
[the original MIT license](../../vcpkg-ports/catch2.LICENSE.txt); include it when
redistributing these build inputs.

[Microsoft documents overlay precedence](https://learn.microsoft.com/en-us/vcpkg/concepts/overlay-ports).
The live official registry still publishes 3.16.0, not 3.16.1. The freshness
gate therefore validates the selected local manifest against the root override
and upstream stable releases, while separately reporting registry lag. It must
not claim that Microsoft published the selected overlay version. Other packages
retain their existing authority contracts. Both native configuration
representations remain mutually exclusive, with the same exact three-path list.

The upstream and native installed BSL-1.0 notice both match the existing retained
notice SHA-256 `c9bff75738922193e67fa726fa225535870d2aa1059f91452c411736284ad566`.
The original notice bytes and expected integrity hash remain unchanged. This
byte comparison does not replace final distribution-specific legal review.

## RED/GREEN and actual checks

The pre-change executable's native `--libidentify` reported 3.16.0. The new
freshness fixture initially failed at the closed two-port allowlist; the
reporting regression independently failed because policy reported 3.16.0.
An additional JSON-array name control exposed PowerShell collection comparison;
the production boundary now explicitly requires a string. A fixture serialization
repair uses `ConvertTo-Json -InputObject` to preserve singleton release arrays,
not a handwritten JSON serializer.

Actual local checks on Windows, Visual Studio 2026 / MSVC 14.51.36231:

| Check                                                                                        | Observed outcome                                                                                                                                                             |
| -------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Bundled vcpkg dry run, isolated manifest and exact overlay                                   | Exit 0; resolves `catch2:x64-windows-static-md@3.16.1` from the owned overlay                                                                                                |
| Native isolated Debug/Release library build and post-build validation                        | Exit 0; SHA-512 verified; original layout patch applies; no `catch.hpp` installed                                                                                            |
| `scripts/tests/Test-DependencyFreshnessContract.ps1`                                         | Exit 0; existing controls plus local identity/version/type/revision disagreement, non-object root, duplicate member, unpublished release and newer actionable stable release |
| `scripts/tests/Test-RepositoryPolicyNegativeControls.ps1`                                    | Exit 0; exact three-path list, missing Catch2, wrong case/type and extra overlay refusals, plus all existing controls                                                        |
| `scripts/tests/Test-RepositoryPolicyReporting.ps1`                                           | Exit 0; reports parsed Catch2 3.16.1                                                                                                                                         |
| Live `scripts/Test-DependencyFreshness.ps1`                                                  | Exit 0; no actionable update; explicitly identifies approved local Catch2 authority and registry 3.16.0 lag                                                                  |
| `scripts/Invoke-Build.ps1 -Architecture x64 -Configuration Release`                          | Exit 0; actual modern application/test solution, locked restore and release PE-reference check                                                                               |
| Built Domain host `--libidentify`                                                            | Native output reports Catch2 3.16.1                                                                                                                                          |
| `scripts/Invoke-TestSuite.ps1 -Architecture x64 -Configuration Release -Project HeadlessAll` | Exit 0; 378 testcase elements across five reports; 864,683 reported assertions; zero failures/errors                                                                         |
| `scripts/Invoke-StaticAnalysis.ps1 -BuildTarget Build -OutputDirectory <fresh owned output>` | Exit 0; native incremental analysis, not a fresh whole-tree rebuild                                                                                                          |

Catch2's JUnit `tests` attribute counts assertions in these reports. The 378
case count comes from native XML `testcase` elements, not that attribute.
The isolated port build and repository build use at most two vcpkg workers;
no persistent environment or concurrency setting changes. No desktop session,
remote workflow or publication is used for this qualification.

## Evidence and resource lifecycle

The input parent is signed commit `c43e306b47ed4abb543bf671b8444ba4ab9251cc`;
the qualification candidate consists of the retained changed-file identities and
this checkpoint's subsequent signed commit. Raw inputs are under external group
`cef1855cbe9642dfa165d1e7c4a9ae02`; actual commands, RED/GREEN streams, native
build logs/binlogs and JUnit reports are under group
`00bbab49bcf949d08d5f9a9c117e50cf`, both in the configured operator evidence
store for execution `352c2077-8e60-441f-ac25-4b13a115a082`. These are actual task
records, not a passing HISEW full-verification receipt.

Preserve the original archive, upstream tag response, Microsoft recipe/patch/
license, exact adopted port, current installed metadata and raw results.
After consumers finish and required evidence has independent readback, recycle
the isolated buildtrees/packages/installation; they are reconstructible working
material, not another retained application dependency cache. The execution owner
owns disposal and reassesses at this signed checkpoint. Preserve failed results.

At that checkpoint, native evidence promotion independently retained 11 source
inputs and 61 qualification records with byte readback. The three isolated
working trees were then recycled through Windows' native Recycle Bin API;
all three calls returned 0 and fresh path checks confirmed source absence.
They contained 1,397 files and 517,995,278 logical bytes. This is recoverable
disposal, not a claim of physical disk-space reclamation. The repository's
active dependency installation and shared vcpkg caches were not deleted.

## Signed-source architecture follow-up

At signed source `e6c17b62658bb7197a220deee3e3982a0dd2e04c`, the native Release
solution builds also pass for x86 and ARM64, one architecture at a time with
the existing two-worker bounds. These are ordinary builds, not independent
clean-source reproducibility qualification. The x86 `HeadlessAll` runtime run
passes all five suites: 378 testcase elements, 864,683 reported assertions and
zero failures/errors. Its native `--libidentify` reports Catch2 3.16.1.
ARM64 is a cross-build only; no native ARM64 runtime result is claimed.

One bounded invocation of the unchanged interactive acceptance driver returns
78 immediately: Windows reports the current session as locked or its lock state
unknown. No product test executes, no desktop input is sent, and no temporary
`JpgSpinner.UI.*` package remains registered. No screen-saver setting is changed.
This is unavailable acceptance qualification, not a passing UI test, product
failure or accepted skip. No unchanged retry follows.

Commands, original binary logs, native result JSON and all five x86 JUnit
reports are retained in external group `44548dd99be246c6b99d389786613213` for
the same execution. That group contains retained evidence only; it creates no
additional isolated dependency installation or disposable checkout. The actual
ARM64 device, supported-OS, trusted-upgrade, full accessibility and release gates
remain required. In particular, neither these tests nor the installed legacy
Store baseline supplies a trusted modern update package for the pre-deletion
upgrade gate; the legacy tree remains quarantined, not silently removed.

## Resumed automated acceptance, 2026-10-24

The official stable-dependency freshness entry point reports no actionable
updates through the approved package authorities. This is a fresh upstream
observation, not a new dependency build or release approval.

The previously unavailable desktop lane now passes its native qualification.
Against the existing signed-source Release x64 candidate, the maintained
interactive driver runs `[automation]~[operator-assisted]`: six actual testcase
elements, 178 reported assertions, zero failures/errors/skips, in 10.329 seconds.
The presentation test executable SHA-256 is
`9eb51da1d9de61267e38b640d2e9e7ba0a6ce3998b7f8ce225a4949ff268df25`.
This is the automated UI acceptance entry point, not a substitute for the two
operator-assisted picker scenarios, Narrator, contrast/scaling, physical ARM64,
the supported-OS matrix, or a trusted signed upgrade.

The operator driver captures the native screen-saver active flag, temporarily
disables it without updating the persistent user profile, and restores the
captured active value of 1 in `finally`, with native readback. Secure and timeout
settings are not changed. Fresh package enumeration finds no remaining
`JpgSpinner.UI.*` registration. Original preflight/acceptance logs, JUnit output,
the bounded operator driver and the freshness report are retained under external
verification-result group `b70f634d955345ee838ae8f64e56c9db`, execution
`1f8e9d2c-38ef-4f91-a9d3-d114332d547e`.

The earlier exit-78 result remains historical counterevidence. The new run does
not relabel it, repeat a full suite for bookkeeping, or qualify a release.
