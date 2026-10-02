# JPG Spinner 2.0 Modernization Implementation Plan

> **For agentic workers:** REQUIRED EXECUTION MODE: inline execution only. Subagents and subagent-driven development are prohibited by project constraint. Execute the tasks sequentially in the active task, preserve RED/GREEN evidence at every behavior boundary, and stop a task rather than bypassing a failed gate.

**Goal:** Replace the unmaintained UWP/C++/CX application with a secure, accessible, Store-updatable WinUI 3/C++/WinRT application that performs provably lossless JPEG orientation normalization, preserves truthful metadata, and cannot replace a source without a verified backup.

**Architecture:** A single-project packaged MSIX presentation shell composes four native modules: domain policy, JPEG transformation, AppContainer storage transactions, and batch orchestration. Dependencies point inward toward immutable value types and capability contracts. The old application remains only as a temporary behavioral reference and is deleted at the verified cutover; no legacy code is wrapped.

**Tech stack:** Visual Studio 2026 / MSVC 14.51 / PlatformToolset v145 with an exact qualification-time `VCToolsVersion`; C++20; WinUI 3 and Windows App SDK 2.5.1; Microsoft.Windows.CppWinRT 3.0.260818.1; Windows SDK BuildTools 10.0.28000.2705; libjpeg-turbo 3.2.0; Exiv2 0.28.9 with XMP; Catch2 3.16.0; Microsoft SBOM Tool CLI 4.1.5; vcpkg manifest mode at baseline `c748cb44f2a435fcf015c35225c9d5545fe0021c`; MSBuild for first-party projects; GitHub Actions on `windows-2025-vs2026` with the exact runner/tool versions recorded in provenance.

**Design specification:** `docs/specs/2026-08-30-jpg-spinner-modernization-design.md`

**Dependency refresh, 2026-10-03:** Before Task 6, qualify Exiv2 0.28.9, Windows App SDK 2.5.1, and libjpeg-turbo 3.2.0 at vcpkg port revision 1 (`3.2.0#1`). The immutable registry baseline above supersedes the original Task 1 baseline. Historical implementation logs retain the versions actually tested at those milestones. See `docs/implementation/2026-10-03-dependency-refresh-log.md` for authority, scope, and evidence.

---

## Mandatory execution protocol

1. Work on one unchecked step at a time. Do not start a later production behavior while an earlier focused test is red.
2. For every behavior step, write one focused test first and run it.
3. Confirm RED is caused by the missing behavior, not compilation damage, a misspelled test selector, or a broken fixture.
4. Implement the smallest semantically complete behavior that can make that test pass.
5. Run the focused test, then the owning project suite, then every known downstream suite.
6. Refactor names, ownership, comments, and duplication only while the relevant suite is green.
7. Commit the coherent test plus implementation slice. Never commit a known-red main branch state.
8. For build files, package metadata, CI, documentation, and generated artifacts, add executable policy validation first, watch it fail, then change the artifact and watch it pass. Do not invent meaningless unit tests for declarative files.
9. Record the exact RED and GREEN commands and salient outputs in the implementation log or pull-request description.
10. If production code was written before its focused failing test, remove that production change and recreate it through RED → GREEN → REFACTOR.
11. After each task's GREEN evidence is recorded and its commit is complete, perform the repository-artifact hygiene pass below before beginning the next task.

### Repository-artifact hygiene after every task

- Treat the currently observed backlog of hundreds of obsolete numbered or timestamped test-run directories and superseded build/dependency caches as implementation cleanup debt, not as a grandfathered baseline. Remove that backlog incrementally at each post-task hygiene pass, beginning with the oldest superseded runs, and do not carry an obsolete run merely because it predates the current task.
- Inventory ignored/generated directories by exact repository-relative path and size. At minimum inspect `artifacts/`, `vcpkg_installed/`, every production/test `obj/` directory, `Generated Files/`, `TestResults/`, `AppPackages/`, `sanitizer-reports/`, and `fuzz-artifacts/`; also inspect any task-specific temporary restore or test-run root created by the task.
- Keep the current task's RED/GREEN logs, binary logs, JUnit results, packages, and generated sources only until their salient evidence has been recorded and the coherent task commit succeeds. After the commit, remove obsolete run directories rather than accumulating numbered or timestamped copies.
- Retain a dependency cache such as `vcpkg_installed/` across adjacent tasks only when the next task will reuse that exact locked graph and the retained size is reported. Remove it when the graph changes, the next task does not need it, or the milestone is handed off. Every removed cache must be reproducible from committed lock/configuration files.
- Preserve tracked files and reviewed release evidence, including `artifacts/release/**/*.spdx.json`. Never use an indiscriminate `git clean -fdx`, wildcard recursive deletion, or a cleanup rooted at the repository itself.
- Before each recursive removal, resolve the literal absolute target, prove it is a descendant of the repository and one of the inventoried ignored/generated roots, then remove that exact target with PowerShell `Remove-Item -LiteralPath`. Re-run `git status --short --ignored` afterward to prove no source, lock file, fixture, or required evidence was lost.
- Record the removed paths, approximate reclaimed size, and any intentionally retained large cache in the implementation log or task handoff. A retained cache requires the exact next task and locked dependency graph that will consume it; “potentially useful later” is not sufficient. The expected steady state between tasks is no obsolete test-run directory and no unexplained large ignored directory.

### Cross-cutting prohibitions

- Do not spawn or delegate to subagents.
- Do not add shims, compatibility facades, bridge assemblies, C++/CX adapters, dual UI runtimes, or wrappers around the legacy application.
- Do not call private libjpeg APIs or include `transupp.h`; use the public TurboJPEG 3 API.
- Do not decode and recompress pixels as a fallback for a rejected coefficient transform.
- Do not add an output state that replaces a source without a verified backup.
- Do not trim partial MCUs unless the user selected `TrimPartialMinimumCodedUnits` during review.
- Do not add custom telemetry or any network capability.
- Do not float package, registry, SDK, or CI action versions.
- Do not use `/std:c++latest` or `/std:c++23preview`; C++20 is the latest stable production language mode for the pinned compiler.
- Do not suppress a compiler, analyzer, sanitizer, accessibility, fuzz, package, or test failure without a root-cause fix and a narrow written rationale.

### Comment standard

Add comments generously where they carry information that names and types cannot:

- the geometric interpretation of Exif orientations;
- DCT coefficient sign changes and MCU completeness;
- checked marker arithmetic and resource-limit rationale;
- why metadata is changed, preserved, removed, or rejected;
- transaction recoverability invariants and the limits of OS flush/replacement guarantees;
- coroutine lifetime, apartment, and UI-thread transitions;
- cancellation boundaries around non-interruptible native work;
- privacy redaction and diagnostic context;
- HRESULT or native error translation.

Do not comment syntax, restate a function name, or leave commented-out code. Public types and capability contracts receive documentation comments. Every non-obvious failure branch explains the corruption or data-loss scenario it prevents.

## Target repository map

Create this structure in the order established by the tasks:

```text
JpgSpinner.sln
.vsconfig
.editorconfig
.clang-format
Directory.Build.props
Directory.Build.targets
Directory.Packages.props    # retained only when Task 2 proves CPM for the exact C++ projects
NuGet.config
vcpkg.json
vcpkg-triplets/
    x86-windows-static-md.cmake
    x64-windows-static-md.cmake
    arm64-windows-static-md.cmake
eng/
    toolchain-lock.json
src/
    JpgSpinner.Domain/
    JpgSpinner.JpegTransformation/
    JpgSpinner.WindowsStorage/
    JpgSpinner.BatchProcessing/
    JpgSpinner.App/
        Localization/
        ViewModels/
        Assets/
        Strings/en-US/
        Strings/en-GB/
        Strings/ru/
tests/
    TestSupport/
    JpgSpinner.Domain.Tests/
    JpgSpinner.JpegTransformation.Tests/
    JpgSpinner.WindowsStorage.Tests/
    JpgSpinner.BatchProcessing.Tests/
    JpgSpinner.Presentation.Tests/
    TestData/README.md
fuzz/JpgSpinner.JpegSegmentScanner.Fuzz/
scripts/
.github/workflows/
README.md
SECURITY.md
THIRD_PARTY_NOTICES.md
```

The file lists under each task are authoritative and refine this compact tree.

## Task 1: Establish the reproducible toolchain and policy verifier

**Files:**

- Create: `.vsconfig`
- Create: `.editorconfig`
- Create: `.clang-format`
- Create: `.clang-tidy`
- Create: `src/JpgSpinner.App/.clang-tidy`
- Create: `Directory.Build.props`
- Create: `Directory.Build.targets`
- Create: `NuGet.config`
- Create: `vcpkg.json`
- Create: `vcpkg-triplets/x86-windows-static-md.cmake`
- Create: `vcpkg-triplets/x64-windows-static-md.cmake`
- Create: `vcpkg-triplets/arm64-windows-static-md.cmake`
- Create: `scripts/JsonObjectMemberValidation.psm1`
- Create: `scripts/StrictJsonWebRequest.psm1`
- Create: `scripts/DumpbinImageMitigationMetadata.psm1`
- Create: `scripts/Resolve-MSBuildToolchain.ps1`
- Create: `scripts/New-MSBuildToolchainLock.ps1`
- Create: `scripts/Test-RepositoryPolicy.ps1`
- Create: `scripts/Test-DependencyFreshness.ps1`
- Create: `scripts/Test-EffectiveBuildPolicy.ps1`
- Create: `scripts/tests/Test-MSBuildToolchainResolution.ps1`
- Create: `scripts/tests/Test-MSBuildToolchainLockCreation.ps1`
- Create: `scripts/tests/Test-RepositoryPolicyReporting.ps1`
- Create: `scripts/tests/Test-RepositoryPolicyNegativeControls.ps1`
- Create: `scripts/tests/Test-ModernCppBuildPolicyScope.ps1`
- Create: `scripts/tests/Test-DependencyFreshnessContract.ps1`
- Create: `scripts/tests/Test-EffectiveBuildPolicyNegativeControls.ps1`
- Create: `scripts/tests/Test-StrictJsonWebRequest.ps1`
- Create: `scripts/tests/Test-DumpbinImageMitigationMetadata.ps1`
- Create: `scripts/tests/Test-ClangTidyNamingBoundaries.ps1`
- Create: `scripts/tests/Test-GitIgnoreReviewedArtifacts.ps1`
- Create: `eng/toolchain-lock.json`
- Create: `eng/BuildPolicyProbe/BuildPolicyProbe.vcxproj`
- Create: `eng/BuildPolicyProbe/BuildPolicyProbe.cpp`
- Create: `docs/implementation/2026-08-31-modernization-task-1-log.md`
- Modify: `.gitignore`
- Modify: `JPG Spinner/JPG Spinner.vcxproj`

### Step 1.1: Write the repository policy check before configuration

- [x] Add a dependency-free PowerShell verifier that parses JSON and XML rather than searching text. Require
  PowerShell 7.6 on .NET 10 and route every repository-controlled JSON consumer through one strict
  `System.Text.Json` boundary with `AllowDuplicateProperties=false` before PowerShell's last-member-wins conversion.
  Distinguish file absence from a successfully parsed JSON `null`, and validate each raw root kind before conversion:
  objects for `.vsconfig`, the toolchain lock, the vcpkg manifest, and any present standalone vcpkg configuration;
  arrays only where an authority's published schema requires one.
- [x] Make Task 1 assert: toolset v145 plus an exact 14.51.x `VCToolsVersion`, C++20, Windows 10.0.19045.0 minimum, Windows 10.0.28000.0 target, x86/x64/ARM64, an isolated credential-free nuget.org v3 source policy, the pinned vcpkg baseline, libjpeg-turbo 3.2.0, Exiv2 0.28.9 with XMP, and Catch2 3.16.0. Task 2 extends the same verifier with App SDK 2.5.1, C++/WinRT 3.0.260818.1, and SDK BuildTools 10.0.28000.2705 only after real project references and locked restore evidence exist.
- [x] Make Task 1 reject preview/experimental labels, floating native versions, `/std:c++latest`, ARM32, and every alternate vcpkg resolution authority: explicit default/additional registries, port/triplet overlays, and both current and legacy embedded configuration fields. Task 2 adds production/test Windows App SDK bootstrapper and auto-initializer boundary checks after those project graphs exist; it must reject their use in `JpgSpinner.App` and permit them only in an explicitly unpackaged test executable that consumes Windows App SDK runtime types.
- [x] Run it before creating the configuration:

```powershell
pwsh -NoProfile -File scripts/Test-RepositoryPolicy.ps1
```

Expected RED: exit code 1 with separate diagnostics for absent root configuration files. It must not fail because the verifier itself cannot parse.

### Step 1.2: Pin the native dependency graph

- [x] Add this exact manifest:

```json
{
  "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg-tool/main/docs/vcpkg.schema.json",
  "name": "jpg-spinner",
  "version-string": "2.0.0",
  "builtin-baseline": "c748cb44f2a435fcf015c35225c9d5545fe0021c",
  "dependencies": [
    {
      "name": "libjpeg-turbo",
      "default-features": false
    },
    {
      "name": "exiv2",
      "default-features": false,
      "features": ["xmp"]
    },
    {
      "name": "catch2",
      "default-features": false
    }
  ],
  "overrides": [
    {"name": "libjpeg-turbo", "version": "3.2.0#1"},
    {"name": "exiv2", "version": "0.28.9"},
    {"name": "catch2", "version": "3.16.0"}
  ]
}
```

- [x] Add one triplet per architecture. Each sets `VCPKG_TARGET_ARCHITECTURE` precisely, `VCPKG_CRT_LINKAGE dynamic`, `VCPKG_LIBRARY_LINKAGE static`, and supported `/Qspectre` plus `/guard:cf` compile/link flags. Treat each triplet as executable CMake: run it with the selected Visual Studio instance's bundled CMake in script mode, consume CMake's versioned `json-v1` trace, and require exactly one executed two-argument `set(...)` record for each of the six reviewed variables with its exact approved value. Reject every other executed command or variable; do not maintain a partial CMake parser. Apply `/CETCOMPAT` only to final x64 executable links, reject it for x86 and ARM64, and inspect the x64 PE load configuration rather than assuming the flag took effect.
- [x] Treat the manifest root plus its `dependencies` and `overrides` arrays as closed schemas. At the root, permit only the reviewed identity, builtin-baseline, dependency, override, and optional current `configuration` members; reject root `features` and `default-features` because either can activate dependency edges outside the reviewed direct sets. Require exactly libjpeg-turbo, Exiv2, and Catch2 once in each relevant set and reject unnamed or additional entries. For each approved entry, permit only its reviewed properties, require JSON strings for names and versions, require the JSON Boolean `false` for dependency `default-features`, and require Exiv2's sole `xmp` feature as a one-element string array. Prove every boundary with isolated behavior-changing field, type-coercion, root-feature, and extra-dependency mutations.
- [x] Do not use a community overlay or a moving registry reference. Parse the real `vcpkg-configuration.json` authority surface plus the manifest's `configuration` field, reject the legacy `vcpkg-configuration` alias instead of treating it as a compatibility shim, and reject simultaneous standalone and embedded representations even when their objects are empty.

### Step 1.3: Pin effective first-party compiler policy

- [x] Create the dependency-free `BuildPolicyProbe.vcxproj` for Debug/Release x86/x64/ARM64 and implement `Test-EffectiveBuildPolicy.ps1` to build it with a binary log, inspect the resolved tool paths plus actual `ClCompile`/`Link` command lines, and inspect the x64 PE load configuration. Before creating shared build policy, run the verifier. Expected RED names the absent language, warning, security, reproducibility, and architecture-specific settings; the probe itself must compile far enough to produce inspectable evaluation evidence.
- [x] Implement read-only `Resolve-MSBuildToolchain.ps1`: resolve the latest installed complete stable Visual Studio 18 instance, map it to the exact `MSBuild.exe`, MSVC tool directory, `cl.exe`, and `link.exe`, and return one structured toolchain record without changing repository state. Implement mutating `New-MSBuildToolchainLock.ps1` separately: call the resolver, write its exact 14.51.x `VCToolsVersion` to absent `eng/toolchain-lock.json`, and refuse to replace an existing lock. Put only early selection properties in `Directory.Build.props`: `PlatformToolset=v145`, the exact locked `VCToolsVersion`, `PreferredToolArchitecture=x64`, `UseEnv=false`, `WindowsTargetPlatformMinVersion=10.0.19045.0`, `WindowsTargetPlatformVersion=10.0.28000.0`, `WindowsAppSDKSelfContained=false`, and structured `SpectreMitigation=Spectre`. Apply those selections only when `JpgSpinnerModernCppBuildPolicyEnabled=true`. The property defaults to true for new projects, while the still-shipping C++/CX project declares false in its pre-import `Globals` group because Microsoft's `/ZW` contract prohibits `/std:c++20` or later. This is a build-policy boundary during parallel replacement, not a compatibility adapter; deleting the legacy project at cutover deletes the exception. The explicit preferred host architecture prevents MSBuild's target-dependent x86-host default from selecting different `cl.exe`/`link.exe` paths for Win32 and ARM64. `UseEnv=false` keeps VC include/library selection under MSBuild and the selected toolset instead of replacing it with caller-owned `INCLUDE`, `LIB`, `LIBPATH`, and `PATH` values. The structured Spectre property—not a raw `/Qspectre` string—must both emit compiler instrumentation and select the installed architecture-specific mitigated runtime libraries.
- [x] Put compiler and linker policy in correctly ordered `ItemDefinitionGroup` entries in `Directory.Build.targets` or a shared property sheet explicitly imported after `Microsoft.Cpp.props`, and guard every group with `JpgSpinnerModernCppBuildPolicyEnabled=true`: `ClCompile` owns `LanguageStandard=stdcpp20`, `ConformanceMode=true`, warning level 4, warnings as errors, `/utf-8`, `/Zc:__cplusplus`, `/sdl`, and `/guard:cf`; `Link` owns `/guard:cf`, x64-only `/CETCOMPAT`, explicit Release `Optimization=MaxSpeed` (`/O2`), whole-program/link-time code generation, and reproducibility switches. Preserve inherited `AdditionalOptions`. Select `ProgramDatabase` (`/Zi`) rather than Edit and Continue (`/ZI`) in Debug because MSVC rejects `/ZI` together with `/guard:cf`. Classify third-party headers as external instead of suppressing first-party warnings.
- [x] Make repository policy parse the XML structure and reject compiler/linker metadata placed as free properties, missing inherited `%(AdditionalOptions)`, an unconditional `/CETCOMPAT`, or a missing/weakened modern-policy boundary. Resolve `Microsoft.Build.dll` from the same selected stable Visual Studio instance as `MSBuild.exe`, and use `ProjectRootElement.Properties` to require every case-insensitive early `Directory.Build.props` selection property exactly once in the canonical policy-conditioned root `PropertyGroup`, including declarations nested beneath `Choose`; require the legacy false declaration in `Globals` before its first import. Use MSBuild's own `-getProperty` and `-getItem` evaluation output to prove the C++/CX compile items do not receive `stdcpp20` while the modern probe does. Let repository evaluation supply `PlatformToolset`, `VCToolsVersion`, `WindowsTargetPlatformVersion`, and `UseEnv` to the probe instead of masking them with immutable command-line global properties; only the configuration/platform matrix and evidence paths are verifier inputs. Pass `-noAutoResponse` so an ambient `MSBuild.rsp` or repository-parent `Directory.Build.rsp` cannot inject global properties into the evidence build. Start effective verification without ambient `CL`, `_CL_`, `LINK`, `_LINK_`, `UseEnv`, `INCLUDE`, `EXTERNAL_INCLUDE`, `LIB`, or `LIBPATH` injection and with the child working directory pinned to the repository being verified. Select exactly one rendered task command that begins with the locked target `cl.exe` or `link.exe` path and contains the expected probe operand; never merge that command with FileTracker or other diagnostic lines. Parse its argument string according to Microsoft's C/C++ process-argument rules, preserve token order, normalize only the documented interchangeable `-` and `/` option specifiers, compare CL option names ordinally and LINK option names ordinal-ignore-case, retain option-specific argument semantics such as the v145 compiler's case-insensitive `cf[-]` argument beneath the case-sensitive `guard` name, and stop compiler-option inspection at the exact case-sensitive `/link` boundary. For every conflicting compiler/linker option family, require the rightmost effective member—covering C++20, conformance, warning level, warnings as errors, SDL, CFG, Spectre v1 mitigation, the updated `__cplusplus` macro, Release `/O2`, whole-program/link-time optimization, and x64 CET—rather than accepting a required token that an option to its right overrides. Treat `/utf-8` differently according to the locked compiler's D8016 contract: require that exact option and reject every separate `/source-charset:` or `/execution-charset:` option in either order because the representations are mutually incompatible, not a rightmost-precedence family. Treat bare `/LTCG` plus current `STATUS`, `NOSTATUS`, and `INCREMENTAL` arguments as enabling states, reject `OFF`, deprecated modes, and unknown arguments, and prove those decisions with real links. Reject CL or LINK `@command-file` arguments because their expanded token streams are not present in this dependency-free evidence path; retain the binary and diagnostic logs for investigation. Query MSBuild's evaluated `LibraryPath`, `WindowsTargetPlatformVersion`, and `UseEnv` values rather than scraping diagnostic prose; require SDK `10.0.28000.0` and reject effective `UseEnv=true`. Model LINK's documented library search order as command-line `/LIBPATH` directories followed by evaluated `LibraryPath`, and require the exact target-specific `lib\spectre\<architecture>` directory to precede the ordinary locked-toolset runtime directory. For x64, run `dumpbin /headers /loadconfig` and require the CFG image characteristic, `CF Instrumented`, `FID table present`, and CET compatibility as separate facts. Task 2 extends this already-executable probe evidence to each production-shaped project; configuration text alone never becomes final evidence.
- [x] Make repository `NuGet.config` independently clear both inherited `packageSources` and inherited `disabledPackageSources`, add only the enabled `https://api.nuget.org/v3/index.json` source, map `*` to that source, and contain no credentials. NuGet merges those collections separately, so clearing sources alone does not prevent a machine-level disabled `nuget.org` entry from disabling the repository's sole authority. Every documented and automated restore passes this file explicitly; repository policy rejects additional, disabled, HTTP, local, or credential-bearing feeds so a developer's machine configuration cannot change the resolved graph.
- [x] Add portable whitespace rules to `.editorconfig` and `.clang-format` with four-space indentation and a 120-column limit. Disable automatic include sorting where WinRT generated-header order is significant. Because Visual Studio's documented C++ EditorConfig surface does not support identifier-naming rules, enforce authored native names with standard clang-tidy checks at the root, then inherit a narrow `src/JpgSpinner.App/.clang-tidy` boundary: ignore only the ABI namespace segment `JpgSpinner`, require PascalCase public/protected WinRT methods, retain lower-camel private methods, and exclude generated projection headers. Prove both scopes with real LLVM fixtures; do not invent ignored `cpp_naming_*` EditorConfig keys.
- [x] Add `.vsconfig` with stable native Windows application, x86/x64 C++, ARM64 C++, Spectre libraries, CMake, vcpkg, NuGet, LLVM, Windows App SDK C++, and C++ AddressSanitizer components. Do not include preview components.

### Step 1.4: Deterministic discovery and freshness checks

- [x] Complete normal `Resolve-MSBuildToolchain.ps1` behavior using installed `vswhere.exe`: require Visual Studio major version 18, order eligible installations by product version descending, return one structured record for the resolved `MSBuild.exe` and exact compiler/linker paths, reject duplicate lock members before conversion, and fail if the existing `eng/toolchain-lock.json` `VCToolsVersion` is not installed exactly. Resolution never rewrites the lock or silently selects another servicing version. `New-MSBuildToolchainLock.ps1` remains the only command permitted to create the initial lock.
- [x] Implement `Test-DependencyFreshness.ps1` to compare pins with official NuGet metadata, the official vcpkg registry, and upstream GitHub stable releases. Reject duplicate manifest, snapshot, and live-response members before conversion. Use `Invoke-WebRequest` only to obtain the live textual response body, require the authority-specific raw root kind (object for vcpkg/NuGet and array for GitHub releases), then pass it through the same .NET 10 strict parser before PowerShell object conversion; never use an auto-deserializing transport at this trust boundary. Set both connection establishment and response-data operation timeouts so a connected server cannot stall the gate indefinitely between body reads. Before any freshness ordering, require every exact manifest pin to exist in its package authority: NuGet's package version index or vcpkg's selected baseline version database. Preserve vcpkg's exact `<version>#<port-version>` identity and ordering semantics: compare registry `port-version` after equal upstream components, omit only revision `#0`, and accept current override syntax such as `3.2.0#1`. It reports stable upgrades but never rewrites manifests or accepts prereleases. Treat a stable upstream release that is not yet present in the selected official package registry as explicit release-channel lag, not as an actionable unrestorable pin: report it separately, state only that no newer version is actionable through the approved package authorities, never add an overlay, source shim, or second dependency mechanism, and fail once the version is consumable from the approved authority.
- [x] Update `.gitignore` for `.vs/`, generated contents beneath `artifacts/`, `build/`, `out/`, vcpkg installation output, generated packages, test results, sanitizer reports, fuzz artifacts, and local signing material. Re-open only `*.spdx.json` beneath `artifacts/release/` so planned versioned release SBOMs remain reviewable without exposing package binaries or ordinary build logs. Never ignore NuGet lock files or reviewed fuzz corpus inputs.

### Step 1.5: Verify and commit

- [x] Run `pwsh -NoProfile -File scripts/Test-RepositoryPolicy.ps1`.
- [x] Run `pwsh -NoProfile -File scripts/tests/Test-ModernCppBuildPolicyScope.ps1`; expected GREEN proves evaluated C++/CX items are outside the C++20 policy and the modern probe remains inside it.

Expected GREEN: exit code 0 and a line naming every package and toolchain version that Task 1 actually parses. The Windows App SDK, C++/WinRT, and SDK BuildTools versions join this success report only in Task 2, after project references and clean locked restores make those pins verifiable evidence rather than planned values.

- [x] Run JSON and XML parsers over every created manifest, require the structural build-policy checks and all focused negative controls green, then run `Test-EffectiveBuildPolicy.ps1`. Include non-object JSON roots, duplicate early build selections, removal of each modern-policy scope guard, re-enabling the legacy C++/CX project, direct-consumer duplicate-lock and duplicate-override controls, live-response duplicate members, masked toolset selection, alternate effective SDK selection, ambient MSVC option/directory variables, ordinary-runtime-before-Spectre search ordering, both incompatible charset orderings, mixed-case compiler CFG controls, every current LTCG enabling state, `/LTCG:OFF`, and parsed load-configuration fixtures. Expected GREEN: every probe configuration selects the locked tool directory and exact Windows SDK `10.0.28000.0` from repository evaluation, rejects effective `UseEnv=true`, requires the rightmost semantic member of every precedence-based option family, and searches the matching Spectre-mitigated MSVC runtime before the ordinary locked-toolset runtime; x64 PE evidence includes the CFG and CET image characteristics plus the `CF Instrumented` and `FID table present` load-configuration flags.
- [x] Commit:

```powershell
git add .vsconfig .editorconfig .clang-format .clang-tidy src/JpgSpinner.App/.clang-tidy Directory.Build.props Directory.Build.targets "JPG Spinner/JPG Spinner.vcxproj" NuGet.config vcpkg.json vcpkg-triplets scripts eng docs/implementation docs/plans/2026-08-30-jpg-spinner-modernization.md .gitignore
git commit -m "build: pin the JPG Spinner 2.0 toolchain"
```

## Task 2: Scaffold the clean solution, build scripts, and test support

**Files:**

- Create: `JpgSpinner.sln`
- Create: `src/JpgSpinner.Domain/JpgSpinner.Domain.vcxproj`
- Create: `src/JpgSpinner.JpegTransformation/JpgSpinner.JpegTransformation.vcxproj`
- Create: `src/JpgSpinner.WindowsStorage/JpgSpinner.WindowsStorage.vcxproj`
- Create: `src/JpgSpinner.BatchProcessing/JpgSpinner.BatchProcessing.vcxproj`
- Create: `src/JpgSpinner.App/JpgSpinner.App.vcxproj`
- Create: `tests/TestSupport/TestSupport.vcxproj`
- Create: `tests/TestSupport/TemporaryDirectory.h`
- Create: `tests/TestSupport/TemporaryDirectory.cpp`
- Create: `tests/TestSupport/DeterministicJpegFixtureFactory.h`
- Create: `tests/TestSupport/DeterministicJpegFixtureFactory.cpp`
- Create: `tests/TestSupport/CoefficientDigest.h`
- Create: `tests/TestSupport/CoefficientDigest.cpp`
- Create: `tests/JpgSpinner.Domain.Tests/JpgSpinner.Domain.Tests.vcxproj`
- Create: `tests/JpgSpinner.JpegTransformation.Tests/JpgSpinner.JpegTransformation.Tests.vcxproj`
- Create: `tests/JpgSpinner.WindowsStorage.Tests/JpgSpinner.WindowsStorage.Tests.vcxproj`
- Create: `tests/JpgSpinner.BatchProcessing.Tests/JpgSpinner.BatchProcessing.Tests.vcxproj`
- Create: `tests/JpgSpinner.Presentation.Tests/JpgSpinner.Presentation.Tests.vcxproj`
- Create: `tests/TestData/README.md`
- Create: `scripts/Invoke-Build.ps1`
- Create: `scripts/Invoke-TestSuite.ps1`
- Create: `scripts/Test-NuGetResolution.ps1`
- Create if qualified by Step 2.2: `Directory.Packages.props`
- Modify: `scripts/Test-RepositoryPolicy.ps1`
- Modify: `scripts/Test-EffectiveBuildPolicy.ps1`

### Step 2.1: Extend policy validation and observe failure

- [ ] Parse `JpgSpinner.sln`; require all named projects, x86/x64/ARM64 configurations, and the designed project-reference direction.
- [ ] Reject a lower architecture layer referencing a higher layer, an ARM32 configuration, or more than one MSIX packaging project.
- [ ] Require `JpgSpinner.App` to be a stable C++ WinUI Blank App (Packaged) single-project MSIX project.
- [ ] Require production projects to use the locked `VCToolsVersion`, and require every unpackaged test executable that references Windows App SDK runtime types either to carry isolated test package identity or use the official test-only bootstrapper/auto-initializer. Reject bootstrapper files and initialization from the production project and package graph.
- [ ] Run the policy test.

Expected RED: the new solution/project assertions fail while Task 1 version assertions remain green.

### Step 2.2: Create projects without product behavior

- [ ] Create `JpgSpinner.sln` from the installed stable Visual Studio 2026 C++ WinUI Blank App (Packaged) template. Keep `JPG Spinner.sln` untouched as a temporary behavioral reference.
- [ ] Add static libraries for Domain, JPEG Transformation, Windows Storage, Batch Processing, and Test Support.
- [ ] Add Catch2 executable tests for Domain, JPEG Transformation, Windows Storage, Batch Processing, and Presentation.
- [ ] Define references exactly: JPEG → Domain; Storage → Domain; Batch → Domain/JPEG plus abstract storage capability; App → all modules. Test projects reference only their subject and Test Support.
- [ ] Extend `Test-EffectiveBuildPolicy.ps1` from the probe to every production and test project, then run it. Expected GREEN confirms the probe policy actually survives each C++/WinUI project’s evaluated import graph and command lines; a project-level override that removes a required setting must make the verifier RED in its focused negative-control test.
- [ ] Before adding Microsoft package references, implement `Test-NuGetResolution.ps1` to parse the exact `.vcxproj` files, require one coherent package-version strategy, restore into a clean isolated packages directory, generate/read lock files where supported, repeat with locked mode, and compare the complete resolved graph for x86, x64, and ARM64.
- [ ] Run the resolution verifier with absent package references. Expected RED: precise diagnostics that the required Windows App SDK, C++/WinRT, and Windows SDK BuildTools graph is unresolved; the verifier itself must parse the C++ projects successfully.
- [ ] First try versionless `PackageReference` elements with this exact `Directory.Packages.props` candidate:

```xml
<Project>
  <PropertyGroup>
    <ManagePackageVersionsCentrally>true</ManagePackageVersionsCentrally>
    <RestorePackagesWithLockFile>true</RestorePackagesWithLockFile>
    <RestoreLockedMode Condition="'$(ContinuousIntegrationBuild)' == 'true'">true</RestoreLockedMode>
  </PropertyGroup>
  <ItemGroup>
    <PackageVersion Include="Microsoft.WindowsAppSDK" Version="2.5.1" />
    <PackageVersion Include="Microsoft.Windows.CppWinRT" Version="3.0.260818.1" />
    <PackageVersion Include="Microsoft.Windows.SDK.BuildTools" Version="10.0.28000.2705" />
  </ItemGroup>
</Project>
```

- [ ] If clean locked restore proves identical graphs on the exact C++ project shape, retain central management. If only central version management fails, remove `Directory.Packages.props`, put the same exact immutable versions directly in every consuming `.vcxproj`, retain per-project lock files, and update repository policy to reject version drift. If the exact projects cannot enforce a locked restore at all, Task 2 remains RED and implementation stops for a supported toolchain resolution; do not invent a custom lock format, compatibility target, restore wrapper, or package shim.
- [ ] Enable vcpkg manifest mode and map each solution platform to its custom triplet.
- [ ] Run `Test-NuGetResolution.ps1` again. Expected GREEN: clean restore and clean locked restore resolve identical transitive graphs for each architecture. Commit every generated NuGet lock file when the qualified project system supports them.

### Step 2.3: Add build and test entry points

- [ ] `Invoke-Build.ps1` accepts only `Debug|Release` and `x86|x64|ARM64`, invokes resolved MSBuild with restoration and a binary log, and sets `ContinuousIntegrationBuild=true` in CI.
- [ ] `Invoke-TestSuite.ps1` accepts one project or `HeadlessAll`, an optional Catch2 test specification, architecture, configuration, and `Headless|Interactive` execution environment; writes JUnit XML and propagates each executable's exit code. `HeadlessAll` rejects UI-Automation/assistive-technology tags, while `Interactive` first requires `Test-InteractiveUiEnvironment.ps1` once that script exists. The singular noun names the suite-level orchestration responsibility precisely.
- [ ] Neither script changes machine-wide vcpkg integration.

### Step 2.4: Test the deterministic fixture contract first

- [ ] Before helper implementations, add tagged tests that demand a fixed 31×19 asymmetric RGB fixture in 4:4:4, 4:2:2, and 4:2:0 forms with a requested Exif orientation.
- [ ] Assert dimensions, component sampling factors, orientation tag, and reviewed SHA-256 values. Independently decode and inspect generated pixels before accepting the hashes.
- [ ] Demand that `TemporaryDirectory` creates a unique child beneath the test output root and removes only that owned directory at scope exit.
- [ ] Run the fixture-tagged tests.

Expected RED: compilation fails only because the named Test Support contracts are absent.

### Step 2.5: Implement minimum test support

- [ ] `TemporaryDirectory` uses a UUID, canonical path containment check, and a non-throwing destructor that exposes cleanup failure for explicit assertion. Comment why containment is rechecked before recursive deletion.
- [ ] Generate fixtures through the public libjpeg compression API with fixed tables, fixed quality, fixed scan choice, no timestamp, and an asymmetric pixel formula documented in the header.
- [ ] `CoefficientDigest` reads public libjpeg virtual coefficient arrays and serializes component index, block dimensions, and every signed coefficient into SHA-256. It must not call production transform code.
- [ ] Document each binary fixture’s provenance, license, intended defect, and expected invariant in `tests/TestData/README.md`.

### Step 2.6: Verify and commit

- [ ] Run:

```powershell
pwsh -NoProfile -File scripts/Invoke-Build.ps1 -Configuration Debug -Architecture x64
pwsh -NoProfile -File scripts/Invoke-TestSuite.ps1 -Project JpgSpinner.JpegTransformation.Tests -TestSpecification "[test-support]"
pwsh -NoProfile -File scripts/Test-NuGetResolution.ps1
pwsh -NoProfile -File scripts/Test-EffectiveBuildPolicy.ps1
pwsh -NoProfile -File scripts/Test-RepositoryPolicy.ps1
```

Expected GREEN: solution builds with the locked tool directory and effective compiler/linker settings, Test Support contract tests pass, clean NuGet restores reproduce one transitive graph, and policy validation confirms project direction, platforms, and production/test bootstrapper boundaries.

- [ ] Commit:

```powershell
git add JpgSpinner.sln src tests scripts
if (Test-Path -LiteralPath Directory.Packages.props) { git add -- Directory.Packages.props }
git commit -m "build: scaffold the WinUI 3 solution and test harness"
```

## Task 3: Specify orientation semantics and transform planning

**Files:**

- Create: `src/JpgSpinner.Domain/include/jpg_spinner/domain/ExifOrientation.h`
- Create: `src/JpgSpinner.Domain/include/jpg_spinner/domain/LosslessTransform.h`
- Create: `src/JpgSpinner.Domain/include/jpg_spinner/domain/EdgeHandlingPolicy.h`
- Create: `src/JpgSpinner.Domain/include/jpg_spinner/domain/OutputScanOrganization.h`
- Create: `src/JpgSpinner.Domain/include/jpg_spinner/domain/ImageDimensions.h`
- Create: `src/JpgSpinner.Domain/include/jpg_spinner/domain/JpegTransformRequest.h`
- Create: `src/JpgSpinner.Domain/include/jpg_spinner/domain/JpegTransformPlan.h`
- Create: `src/JpgSpinner.Domain/include/jpg_spinner/domain/JpegTransformPlanner.h`
- Create: `src/JpgSpinner.Domain/include/jpg_spinner/domain/JpegImageAnalysis.h`
- Create: `src/JpgSpinner.Domain/include/jpg_spinner/domain/JpegAnalysisFinding.h`
- Create: `src/JpgSpinner.Domain/include/jpg_spinner/domain/JpegResourceLimits.h`
- Create: `src/JpgSpinner.Domain/include/jpg_spinner/domain/ImageProcessingError.h`
- Create: `src/JpgSpinner.Domain/include/jpg_spinner/domain/ImageProcessingResult.h`
- Create: `src/JpgSpinner.Domain/src/ExifOrientation.cpp`
- Create: `src/JpgSpinner.Domain/src/JpegTransformPlanner.cpp`
- Create: `tests/JpgSpinner.Domain.Tests/ExifOrientationTests.cpp`
- Create: `tests/JpgSpinner.Domain.Tests/JpegTransformPlannerTests.cpp`
- Create: `tests/JpgSpinner.Domain.Tests/ImageProcessingResultTests.cpp`
- Create: `tests/JpgSpinner.Domain.Tests/DomainStringMakers.h`

### Step 3.1: RED for the eight-value semantic mapping

- [ ] Write this table-driven test before the enum or mapping exists:

```cpp
struct OrientationTransformCase final
{
    ExifOrientation sourceOrientation;
    LosslessTransform expectedTransform;
};

TEST_CASE("every Exif orientation maps to the transform that produces TopLeft", "[domain][orientation]")
{
    constexpr std::array cases{
        OrientationTransformCase{ExifOrientation::TopLeft, LosslessTransform::None},
        OrientationTransformCase{ExifOrientation::TopRight, LosslessTransform::FlipHorizontal},
        OrientationTransformCase{ExifOrientation::BottomRight, LosslessTransform::Rotate180},
        OrientationTransformCase{ExifOrientation::BottomLeft, LosslessTransform::FlipVertical},
        OrientationTransformCase{ExifOrientation::LeftTop, LosslessTransform::Transpose},
        OrientationTransformCase{ExifOrientation::RightTop, LosslessTransform::Rotate90Clockwise},
        OrientationTransformCase{ExifOrientation::RightBottom, LosslessTransform::Transverse},
        OrientationTransformCase{ExifOrientation::LeftBottom, LosslessTransform::Rotate270Clockwise},
    };

    const auto testCase = GENERATE_REF(Catch::Generators::from_range(cases));

    DYNAMIC_SECTION(
        "source "
        << Catch::StringMaker<ExifOrientation>::convert(testCase.sourceOrientation))
    {
        REQUIRE(losslessTransformFor(testCase.sourceOrientation) == testCase.expectedTransform);
    }
}
```

- [ ] Before this test compiles, add test-only `Catch::StringMaker` specializations for `ExifOrientation`, `LosslessTransform`, `EdgeHandlingPolicy`, `OutputScanOrganization`, `ImageProcessingErrorCode`, and `JpegAnalysisFindingCode`. Require their strings to use the semantic enumerator names rather than underlying integers so a failing generated case identifies the behavior immediately.
- [ ] Add parsing tests for raw values 0, 9, and 255 returning `std::nullopt`; never coerce an invalid tag to `TopLeft`.
- [ ] Run only `[domain][orientation]` and observe a missing-contract compilation failure.

### Step 3.2: GREEN with precise value types

- [ ] Implement the enums exactly as specified in the design document.
- [ ] Implement `tryParseExifOrientation(std::uint16_t)` and `losslessTransformFor(ExifOrientation)` as total, side-effect-free functions.
- [ ] Comment the row/column semantics and why `LeftTop` and `RightBottom` are transpose/transverse rather than rotations.
- [ ] Run the focused test, then all Domain tests.

### Step 3.3: RED for dimensions and MCU completeness

- [ ] Add tests proving transforms 5–8 swap width and height and transforms 1–4 do not.
- [ ] Add table tests using sampling-derived MCU dimensions. Prove a perfect 90-degree transform is rejected when the affected source edge contains a partial MCU and accepted when aligned.
- [ ] Prove `TrimPartialMinimumCodedUnits` returns the exact smaller dimensions and `willDiscardEdgePixels=true`.
- [ ] Prove `PreserveSource`, `SequentialDct`, and `ProgressiveDct` remain distinct values; do not use Boolean scan semantics.
- [ ] Run planner tests and observe missing planner failure.

### Step 3.4: GREEN with the smallest planner

- [ ] Define immutable aggregate types with strong width/height semantics and checked pixel-count multiplication.
- [ ] Before implementing `ImageProcessingResult<TValue>`, add focused tests for success, failure, move-only values, `valueIfPresent()`/`errorIfPresent()` returning only the active alternative, and construction/move operations that cannot leave the result empty. Expected RED is the missing result contract.
- [ ] Have `JpegTransformPlanner` return the project-owned discriminated result `ImageProcessingResult<JpegTransformPlan>`. Implement it from stable C++20 discriminated-storage facilities with an enforced never-empty invariant. It exposes only domain-named construction and inspection operations, not monadic methods or a `std::expected`-compatible facade; do not add `tl::expected`, Boost.Outcome, Abseil, or inject anything into `namespace std`.
- [ ] Define the complete `ImageProcessingErrorCode` taxonomy from the design now so later modules extend diagnostic context without adding vague catch-all codes.
- [ ] Define `JpegAnalysisFindingCode` with `ExifXmpOrientationConflict`, `EmbeddedThumbnailRemovalRequired`, and `PartialMinimumCodedUnitTrimRequired`; keep reviewable non-fatal facts separate from errors.
- [ ] Reject over-limit dimensions before multiplication can overflow.
- [ ] Make the plan contain transform, source/output dimensions, edge policy, scan organization, MCU geometry, and explicit discarded-edge extents.
- [ ] Document the libjpeg-turbo perfect-transform rule beside the enforcing branch.

### Step 3.5: Verify and commit

- [ ] Run `pwsh -NoProfile -File scripts/Invoke-TestSuite.ps1 -Project JpgSpinner.Domain.Tests -TestSpecification "[domain]"`.

Expected GREEN: all mappings, invalid tags, axis swaps, bounds, perfect-transform cases, trim dimensions, and scan values pass.

- [ ] Commit:

```powershell
git add src/JpgSpinner.Domain tests/JpgSpinner.Domain.Tests
git commit -m "feat: define lossless orientation transform semantics"
```

## Task 4: Build the bounds-checked JPEG segment scanner and fuzz entry point

**Files:**

- Create: `src/JpgSpinner.JpegTransformation/src/internal/JpegMarkerInventory.h`
- Create: `src/JpgSpinner.JpegTransformation/src/internal/JpegSegmentScanner.h`
- Create: `src/JpgSpinner.JpegTransformation/src/internal/JpegSegmentScanner.cpp`
- Create: `tests/JpgSpinner.JpegTransformation.Tests/JpegSegmentScannerTests.cpp`
- Create: `fuzz/JpgSpinner.JpegSegmentScanner.Fuzz/JpgSpinner.JpegSegmentScanner.Fuzz.vcxproj`
- Create: `fuzz/JpgSpinner.JpegSegmentScanner.Fuzz/FuzzJpegSegmentScanner.cpp`
- Modify: `JpgSpinner.sln`

### Step 4.1: RED on minimal valid and truncated structures

- [ ] Hand-construct byte vectors for SOI/EOI, baseline SOF, APP1 Exif, APP1 XMP, APP2 ICC, APP2 MPF, COM, SOS, stuffed `0xFF00`, restart markers, and EOI.
- [ ] Require a minimal valid baseline stream to produce exact marker offsets/dimensions and every truncation point of a length-bearing marker to return `MalformedJpegStructure` without reading past the span.
- [ ] Use a Catch2 generator for each truncation length from zero through the complete marker, with `DYNAMIC_SECTION` naming the marker kind and exact truncation offset.
- [ ] Run `[jpeg][scanner]`; expected RED is absent scanner compilation.

### Step 4.2: GREEN with checked marker walking

- [ ] Accept `std::span<const std::byte>` and return an immutable inventory containing checked integer offset/length pairs plus the scanned source length. Do not retain spans or pointers, so the inventory cannot dangle.
- [ ] Centralize big-endian 16-bit reads in one checked function. Subtract length bytes only after proving `declaredLength >= 2` and full containment.
- [ ] Handle entropy-coded data as a distinct state, including byte stuffing and restart markers; never interpret entropy bytes as length-bearing metadata.
- [ ] Inventory SOF process, dimensions, component sampling, SOS count, Exif, standard/extended XMP, ICC chunks, MPF, unknown APPn, and COM.
- [ ] Explain every offset addition and entropy-state transition with an invariant comment.

### Step 4.3: RED/GREEN every resource and semantic rejection

- [ ] Add one focused failing test, then minimum implementation, for each condition:
  - encoded length at and one byte beyond 512 MiB through the numeric resource-policy function, plus scanner integration with a smaller injected immutable test limit;
  - metadata aggregate at and one byte beyond 32 MiB through the numeric resource-policy function, plus scanner integration with a smaller injected immutable test limit;
  - pixel count at and one pixel beyond 268,435,456;
  - progressive scan count at 100 and 101;
  - ICC chunks with duplicate, missing, zero, or out-of-range sequence numbers or inconsistent total counts; accept APP2 marker reordering and reconstruct logical order by the authoritative 1-based sequence number, matching the locked libjpeg-turbo 3.2 public ICC reader;
  - extended-XMP uppercase GUID syntax plus complete, missing, duplicate, overlapping, wrong-GUID, wrong-full-length, and out-of-range chunk envelopes; accept APP1 marker reordering and reconstruct logical order by the Adobe-defined chunk offset. The scanner does not parse RDF; Task 6 extracts `xmpNote:HasExtendedXMP` through Exiv2 and correlates it with this structural inventory;
  - MPF detection;
  - APP11 JUMBF detection, C2PA 2.4 manifest-store detection, and distinction from non-C2PA JUMBF; external C2PA XMP references are interpreted through Exiv2 in Task 6;
  - exact EOI termination and a checked range containing every appended byte, including zero-valued bytes; semantic interpretation of Motion Photo/container XMP belongs to Task 6;
  - 8-bit and 12-bit lossy precision plus rejection of lossless predictive, hierarchical, invalid-precision, unknown SOF, and unsupported component organizations;
  - duplicate SOF, missing SOI, missing EOI, impossible length, and offset overflow.
- [ ] Keep `MultiPictureJpegNotSupported` distinct from corruption.
- [ ] Return `ContentCredentialsWouldBeInvalidated` for C2PA and `UnsupportedJumbfMetadata` for other JUMBF; neither marker class may reach transformation or generic unknown-marker preservation.
- [ ] Expose every byte after the first EOI through the immutable inventory, without interpreting marker-shaped bytes in appended assets. Scan success is structural evidence, not permission to transform. Task 6 owns the rejection decision through the existing Exiv2 boundary, avoiding a second XML parser.

### Step 4.4: Add the fuzz target

- [ ] Add `LLVMFuzzerTestOneInput` that calls only `JpegSegmentScanner` under production limits.
- [ ] Assert no exception crosses the C ABI, no successful offset/length range lies outside the recorded input length, and rescanning successful input produces the same inventory.
- [ ] Before relying on the fuzz configuration, add an installed-toolchain smoke target whose one-input corpus proves the locked x64 MSVC toolset accepts `/fsanitize=fuzzer` with `/fsanitize=address`, links the correct `LLVMFuzzerTestOneInput` entry point, runs, and reports a deliberately seeded crash in a disposable negative-control build. Remove the negative control after RED evidence, then keep the non-crashing smoke.
- [ ] Build with AddressSanitizer and libFuzzer in a dedicated x64 configuration; do not advertise ARM64 fuzz execution without separate toolchain proof.
- [ ] Seed with minimal test structures, never user photos.

### Step 4.5: Verify and commit

- [ ] Run scanner tests, a full Debug x64 build, and a 60-second fuzz smoke. Require no crash, leak, hang, or excessive allocation.
- [ ] Commit:

```powershell
git add src/JpgSpinner.JpegTransformation tests/JpgSpinner.JpegTransformation.Tests fuzz JpgSpinner.sln
git commit -m "feat: validate JPEG structure before codec processing"
```

## Task 5: Implement coefficient-exact transforms through TurboJPEG 3

Implementation clarification (2026-10-03): `TJPARAM_MAXMEMORY=512` is 512 MiB,
as established by the pinned upstream implementation, not decimal MB. An axis
exchange also transposes frequency-indexed quantization tables. The x64 ASan
suite uses a fourth, exact policy-validated `x64-windows-static-md-asan` triplet
and Release configuration so every dependency shares MSVC's annotation ABI;
ordinary application triplets remain unchanged. Do not disable container
annotations to link ordinary libraries into the sanitizer executable. See the
[Task 5 evidence record](../implementation/2026-10-03-modernization-task-5-log.md).

**Files:**

- Create: `src/JpgSpinner.JpegTransformation/src/internal/LibJpegTurboCoefficientTransformer.h`
- Create: `src/JpgSpinner.JpegTransformation/src/internal/LibJpegTurboCoefficientTransformer.cpp`
- Create: `src/JpgSpinner.JpegTransformation/src/internal/TurboJpegResourceLimits.h`
- Create: `tests/JpgSpinner.JpegTransformation.Tests/LibJpegTurboCoefficientTransformerTests.cpp`
- Create: `tests/JpgSpinner.JpegTransformation.Tests/TurboJpegResourceLimitTests.cpp`
- Modify: `src/JpgSpinner.JpegTransformation/JpgSpinner.JpegTransformation.vcxproj`

### Step 5.1: RED on all asymmetric transformations

- [ ] Generate non-square, asymmetric fixtures for 4:4:4, 4:2:2, and 4:2:0 sampling, aligned to perfect MCU boundaries, at both 8-bit and 12-bit lossy precision.
- [ ] Include grayscale, RGB/YCbCr, and CMYK/YCCK sources. Require no component-count or colorspace conversion.
- [ ] Extract source DCT blocks independently with public libjpeg APIs.
- [ ] For each orientation, hand-derive expected block-coordinate permutation and horizontal/vertical AC coefficient sign changes in test code. The expectation must not call production mapping or transform routines.
- [ ] Assert exact coefficient equality component by component, exact dimensions, and quantization-table preservation. Do not require byte-identical Huffman tables because an equivalent entropy table does not change coefficient fidelity.
- [ ] Run `[jpeg][transform]`; expected RED is absent coefficient-transformer compilation.

### Step 5.2: GREEN using only the public TurboJPEG API

- [ ] Define the internal `LibJpegTurboCoefficientTransformer::transformCoefficients` around immutable bytes, `JpegTransformPlan`, and a bounded output sink. Application callers must not see this internal seam.
- [ ] Own `tjhandle` in narrow RAII; always call `tj3Destroy`.
- [ ] First write tests proving that `JpegResourceLimits::maximumEncodedFileLengthBytes` is enforced before codec parsing and is not used as TurboJPEG memory configuration. Separately require `TurboJpegResourceLimits::maximumIntermediateBufferMemoryMebibytes=512` to map unchanged to `TJPARAM_MAXMEMORY`; document and test that TurboJPEG interprets this value as 536,870,912 bytes (512 MiB) of intermediate-buffer budget and not an encoded-file-size limit.
- [ ] Before input parsing, set `TJPARAM_STOPONWARNING=1`, `TJPARAM_MAXMEMORY` from the mebibyte adapter limit, `TJPARAM_MAXPIXELS=268435456`, `TJPARAM_SCANLIMIT=100`, and `TJPARAM_SAVEMARKERS=0`.
- [ ] Map each `LosslessTransform` to the exact `TJXOP` and edge policy to `TJXOPT_PERFECT` or `TJXOPT_TRIM`. Set `TJXOPT_COPYNONE` on every `tjtransform`; this operation-local marker prohibition is deliberate defense in depth with `TJPARAM_SAVEMARKERS=0`.
- [ ] Call `tj3Transform`; release TurboJPEG output through `tj3Free` RAII on every path. No codec-owned buffer escapes.
- [ ] Comment why the C call is non-interruptible and cancellation is checked immediately before and after it.

### Step 5.3: RED/GREEN perfect edges and scan organization

- [ ] Test each transform family on partial horizontal and vertical MCUs: perfect policy returns `PerfectCoefficientTransformUnavailable`; trim policy returns exact planned dimensions.
- [ ] Test sequential/Huffman, progressive/Huffman, sequential/arithmetic, and progressive/arithmetic sources with `PreserveSource`, `SequentialDct`, and `ProgressiveDct` separately.
- [ ] After reading the source header, set `TJXOPT_PROGRESSIVE` and `TJXOPT_ARITHMETIC` explicitly on each transform according to the reviewed plan. Do not assume caller-set `TJPARAM_PROGRESSIVE` or `TJPARAM_ARITHMETIC` survives another header/transform operation. Preserve or geometrically reconcile restart intervals; do not invent restart markers silently.
- [ ] Add a marker-ownership regression fixture containing Exif, XMP, ICC, APP13, unknown APPn, and COM markers. Require the raw TurboJPEG result to contain none of those copied markers before the application reconciler runs, proving `TJXOPT_COPYNONE` prevents duplication.
- [ ] Return `UnsupportedJpegCodingProcess`, `UnsupportedJpegSamplePrecision`, or `UnsupportedJpegComponentOrganization` when the public interface cannot preserve the source; never convert silently.
- [ ] Elevate codec warnings to structured failure and require zero output bytes on failure.

### Step 5.4: Verify and commit

- [ ] Run transform and Domain suites under normal builds, then the transform suite under AddressSanitizer.
- [ ] Require coefficient equality for every transform and sampling geometry.
- [ ] Commit:

```powershell
git add src/JpgSpinner.JpegTransformation tests/JpgSpinner.JpegTransformation.Tests
git commit -m "feat: transform JPEG coefficients with libjpeg-turbo"
```

## Task 6: Reconcile Exif, XMP, ICC, thumbnails, and marker payloads

**Files:**

- Create: `src/JpgSpinner.JpegTransformation/include/jpg_spinner/jpeg/JpegImageAnalyzer.h`
- Create: `src/JpgSpinner.JpegTransformation/src/JpegImageAnalyzer.cpp`
- Create: `src/JpgSpinner.JpegTransformation/src/internal/MetadataReconciler.h`
- Create: `src/JpgSpinner.JpegTransformation/src/internal/MetadataReconciler.cpp`
- Create: `tests/JpgSpinner.JpegTransformation.Tests/MetadataReconcilerTests.cpp`
- Create: `tests/JpgSpinner.JpegTransformation.Tests/ExtendedXmpPolicyTests.cpp`
- Create: `tests/JpgSpinner.JpegTransformation.Tests/JpegImageAnalyzerTests.cpp`

### Step 6.1: RED for truthful metadata after rotation

- [ ] Build a fixture containing Exif orientation 6, Exif dimensions, mapped XMP orientation/dimensions, unrelated Exif/XMP values, IPTC 2025.1 Core/Extension rights and AI-disclosure properties, IPTC IIM/Photoshop APP13, deterministic Exif/JFIF/JFXX/XMP/Photoshop thumbnails, split ICC chunks, COM, and an unrelated unknown APP marker.
- [ ] After 90-degree clockwise transformation require canonical orientation 1, swapped derived dimensions, unchanged unrelated properties, identical assembled ICC SHA-256, preserved COM/non-thumbnail APP13 order, and no Exif/JFIF/JFXX/XMP/Photoshop derived preview.
- [ ] Require MPF input to return `MultiPictureJpegNotSupported` before mutation.
- [ ] Run `[jpeg][metadata]`; expected RED is absent reconciler compilation.

### Step 6.2: GREEN with Exiv2 and an explicit marker policy

- [ ] Parse supported Exif/XMP through public Exiv2 0.28.9 APIs only.
- [ ] Read the standard packet's `xmpNote:HasExtendedXMP` property through Exiv2, compare its value byte-for-byte with the scanner-validated uppercase extended-XMP GUID, and reject a missing or mismatched property as `MalformedImageMetadata`. Do not locate the property with substring, regular-expression, or application-owned XML parsing.
- [ ] Feed Exiv2 only isolated, owned Exif or standard-XMP payload bytes. A completely reassembled extended-XMP payload may be passed to the public XMP parser for read-only property inspection; never serialize that extension through Exiv2, never give Exiv2 the complete JPEG container, and never accept a JPEG serialization from it. `JpegSegmentScanner` and the application-owned reconciler exclusively own marker framing, source-relative ordering, unknown APPn/COM payloads, ICC chunks, and extended-XMP chunks. The JPEG Transformation module receives no path and cannot widen AppContainer file authority or reopen a user file behind the transaction engine.
- [ ] Update every present orientation/dimension representation required by CIPA DC-008-Translation-2026 and DC-010-2026; do not invent unrelated tags.
- [ ] Before accessing an IPTC 2025.1 property whose namespace is not built into Exiv2 0.28.9, call the public namespace-registration API with the exact official URI/prefix. Add a table-driven test for AI Prompt Information, AI Prompt Writer Name, AI System Used, and AI System Version Used that observes the unregistered failure first, then proves read/write after registration without changing rights, licensing, or disclosure values.
- [ ] Remove Exif IFD1, JFIF/JFXX, XMP `xmp:Thumbnails`, and Photoshop IRB thumbnail resources whenever pixels are transformed; preserve JFIF density/version and all unrelated resources. Preserve a valid thumbnail only for `LosslessTransform::None`.
- [ ] If a known preview container cannot be parsed and rewritten safely, return `UnsupportedEmbeddedPreviewMetadata`; never leave a stale preview or discard the whole metadata container silently.
- [ ] Preserve assembled ICC bytes exactly and split them into legal APP2 chunks.
- [ ] Preserve valid unknown APPn and COM payload bytes exactly in source-relative order.
- [ ] Preserve IPTC IIM/Photoshop APP13 bytes when no thumbnail resource must be removed. Where a standard XMP packet must be reserialized to update derived facts, require semantic equality for every unrelated IPTC/XMP value; rights, licensing, and AI-disclosure values must remain unchanged.
- [ ] Explain in code why MPF and invalid authoritative metadata are rejected rather than silently dropped.
- [ ] Explain why C2PA/JUMBF is rejected: coefficient changes invalidate asset bindings, and this app owns no signing identity with which to create a truthful update manifest.

### Step 6.3: RED/GREEN malformed and absent metadata

- [ ] Add one cycle each for absent orientation, invalid orientation, Exif-only, XMP-only, conflicting Exif/XMP, malformed TIFF offsets, malformed RDF, duplicate ICC, and maximum legal metadata.
- [ ] Add focused Extended XMP cycles for a complete packet, missing/duplicate/overlapping chunks, accepted physical chunk reordering, GUID mismatch, wrong full length, an unaffected extension packet, an unparseable extension, and orientation/dimension/thumbnail facts held in the extension. Reassemble complete chunks only for read-only property inspection. Preserve every complete unaffected chunk byte-for-byte only when parsing proves the affected facts are absent and the standard packet remains truthful. Return `ExtendedXmpMutationNotSupported` before transformation when inspection is ambiguous or the requested operation would require changing extension-held facts; never partially rewrite or silently drop the extension.
- [ ] Apply one rule: valid Exif orientation is authoritative; otherwise valid XMP is used. Surface a conflict during analysis, then canonicalize both only after the reviewed transform succeeds.
- [ ] Reject an invalid authoritative value; never pretend it is `TopLeft`.

### Step 6.4: RED/GREEN the deep image-analyzer module

- [ ] Inspect C2PA external-manifest XMP references through Exiv2 and return `ContentCredentialsWouldBeInvalidated` before transformation. APP11 manifest-store recognition remains the scanner's bounded envelope responsibility.
- [ ] Correlate the scanner's exact trailing-data range with Motion Photo/container properties read through Exiv2. Return `MotionPhotoNotSupported` for a recognized Motion Photo and `UnsupportedTrailingPayload` for other unexplained appended bytes before any transform. Do not infer disposable padding from byte values or a Motion Photo solely from an XMP flag. Follow [Motion Photo format 1.0](https://developer.android.com/media/platform/motion-photo-format) for primary-item padding and positive secondary-item lengths; [T.81 B.2.1](https://www.w3.org/Graphics/JPEG/itu-t81.pdf) defines the JPEG EOI boundary. Responsibility clarification checked 2026-09-30.
- [ ] Before composing the analyzer, add a failing `JpegImageAnalyzer` test proving one `analyze` call returns dimensions, sampling, coding process, authoritative orientation, exact transform plan, support status, and typed findings without exposing marker or Exiv2 types.
- [ ] Implement the deep analyzer with the internal scanner, Exiv2 metadata reader, and Domain planner. Tests use real deterministic JPEGs; do not create an analyzer interface or fake when no behavior varies.

### Step 6.5: Verify and commit

- [ ] Run scanner, transform, and metadata suites normally and under AddressSanitizer.
- [ ] Prove no Exiv2 exception escapes the owning module interface; translate it into a precise domain error with redacted context.
- [ ] Commit:

```powershell
git add src/JpgSpinner.JpegTransformation tests/JpgSpinner.JpegTransformation.Tests
git commit -m "feat: preserve and reconcile JPEG metadata"
```

## Task 7: Expose one deep transform-and-validate interface

**Files:**

- Create: `src/JpgSpinner.Domain/include/jpg_spinner/domain/ValidatedJpegOutput.h`
- Create: `src/JpgSpinner.JpegTransformation/include/jpg_spinner/jpeg/JpegTransformationEngine.h`
- Create: `src/JpgSpinner.JpegTransformation/include/jpg_spinner/jpeg/LibJpegTurboTransformationEngine.h`
- Create: `src/JpgSpinner.JpegTransformation/src/LibJpegTurboTransformationEngine.cpp`
- Create: `src/JpgSpinner.JpegTransformation/src/internal/JpegOutputValidator.h`
- Create: `src/JpgSpinner.JpegTransformation/src/internal/JpegOutputValidator.cpp`
- Create: `tests/JpgSpinner.JpegTransformation.Tests/JpegOutputValidatorTests.cpp`
- Create: `tests/JpgSpinner.JpegTransformation.Tests/JpegTransformationEngineTests.cpp`
- Create: `tests/TestSupport/DeterministicJpegTransformationEngine.h`
- Create: `tests/TestSupport/DeterministicJpegTransformationEngine.cpp`

### Step 7.1: RED for every validation invariant

- [ ] Start from a known-good transformed fixture, mutate one property at a time, and require `OutputValidationFailed` for truncation, wrong dimensions, wrong scan organization, wrong Huffman/arithmetic mode, decode warning/failure, non-canonical Exif, non-canonical standard XMP, stale derived dimensions, changed ICC, missing/reordered preserved marker, changed extended-XMP chunk/GUID/offset, stale thumbnail, and residual MPF.
- [ ] Require a stable symbolic validation-rule value in error context, never localized prose.
- [ ] Run `[jpeg][validator]`; expected RED is absent validator compilation.

### Step 7.2: GREEN with independent structural and decode validation

- [ ] Reuse the internal `JpegSegmentScanner` for outer structure and limits, then parse metadata through the internal metadata module interface.
- [ ] Fully decode 8-bit and 12-bit sources into the correctly typed bounded discard sink for grayscale/RGB/YCbCr/CMYK/YCCK. Compute row storage with checked arithmetic; do not allocate a full uncompressed image or perform color conversion merely for validation.
- [ ] Compare exact planned dimensions, process/scan organization, marker inventory, ICC hash, and derived fields.
- [ ] Keep `JpegOutputValidator` at an internal seam. Return the move-only Domain value `ValidatedJpegOutput`, which owns output bytes, SHA-256, dimensions, scan organization, metadata evidence, and preserved-marker evidence required by commit.
- [ ] Pass owned request/source values across coroutine suspension and move `ValidatedJpegOutput` into transaction execution; no span, WinRT borrow, or codec buffer may outlive its documented synchronous call.
- [ ] Comment why validation consumes an immutable completed byte sequence through an independent reader and why storage later re-hashes the closed staged file.

### Step 7.3: RED/GREEN the external module interface

- [ ] Before the production adapter exists, write tests through `JpegTransformationEngine::createValidatedOutput` for all eight transforms, perfect/trim, metadata reconciliation, validator rejection, cancellation before/after the native call, and structured native failures.
- [ ] Compose `LibJpegTurboCoefficientTransformer`, `MetadataReconciler`, and `JpegOutputValidator` behind `LibJpegTurboTransformationEngine`. The application learns one operation and cannot reorder internal phases.
- [ ] Provide a deterministic test adapter for Batch tests. Its output is fixed by the request and it records concurrency only as observable test state; do not expose internal production calls.

### Step 7.4: Verify and commit

- [ ] Run all JPEG Transformation tests; require the good fixture to pass and each one-property corruption to fail for its exact rule.
- [ ] Run scanner, transform, metadata, and validator under AddressSanitizer.
- [ ] Commit:

```powershell
git add src/JpgSpinner.Domain src/JpgSpinner.JpegTransformation tests/JpgSpinner.JpegTransformation.Tests
git commit -m "feat: validate transformed JPEGs before commit"
```

## Task 8: Calculate authoritative source revisions

**Files:**

- Create: `src/JpgSpinner.Domain/include/jpg_spinner/domain/SourceFileRevision.h`
- Create: `src/JpgSpinner.WindowsStorage/include/jpg_spinner/storage/SourceFileRevisionCalculator.h`
- Create: `src/JpgSpinner.WindowsStorage/src/SourceFileRevisionCalculator.cpp`
- Create: `tests/JpgSpinner.WindowsStorage.Tests/SourceFileRevisionCalculatorTests.cpp`

### Step 8.1: RED on revisions of real files

- [ ] Use `TemporaryDirectory` and real files, not mocked streams.
- [ ] Require a revision to contain encoded length, normalized UTC last-write instant, and hand-checked SHA-256.
- [ ] Modify bytes without changing length and restore the timestamp; require the SHA-256 difference to make revisions unequal.
- [ ] Replace a file between open and hash completion through a controlled test barrier; require a structured change/access error, never a digest of mixed content.
- [ ] Run `[storage][revision]`; expected RED is absent calculator compilation.

### Step 8.2: GREEN with streamed SHA-256

- [ ] Implement hashing with CNG/BCrypt RAII handles or an AppContainer-compatible Windows cryptography API; do not add another cryptography package.
- [ ] Hash through a fixed bounded buffer with checked length accumulation and cancellation checks between reads.
- [ ] Capture file metadata before and after streaming. If length or timestamp differs, discard the digest and return `SourceChangedAfterAnalysis`.
- [ ] Translate access denied, sharing violation, cancellation, and I/O failure to structured error context.
- [ ] Explain why length/time aid diagnostics but SHA-256 is authoritative at commit.

### Step 8.3: Verify and commit

- [ ] Run the revision suite on x64 and x86 and run MSVC static analysis on the calculator.
- [ ] Require zero handle-lifetime or arithmetic warnings.
- [ ] Commit:

```powershell
git add src/JpgSpinner.Domain src/JpgSpinner.WindowsStorage tests/JpgSpinner.WindowsStorage.Tests
git commit -m "feat: detect source changes with authoritative revisions"
```

## Task 9: Execute non-destructive corrected-copy transactions

**Files:**

- Create: `src/JpgSpinner.Domain/include/jpg_spinner/domain/OutputDisposition.h`
- Create: `src/JpgSpinner.Domain/include/jpg_spinner/domain/TraversalScope.h`
- Create: `src/JpgSpinner.WindowsStorage/include/jpg_spinner/storage/ImageFileTransactionEngine.h`
- Create: `src/JpgSpinner.WindowsStorage/include/jpg_spinner/storage/AppContainerImageFileTransactionEngine.h`
- Create: `src/JpgSpinner.WindowsStorage/src/AppContainerImageFileTransactionEngine.cpp`
- Create: `src/JpgSpinner.WindowsStorage/src/internal/RecoverableImageFileTransaction.h`
- Create: `src/JpgSpinner.WindowsStorage/src/internal/RecoverableImageFileTransaction.cpp`
- Create: `tests/JpgSpinner.WindowsStorage.Tests/ImageFileTransactionTests.cpp`
- Create: `tests/TestSupport/DeterministicImageFileTransactionEngine.h`
- Create: `tests/TestSupport/DeterministicImageFileTransactionEngine.cpp`

### Step 9.1: RED on stage ownership and copy commit

- [ ] Use real temporary source/output directories and a `ValidatedJpegOutput` produced by the real test fixture pipeline.
- [ ] Require stage name `.jpg-spinner-staged-<transaction-guid>.jpg` and uniqueness across repeated transactions. For copy output it lives beside the final file in the unique batch tree; for replacement it lives beside the original.
- [ ] Require `CreateCorrectedCopy` to commit only after validation to `JPG Spinner Output/<batch-directory>/<relative-source-path>`.
- [ ] Require unique batch roots to prevent collisions; do not add ambiguous numeric suffixes to source filenames.
- [ ] Cancel before stage creation and after the stage hash check; require unchanged source, absent destination, and removal of only the owned stage.
- [ ] Run `[storage][transaction][copy]`; expected RED is absent transaction compilation.

### Step 9.2: GREEN with a deep transaction interface

- [ ] Define `ImageFileTransactionEngine::execute(request, validatedJpegOutput, cancellationToken)` as the only application operation for a new transaction. It returns a terminal result and never exposes journal/state-machine methods.
- [ ] Put per-file phase transitions in internal `RecoverableImageFileTransaction`; privately retain transaction UUID, source identity/revision, stage identity, destination, and state.
- [ ] Write stage output exclusively, request the strongest supported flush, close all handles, SHA-256 the staged bytes through a new read handle, and require equality with `ValidatedJpegOutput`. This byte-equality proof ensures the closed staged file is the independently validated output without treating a WinRT flush as a physical-media durability guarantee.
- [ ] Recalculate source revision after staged-output hash verification and immediately before commit. Mismatch returns `SourceChangedAfterAnalysis`.
- [ ] Move the stage to the unique copy destination only after validation. Never overwrite an existing destination.
- [ ] Make cleanup idempotent and prove canonical containment before deleting the owned stage.
- [ ] Explain handle-close, re-hash, move, and cleanup boundaries in comments.

### Step 9.3: RED/GREEN each storage fault

- [ ] Add a narrow `FileTransactionOperations` seam inside Windows Storage only when Windows cannot deterministically produce the failure. Name operations by effect; do not expose a generic filesystem mock.
- [ ] One cycle at a time, inject stage-create, write, flush, close, reopen, staged-hash verification, source re-hash, destination-create, and move failures.
- [ ] A staged hash unequal to `ValidatedJpegOutput` returns `StagedOutputHashMismatch`; it never reaches commit.
- [ ] After every failure require the original SHA-256 unchanged and no unvalidated destination.
- [ ] Map an observable disk-full condition to `InsufficientStorageSpace`.

### Step 9.4: Verify and commit

- [ ] Run Domain, JPEG Transformation, and Storage suites.
- [ ] Repeat the storage suite 100 times to expose UUID, handle-lifetime, or cleanup races.
- [ ] Commit:

```powershell
git add src/JpgSpinner.Domain src/JpgSpinner.WindowsStorage tests/JpgSpinner.WindowsStorage.Tests
git commit -m "feat: commit validated transformations as corrected copies"
```

## Task 10: Add mandatory backups, immutable journal generations, and deterministic recovery

**Files:**

- Create: `src/JpgSpinner.WindowsStorage/src/internal/ImageFileTransactionJournal.h`
- Create: `src/JpgSpinner.WindowsStorage/src/internal/ImageFileTransactionJournal.cpp`
- Create: `tests/JpgSpinner.WindowsStorage.Tests/ImageFileTransactionRecoveryTests.cpp`
- Create: `tests/JpgSpinner.WindowsStorage.Tests/StorageProviderQualificationTests.cpp`
- Modify: `src/JpgSpinner.WindowsStorage/include/jpg_spinner/storage/ImageFileTransactionEngine.h`
- Modify: `src/JpgSpinner.WindowsStorage/include/jpg_spinner/storage/AppContainerImageFileTransactionEngine.h`
- Modify: `src/JpgSpinner.WindowsStorage/src/AppContainerImageFileTransactionEngine.cpp`
- Modify: `src/JpgSpinner.WindowsStorage/src/internal/RecoverableImageFileTransaction.h`
- Modify: `src/JpgSpinner.WindowsStorage/src/internal/RecoverableImageFileTransaction.cpp`
- Modify: `tests/JpgSpinner.WindowsStorage.Tests/ImageFileTransactionTests.cpp`

### Step 10.1: RED on the impossibility of unbacked replacement

- [ ] Add static tests proving the only dispositions are `CreateCorrectedCopy` and `ReplaceOriginalWithVerifiedBackup`.
- [ ] Require a backup at `JPG Spinner Backups/<batch-directory>/<relative-source-path>` before the original changes.
- [ ] Hash source, backup, stage, and committed original. Require source hash = backup hash and stage hash = committed hash.
- [ ] Inject backup failure; require the original unchanged and transaction non-committed.
- [ ] Run `[storage][transaction][replace]`; expected RED is absent replacement behavior.

### Step 10.2: GREEN through the backup boundary

- [ ] Copy source to its unique backup path, request the strongest supported flush, close it, reopen it, and verify SHA-256 against `SourceFileRevision`. Document that verification proves observable bytes, not survival of physical-device loss.
- [ ] Re-hash the source immediately after backup verification.
- [ ] Return an internal verified-backup fact and keep the original untouched. Do not invoke `MoveAndReplaceAsync` yet; the replacement portion of the focused test remains RED until the journal protocol is implemented in Step 10.4.
- [ ] Retain verified backups; never use backup deletion as rollback.
- [ ] Return `BackupCreationFailed`, `BackupVerificationFailed`, or `OriginalReplacementFailed` precisely and retain recoverable artifacts.

### Step 10.3: RED on every journal transition

- [ ] Define monotonic states `TransactionInitialized`, `StagedOutputWritten`, `StagedOutputHashVerified`, `VerifiedBackupCreated`, `OutputCommitted`, and `OwnedStagingArtifactsCleaned`.
- [ ] Test the only legal graphs: copy skips the backup state; replacement requires it before commit. Reject every backward, repeated-with-different-data, or out-of-graph transition.
- [ ] Inject failure before, during, and after each immutable state-generation write, validation, publication under its unique name, and next state-changing operation.
- [ ] Restart against the persisted `Windows.Data.Json` journal and assert exact recovery.
- [ ] Corrupt, truncate, duplicate, reorder, and version-skew journal generations; require recovery to select the highest complete valid monotonic generation. If no trustworthy generation explains the artifacts, return `RecoveryConflict` with no source/backup deletion.
- [ ] Require repeated recovery to converge on the same state.

### Step 10.4: GREEN with a versioned monotonic journal

- [ ] Store schema version, UUID, monotonic generation number, source identity, disposition, source revision, stage/output hash, stage identity, backup identity, destination identity, and state.
- [ ] Write each transition to a uniquely named pending file, request flush, close, reopen, parse, and validate it, then publish it under a never-before-used generation filename. Never mutate or replace an existing generation. Recovery ignores incomplete pending files and chooses the highest complete generation whose predecessor and artifact hashes are consistent. Retain earlier valid generations until the transaction reaches a verified terminal state.
- [ ] Reject backward transitions and unknown future schema versions.
- [ ] After publishing and re-reading the `VerifiedBackupCreated` generation, invoke `MoveAndReplaceAsync`; no code path may request replacement from an earlier state.
- [ ] Implement exact hash recovery:
  - original equals captured source: commit did not occur; remove only a verified owned stage;
  - original equals transformed output: record committed and retain backup;
  - original missing, unknown, or ambiguous: retain all artifacts and return `RecoveryConflict`.
- [ ] Explain the preservation proof for every branch.
- [ ] Expose recovery only through `ImageFileTransactionEngine::recoverIncompleteTransactions`; keep journal parsing and state transitions internal.
- [ ] Treat `MoveAndReplaceAsync` completion as an observation, not a power-fail atomicity guarantee. On restart, infer whether replacement occurred from the original, stage, output, backup, and journal hashes. Do not substitute `ReplaceFileW` unless a separate AppContainer prototype and this same fault suite prove a materially stronger supported contract.

### Step 10.5: Verify and commit

- [ ] Run copy/replacement fault tests 100 times.
- [ ] From a parent harness, kill the transaction test process at every journal boundary, restart, and require the same recovery results as in-process injection.
- [ ] Run provider qualification on NTFS, supported removable filesystems, ReFS where supported, and representative cloud-backed picker folders. Exercise sharing violations, access revocation, offline placeholders, external mutation, interrupted flush/move/replace, and unsupported providers. Require a precise preflight refusal when the recoverability contract cannot be established.
- [ ] Commit:

```powershell
git add src/JpgSpinner.WindowsStorage tests/JpgSpinner.WindowsStorage.Tests
git commit -m "feat: make original replacement verifiably recoverable"
```

## Task 11: Orchestrate deterministic, bounded, cancellable batches

**Files:**

- Create: `src/JpgSpinner.BatchProcessing/include/jpg_spinner/batch/BatchIdentifier.h`
- Create: `src/JpgSpinner.BatchProcessing/include/jpg_spinner/batch/BatchProcessingRequest.h`
- Create: `src/JpgSpinner.BatchProcessing/include/jpg_spinner/batch/BatchProcessingProgress.h`
- Create: `src/JpgSpinner.BatchProcessing/include/jpg_spinner/batch/BatchProcessingSummary.h`
- Create: `src/JpgSpinner.BatchProcessing/include/jpg_spinner/batch/BatchFileOutcome.h`
- Create: `src/JpgSpinner.BatchProcessing/include/jpg_spinner/batch/BatchProcessingCoordinator.h`
- Create: `src/JpgSpinner.BatchProcessing/src/BatchProcessingCoordinator.cpp`
- Create: `tests/JpgSpinner.BatchProcessing.Tests/BatchProcessingCoordinatorTests.cpp`

### Step 11.1: RED on discovery and stable ordering

- [ ] Build a real nested folder containing mixed-case `.jpg`/`.jpeg`/`.jpe`/`.jfif`, a non-JPEG carrying a JPEG extension, valid JPEG bytes under an unrelated extension, unrelated files, app output/backup roots, inaccessible entries where the test environment permits, and names whose locale order differs from ordinal order.
- [ ] Require `SelectedFolderOnly` to exclude descendants and `SelectedFolderAndDescendants` to include them.
- [ ] Resolve and exclude exact app-owned output/backup storage identities; prove a sibling named `JPG Spinner Output Notes` is not incorrectly excluded.
- [ ] Add a directory reparse point that would escape or loop back into the root. Require it to be reported and skipped without traversal, output creation, or infinite enumeration.
- [ ] Require normalized relative-path ordinal order independent of current locale.
- [ ] Include long Unicode names, canonically equivalent Unicode spellings, and case-only path differences. Require original display names to remain unchanged and conservative ordinal case-insensitive output collisions to surface as `OutputRelativePathCollision` before processing.
- [ ] Add a production-shaped 10,000-file fixture and measure first-result latency, total enumeration time, peak memory, cancellation latency, inaccessible descendants, directory reparse points, removable media, and cloud placeholders. Record a baseline before choosing a lower-level enumeration API.
- [ ] Run `[batch][discovery]`; expected RED is absent coordinator compilation.

### Step 11.2: GREEN for discovery and analysis

- [ ] Define `BatchIdentifier` around a full UUID; derive display directory from UTC `yyyyMMddTHHmmssZ-<first-eight-hex>` with invariant formatting.
- [ ] Enumerate lazily through supported AppContainer storage access and never materialize file bytes during discovery. Page Storage API results in batches no larger than 500. Consider a Win32 enumeration path only if the preceding benchmark proves a material benefit and picker-granted access succeeds across every supported storage/provider test; do not select `FindFirstFileEx` from an unverified performance assumption.
- [ ] If the evidence selects a Win32 handle path, acquire handles from picker-granted storage items through the documented `IStorageItemHandleAccess` interop contract, keep authority beneath the selected root, and rerun the traversal/security/provider suite. Do not assume an unrestricted filesystem path from a broker grant.
- [ ] Recognize only the four documented extensions with ordinal case-insensitive comparison, then require scanner proof before calling a file a JPEG. Preserve the source extension for copy output.
- [ ] Analyze each JPEG sequentially through the one-operation `JpegImageAnalyzer` module and retain its immutable `JpegImageAnalysis`; callers do not orchestrate scanner, metadata, or planner internals.
- [ ] Record unsupported candidates with exact error codes; never silently omit them.
- [ ] Keep path normalization and storage-identity exclusion in narrowly named functions with traversal-safety comments.
- [ ] Prove every derived output relative path belongs to a source storage item beneath the selected root; do not treat normalized string prefixes as containment proof.

### Step 11.3: RED/GREEN progress and one-at-a-time processing

- [ ] Test discovered, analyzed, eligible, completed, failed, skipped, cancelled, and remaining counts after every event.
- [ ] Use a blocking test engine to prove no second transform begins while the first is active.
- [ ] Prove failures are per-file and do not abort later eligible files unless transaction integrity is uncertain.
- [ ] Prove `BatchProcessingSummary` has one terminal result for every discovered JPEG candidate.
- [ ] Define exact outcomes `CorrectedCopyCreated`, `OriginalReplacedWithVerifiedBackup`, `NoOrientationNormalizationRequired`, `UnsupportedSourceSkipped`, `ProcessingFailed`, `CancelledBeforeTransformation`, and `CancelledBeforeCommit`; require structured errors only for outcomes that need them.
- [ ] Prove absent or `TopLeft` orientation produces `NoOrientationNormalizationRequired` without writing an output file.
- [ ] Implement exactly one active transformation transaction. Do not add a worker pool.

### Step 11.4: RED/GREEN safe cancellation boundaries

- [ ] Cancel during discovery, between analysis files, before native transform, after native transform, before validation, before copy commit, during backup, and immediately before replacement.
- [ ] Require no unsafe abort inside native C code and no cancellation observation between persisted commit intent and completion of that file’s commit transition.
- [ ] Complete journal transition for the current file, then stop before the next file.
- [ ] Require deterministic summary counts and original/backup hashes for each cancellation point.

### Step 11.5: Verify and commit

- [ ] Run Batch, Storage, JPEG, and Domain suites; repeat cancellation tests 100 times.
- [ ] Measure peak memory with separate simulators for the 512 MiB application encoded-file limit and TurboJPEG's 512-MiB intermediate-buffer setting. Prove only one codec budget can be active and that neither limit is mistaken for the other.
- [ ] Commit:

```powershell
git add src/JpgSpinner.BatchProcessing tests/JpgSpinner.BatchProcessing.Tests
git commit -m "feat: orchestrate safe deterministic image batches"
```

## Task 12: Build presentation state and localized error semantics test-first

**Files:**

- Create: `src/JpgSpinner.App/Presentation.idl`
- Create: `src/JpgSpinner.App/ViewModels/MainWindowViewModel.h`
- Create: `src/JpgSpinner.App/ViewModels/MainWindowViewModel.cpp`
- Create: `src/JpgSpinner.App/ViewModels/ImageProcessingRowViewModel.h`
- Create: `src/JpgSpinner.App/ViewModels/ImageProcessingRowViewModel.cpp`
- Create: `src/JpgSpinner.App/Localization/ImageProcessingErrorText.h`
- Create: `src/JpgSpinner.App/Localization/ImageProcessingErrorText.cpp`
- Create: `tests/JpgSpinner.Presentation.Tests/MainWindowViewModelTests.cpp`
- Create: `tests/JpgSpinner.Presentation.Tests/LocalizationCompletenessTests.cpp`

### Step 12.1: RED on the presentation state machine

- [ ] Define expected states `SourceSelection`, `Analysis`, `Review`, `Processing`, `Results`, and `RecoveryRequired` in tests before the view model.
- [ ] Test valid transitions and reject invalid ones such as `SourceSelection → Processing` without a reviewed plan.
- [ ] Require projected commands to expose WinRT-conventional predicates named by action: `CanSelectSourceFolder`, `CanBeginProcessing`, `CanCancelCurrentOperation`, `CanReturnToSourceSelection`.
- [ ] Require progress percentage to be absent while denominator is unknown, not represented as zero.
- [ ] Run `[presentation][view-model]`; expected RED is missing generated/runtime types.

### Step 12.2: GREEN with no storage or codec behavior in presentation

- [ ] Define WinRT-observable runtime classes in `JpgSpinner.Presentation`; keep the native domain values behind narrow projections.
- [ ] Inspect generated WinMD and require every public property, method, and event to use PascalCase. Native-only helpers and local variables remain camelCase; do not mechanically rename their separate C++ convention.
- [ ] Make the view model consume `BatchProcessingCoordinator` and `ImageFileTransactionEngine` recovery interface through constructor composition.
- [ ] Marshal UI updates through `DispatcherQueue`; pass coroutine inputs by value across suspension and never call `.get()` on the UI thread.
- [ ] Keep JPEG bytes, Exiv2 values, and transaction operations out of the view model.
- [ ] Explain every apartment/thread transition and cancellation ownership rule.

### Step 12.3: RED/GREEN localized error completeness

- [ ] Enumerate every `ImageProcessingErrorCode` and require localized title, explanation, and remedy keys for en-US, en-GB, and ru.
- [ ] Require unknown future error values to produce one safe generic resource, not a native library message.
- [ ] Test path redaction: diagnostic context may expose a file’s display name in a row but must not place a full path in a localized error or log string.
- [ ] Implement `ImageProcessingErrorText` as the sole presentation seam from symbolic errors to localized resources.

### Step 12.4: RED/GREEN replacement-review honesty

- [ ] Require default disposition `CreateCorrectedCopy`, default traversal `SelectedFolderOnly`, default edge policy `RequirePerfectCoefficientTransform`, and default scans `PreserveSource`.
- [ ] Require `ReplaceOriginalWithVerifiedBackup` to expose the computed backup root before `CanBeginProcessing=true`.
- [ ] Require trim review to expose exact discarded right/bottom pixel counts and a specific acknowledgment state.
- [ ] Prohibit a view-model state representing replace-without-backup or implicit trim.

### Step 12.5: Verify and commit

- [ ] Run Presentation, Batch, Storage, JPEG, and Domain suites.
- [ ] Inspect generated WinRT projection warnings and require a clean build.
- [ ] Commit:

```powershell
git add src/JpgSpinner.App/Presentation.idl src/JpgSpinner.App/ViewModels src/JpgSpinner.App/Localization tests/JpgSpinner.Presentation.Tests
git commit -m "feat: model the image-processing experience precisely"
```

## Task 13: Implement the WinUI 3 shell and folder-review workflow

**Files:**

- Create: `src/JpgSpinner.App/App.xaml`
- Create: `src/JpgSpinner.App/App.idl`
- Create: `src/JpgSpinner.App/App.xaml.h`
- Create: `src/JpgSpinner.App/App.xaml.cpp`
- Create: `src/JpgSpinner.App/MainWindow.xaml`
- Create: `src/JpgSpinner.App/MainWindow.idl`
- Create: `src/JpgSpinner.App/MainWindow.xaml.h`
- Create: `src/JpgSpinner.App/MainWindow.xaml.cpp`
- Create: `tests/JpgSpinner.Presentation.Tests/WindowStateAutomationTests.cpp`
- Create: `scripts/Test-InteractiveUiEnvironment.ps1`
- Modify: `src/JpgSpinner.App/JpgSpinner.App.vcxproj`

### Step 13.1: RED with black-box UI Automation contracts

- [ ] Implement `Test-InteractiveUiEnvironment.ps1` first. It requires an unlocked interactive desktop, a foreground-capable test user, UI Automation availability, isolated package deployment rights, and no service-session execution. It exits with a distinct environment-not-qualified result rather than misreporting an automation timeout as product failure.
- [ ] Register and launch an isolated local test package from the integration test, then attach through Windows UI Automation by process ID.
- [ ] Require stable automation IDs for the source picker, traversal scope, analysis status, result table, output disposition, edge handling, scan organization, backup destination, start, cancel, and results summary.
- [ ] Require only controls relevant to the current semantic state to be enabled and focusable.
- [ ] Require a keyboard-only path from source selection through review without invoking processing in the test.
- [ ] Run `[presentation][automation][state]`; expected RED is the absent window/control tree, not an automation timeout.

### Step 13.2: GREEN with one window and semantic states

- [ ] Compose one `MainWindow` with state-specific regions; do not port old scenario pages, settings flyouts, or explanation pages.
- [ ] Declare the projected `App` and `MainWindow` runtime classes in `App.idl` and `MainWindow.idl`, keep each IDL contract focused, and verify generated metadata before relying on `x:Bind`.
- [ ] Apply `MicaBackdrop`, the stable WinUI integrated title-bar pattern, transparent root surfaces, layer/card theme brushes, and `InfoBar` for non-modal status. Rely on WinUI’s solid fallback rather than implementing a material shim.
- [ ] Use only first-party WinUI controls. Use `ListView` for processing rows, standard buttons/pickers/progress, and labeled Segoe Fluent Icons where helpful; do not add a third-party control package or icon-only action.
- [ ] Use `x:Bind` where compile-time binding catches naming errors; use observable properties only where state changes.
- [ ] Show source selection, analysis, review, processing, results, and recovery-required regions from `MainWindowViewModel` state.
- [ ] Keep code-behind limited to window/picker interop and presentation-only events. All processing decisions remain in tested view models and native modules.
- [ ] Assign stable `AutomationProperties.AutomationId`, name, help text, and live-setting values in XAML.
- [ ] Add visual states at 640 and 1008 effective pixels: stack review facts below 640, use compact list/detail at 640–1007, and expose non-essential review columns at 1008 and above. Never constrain localized prose to a fixed height.

### Step 13.3: RED/GREEN folder picking and AppContainer access

- [ ] Test cancellation from `FolderPicker` leaves the previous stable state and selected source unchanged.
- [ ] Test a picked folder produces the exact storage capability consumed by the batch coordinator and does not grant access to parents or siblings.
- [ ] Initialize the WinRT picker with the WinUI HWND through the documented window-initialization interface, then await it; never block the UI thread.
- [ ] Add only the FutureAccessList token needed to resume a user-selected folder during the current/recoverable transaction. Bound and remove stale tokens deliberately.
- [ ] Explain HWND initialization, token lifetime, and AppContainer authority in comments.

### Step 13.4: RED/GREEN honest review controls

- [ ] In automation tests require default values: create copy, selected folder only, perfect coefficients, preserve source scans.
- [ ] Select recursive traversal and prove app-owned output/backup roots appear as excluded explanation, not silently omitted.
- [ ] Select replacement and require the backup destination to become visible before the Start button enables.
- [ ] Select trim for an imperfect file and require exact discarded-edge dimensions plus an explicit acknowledgment.
- [ ] Require unsupported rows to expose their reason and remain excluded from execution totals.

### Step 13.5: Verify and commit

- [ ] Run view-model, localization-contract, WinMD, and x:Bind build tests on the normal headless lane. Run black-box automation only after `Test-InteractiveUiEnvironment.ps1` passes in the isolated interactive Windows VM/self-hosted lane; this lane remains required and cannot be converted to an optional hosted-runner skip.
- [ ] Run the app manually once to verify picker ownership, resize behavior, minimum window size, and clean shutdown.
- [ ] Commit:

```powershell
git add src/JpgSpinner.App tests/JpgSpinner.Presentation.Tests scripts/Test-InteractiveUiEnvironment.ps1
git commit -m "feat: add the accessible WinUI image workflow"
```

## Task 14: Integrate startup recovery, suspension-safe cancellation, and local diagnostics

**Files:**

- Create: `src/JpgSpinner.App/Diagnostics/LocalDiagnosticLog.h`
- Create: `src/JpgSpinner.App/Diagnostics/LocalDiagnosticLog.cpp`
- Create: `tests/JpgSpinner.Presentation.Tests/StartupRecoveryTests.cpp`
- Create: `tests/JpgSpinner.Presentation.Tests/LocalDiagnosticLogTests.cpp`
- Modify: `src/JpgSpinner.App/App.xaml.cpp`
- Modify: `src/JpgSpinner.App/MainWindow.xaml`
- Modify: `src/JpgSpinner.App/MainWindow.xaml.cpp`
- Modify: `src/JpgSpinner.App/ViewModels/MainWindowViewModel.cpp`
- Modify: `src/JpgSpinner.WindowsStorage/src/internal/ImageFileTransactionJournal.cpp`

### Step 14.1: RED on recovery before normal interaction

- [ ] Seed local app data with each incomplete journal state and launch the presentation composition root.
- [ ] Require journal inspection before source selection enables.
- [ ] For deterministic recovery, require normal source-selection state plus a concise recovered-transaction result.
- [ ] For `RecoveryConflict`, require `RecoveryRequired`, artifact locations expressed safely, no automatic mutation, and normal batch start disabled.
- [ ] Run `[presentation][recovery]`; expected RED is startup ignoring journals.

### Step 14.2: GREEN with ordered startup

- [ ] Compose dependencies, open the local journal store, recover each incomplete transaction serially, then initialize the view model.
- [ ] Keep the splash/window responsive using coroutines; never call `.get()`.
- [ ] If the app closes during processing, request cancellation and allow the current non-interruptible/commit boundary to reach an immutable journal-generation boundary. Do not claim that Windows guarantees unlimited shutdown time or physical-media durability; validated artifacts plus monotonic journal generations are the recovery mechanism.
- [ ] Explain why recovery precedes normal folder selection and why ambiguous artifacts are never auto-resolved.

### Step 14.3: RED/GREEN privacy-preserving diagnostics

- [ ] Before the logger exists, require records with UTC time, app/package version, architecture, correlation UUID, symbolic stage, symbolic error code, elapsed milliseconds, and resource-limit name.
- [ ] Feed full source paths, Exif strings, XMP values, and native exception messages; assert none occur in serialized logs.
- [ ] Require bounded rotation by total bytes and file count, deterministic UTF-8 JSON Lines, and explicit user export.
- [ ] Implement local-only logging with a narrow redaction boundary. No upload, endpoint, device identifier, image byte, metadata value, or full path is accepted by its interface.
- [ ] Log exceptions only after mapping to stable safe fields; preserve native diagnostic detail in debugger output only in Debug builds when it contains no user data.
- [ ] Add an `ExportDiagnostics` command and UI Automation test. It must open an HWND-initialized `FileSavePicker`, write only the already-redacted snapshot to the user-selected destination, and leave local logs unchanged when picking or writing is cancelled.

### Step 14.4: Verify and commit

- [ ] Run recovery, diagnostic, view-model, storage-recovery, and transaction suites.
- [ ] Inspect package capabilities and network traces during a representative batch; require no network capability or connection.
- [ ] Commit:

```powershell
git add src/JpgSpinner.App tests/JpgSpinner.Presentation.Tests src/JpgSpinner.WindowsStorage
git commit -m "feat: recover transactions and record local diagnostics"
```

## Task 15: Complete localization and accessibility release behavior

**Files:**

- Create: `src/JpgSpinner.App/Strings/en-US/Resources.resw`
- Create: `src/JpgSpinner.App/Strings/en-GB/Resources.resw`
- Create: `src/JpgSpinner.App/Strings/ru/Resources.resw`
- Create: `tests/JpgSpinner.Presentation.Tests/AccessibilityContractTests.cpp`
- Create: `docs/accessibility-release-checklist.md`
- Modify: `src/JpgSpinner.App/MainWindow.xaml`
- Modify: `tests/JpgSpinner.Presentation.Tests/LocalizationCompletenessTests.cpp`

### Step 15.1: RED on complete semantic resources

- [ ] Extract the complete visible-state/resource contract from view models and error mappings.
- [ ] Require every locale to contain the same keys, non-empty values, format-placeholder sets, accelerators where applicable, and plural forms used by the UI.
- [ ] Name keys semantically, such as `Error_MalformedJpegStructure_Explanation`; reject keys based on English sentence fragments.
- [ ] Run `[presentation][localization]`; expected RED lists each missing key in each locale.

### Step 15.2: GREEN by migrating meaning, not old page structure

- [ ] Migrate accurate existing en-US/en-GB/ru terminology, then add new transaction, backup, edge, metadata, and recovery language.
- [ ] Use localized formatting APIs for counts, dates, byte sizes, and direction-aware text.
- [ ] Give translators explanatory comments for “lossless,” “minimum coded unit,” “embedded thumbnail,” “verified backup,” and “progressive DCT.”
- [ ] Do not expose `TJXOP`, marker names, HRESULTs, or native library messages to users.

### Step 15.3: RED on programmatic accessibility

- [ ] Run headless accessibility-contract tests for resource coverage, WinMD metadata, automation IDs, names/help-text declarations, and view-model state before launching a window. These tests belong on every pull request.
- [ ] Use black-box UI Automation to require name, control type, enabled state, focusability, value/range semantics, and help text for every interactive control.
- [ ] Require focus order to follow the visual workflow and focus restoration after picker/dialog dismissal.
- [ ] Require analysis/progress/result changes to announce through a restrained live region without repeating the full table.
- [ ] Require error rows to convey status through text and automation properties, never color alone.
- [ ] Run `[presentation][accessibility]` in the qualified interactive lane; expected RED names the exact missing property/control rather than an absent desktop or service-session timeout.

### Step 15.4: GREEN and manual release protocol

- [ ] Correct XAML semantics, keyboard accelerators, access keys, focus behavior, high-contrast brushes, hit targets, and text reflow.
- [ ] Run Accessibility Insights for Windows FastPass and resolve every confirmed failure.
- [ ] Execute and record the checklist with keyboard only, Narrator, high contrast, 200% text scaling, 400% effective zoom, 1024×768-equivalent viewport, en-US, en-GB, ru, and right-to-left pseudo-localization.
- [ ] WCAG 2.2 Level AA is the cross-platform heuristic; Windows UI Automation and Microsoft accessibility guidance are the native acceptance authority.

### Step 15.5: Verify and commit

- [ ] Run all headless Presentation tests in all three locales on hosted CI. Run black-box UI Automation, Narrator, keyboard, focus, and Accessibility Insights checks in the qualified interactive Windows lane and attach its results to release artifacts.
- [ ] Commit:

```powershell
git add src/JpgSpinner.App/Strings src/JpgSpinner.App/MainWindow.xaml tests/JpgSpinner.Presentation.Tests docs/accessibility-release-checklist.md
git commit -m "feat: localize and verify the accessible experience"
```

## Task 16: Preserve Store identity in an AppContainer single-project MSIX

**Files:**

- Create: `src/JpgSpinner.App/Package.appxmanifest`
- Copy unchanged identity association: `JPG Spinner/Package.StoreAssociation.xml` → `src/JpgSpinner.App/Package.StoreAssociation.xml`
- Copy/migrate: `JPG Spinner/Assets/*` → `src/JpgSpinner.App/Assets/`
- Create: `scripts/Test-PackageManifest.ps1`
- Create: `tests/JpgSpinner.Presentation.Tests/PackageUpgradeSmokeTests.cpp`
- Modify: `src/JpgSpinner.App/JpgSpinner.App.vcxproj`
- Modify: `scripts/Test-RepositoryPolicy.ps1`

### Step 16.1: RED on exact identity and least privilege

- [ ] Write `Test-PackageManifest.ps1` before the new manifest. Parse XML and require exact identity name `HaddenIndustriesLtd.JPGSpinner`, publisher `CN=42458E53-5B1F-4F49-97F4-ABE6B4A48BB3`, application ID `App`, version 2.0.0.0, minimum 10.0.19045.0, tested maximum 10.0.28000.0, AppContainer trust, packaged-classic runtime, and Windows.Desktop family.
- [ ] Compare Store association XML semantically and require Store ID `9NBLGGH3TVGW`.
- [ ] Reject `internetClient`, `broadFileSystemAccess`, library capabilities, `runFullTrust`, `unvirtualizedResources`, other restricted capabilities, file-type association broadening, or medium-integrity trust. The application uses picker-mediated access and never substitutes a broad capability.
- [ ] Run it against the absent manifest; expected RED is a precise missing-file diagnostic.

### Step 16.2: GREEN with the exact manifest core

- [ ] Build the manifest around this identity/device-family contract:

```xml
<Identity
  Name="HaddenIndustriesLtd.JPGSpinner"
  Publisher="CN=42458E53-5B1F-4F49-97F4-ABE6B4A48BB3"
  Version="2.0.0.0" />
<Dependencies>
  <TargetDeviceFamily
    Name="Windows.Desktop"
    MinVersion="10.0.19045.0"
    MaxVersionTested="10.0.28000.0" />
</Dependencies>
```

- [ ] Set the application declaration to `uap10:RuntimeBehavior="packagedClassicApp"` and `uap10:TrustLevel="appContainer"` with the correct uap10 namespace.
- [ ] Declare no capability not demanded by a test and a documented user operation.
- [ ] Preserve package identity and Store association byte-for-byte where the format permits; update only package version and modern application metadata.
- [ ] Rebuild required scale and target-size PNG assets deterministically from the highest-quality owned existing artwork under current Windows app-icon guidance. Inspect native-size shell rendering; do not invent or AI-generate a new brand during modernization.
- [ ] Generate x86, x64, and ARM64 packages plus one `.msixbundle`; remove ARM32.
- [ ] Verify the bundle declares the Windows App SDK framework dependency and contains no private Windows App Runtime, bootstrapper, auto-initializer, or test-support payload. Test-only initialization remains outside the package graph.

### Step 16.3: RED/GREEN the upgrade path

- [ ] In an isolated Windows test user or disposable VM, install a trusted 1.1.3.0 package, launch it, create representative local settings and user-selected sample folders, then install signed 2.0.0.0 as an update.
- [ ] Before migration support, require the smoke test to fail if identity changes, the old package is uninstalled rather than upgraded, activation fails, or local-state inspection is not deterministic.
- [ ] Add only the local-state migration actually required. Do not add legacy API shims; ignore obsolete state safely when no current behavior consumes it.
- [ ] Prove launch, folder picker, copy output, backup replacement, recovery, uninstall, and no orphan stage after update.

### Step 16.4: Validate package and commit

- [ ] Run manifest policy, MakeAppx bundle validation, signature verification, and Windows App Certification Kit against Release packages for all architectures.
- [ ] Keep certificates and passwords outside the repository; CI obtains signing material only from protected release secrets.
- [ ] Commit:

```powershell
git add src/JpgSpinner.App/Package.appxmanifest src/JpgSpinner.App/Package.StoreAssociation.xml src/JpgSpinner.App/Assets scripts tests/JpgSpinner.Presentation.Tests
git commit -m "build: preserve Store identity in the WinUI package"
```

## Task 17: Add continuous integration, security analysis, and supply-chain evidence

**Files:**

- Create: `scripts/Invoke-StaticAnalysis.ps1`
- Create: `scripts/Invoke-Sbom.ps1`
- Create: `scripts/Test-SbomToolContract.ps1`
- Create: `scripts/New-PayloadManifest.ps1`
- Create: `scripts/Test-BuildProvenance.ps1`
- Create: `tests/TestData/SbomContractFixture/payload/component.txt`
- Create: `tests/TestData/SbomContractFixture/source/vcpkg.json`
- Create: `tests/TestData/SbomContractFixture/source/packages.lock.json`
- Create: `tests/TestData/SbomContractFixture/expected-contract.json`
- Create: `.github/workflows/continuous-integration.yml`
- Create: `.github/workflows/codeql.yml`
- Create: `.github/workflows/nightly-fuzz.yml`
- Create: `.github/workflows/store-package.yml`
- Create: `.github/dependabot.yml`
- Modify: `scripts/Test-RepositoryPolicy.ps1`

### Step 17.1: RED on immutable CI policy

- [ ] Extend repository policy to parse workflow YAML and reject mutable action tags, `windows-latest`, prerelease tools, unbounded permissions, missing timeouts, artifact retention over 30 days, and any secret passed to pull-request builds.
- [ ] Require `scripts/Invoke-Sbom.ps1` to name Microsoft SBOM Tool CLI version 4.1.5, its versioned release URL, and Windows x64 SHA-256 `625767b371b7fdd58f40f618b8a86da0247a33c89e419039c86b4edba1dad4b5`; reject `releases/latest`, an absent digest, and `Microsoft.Sbom.Targets`, whose NuGet `Pack` integration does not describe an MSIX release drop.
- [ ] Require these reviewed action commits with a version comment:
  - `actions/checkout` v6.0.2 at `de0fac2e4500dabe0009e67214ff5f5447ce83dd`;
  - `actions/cache` v5.1.0 at `caa296126883cff596d87d8935842f9db880ef25`;
  - `actions/upload-artifact` v7.0.1 at `043fb46d1a93c77aae656e7c1c64a875d1fc6a0a`;
  - `github/codeql-action` v4.37.9 at `cdf488f595d80d6e07e03d4674febd5ab45fa938`.
- [ ] Require `windows-2025-vs2026`, least-privilege workflow permissions, job timeouts, and concurrency cancellation for superseded pull requests.
- [ ] Require every build/package job to capture runner `ImageVersion`, OS build, exact `VCToolsVersion`, full `cl.exe`/`link.exe` versions and paths, Windows SDK/MakeAppx versions, NuGet lock hashes, vcpkg baseline, action commits, and source commit. The runner label is selection policy, not immutable provenance.
- [ ] Implement `Test-BuildProvenance.ps1` before workflow provenance emission. Give it an empty or deliberately incomplete JSON record and observe RED diagnostics for every missing field; then make the workflow emit one versioned, schema-checked record whose executable/file paths are normalized without leaking the developer profile path.
- [ ] Run policy; expected RED names the absent workflows and the absent SBOM script/tool pin.

### Step 17.2: GREEN continuous integration matrix

- [ ] On every pull request, restore qualified locked NuGet/vcpkg inputs, build Debug and Release for x86/x64, run all headless Domain/JPEG/Storage/Batch/Presentation-contract tests on x86/x64, compile Release ARM64, run x64 AddressSanitizer suites, run MSVC `/analyze`, validate manifests, and upload binary logs/test reports on failure. Do not schedule black-box UI Automation, Narrator, or Accessibility Insights on a GitHub-hosted service session.
- [ ] Define a required separate interactive Windows VM/self-hosted workflow or protected qualification job for black-box UI Automation and assistive-technology checks. It begins with `Test-InteractiveUiEnvironment.ps1`, never has pull-request secrets, and publishes signed test evidence; a hosted headless pass cannot substitute for this gate.
- [ ] Use a separate package job for the unsigned x86/x64/ARM64 bundle. Package creation must not require Store credentials.
- [ ] Cache only content-addressed dependency/build inputs whose key includes vcpkg baseline, manifest hash, architecture, toolset, and configuration. Never cache signing material or staged packages.
- [ ] Require warnings as errors and fail on sanitizer findings, leaked handles, unhandled exceptions, or flaky-test retries.

### Step 17.3: GREEN CodeQL and fuzz workflows

- [ ] CodeQL runs C++ analysis on a clean x64 build with `security-extended`; no high-severity result may remain open at release.
- [ ] Nightly fuzz runs the scanner target under AddressSanitizer for a bounded duration, minimizes new corpus inputs, uploads only non-user corpus/reproducers, and fails on crash, timeout, leak, or memory-limit violation.
- [ ] A pull request adding a fuzzer reproducer first adds a named deterministic regression test that fails without the fix.

### Step 17.4: Pin SBOM generation and dependency updates

- [ ] Before implementing `Invoke-Sbom.ps1`, implement `Test-SbomToolContract.ps1` around the fixed `tests/TestData/SbomContractFixture` payload, source manifests, and reviewed expected-contract JSON. Run it with the verified 4.1.5 executable absent or an intentionally wrong selector. Expected RED names the missing exact tool/contract, not malformed fixture data.
- [ ] Implement `Invoke-Sbom.ps1` around the versioned official asset `https://github.com/microsoft/sbom-tool/releases/download/v4.1.5/sbom-tool-win-x64.exe`. Download only into the ignored build-tool cache, compute SHA-256 before execution, require `625767b371b7fdd58f40f618b8a86da0247a33c89e419039c86b4edba1dad4b5`, and delete a mismatched file. Never fall back to another version or a moving URL.
- [ ] Have the script recursively unpack the exact input `.msixbundle` and each contained architecture `.msix` into a newly created inspection directory. Reject path traversal, reparse points, duplicate output paths, an unexpected architecture, a package manifest that differs from the bundle declaration, or a signature/hash verification failure. Treat the extracted tree as read-only input and put the generated manifest outside it so the SBOM cannot inventory itself.
- [ ] Invoke the tool with manifest selector `SPDX:3.0`, the extracted payload tree as `BuildDropPath`, and the repository as `BuildComponentPath`. Require Component Detection to inventory the locked NuGet and vcpkg manifests; compare the result against resolved runtime dependencies so a missed detector cannot silently omit a component.
- [ ] Require root `@context` to equal `https://spdx.org/rdf/3.0.1/spdx-context.jsonld`, every emitted `CreationInfo.specVersion` to equal `3.0.1`, and every shipped payload file to have the expected SHA-256. Validate once with Microsoft SBOM Tool and once against `https://spdx.org/schema/3.0.1/spdx-json-schema.json`; preserve the SBOM beside the bundle and record the outer signed-bundle SHA-256 in provenance. Do not emit an obsolete-format duplicate without a named consumer requirement.
- [ ] Run `Test-SbomToolContract.ps1` against the exact downloaded executable. Expected GREEN proves selector `SPDX:3.0`, output directory, 3.0.1 context/specVersion/schema, expected fixture file hash, and expected package/file relationships before any release bundle relies on the tool.
- [ ] Dependabot version 2 monitors NuGet and GitHub Actions weekly, groups related Microsoft Windows packages, and never auto-merges.
- [ ] Schedule `Test-DependencyFreshness.ps1` for vcpkg/upstream release drift. An upgrade updates baseline/override/notice together and runs the entire release suite.

### Step 17.5: Protected Store packaging workflow

- [ ] Trigger Store packaging only from a signed release tag or manual protected environment.
- [ ] Write `New-PayloadManifest.ps1` test-first. Given an unpacked package tree, it emits a path-ordinal, UTF-8 manifest of normalized relative path, byte length, and SHA-256 and rejects reparse points, duplicate normalized paths, or files outside the root. A one-byte fixture mutation must produce an observed RED mismatch before the final comparison is accepted.
- [ ] Rebuild twice from separate clean directories with immutable inputs, verify the source commit, recursively unpack both unsigned bundles, and require identical payload manifests. Record each container hash separately. Claim byte-identical unsigned MSIX/MSIXBUNDLE output only if the two independent container hashes also match under the locked toolchain; payload reproducibility is the mandatory contract.
- [ ] Sign the exact qualified container from protected secret material, verify that signed bundle, generate and validate the external SBOM from its recursively unpacked payloads, run WACK, and retain source, runner, toolchain, payload-manifest, unsigned-container, and signed-container provenance.
- [ ] Do not submit or stage rollout automatically. Publishing remains a deliberate Partner Center operation after qualification evidence is reviewed.

### Step 17.6: Verify and commit

- [ ] Validate every workflow locally with a YAML parser and repository policy; open a test pull request and require every hosted headless job green. Separately exercise the interactive workflow in its qualified environment and require its environment check plus UI suite green.
- [ ] Run `Test-BuildProvenance.ps1` against every job's record, `Test-SbomToolContract.ps1` against the pinned executable, and the two-clean-build payload-manifest comparison before accepting the workflow commit.
- [ ] Commit:

```powershell
git add scripts/Invoke-StaticAnalysis.ps1 scripts/Invoke-Sbom.ps1 scripts/Test-SbomToolContract.ps1 scripts/New-PayloadManifest.ps1 scripts/Test-BuildProvenance.ps1 tests/TestData/SbomContractFixture .github
git commit -m "ci: enforce build security and supply-chain gates"
```

## Task 18: Document the shipped system and remove secret-bearing telemetry

**Files:**

- Create: `README.md`
- Create: `SECURITY.md`
- Create: `THIRD_PARTY_NOTICES.md`
- Create: `docs/architecture.md`
- Create: `docs/privacy.md`
- Create: `docs/release-qualification.md`
- Create: `scripts/Test-DocumentationPolicy.ps1`
- Modify: `scripts/Test-RepositoryPolicy.ps1`
- Remove during this task: every `ApplicationInsights-CPP` reference and instrumentation-key occurrence from files that will survive cutover

### Step 18.1: RED on documentation and privacy truth

- [ ] Before writing documents, create a policy check that requires:
  - supported Windows versions and architectures;
  - default create-copy behavior and exact output/backup roots;
  - perfect-transform default and explicit trimming disclosure;
  - metadata preservation/removal/rejection rules;
  - resource limits and unsupported JPEG forms;
  - local-only processing, no network telemetry, diagnostic-log schema, and export behavior;
  - tested recoverability behavior, backup retention, and explicit limits of flush/replacement guarantees;
  - build/test/package commands;
  - vulnerability reporting and supported release policy;
  - dependency names, versions, licenses, source links, and required notices.
- [ ] During the parallel-build phase, reject the obsolete Application Insights package ID and exact instrumentation-key value everywhere except the explicitly quarantined `JPG Spinner/` reference tree. Task 19 tightens this to the entire working tree after deletion.
- [ ] Run it; expected RED is missing documentation or any telemetry reference copied into the new tree.

### Step 18.2: GREEN public and maintainer documentation

- [ ] `README.md` leads with what the app does, its safe defaults, supported formats, build prerequisites, reproducible build/test commands, and license.
- [ ] `docs/architecture.md` links the approved design, maps module dependencies, names transaction recoverability invariants and documented guarantee limits, describes AppContainer security boundaries, and states that capability interfaces live at current seams rather than preserving legacy abstractions.
- [ ] `docs/privacy.md` states no custom telemetry/network operation, exactly what local diagnostics contain/exclude, retention/rotation, and explicit export.
- [ ] `SECURITY.md` defines supported versions, private vulnerability-reporting route, expected response process, malformed-image threat model, and disclosure coordination. Do not promise an SLA the maintainers cannot meet.
- [ ] `docs/release-qualification.md` contains every automated/manual gate from Task 20 with evidence locations and sign-off roles, including qualified legal review of the exact GPL-3.0 application, Exiv2 GPL-2.0-or-later terms, corresponding-source delivery, and then-current Microsoft Store agreements.
- [ ] Keep comments and documentation synchronized by linking symbolic limits/error names rather than duplicating unstable implementation details unnecessarily.

### Step 18.3: Generate and verify third-party notices

- [ ] Generate the first notice inventory from resolved NuGet/vcpkg license metadata, then manually compare every direct/transitive dependency’s shipped files and upstream license.
- [ ] Record libjpeg-turbo 3.2.0, Exiv2 0.28.9, Catch2 3.16.0 for test distributions where applicable, Microsoft Windows packages, Microsoft SBOM Tool CLI 4.1.5 as release tooling, and each transitive native dependency actually present in the bundle.
- [ ] Confirm Exiv2’s selected GPL-2.0-or-later terms are used compatibly under this repository’s GPL-3.0 license and include the required source/notice offer.
- [ ] Prepare the exact corresponding source, patches, dependency locks, and reproducible build instructions for the distributed binary. Do not assume that static versus dynamic linkage or a separate process removes license obligations, and do not introduce a loader/process boundary merely as a licensing shim.
- [ ] Include libjpeg-turbo’s applicable BSD-style and IJG notices verbatim from its packaged copyright files, not from memory.
- [ ] Ensure test-only dependencies are not falsely described as runtime components.

### Step 18.4: Revoke the historical instrumentation credential

- [ ] Remove the beta Application Insights package, initialization, key, telemetry calls, configuration artifacts, and package lock entry from all surviving new-project files; the quarantined old tree is deleted in Task 19 rather than modified.
- [ ] A credentialed maintainer revokes or rotates the exposed key in the owning Azure/Application Insights resource and records only completion date/resource owner in the private release evidence. Do not store a replacement key.
- [ ] Query package capabilities and application traffic to prove the new app has no telemetry transport.

### Step 18.5: Verify and commit

- [ ] Run documentation and repository policy, link validation, spelling checks for en-US maintainer prose, and third-party notice comparison.
- [ ] Commit:

```powershell
git add README.md SECURITY.md THIRD_PARTY_NOTICES.md docs scripts src
git commit -m "docs: define support privacy and release contracts"
```

## Task 19: Cut over in one auditable change and delete the legacy implementation

**Files:**

- Delete: `JPG Spinner.sln`
- Delete: `JPG Spinner/` and all descendants, including vendored JPEG 9a, `transupp`, C++/CX, old XAML, beta package config, legacy bundle artifacts, and duplicate assets already migrated
- Modify: `scripts/Test-RepositoryPolicy.ps1`
- Modify: `JpgSpinner.sln`
- Modify: documentation links that still describe the legacy tree as present

### Step 19.1: RED on the legacy-free repository policy

- [ ] Extend policy before deletion to reject:
  - `JPG Spinner.sln` or the old source directory;
  - `packages.config`;
  - `ApplicationInsights`, the historical key, or telemetry initialization;
  - `jpeg-9a`, copied `jpeglib.h`, `transupp.c`, `transupp.h`, or private libjpeg headers;
  - `Windows::UI::Xaml`, `Platform::String`, C++/CX `ref class`, or caret handles in first-party code;
  - `Microsoft.Toolkit`, compatibility, legacy-adapter, facade, bridge, or shim project/type names;
  - ARM32 configurations and Windows SDK targets below 19045;
  - obsolete documentation paths.
- [ ] Run it; expected RED enumerates the legacy tree and only the legacy tree.

### Step 19.2: Prove behavioral coverage before deletion

- [ ] Build a coverage matrix mapping every supported legacy user outcome to a new executable test: folder selection, direct/recursive traversal, eight orientations, perfect/trim behavior, sequential/progressive choice, copy output, backed replacement, localization, cancellation, and result reporting.
- [ ] Explicitly mark retired behavior with rationale: custom Application Insights, obsolete settings flyouts, ARM32, silent/ambiguous destructive paths, copied codec internals, and unsupported MPO mutation.
- [ ] Run every automated test, sanitizer suite, static analysis, manifest validation, and package upgrade smoke before deleting any legacy file.
- [ ] Save binary logs and JUnit results as the pre-cutover evidence set.

### Step 19.3: Delete rather than wrap

- [ ] Remove the exact legacy solution and directory after resolving them beneath the repository root. This deletion is recoverable from Git history.
- [ ] Do not copy old implementation bodies into new modules. Only already reviewed assets/resources and behavior encoded by tests survive.
- [ ] Remove stale solution references, filters, package locks, scripts, and documentation.

### Step 19.4: GREEN legacy absence and full system

- [ ] Run:

```powershell
pwsh -NoProfile -File scripts/Test-RepositoryPolicy.ps1
pwsh -NoProfile -File scripts/Invoke-Build.ps1 -Configuration Release -Architecture x86
pwsh -NoProfile -File scripts/Invoke-Build.ps1 -Configuration Release -Architecture x64
pwsh -NoProfile -File scripts/Invoke-Build.ps1 -Configuration Release -Architecture ARM64
pwsh -NoProfile -File scripts/Invoke-TestSuite.ps1 -Project HeadlessAll -Architecture x64 -Configuration Release -ExecutionEnvironment Headless
pwsh -NoProfile -File scripts/Test-InteractiveUiEnvironment.ps1
pwsh -NoProfile -File scripts/Invoke-TestSuite.ps1 -Project JpgSpinner.Presentation.Tests -TestSpecification "[automation]" -Architecture x64 -Configuration Release -ExecutionEnvironment Interactive
```

Expected GREEN: policy finds no prohibited legacy/shim surface; all architectures build; every headless x64 suite passes; and the separately qualified interactive presentation suite passes.

- [ ] Also run `rg` with explicit patterns for the old package, key, C++/CX, UWP XAML, private JPEG files, compatibility names, and shims. A no-match result is required except for the modernization documents’ historical descriptions.
- [ ] Commit the auditable deletion separately:

```powershell
git add --all
git commit -m "refactor: remove the retired UWP implementation"
```

## Task 20: Qualify release 2.0.0.0 and stage the Store rollout

**Files:**

- Modify: `docs/release-qualification.md`
- Create: `docs/releases/2.0.0.md`
- Generate: `artifacts/release/2.0.0.0/` package, SBOM, WACK, test, analysis, accessibility, benchmark, and provenance evidence; do not commit signed package binaries

### Step 20.1: Re-audit all version pins before release

- [ ] Run the stable-only freshness checker against official Windows App SDK, Windows SDK, C++/WinRT, NuGet, vcpkg, libjpeg-turbo, Exiv2, Catch2, Microsoft SBOM Tool, and GitHub Action releases.
- [ ] If any newer stable release exists, update the relevant immutable pin, lock, notice, and action SHA through a separate RED/GREEN dependency-change commit, then restart Task 20 from the clean-clone gate.
- [ ] Do not substitute experimental, preview, release-candidate, nightly, or moving “latest” references for stable pins.

### Step 20.2: Clean-clone automated qualification

- [ ] From a fresh clone with no global vcpkg integration or warm build output, restore locked inputs and run repository/documentation policy.
- [ ] Build Debug and Release x86/x64 and Release ARM64 twice from independent clean roots. Recursively unpack each unsigned bundle and require identical normalized payload manifests. Record both unsigned container hashes; require byte identity only if the locked packaging toolchain actually demonstrates it.
- [ ] Execute all headless tests on x86/x64; execute the transform, transaction, recovery, batch, and packaged-app smoke suites on physical ARM64. Execute black-box UI Automation and assistive-technology checks in the separately qualified interactive Windows environment.
- [ ] Run x64 AddressSanitizer, MSVC `/analyze`, CodeQL `security-extended`, and a minimum 24-hour aggregate fuzz campaign with the reviewed corpus.
- [ ] Require no compiler warning, sanitizer finding, leak, static-analysis error, CodeQL high-severity alert, fuzz crash/hang, or flaky retry.

### Step 20.3: Corpus and transaction qualification

- [ ] Run every orientation across 8-bit and 12-bit lossy precision; 4:4:4, 4:2:2, 4:2:0; grayscale, RGB/YCbCr, CMYK/YCCK; sequential, progressive, Huffman, arithmetic; restart intervals; valid ICC v2/v4; Exif 3.1; XMP standard/extended; IPTC Photo Metadata 2025.1 in XMP/IIM; unknown APPn/COM; absent metadata; exact resource boundaries; and explicit rejection fixtures for C2PA 2.4, other JUMBF, MPF/Ultra HDR, Motion Photo, and generic trailing payloads.
- [ ] Run reviewed malformed corpus: all marker truncations, integer boundaries, corrupt entropy, excessive scans, malformed Exif/XMP/ICC, MPF, unsupported SOF, and decompression-bomb dimensions.
- [ ] Re-run coefficient equality and metadata hashes, not only screenshots or decode success.
- [ ] Inject process termination and each storage error before, during, and after every immutable journal-generation publication and replacement boundary on NTFS, supported removable filesystems, ReFS where supported, and representative cloud-backed picker providers. Infer outcome from validated hashes and require recoverability in every modeled case without asserting undocumented power-fail atomicity or physical-media durability.
- [ ] Simulate source modification during analysis, transformation, validation, backup, and immediately before replacement. Require no stale-plan commit.

### Step 20.4: Performance and responsiveness qualification

- [ ] Record reference hardware, power profile, OS build, architecture, corpus hashes, cold/warm state, and tool versions.
- [ ] Measure cold launch to interactive source selection, analysis throughput for 10,000 small files, coefficient-transform throughput for representative large files, peak committed memory at resource limits, cancellation latency between safe boundaries, and UI-thread stalls through ETW/WPA.
- [ ] Compare the supported paged Storage API path with any proposed Win32 enumeration candidate under the same AppContainer picker grant and corpus. Record first-result latency, total time, peak memory, cancellation, inaccessible descendants, reparse points, removable media, and cloud placeholders. Retain the simpler supported path unless the alternative proves a material, repeatable benefit without reducing authority or correctness.
- [ ] Require no synchronous UI-thread operation longer than 50 ms during analysis/processing and no unbounded growth with file count.
- [ ] Save the first accepted results as the 2.0 baseline. Future releases fail qualification on a statistically repeatable regression greater than 10% unless a reviewed correctness/accessibility change explains and accepts it.

### Step 20.5: Accessibility, localization, and package qualification

- [ ] Repeat the full manual accessibility checklist on current Windows 10 22H2 ESU and maintained Windows 11 24H2 and 25H2 systems, plus Windows 11 26H1 hardware where available. Record that 26H1 is a supported new-device hardware cohort rather than an assumed in-place update path for existing 24H2/25H2 systems.
- [ ] Review every screen and error in en-US, en-GB, and ru; run pseudo-localization for expansion and bidirectional layout defects.
- [ ] Install 1.1.3.0, update to 2.0.0.0 for x86/x64 and native ARM64 paths, then verify identity, launch, processing, recovery, uninstall, and no orphan staging artifacts.
- [ ] In the protected release workflow, reproduce the qualified unpacked payload manifest, record the produced unsigned container hash, sign that exact container, verify the exact signed bundle, generate and validate the external SPDX 3.0.1 SBOM from its recursively extracted packages, then run Windows App Certification Kit, bundle manifest validation, malware scanning, and Store package ingestion validation against that same signed-bundle hash.

### Step 20.6: Privacy and integrity go/no-go

- [ ] Verify the final manifest has no network/restricted/full-trust capability and observe zero network traffic during the complete workflow.
- [ ] Search the extracted bundle and symbols for the historical key, source paths, signing secrets, test data, private metadata, and local developer paths. Require no match.
- [ ] Confirm local diagnostics contain only documented redacted fields and rotate at documented limits.
- [ ] Require completed qualified legal review of the exact final dependency graph, GPL-3.0 distribution, Exiv2 GPL-2.0-or-later obligations, corresponding-source/build-instruction delivery, third-party notices, and current Store agreements. Do not accept process separation or dynamic loading as an unreviewed substitute.
- [ ] A single unexplained source-hash mismatch, missing backup, recovery ambiguity caused by app behavior, or data-integrity report is an unconditional no-go.

### Step 20.7: Release evidence and staged Store rollout

- [ ] Write `docs/releases/2.0.0.md` with supported systems, safe defaults, precise format limitations, user-visible changes, retired behavior, privacy statement, and backup/recovery guidance.
- [ ] Archive the exact source commit, dependency locks, runner `ImageVersion`, exact `VCToolsVersion` and compiler/linker paths/versions, SDK/MakeAppx versions, payload manifests, unsigned/signed package hashes, SBOMs, provenance, test/analysis/fuzz reports, WACK output, interactive accessibility evidence, performance baseline, legal approval record, and upgrade evidence.
- [ ] Submit the existing Store product as version 2.0.0.0; never create a second product identity.
- [ ] Release to 5%. Observe at least 48 hours and 200 active-device health samples; if volume is lower, observe seven days.
- [ ] Advance to 25% only when no file-integrity incident exists, no new crash/hang signature affects 0.1% or more observed devices, and the affected-device rate is no more than 0.1 percentage point above the 1.1.3.0 baseline.
- [ ] Observe 25% for at least 72 hours and 500 active-device samples, or seven days at lower volume, before 100%.
- [ ] Pause immediately for any source/backup/recovery integrity report regardless of aggregate health rate. Diagnose through Partner Center health plus opt-in exported local diagnostics; do not add telemetry as a rollout remedy.

### Step 20.8: Final verification and release commit

- [ ] Run the complete release script one final time against the exact signed bundle and verify all artifact hashes match archived provenance.
- [ ] Mark every gate in `docs/release-qualification.md` with date, operator, command/report location, and result; no blank waiver is permitted.
- [ ] Commit only source documentation and reproducible evidence manifests, not certificates, secrets, or signed binaries:

```powershell
git add docs/releases/2.0.0.md docs/release-qualification.md THIRD_PARTY_NOTICES.md
git commit -m "release: qualify JPG Spinner 2.0.0"
```

---

## Definition of done

Implementation is complete only when:

- every production behavior has retained RED/GREEN/REFACTOR evidence;
- the legacy solution and source tree are gone with no shim or compatibility path;
- libjpeg-turbo, Exiv2, Catch2, Windows packages, SBOM tooling, and CI actions are the latest stable versions at qualification time and are immutable;
- all eight orientations are coefficient-exact across representative sampling;
- every output is independently validated before commit;
- replacement has no representable path without a verified backup;
- journal recovery preserves source/backup data under every tested interruption;
- source changes after analysis prevent commit;
- AppContainer, Store identity, x86/x64 update continuity, and ARM64 operation are verified;
- resource limits, metadata policy, accessibility, localization, privacy, security, and supply-chain gates pass;
- encoded-file and TurboJPEG intermediate-memory limits remain distinct; Extended XMP follows the tested preservation/refusal policy; public WinRT metadata is PascalCase and complete;
- hosted headless and interactive UI qualification lanes both pass; exact toolchain provenance and reproducible unpacked-payload manifests are archived;
- every post-task artifact-hygiene pass is recorded, the pre-existing obsolete-run backlog is eliminated, and no unexplained build, test, fuzz, package, or dependency cache remains;
- final documentation describes the exact shipped behavior;
- rollout evidence meets the staged health and zero-integrity-incident gates.

## Official research anchors

- [Microsoft UWP to Windows App SDK migration](https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/migrate-to-windows-app-sdk/migrate-to-windows-app-sdk-ovw)
- [Microsoft single-project MSIX guidance](https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/single-project-msix)
- [Microsoft Windows App SDK downloads](https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/downloads)
- [Microsoft C++/WinRT concurrency guidance](https://learn.microsoft.com/en-us/windows/apps/develop/cpp-winrt/concurrency)
- [Microsoft Windows accessibility guidance](https://learn.microsoft.com/en-us/windows/apps/develop/accessibility)
- [Microsoft modern WinUI 3 application structure](https://learn.microsoft.com/en-us/windows/apps/develop/ui/windows-app-sdk-app-structure)
- [Microsoft responsive Windows application design](https://learn.microsoft.com/en-us/windows/apps/design/layout/responsive-design)
- [Microsoft Windows app icon guidance](https://learn.microsoft.com/en-us/windows/apps/design/iconography/app-icon-design)
- [Microsoft secure C++ build guidance](https://learn.microsoft.com/en-us/cpp/code-quality/build-reliable-secure-programs)
- [Microsoft C++ MSBuild customization and import ordering](https://learn.microsoft.com/en-us/visualstudio/msbuild/customize-cpp-builds?view=visualstudio)
- [Microsoft C++ item-definition/property-page model](https://learn.microsoft.com/en-us/cpp/build/reference/property-page-xml-files?view=msvc-170)
- [Microsoft `/CETCOMPAT` reference](https://learn.microsoft.com/en-us/cpp/build/reference/cetcompat?view=msvc-170)
- [Microsoft unpackaged Windows App SDK deployment](https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/deploy-unpackaged-apps)
- [Microsoft application capability declarations](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/app-capability-declarations)
- [Microsoft C++/WinRT property binding and IDL requirements](https://learn.microsoft.com/en-us/windows/apps/develop/cpp-winrt/binding-property)
- [Microsoft file replacement API contract](https://learn.microsoft.com/en-us/uwp/API/windows.storage.istoragefile.moveandreplaceasync?view=winrt-22000)
- [Microsoft storage-item handle interop](https://learn.microsoft.com/en-us/windows/win32/api/windowsstoragecom/nf-windowsstoragecom-istorageitemhandleaccess-create)
- [Microsoft large file-query paging guidance](https://learn.microsoft.com/en-us/windows/apps/develop/files/fast-file-properties)
- [Microsoft MSIX health report](https://learn.microsoft.com/en-us/partner-center/insights/msix-health-report)
- [Microsoft vcpkg version locking](https://learn.microsoft.com/en-us/vcpkg/consume/lock-package-versions)
- [libjpeg-turbo releases and transform documentation](https://github.com/libjpeg-turbo/libjpeg-turbo/releases)
- [TurboJPEG 3.2.0 public interface](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/3.2.0/src/turbojpeg.h)
- [TurboJPEG 3.2.0 `TJPARAM_MAXMEMORY` implementation (1,048,576 bytes per unit)](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/3.2.0/src/turbojpeg.c)
- [Exiv2 releases](https://github.com/Exiv2/exiv2/releases)
- [Exiv2 maintainer statement on Extended XMP limitations](https://dev.exiv2.org/boards/3/topics/3124)
- [Catch2 3.16.0 release](https://github.com/catchorg/Catch2/releases/tag/v3.16.0)
- [CIPA camera and imaging standards](https://www.cipa.jp/e/std/std-sec.html)
- [ITU-T T.81 JPEG specification](https://www.itu.int/ITU-T/recommendations/rec.aspx?lang=en&rec=2633)
- [Adobe XMP specifications](https://developer.adobe.com/xmp/docs/xmp-specifications/)
- [ICC.1:2022 profile specification](https://www.color.org/icc-1_specification/)
- [W3C WCAG 2.2](https://www.w3.org/TR/WCAG22/)
- [Microsoft SBOM Tool](https://github.com/microsoft/sbom-tool)
- [Microsoft SBOM Tool 4.1.5](https://github.com/microsoft/sbom-tool/releases/tag/v4.1.5)
- [SPDX 3.0.1 specification](https://spdx.github.io/spdx-spec/)
- [SPDX 3.0.1 JSON schema](https://spdx.org/schema/3.0.1/spdx-json-schema.json)
- [C2PA 2.4 Content Credentials specification](https://spec.c2pa.org/specifications/specifications/2.4/)
- [IPTC Photo Metadata Standard 2025.1](https://www.iptc.org/std/photometadata/specification/IPTC-PhotoMetadata-2025.1.html)
- [GitHub hosted-runner images](https://github.com/actions/runner-images)
- [Windows 11 26H1 release status](https://learn.microsoft.com/en-us/windows/release-health/status-windows-11-26H1)
- [Microsoft Store policies](https://learn.microsoft.com/en-us/windows/apps/publish/store-policies)
- [Microsoft Publisher Agreement](https://learn.microsoft.com/en-us/legal/marketplace/msft-publisher-agreement)
