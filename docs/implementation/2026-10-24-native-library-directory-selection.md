# Native library directory selection

Status: focused native RED/GREEN and supported final headless checks pass;
independent clean-source, device and release qualification remain pending.
No desktop, security
review, external review, hosted execution, SDK mutation or release is performed.

## Reproducer and root cause

The original task-level reproducer invokes the installed vcpkg
`GetGlobalProperties` and `GetVcpkgEnvVars` tasks after Microsoft's
`SetBuildDefaultEnvironmentVariables` target in the real Domain project. It
returns CS1668 twice and exit 1 under native `-warnAsError:CS1668` in under a
second. It compiles no application code and installs no dependency. Ordinary
incremental and Domain rebuild probes pass because the up-to-date manifest
stamp skips that task path; those probes are not false GREEN repair evidence.

The selected C++ targets assign evaluated `LibraryPath` to process `LIB`.
Installed `Microsoft.Cpp.VCTools.Content.props` contributes an absent
`VC/Auxiliary/VS/lib/x64` default. ARM64's broad SDK defaults also include an
absent NETFXSDK 4.8 native library directory. The parent shell's `LIB` is empty.
The same original inline tasks pass when invoked before native C++ environment
publication, isolating the boundary without changing task references or compilers.

Microsoft's [CS1668 guidance](https://learn.microsoft.com/dotnet/csharp/misc/cs1668)
requires valid accessible search directories. Its
[VC++ directory guidance](https://learn.microsoft.com/cpp/build/reference/vcpp-directories-property-page)
defines `LibraryPath` as the configurable native library search surface and
recommends macros/property sheets. The
[Roslyn task-factory source](https://github.com/dotnet/msbuild/blob/main/src/Tasks/RoslynCodeTaskFactory/RoslynCodeTaskFactory.cs)
and [managed compiler source](https://github.com/dotnet/roslyn/blob/main/src/Compilers/Core/MSBuildTask/ManagedCompiler.cs)
explain why an inline managed compiler sees the C++ process environment.
An [upstream vcpkg report](https://github.com/microsoft/vcpkg/issues/51593)
contains the same optional Visual Studio path warning, alongside a different
task-registration failure; that separate failure is not reproduced or claimed
fixed here.

## Maintained repair

`eng/NativeLibraryDirectories.props` selects the required native search set through the
supported project property: the toolset-selected MSVC runtime macro, Universal
CRT macro and selected Windows SDK architecture import-library directory. The
native VC runtime macro preserves its Spectre/platform selection. The maintained
projects do not use optional Visual Studio extension libraries or .NET Framework
native SDK libraries. The sheet is imported through Microsoft's documented
[`ForceImportAfterCppProps` hook](https://learn.microsoft.com/visualstudio/msbuild/customize-cpp-builds),
after native macros exist and before project-owned path extensions. There is no generic missing-path filter, hard-coded
workstation path, environment-clearing wrapper, warning suppression, fake
directory or replacement task implementation.

Before the native C++ environment target runs, MSBuild's own item expansion and
batching validate every selected directory with `System.IO.Directory.Exists`.
An empty selection, absent directory or ordinary file refuses. Missing required
toolchain material must not silently drop out of the search path or select an
unmitigated fallback. Additional linker inputs retain their existing native
metadata path; installed SDK files and user-wide environment settings are unchanged.

## Focused evidence

`Test-NativeInlineTaskEnvironment.ps1` first fails against the previous policy
with native CS1668. After the repair it passes in Debug/Release for x86, x64 and
ARM64, including missing-directory and ordinary-file refusals and unchanged
sentinel readback. The committed diagnostic fixture invokes original installed
tasks after real C++ environment publication; no compiler or task is mocked.
The consolidated ordinary review caught a late-assignment defect in the first
candidate: a targets-time base reset erased instrumented projects' path appends.
A new native regression first reproduces that omission, then passes for ASan
Debug/Release and both actual Fuzz projects after moving the base to the earlier
Microsoft import hook. MSBuild writes one expanded directory per line; the test
requires the ordinary ASan/libFuzzer directory after the Spectre directory.
No custom search-path parser or weakened runtime precedence is introduced.

All-warning failure is requested only for this diagnostic invocation and does
not suppress or reclassify any application warning.

The existing native YAML source-wiring contract first refuses the absent new
regression call. The manual CI matrix then calls it for every actual lane with
results under the existing always-uploaded build-log root, and the contract
passes. Native scanner-workflow syntax/policy and repository policy also pass.
No hosted workflow is dispatched; all triggers remain manual-only.

The owner separately waived Task 20's 24-hour fuzz campaign on 2026-10-24. Its
replacement is the maintained 60-second scanner mutation budget with full replay,
native minimization and zero-growth audit. That accepted reduction in assurance
does not waive malformed-input, sanitizer, resource-limit or recovery controls.

Native working inventory `edeb8c155b744610affa7d0853b0fd24` owns the reproducer,
original streams/binlogs and temporary workflow validators. Preserve required
original evidence through HISEW and dispose released working material through
its native route; existing blocked resource groups remain intact.

## Final supported headless results

The consolidated ordinary review reports one actionable ASan/libFuzzer import-
ordering issue, repaired above. Its narrowly scoped follow-up reports that
finding addressed and no actionable residual issue. No security or external
review is performed.

The established build driver then passes Release x86/x64/ARM64 and Debug x86/x64
with two MSBuild workers and sequential architecture builds. All five original
binlogs report **0 warnings and 0 errors** through Microsoft's native replay
interface. This includes the previous CS1668 warning. These are local supported
builds, not the required independent clean-clone package pair.

All four x86/x64 Debug/Release headless runs return exit 0. Each has 378 testcase
elements across five original JUnit reports, with no failures, errors or skips:
1,512 executions of the same 378 tests across four lanes. The real x64 JPEG
AddressSanitizer run also passes 14 transform cases / 836,957 assertions. After
that instrumented run, the ordinary x64 Release JPEG executable is rebuilt and
its 165-case suite passes, leaving the normal development output usable.

The final bounded scanner regression uses the retained minimized synthetic
corpus as read-only input and passes 826,304 executions in 61 seconds. Full
replay, native minimization and audit pass; the audit reports zero new files,
features or coverage edges. The resulting minimized set has 236 files / 77,277
bytes. This is the approved bounded regression, not the waived 24-hour campaign.

All six ordinary environment lanes pass against the final import seam. The
four affected instrumented lanes separately prove that ASan/libFuzzer's ordinary
MSVC directory remains after Spectre. The manual CI source-wiring contract,
native workflow validation and repository policy pass.

Final raw streams, five original build binlogs, 20 original headless JUnit
reports, native environment observations and complete bounded fuzz output are
retained under external operator group `a60716381c0e41b399cdd38d705ba39b`, native
inventory `0512d7d516394ac9a9f117cbc1d63bc1`. The first x64 Release shell capture
does not contain the complete native console stream; its original binlog and
native replay remain the build record. Later captures use the existing shared
native child-process boundary. An early copy of the still-running x86 Debug
binlog produced an incomplete replay and is retained with an `incomplete`
filename; the final original was captured after process exit. Neither capture
limitation causes a product rerun or a false failed/passing build claim.

Native preservation checkpoint `87c9ab8c0b2e4ed8a126f17ad23de2d6` independently
verifies all 109 required diagnosis originals, with zero missing evidence.
The source group is 49,958,841 bytes; pinned validator installations and a
32,236,700-byte derivative replay are explicitly reconstructible. After the
signed source checkpoint, dispose this released group through native HISEW.
The older corpus's waived long-duration consumer is explicitly released in
native disposition `b950f2184f914ea5a315a1a9bb8303e3`; its unpreserved historical
full/minimized pair remains held for supported preservation, not for a fictitious
future soak campaign. Earlier blocked groups stay intact.

Exact new-bundle SBOM/independent comparison, supported device/OS, interactive
acceptance, legal/source delivery, trusted Store upgrade and certification
remain separate. Security and external reviews remain prohibited by the owner.
