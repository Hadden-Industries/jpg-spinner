# JPG Spinner 2.0 Modernization Implementation Plan

> **For agentic workers:** REQUIRED EXECUTION MODE: inline execution only. Subagents and subagent-driven development are prohibited by project constraint. Execute the tasks sequentially in the active task, preserve RED/GREEN evidence at every behavior boundary, and stop a task rather than bypassing a failed gate.

**Goal:** Replace the unmaintained UWP/C++/CX application with a secure, accessible, Store-updatable WinUI 3/C++/WinRT application that performs provably lossless JPEG orientation normalization, preserves truthful metadata, and cannot replace a source without a verified backup.

**Architecture:** A single-project packaged MSIX presentation shell composes four native modules: domain policy, JPEG transformation, AppContainer storage transactions, and batch orchestration. Dependencies point inward toward immutable value types and capability contracts. The old application remains only as a temporary behavioral reference and is deleted at the verified cutover; no legacy code is wrapped.

**Tech stack:** Visual Studio 2026 / MSVC 14.51 / PlatformToolset v145 with an exact qualification-time `VCToolsVersion`; C++20; WinUI 3 and Windows App SDK 2.5.1; Microsoft.Windows.CppWinRT 3.0.260818.1; Microsoft.Windows.ImplementationLibrary 1.0.260126.7; Windows SDK BuildTools 10.0.28000.2705; libjpeg-turbo 3.2.0; Exiv2 0.28.9 with XMP; Catch2 3.16.0; Syft SDK 1.54.1 and SPDX Workgroup Tools 2.0.7; vcpkg manifest mode at baseline `c748cb44f2a435fcf015c35225c9d5545fe0021c`; MSBuild for first-party projects; GitHub Actions on `windows-2025-vs2026` with the exact runner/tool versions recorded in provenance.

**Design specification:** `docs/specs/2026-08-30-jpg-spinner-modernization-design.md`

**Dependency refresh, 2026-10-03:** Before Task 6, qualify Exiv2 0.28.9, Windows App SDK 2.5.1, and libjpeg-turbo 3.2.0 at vcpkg port revision 1 (`3.2.0#1`). The immutable registry baseline above supersedes the original Task 1 baseline. Historical implementation logs retain the versions actually tested at those milestones. See `docs/implementation/2026-10-03-dependency-refresh-log.md` for authority, scope, and evidence.

**Approved Task 6 backend amendment, 2026-10-03:** The user approved bounded qualification of Adobe XMP Toolkit SDK v2025.03 behind the public Exiv2 API and then the narrow native language-alternative preservation patch. Adopt only the two repository-owned ports `vcpkg-ports/adobe-xmp-core` (2025.03#1, immutable upstream commit and archive digest) and `vcpkg-ports/exiv2` (0.28.9#1, external XMP enabled). This explicitly supersedes the blanket overlay prohibition below for this pair only; alternate registries, arbitrary overlays, and simultaneous configuration representations remain prohibited. Native vcpkg owns source acquisition, patch application, ABI caching and installation; no research binaries, XML parser, or API shim enters production. Freshness checks must validate the local Exiv2 revision against its local manifest, compare source versions independently of official registry revision counters, and monitor official Adobe releases. See [qualification evidence](../implementation/2026-10-03-task-6-xmp-backend-qualification.md). Production sanitizer and milestone verification remain required before adoption is complete.

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
- [ ] Before accessing an IPTC 2025.1 property whose namespace is not built into Exiv2 0.28.9, call the public namespace-registration API with the exact official URI/prefix. Add a table-driven test for AI Prompt Information, AI Prompt Writer Name, AI System Used, and AI System Version Used proving read/write without changing rights, licensing, or disclosure values. Exiv2 0.28.9 already registers the relevant namespaces: retain passing characterization coverage rather than manufacturing an unregistered failure. Require RED only for an actually absent behavior.
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

**Approved semantic naming amendment, 2026-10-03:** Use `JpegFrameProperties`
for the owned coding-process, precision, dimension and component facts shared by
source analysis and validated output. The source-only name is replaced outright
in its header and all consumers; no compatibility alias is retained. These facts
correspond to JPEG frame-header parameters, not the complete codestream or metadata.

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
git add src/JpgSpinner.Domain src/JpgSpinner.JpegTransformation tests/JpgSpinner.JpegTransformation.Tests tests/TestSupport docs/implementation/2026-10-03-modernization-task-7-log.md docs/plans/2026-08-30-jpg-spinner-modernization.md
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
- [ ] Attempt replacement between open and hash completion through a controlled test barrier. Reader-only sharing may reject replacement with a native access/sharing error; then prove retained source bytes and a consistent original revision. If a provider permits replacement, require a structured capture-change/access error and no digest. Never loosen the production lock to manufacture a mutation; also exercise a permitted metadata-only modification during hashing and require digest rejection. See the Task 8 native qualification in `docs/implementation/2026-10-03-modernization-task-8-log.md`.
- [ ] Run `[storage][revision]`; expected RED is absent calculator compilation.

### Step 8.2: GREEN with streamed SHA-256

- [ ] Implement hashing with CNG/BCrypt RAII handles or an AppContainer-compatible Windows cryptography API; do not add another cryptography package.
- [ ] Hash through a fixed bounded buffer with checked length accumulation and cancellation checks between reads.
- [ ] Capture file metadata before and after streaming. If length or timestamp differs, discard the digest and return `SourceChangedAfterAnalysis`.
- [ ] Translate access denied, sharing violation, cancellation, and I/O failure to structured error context.
- [ ] Explain why length/time aid diagnostics but SHA-256 is authoritative at commit.
- [ ] Expose a synchronous initialized-MTA worker operation, matching the JPEG engine; reject UI/STA and uninitialized callers before I/O. Task 11 owns scheduling. The native PPL task holder is incompatible with the accepted never-empty/nonassignable result contract; do not weaken it or introduce a task shim.

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
- Create: `src/JpgSpinner.WindowsStorage/include/jpg_spinner/storage/WindowsStorageImageFileTransactionEngine.h`
- Create: `src/JpgSpinner.WindowsStorage/src/WindowsStorageImageFileTransactionEngine.cpp`
- Create: `src/JpgSpinner.WindowsStorage/src/internal/RecoverableImageFileTransaction.h`
- Create: `src/JpgSpinner.WindowsStorage/src/internal/RecoverableImageFileTransaction.cpp`
- Create: `tests/JpgSpinner.WindowsStorage.Tests/ImageFileTransactionTests.cpp`
- Create: `tests/TestSupport/DeterministicImageFileTransactionEngine.h`
- Create: `tests/TestSupport/DeterministicImageFileTransactionEngine.cpp`

### Step 9.1: RED on stage ownership and copy commit

The approved concrete name denotes the Windows Storage API mechanism, not a claim
that the process is isolated. AppContainer deployment and capability restrictions
remain required separately; using `StorageFile` does not establish that boundary.

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
- Modify: `src/JpgSpinner.WindowsStorage/include/jpg_spinner/storage/WindowsStorageImageFileTransactionEngine.h`
- Modify: `src/JpgSpinner.WindowsStorage/src/WindowsStorageImageFileTransactionEngine.cpp`
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
- [ ] Approved lifecycle amendment (2026-10-04): add terminal `TransactionAbandonedBeforeCommit`, reachable from a non-committed state only after proving no commit occurred and removing the identity-bound owned stage. Preserve source/backup files and all immutable generations. Skip verified terminal records rather than reopening recovery after legitimate later user edits. Do not infer abandonment from a missing stage or a failed native move alone; uncertain outcomes remain `RecoveryConflict`.
- [ ] Test the only legal graphs: copy skips the backup state; replacement requires it before commit. Reject every backward, repeated-with-different-data, or out-of-graph transition.
- [ ] Inject failure before, during, and after each immutable state-generation write, validation, publication under its unique name, and next state-changing operation.
- [ ] Restart against the persisted `Windows.Data.Json` journal and assert exact recovery.
- [ ] Corrupt, truncate, duplicate, reorder, and version-skew journal generations; require recovery to select the highest complete valid monotonic generation. If no trustworthy generation explains the artifacts, return `RecoveryConflict` with no source/backup deletion.
- [ ] Require repeated recovery to converge on the same state.
- [ ] Isolate transaction reconciliation: a conflicting record must not prevent examination of other incomplete records. Limit reconciliation to the granted selected root; do not open another root merely because its record shares the application store. Aggregate unresolved conflicts without deleting their artifacts.

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
- Create: `src/JpgSpinner.App/ViewModels/ImageSourceDiscoveryIssueViewModel.h`
- Create: `src/JpgSpinner.App/ViewModels/ImageSourceDiscoveryIssueViewModel.cpp`
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
- [ ] Keep terminal publication in private native async actions that public `IAsyncAction.Cancel()` cannot short-circuit. Use the official cancellation-callback pattern to propagate cooperative native stop requests, not cancellation of mandatory UI finalization. Cover analysis and committed processing through both public cancellation paths. During recovery, require `CanCancelCurrentOperation=false` and rejection of the batch cancel command without requesting stop; separately cover returned-action cancellation and fail-closed retry. Observe native completion before asserting terminal state.
- [ ] Preserve the accepted no-silent-loss invariant with immutable `ImageSourceDiscoveryIssueViewModel` snapshots exposed as `DiscoveryIssues`, separately from image-result rows. Project exact traversal issue kinds, relative display context, and any localized symbolic error; retain them through review/results and notify snapshot replacement. Never invent candidate outcomes or disturb image-result ordinals to represent traversal exclusions.
- [ ] Use the official Windows App SDK threading integration, `wil::resume_foreground`, with Microsoft.Windows.ImplementationLibrary 1.0.260126.7 (latest stable refreshed 2026-10-07). The general `winrt::resume_foreground` examples do not supply the Microsoft.UI.Dispatching overload. Retain WIL's MIT notice and qualify the locked graph; do not implement an equivalent awaiter or apartment owner. Tests use one explicitly initialized STA/XAML/DispatcherQueue lifetime for their console host, native queue shutdown, and serialized assertions. Expose immutable owned `IVectorView` row snapshots and notify `Rows` when replacing them; do not expose a mutable review vector.
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

Development checkpoint (2026-10-14): Task 13's source checkpoint is committed as
`2b169b45787421019bd23dd3603666c7c22cbd4a`; the task remains active and
incomplete under execution `21e2eb0a-48a5-4f37-a7df-e1f113a30d98`. The authorized
dev23 engine transition transferred this same chat's installation actor, paused
the execution, then restored it at generation 4. Runtime selection is generation
6; earlier assurance is invalidated, not erased. Any later pending-interaction
generation is authoritative in the retained native execution record.
The operator now requires automated-first implementation and desktop use only
for bounded final acceptance. Use native compilation/static analysis, headless
model and real-storage composition tests, then automated native package/UIA
checks. Computer Use is reserved for the remaining real-picker/visual checks,
not repeated for each assertion. UIA still requires an interactive session; it
must not be described as headless. Builds use the conservative two-process
default, one architecture at a time. Consolidate before final independent
assurance. No environment refusal, interrupted run or unexecuted manual case
counts as product acceptance.

The first-party shell uses compiled bindings, the real selected-folder composition
and an HWND-owned asynchronous picker. Native identity-bound discovery excludes
actually owned transaction folders without excluding unrelated similarly named
containers. Fresh batch UUIDs are composed after completed processing/recovery.
FutureAccessList ownership now belongs to `SourceFolderAccessRetention`, consumed
by the real window. A native AppContainer test worker qualifies token metadata,
idempotent retention, explicit release, retention across object teardown, and
denial of existing ungranted parent/sibling fixtures with an app-local positive
control. It does not claim that those fixtures were picker-granted. The real
picker remains a separate acceptance boundary.

Dependency-order reconciliation: Task 13's required authority checks cannot be
qualified against Task 16's former full-trust placeholder. Bring forward only
the already-approved identity/trust/initialization prerequisite: the exact Store
identity/association, Desktop family, `packagedClassicApp`/`appContainer`, no
capabilities, and framework-dependent native initialization. Task 16's asset,
bundle, signed upgrade, certification and release obligations remain pending.
This changes execution order, not the trust requirement or release authority.

Microsoft's [activation-attribute specification](https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/element-application)
requires non-conflicting activation declarations. The native manifest reader
rejected the generated full-trust `EntryPoint` token alongside AppContainer
trust; removing the redundant entry point made the same consumer check pass.
The SDK's `AppxAddDefaultTargetDeviceFamilyItem=false` prevents its C++ UAP targets
from adding Universal to the generated Desktop-only manifest. Explicit native
auto-initializer opt-outs avoid an unnecessary restricted `packageManagement`
capability or private runtime, following [Microsoft's project guidance](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/project-properties).

Live testing exposed a XAML collection-boundary defect missed by model tests.
The corrected boundary uses native owning `param::vector_view<IInspectable>`
snapshots, `IVectorView<Object>` properties and typed DataTemplates. Native
consumer tests require correct item counts and reject mutable QueryInterface
protocols. This follows Microsoft's
[XAML collection interoperability guidance](https://learn.microsoft.com/en-us/windows/apps/develop/cpp-winrt/move-to-winrt-from-csharp#making-a-data-source-available-to-xaml-markup)
and [ItemsSource contract](https://learn.microsoft.com/en-us/uwp/api/windows.ui.xaml.controls.itemscontrol.itemssource).
MRT-backed descriptions distinguish owned-folder exclusions, skipped reparse
points and access failures in all three supported locales.

Retained development evidence is deliberately separate from final verification:

Task 13 support-contract amendment, approved by the owner on 2026-10-14:
retain native brokered Storage-item handles and require the application's
`longPathAware=true` manifest together with Windows `LongPathsEnabled=1` for
identity-bound transactions and recovery. This supersedes the former
unconditional/no-machine-opt-in staging expectation; it is not a claim that
ordinary Win32 long paths work on policy-disabled hosts. Both native test hosts
must declare the same process policy as the app. The adapter only reads the
typed Windows prerequisite; it never enables it, adds capabilities or falls back
to path-based opens. Transaction/recovery preflight refuses unqualified policy
before creating a stage, output, backup, journal generation or journal lease,
with the distinct `WindowsLongPathSupportUnavailable` error. This gate applies
even to short source paths because generated artifacts can be longer. Keep
negative refusal controls with unchanged originals and preserved recovery
artifacts; native policy-disabled VM qualification remains an explicit release
test, not inferred from this host's already-enabled setting. The previous failing
tests and native alternative probes remain retained counterevidence.
This follows [Microsoft's long-path prerequisites](https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation)
and uses [RegGetValueW's native type restriction](https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-reggetvaluew).

| Check | Observed result and boundary |
| --- | --- |
| Real-storage headless review | Two new integration cases passed 60 assertions through actual selected-folder composition, coordinator, native dispatcher and view model: owned exclusion explanations, unsupported/normalized row accounting, exact 7/0 trim disclosure, backup destination and explicit consent, with no fresh transaction or source mutation. No picker or rendered UI claim. |
| Updated headless presentation | Both x64/x86 pass 2,695 assertions after the formatted hybrid picker harness builds under dev23. Package-only workers, UIA and assistive-technology cases are excluded. These focused development reports are not canonical full verification. |
| Native source-access worker | The consolidated AppContainer worker passes 48 assertions in three cases: real app-local adapter analysis, exact unchanged source bytes, native FutureAccessList lifetime and denial of existing ungranted paths. The final real external picker control passes 22 worker assertions plus three controller assertions, including exact abc capture, actual escaping-link rejection and ungranted parent/sibling denial after the child grant. No shipping test hook, custom parser or broader capability. |
| AppContainer UIA component evidence | The latest five-case non-operator selection passed four cases/116 of 117 assertions. Its new OS-provider case revealed a numeric AutomationId collision with a list item. The locator now uses native CreateAndCondition for SDK ID plus required button type; the focused repair passes 19 assertions. Original failed reports are retained. Product trust, safe initial state, layout and minimum-height reachability remain qualified; this is not Narrator acceptance. |
| Real-picker acceptance | Real AppContainer folder-review/cancel passes 55 assertions; trim/backup policy and consent passes 66, including exact 7/0 loss, wrapped backup display and complete minimum-height reachability. A genuine SourceAccessDenied failure first exposed the path-based metadata-open defect; the shared native handle repair fixes it without capability expansion or fallback. Start was never invoked and source bytes stayed unchanged. Earlier incomplete attempts remain historical evidence. Screen-saver active=1, secure=0, timeout=600 was restored after every applied suppression, and the desktop was released to the user. |
| Manifest policy and native consumer | Authored/generated manifest identity, Desktop family, trust, empty capabilities and effective initialization checks pass. Wrong identity/trust, conflicting entry point, additional family/capabilities and file association negative controls pass. The copied Store association is byte-identical to the historical association. Native Appx manifest-reader RED/GREEN is retained separately from XML policy. |
| Native test selection | A comma-alternative negative control failed first: trailing exclusions did not constrain an earlier Catch2 filter. The ordinary native host now checks Catch2's own parsed selection and tag identities, refusing an unsafe headless/worker selection before any case executes. Positive/negative non-executing `--list-tests` controls pass on both architectures; no replacement test-spec grammar. |
| MSVC static analysis | Prepared-candidate scoped `/analyze` reports six C28182 diagnostics at WIL `resource.h:4462`. A locked-compiler reproducer independently diagnoses the aggregate-initialization false positive: a minimal braced initializer warns, equivalent parentheses do not, and both skip construction on deterministic allocation failure. The real WIL event-watcher returns `E_OUTOFMEMORY` without a null dereference; an eager-construction negative control fails as expected. This follows [C++ new-expression semantics](https://eel.is/c++draft/expr.new#22) and matches [WIL issue 610](https://github.com/microsoft/wil/issues/610). Original diagnostics remain visible: no clean-analysis claim, warning suppression or vendor patch. |
| Native picker provider and mixed-input harness | Owned broker HWND and focus are verified; real keyboard folder entry supplies the grant, then UIA drives analysis/cancellation/consent. The two mixed-input cases now pass as recorded above. The final provider locator qualifies 19 assertions using native ID/type conditions: Microsoft does not guarantee AutomationId uniqueness across a subtree. ValuePattern typed-input attempts remain failed diagnostic evidence, not a supported driver. No private picker notification, synthetic capability grant or production test hook. |
| x64/x86 headless presentation | Refreshed after test-host DPI manifest integration: 2,634 assertions per architecture passed. UI Automation and assistive-technology tests were explicitly excluded. This is development verification, not final full acceptance. |
| Affected storage/batch regression | All 13,136 x64 storage assertions and 324 x64 batch assertions passed; focused x86 discovery regression also passed. Unchanged Domain/JPEG suites were not redundantly rerun. |
| Live initial state and safe defaults | 56 assertions passed on the approved local desktop. |
| Real-picker keyboard review and cancellation | 30 assertions passed: one eligible synthetic image, retained review/source after picker cancellation, clean shutdown and unchanged source bytes. |
| Minimum size and adaptive policy layout | Current DPI-qualified x64 run passes 24 assertions, including physical width preconditions, minimum dimensions and narrow/wider reflow. Minimum-height scrolling/reachability is not implied. Earlier failures remain preserved but did not establish the intended effective viewport in a DPI-virtualized test host. |
| Minimum-height reachability | A separate contract failed first on the absent native viewport. Native scrolling now brings all five source/policy controls fully into view at 360×480 effective pixels; actions remain outside that scroller and ListView height is bounded by its native viewport. Post-change layout/defaults and real-picker keyboard/cancellation regressions also pass. |
| Visual-state host and test environment | Native UserControl content root remains the selected composition. A new native DPI precondition failed first; merging a PMv2 test-host manifest made it pass. Both architecture builds pass. A bounded layout wait alone failed before the DPI repair; do not claim it fixed the underlying coordinate mismatch. |
| Native foreground qualification | Immediate positive activation and the unactivated 30-second refusal passed previously. Current waiting-branch qualification also passes after ordinary Computer Use activation of the owned probe. |
| Headless visual-state source contract | Positive declarations, equivalent XML numeric spelling, and wrong-host/wrong-width negative controls pass without compilation or desktop access; this is not XAML compiler or rendered-layout acceptance. |

The location-specific `E_INVALIDARG` from a synthetic source under
`%LOCALAPPDATA%/Temp` remains counterevidence. Moving only the owned fixture to
`artifacts/tests` allowed the original enumeration to pass. Do not infer that all
temporary folders are unsupported or add a production enumeration fallback.
Microsoft's [file-access guidance](https://learn.microsoft.com/en-us/windows/apps/develop/files/file-access-permissions)
distinguishes app-data access from picker access.

Native foreground qualification permits ordinary activation for a bounded
30 seconds and still requires the owned foreground HWND and matching UIA PID.
It does not force input queues or bypass
[SetForegroundWindow restrictions](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setforegroundwindow).
The native presenter minimum uses display-scale-aware 360-by-480 effective pixels.
Visual states belong on the control content root, per
[Microsoft's XAML layout guidance](https://learn.microsoft.com/en-us/windows/apps/develop/ui/layouts-with-xaml).
The new XML source guard protects that selected structure and the approved
0/640/1008 thresholds; native XAML compilation, bindings, reflow, scrolling and
control reachability still need their own acceptance checks.

Host-crash investigation found an 08:55–08:56 local-time cluster of MSBuild
page-file/worker-launch failures and a DWM fatal-memory-exhaustion termination,
strongly correlated with this chat's formerly unbounded build. This is a
contributor, not proof of the sole cause of the host hangs. Both subsequent boot
reports also contain fatal WHEA/Intel PMC crash-log signatures; their root cause
remains unresolved. The later bounded build finished before the next interruption.
The operator subsequently enabled per-drive system-managed paging on C: and E:.
At 11:03 local, observed commit headroom was approximately 17.8 GiB; this improves
memory capacity but does not resolve the WHEA reports. Builds subsequently resumed
under explicit operator approval, with desktop testing still withheld.

`Invoke-Build.ps1` now defaults to two concurrent MSBuild processes and accepts a
positive `MaximumBuildProcessCount` override. The entry-point contract and
zero-count negative control passed. Bare `-m` selects the processor count, per
the [MSBuild specification](https://learn.microsoft.com/en-us/visualstudio/msbuild/msbuild-command-line-reference).
Two is a conservative recovery default, not an established performance optimum.
No concurrency benchmark was run. Current sequential x64/x86 builds pass at two
workers; those incremental runs do not establish a safe maximum or optimal speed.

For any later desktop-test session in this thread, the operator permits temporary
screen-saver suppression with captured-state restoration and readback afterward.
Do not alter lock/security policy. The bounded external helper and raw failure
logs remain retained evidence; hard interruption can prevent its finally block.
Both current desktop-test sessions restored and verified the original active,
secure and timeout values. Suppression is limited to an active bounded desktop
test session; it is unnecessary during headless-only work.

The final real-picker checks have passed under AppContainer trust, including
minimum-size review reachability after consent; the desktop controls were
restored. Reuse those acceptance observations rather than repeating manual input.
The final ordinary review found a cross-selection exclusion defect: historical
output/backup grants outside the selected tree must not be opened during discovery.
Retain each actually created folder's native identity and canonical location,
scope its relevance by native path components, and revalidate both for relevant
folders. A recorded path is comparison metadata, never an I/O authority. Preserve
exclusions when recursively selecting an ancestor and refuse relevant identity
substitution. Match encountered directory IDs against historical records as well:
a relocated known output/backup must be refused, not traversed as new input.
Focused repair evidence is separate from final task assurance;
neither this source checkpoint nor the repair claims completion or release.

**Files:**

- Create: `src/JpgSpinner.App/App.xaml`
- Create: `src/JpgSpinner.App/App.xaml.h`
- Create: `src/JpgSpinner.App/App.xaml.cpp`
- Create: `src/JpgSpinner.App/MainWindow.xaml`
- Create: `src/JpgSpinner.App/MainWindow.idl`
- Create: `src/JpgSpinner.App/MainWindow.xaml.h`
- Create: `src/JpgSpinner.App/MainWindow.xaml.cpp`
- Create: `tests/JpgSpinner.Presentation.Tests/WindowStateAutomationTests.cpp`
- Create: `tests/JpgSpinner.Presentation.Tests/PresentationTestHost.manifest`
- Create: `src/JpgSpinner.App/Composition/SourceFolderAccessRetention.h`
- Create: `tests/JpgSpinner.Presentation.Tests/SourceFolderAccessRetentionTests.cpp`
- Create: `scripts/tests/Test-PresentationTestSelection.ps1`
- Create (Task 16 prerequisite): `scripts/PackageManifestPolicy.psm1`
- Create (Task 16 prerequisite): `scripts/Test-PackageManifest.ps1`
- Create (Task 16 prerequisite): `scripts/tests/Test-PackageManifestPolicy.ps1`
- Create: `scripts/Test-InteractiveUiEnvironment.ps1`
- Create: `scripts/tests/Test-ShellVisualStateDeclarations.ps1`
- Modify: `scripts/Invoke-Build.ps1`
- Modify: `src/JpgSpinner.App/JpgSpinner.App.vcxproj`

### Step 13.1: RED with black-box UI Automation contracts

- [ ] Implement `Test-InteractiveUiEnvironment.ps1` first. It requires an unlocked interactive desktop, a foreground-capable test user, UI Automation availability, isolated package deployment rights, and no service-session execution. It exits with a distinct environment-not-qualified result rather than misreporting an automation timeout as product failure.
- [ ] Register and launch an isolated local test package from the integration test, then attach through Windows UI Automation by process ID.
- [ ] Require stable automation IDs for the source picker, traversal scope, analysis status, result table, output disposition, edge handling, scan organization, backup destination, start, cancel, and results summary.
- [ ] Require only controls relevant to the current semantic state to be enabled and focusable.
- [x] Require a keyboard-only path from source selection through review without invoking processing in the test. The owned local-desktop scenario passed with unchanged fixture bytes; native UIA supplied only initial focus and independent assertions.
- [ ] Run `[presentation][automation][state]`; expected RED is the absent window/control tree, not an automation timeout.

### Step 13.2: GREEN with one window and semantic states

- [x] Compose one `MainWindow` with state-specific regions; do not port old scenario pages, settings flyouts, or explanation pages.
- [x] Use Microsoft's native `ApplicationDefinition`-generated `AppT`/`IXamlMetadataProvider` contract for `App`, and declare the bindable `MainWindow` and presentation runtime classes in their focused IDL files. Verify generated metadata before relying on `x:Bind`. The native template's `App.idl` can contain an empty namespace; no consumer here requires a separately projected `App` runtime class. Do not add redundant activation metadata or handwritten ABI declarations merely to satisfy an implementation-detail checklist.
- [ ] Apply `MicaBackdrop`, the stable WinUI integrated title-bar pattern, transparent root surfaces, layer/card theme brushes, and `InfoBar` for non-modal status. Rely on WinUI’s solid fallback rather than implementing a material shim.
- [ ] Use only first-party WinUI controls. Use `ListView` for processing rows, standard buttons/pickers/progress, and labeled Segoe Fluent Icons where helpful; do not add a third-party control package or icon-only action.
- [x] Use `x:Bind` where compile-time binding catches naming errors; use observable properties only where state changes.
- [ ] Show source selection, analysis, review, processing, results, and recovery-required regions from `MainWindowViewModel` state.
- [x] Keep code-behind limited to window/picker interop and presentation-only events. All processing decisions remain in tested view models and native modules.
- [ ] Assign stable `AutomationProperties.AutomationId`, name, help text, and live-setting values in XAML.
- [ ] Add visual states at 640 and 1008 effective pixels: stack review facts below 640, use compact list/detail at 640–1007, and expose non-essential review columns at 1008 and above. Never constrain localized prose to a fixed height.

### Step 13.3: RED/GREEN folder picking and AppContainer access

- [x] Test cancellation from `FolderPicker` leaves the previous stable state and selected source unchanged. The same native scenario reopens the system picker, observes owner disable/re-enable, and verifies retained review, source and eligibility after cancellation.
- [ ] Test a picked folder produces the exact storage capability consumed by the batch coordinator and does not grant access to parents or siblings.
- [x] Initialize the WinRT picker with the WinUI HWND through the documented window-initialization interface, then await it; never block the UI thread.
- [ ] Add only the FutureAccessList token needed to resume a user-selected folder during the current/recoverable transaction. Bound and remove stale tokens deliberately.
- [ ] Explain HWND initialization, token lifetime, and AppContainer authority in comments.

### Step 13.4: RED/GREEN honest review controls

- [x] In automation tests require default values: create copy, selected folder only, perfect coefficients, preserve source scans. Native UIA SelectionPattern independently compares the selected items with the localized resource candidates; x64 local-desktop qualification passed (56 assertions including initial state).
- [ ] Select recursive traversal and prove app-owned output/backup roots appear as excluded explanation, not silently omitted.
- [ ] Select replacement and require the backup destination to become visible before the Start button enables.
- [ ] Select trim for an imperfect file and require exact discarded-edge dimensions plus an explicit acknowledgment.
- [ ] Require unsupported rows to expose their reason and remain excluded from execution totals.

### Step 13.5: Verify and commit

- [x] Run `./scripts/tests/Test-ShellVisualStateDeclarations.ps1` without compilation or app activation. Require the authored control-root/threshold contract and retained wrong-host/wrong-width negative controls; equivalent XML numeric values must pass. This supplementary source guard does not replace native XAML compilation or interactive acceptance and is not yet wired into the canonical verification profile.
- [ ] Run view-model, localization-contract, WinMD, and x:Bind build tests on the normal headless lane. Run black-box automation only after `Test-InteractiveUiEnvironment.ps1` passes in an isolated interactive Windows VM/self-hosted lane or the explicitly operator-approved local desktop lane. This interactive qualification remains required and cannot become an optional hosted-runner skip; record which lane actually ran, without claiming isolation for the local desktop.
- [ ] Run the app manually once to verify picker ownership, resize behavior, minimum window size, and clean shutdown.
- [ ] Commit:

```powershell
git add src/JpgSpinner.App tests/JpgSpinner.Presentation.Tests scripts/Test-InteractiveUiEnvironment.ps1
git commit -m "feat: add the accessible WinUI image workflow"
```

## Task 14: Integrate startup recovery, suspension-safe cancellation, and local diagnostics

First native slice: `WindowsStorageImageFileTransactionRecoveryCatalog` inspects
the app-owned store through the existing immutable-chain validator. It reports
only transaction IDs and native selected-root identities; source path strings
are not startup permissions or diagnostic fields. Windows' native hexadecimal
decoder owns identity conversion. The catalog creates no journal lease/artifact
and is not an atomic global snapshot or permission to release a cache entry.
Only the two existing terminal journal states close a requirement; a recorded
commit still needs stage reconciliation. Startup composition will restore actual
folders through [FutureAccessList](https://learn.microsoft.com/en-us/uwp/api/windows.storage.accesscache.storageitemaccesslist?view=winrt-28000),
match native identities and delegate effects to the current recovery engine.
Keep normal processing gated until that startup orchestration is complete.
This read-only catalog is an intermediate source checkpoint, not implementation
of startup orchestration, grant cleanup, closing behavior or local diagnostics.

The next source slice composes startup recovery from native FutureAccessList
restoration and `WindowsStorageStartupRecovery`. It matches actual granted-root
identities before any recovery effects, pins required grants, delegates serial
reconciliation to the existing transaction engine, and reinspects the catalog
before permitting normal selection. Missing/wrong grants and cancellation retain
artifacts and keep the startup gate closed. The WinUI shell awaits this work off
the UI thread, shows a localized recovery refusal and recovered-transaction count,
and offers a separate HWND-owned picker to restore access for recovery only.
Tokens are not transaction IDs; native cache metadata never supplies access, and
startup does not evict entries merely because an inventory appears empty.
The windowless AppContainer test worker qualifies actual native cache restoration
and original/stage/journal outcomes. Headless projection/resource tests do not
claim an interactive picker acceptance pass. Closing, local diagnostics, export,
and final Task 14 acceptance remain subsequent work.

Shutdown/diagnostic implementation now uses native AppWindow closing admission
with cooperative async cancellation and actual-work completion. The AppWindow
source does not support weak references: subscribe/remove by event token with
a weak receiver, following [Microsoft's event guidance](https://learn.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/handle-events),
not `auto_revoke` on that source. Diagnostic records expose no arbitrary text,
paths, image/metadata payloads, endpoint or device ID. Windows JSON owns wire
syntax, and standard chrono/from_chars/native GUID consumers validate replay.
Shared Domain symbols also supply localized error resource identities.
The app-private log defaults to 64 retained files/256 KiB, with a 4096-byte
record bound, native exclusive writer lease, monotonic publication sequence,
and one reserved scratch slot renamed only after its bytes are written.
Export rehydrates the closed typed schema instead of copying persisted text.
Observed unknown/corrupt entries remain untouched and refuse logging/export;
native Storage can omit unsupported objects, so this is not a physical audit.
Reported files reuse brokered direct-child containment proof; no path fallback
exists in production. One representative structured failure is sampled per
awaited operation to bound logging overhead; logger failure cannot change the
coordinator/recovery outcome. The actual HWND-owned export picker is cancellable,
and successful provider updates are awaited. Its real cancel acceptance compares
all local record bytes and empty-lease sizes without granting a source folder.
Remaining Task 14 evidence is kept distinct: native x86 Debug UI deployment needs
the missing VC Debug framework, and final candidate review/checks do not imply
release or HISEW completion while the known evidence-refresh bug remains.

The shutdown/diagnostic source checkpoint also preserves each startup refusal's
localized explanation and remedy alongside the artifact-preservation notice.
In particular, a Windows long-path prerequisite refusal must retain its
administrator/restart guidance rather than suggest reconnecting a drive.
The final review's targeted repair has a headless native-resource regression
for long-path and recovery-conflict guidance in all three supported languages.
`WindowsAppSDKSelfContained` remains root-owned; the app inherits the
framework-dependent setting instead of redeclaring it.
This is an implementation checkpoint, not final Task 14 acceptance: active-work
close, successful provider export, policy-disabled startup UI presentation,
and the unavailable x86 Debug UI lane remain explicit acceptance obligations.
Pause after the signed shutdown/diagnostic commit as requested; resume those
bounded acceptance checks before Task 15, without repeating unchanged evidence
solely for the known HISEW receipt-admission defect.

On the subsequently authorized resume, diagnostic export is one app-private
`exportDiagnosticLogAsync` operation used by both the window and native tests.
It accepts only an existing StorageFile grant and logger, validates the .jsonl
destination before effects, snapshots on the native background MTA, restores
the caller's apartment for provider work, and awaits native write/update
completion with cancellation propagation. No path-based grant, test hook or
provider shim is added. Microsoft owns
[cached-provider completion](https://learn.microsoft.com/en-us/uwp/api/windows.storage.cachedfilemanager.completeupdatesasync)
and [C++/WinRT cancellation](https://learn.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/concurrency-2).
Directly created unpackaged test files on this lane lack cached-provider completion metadata;
their explicit native ERROR_NOT_FOUND precondition qualifies error propagation,
not successful picker export. Corrupt-log refusal preserves destination bytes.
The actual AppContainer's local files do support provider completion; the
separate windowless packaged-worker positive control proves that precondition
and requires completed export with the same independent byte/retention oracle.
The actual Save-provider ID 1001 and its supported Value/Invoke patterns qualify
the real picker grant and successful local-provider export, with exact output
bytes and unchanged retained records. This is native UI Automation acceptance,
not a claim of keyboard-only acceptance or arbitrary cloud-provider coverage.
The earlier Folder-provider edt1 assumption and unavailable Computer Use modal
observation were fixture/input gaps, not product-regression RED.
Remaining active-processing close and policy-disabled startup UI observations
stay in the final acceptance ledger; source implementation can proceed to Task
15 without declaring those release obligations passed.

**Files:**

- Create: `src/JpgSpinner.WindowsStorage/include/jpg_spinner/storage/WindowsStorageImageFileTransactionRecoveryCatalog.h`
- Create: `src/JpgSpinner.WindowsStorage/src/WindowsStorageImageFileTransactionRecoveryCatalog.cpp`
- Create: `tests/JpgSpinner.WindowsStorage.Tests/ImageFileTransactionRecoveryCatalogTests.cpp`
- Create: `src/JpgSpinner.WindowsStorage/include/jpg_spinner/storage/WindowsStorageStartupRecovery.h`
- Create: `src/JpgSpinner.WindowsStorage/src/WindowsStorageStartupRecovery.cpp`
- Create: `src/JpgSpinner.App/Composition/StartupRecoveryComposition.h`

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

Implementation proceeds in logical source checkpoints; source implementation is
not final assistive-technology qualification. Native Windows `DecimalFormatter`
formats unsigned 64-bit counts/pixel extents without a Double intermediate, with
whole-number/grouping options; numeric coordinator facts remain numeric. See
[Microsoft's global-ready formatting guidance](https://learn.microsoft.com/en-us/windows/apps/design/globalizing/use-global-ready-formats)
and [the UInt64 entry point](https://learn.microsoft.com/en-us/uwp/api/windows.globalization.numberformatting.decimalformatter.formatuint).
No dates or byte sizes are currently displayed, so no unused formatters are added.
Current label/value separation uses no interpolated placeholders or plural
templates; source-contract tests reject their accidental introduction until a
native formatting/plural contract and per-locale tests are deliberately adopted.

Native XML DOM reads test-only copies of the owning XAML/RESW declarations, with
external entities and DTDs disabled. It complements, not replaces, MIDL/compiled
XAML and native MRT language-provenance tests. Locale key equality, per-entry
translator context, single-character collision-free native access keys, restrained
live declarations and row-major policy declaration order are checked headlessly.
Native controls/peers remain the keyboard and accessibility implementation; no
custom key parser, speech implementation, automation peer or production test hook
is added. [Access-key localization](https://learn.microsoft.com/en-us/windows/apps/develop/input/access-keys)
and [native keyboard order](https://learn.microsoft.com/en-us/windows/apps/design/accessibility/keyboard-accessibility)
own those contracts. The explicit release protocol is
[`docs/accessibility-release-checklist.md`](../accessibility-release-checklist.md).
Human Narrator/FastPass/contrast/reflow/RTL and actual UIA focus observations
remain distinct release obligations; headless passes cannot close them.
The consolidated checkpoint passes 12,120 headless Presentation assertions on
both x64 and x86, with compiled-resource language provenance and repository
policy passing. The bounded local UIA follow-up could not qualify native
foreground focus, so no product/UI focus test executed and no such pass is
claimed. Screen-saver active/secure/timeout state was restored exactly. Do not
repeat that unavailable check without a changed lane/foreground condition;
continue subsequent safe source work while retaining final release obligations.

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

- [ ] Run headless accessibility-contract tests for resource coverage, WinMD metadata, automation IDs, names/help-text declarations, and view-model state before launching a window. Run locally during development and through explicit manual qualification; pull requests must not automatically start remote CI.
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

The manifest/association and least-privilege policy were brought forward as
Task13 prerequisites; preserve those qualified native contracts rather than
adding alternate identities or full-trust bootstrap paths. The artwork source
checkpoint retains byte-identical existing per-context masters under
[`assets-source`](../../assets-source/README.md). Native Windows GDI+ generates
55 PNG variants: manifest scales100/200/400 plus fourteen exact app-list target
sizes in three theme forms. No custom PNG codec, AI branding or runtime image
generation is added. Two same-lane invocations produce identical hashes; this
does not promise byte identity across OS/.NET/GDI+ versions. Required larger
rasters are documented as upsampled, not higher-detail sources. Explicit owned
orange/white palette controls allow only native8-bit resampling quantization;
exact dimensions, white-glyph presence and original master hashes remain checked.
[Microsoft's current icon guidance](https://learn.microsoft.com/en-us/windows/apps/design/iconography/app-icon-construction)
owns the qualifier/dimension contract. Actual shell rendering/contrast and Store
qualification remain final acceptance. The unused template lock-screen logo is
recycled recoverably; only generated shipping PNGs enter the asset inventory.
Native x64 Debug build and existing manifest-preservation policy pass with the
new inventory; this is not signed bundle, trusted upgrade or WACK qualification.

Unsigned package production reuses `Invoke-Build.ps1` with a closed native build
entry point (`PackagedApplication`), an absent explicit output directory and the
selected SDK's public `GenerateAppxPackageOnBuild`/`AppxPackageDir` properties.
Default builds still use the whole modern solution. Project-owned compiler,
SDK and framework settings are not masked by new toolchain globals. Native
single-project MSIX targets own package construction; no private target call,
hand-written ZIP or legacy packaging shim is introduced.
`Test-PackageBuildContract.ps1` observed successful native app build without MSIX
(RED), then actual x64 Debug MSIX production (GREEN). Separate native MakeAppx
extraction/manifest/payload/artwork checks establish unsigned app identity,
architecture and framework dependency, rather than accepting dependency sidecars
as the product. Output-entry/fresh-directory refusals preserve the source sentinel.

The locked MSIX targets' CI behavior is configuration-dependent: Release requests
upload artifacts, while Debug remains a qualification package. Actual Release
execution also emits sideload/dependency working material despite pre-build
property evaluation. Do not infer file absence from that evaluation or general
CI examples, or bypass the consumer with a private property shim. Measure the
actual drop and reconcile duplicate working copies within its finite allowance.
Duplicate Debug framework/symbol working copies were hash-matched to retained
originals and recycled; raw outcomes and both main packages remain. Release
multi-architecture packages/bundle, trusted upgrade, physical ARM64 and WACK
qualification remain subsequent obligations. See
[official CI packaging guidance](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/ci-for-winui3)
and [native MakeAppx](https://learn.microsoft.com/en-us/windows/msix/package/create-app-package-with-makeappx-tool).

ARM64 WinUI compilation additionally requires the stable
`Microsoft.VisualStudio.Component.UWP.VC.ARM64` platform-support component;
`Microsoft.VisualStudio.Component.VC.Tools.ARM64` alone supplies the compiler,
not the UAP platform metadata. Native `CppWinRTVerifyKitVersion` reproduced
MSB8036 with `TargetPlatformVersion=7.0` / `TargetPlatformIdentifier=Windows`
on the incomplete installation, while x64 selected the locked SDK and UAP.
The repository configuration and prerequisite policy now require the component.
The exact component installation initially stopped at 0x80131500 under
`--noUpdateInstaller`. After separate human approval, Installer updated from
4.10.30.62513 to 4.11.24.2075 and added the component; native discovery now
confirms it and the same `CppWinRTVerifyKitVersion` target passes. Build Tools
remains 18.10.12306.391. No SDK retargeting, vendor-file edit, toolchain-property
override or automatic restart was requested. The ARM64 package/runtime gates
remain distinct from this repaired prerequisite. See
[Microsoft's Installer parameters](https://learn.microsoft.com/en-us/visualstudio/install/use-command-line-parameters-to-install-visual-studio)
for component modification and the no-update/no-restart controls.

The repaired prerequisite now permits an actual unsigned ARM64 Release package;
native MakeAppx extraction, the existing manifest/framework/artwork policy and
architecture checks pass for all three independently produced Release packages.
`New-MsixBundle.ps1` composes only those selected packages with the owning SDK's
MakeAppx, semantic validation enabled, `/no`, and explicit `/bv 2.0.0.0`.
The positive contract reached the unimplemented producer (RED), then built and
unbundled a real x86/x64/ARM64 container (GREEN). The native reader verifies
the exact Store identity, publisher and version, and architecture membership.
An additional real negative control proves MakeAppx rejects three distinct paths
containing one repeated architecture (0x80080204); input count/path checks do
not substitute for native semantic validation. Qualified inputs remain unchanged.
The first successful composition exposed a PowerShell `SetEquals` type-binding
error in the test; that setup defect was corrected with a typed string array,
and a fresh composition passed. Six repeated ARM64 drop dependency files were
hash-matched to retained x64-drop counterparts and recycled; the unique ARM64
VCLibs package, actual app packages/bundles, symbols and raw failures remain.
No signed package, trusted upgrade, physical ARM64 runtime, WACK, Store
submission or complete Task 16 acceptance is claimed by this source checkpoint.

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

**Deployment-only interactive workflow source checkpoint (2026-10-15):**
`New-InteractiveTestDrop.ps1` preserves the complete prebuilt app and presentation
test-host outputs; a separate hosted main publisher authenticates their input
context using GitHub's native attestation action. The manual interactive workflow
admits only a successful same-commit/manual CI producer and an existing
reviewer-protected environment. The dedicated self-hosted job has read-only
permissions, native input-attestation/hash verification and the existing bounded
native desktop qualification/test host; no build is performed on the test VM.
A separate hosted job signs schema-bound, nonempty, fresh, passing native JUnit
evidence, with operator acceptance explicitly pending. Native Catch2 assertion
and reported-case counts retain their actual meanings. Automated UIA excludes
operator-assisted cases rather than waiting for unattended human input. Source
contracts, typed REST/JSON refusal controls and native XML result controls pass;
actual protected environment/runner configuration, hosted dispatch and all
assistive-technology/operator acceptance remain open. See the maintained
[input and execution evidence procedure](../supply-chain-evidence.md#prebuilt-interactive-inputs).

**Incremental payload-reproducibility source checkpoint (2026-10-15):**
`New-PayloadManifest.ps1` uses the already-adopted PowerShell/.NET runtime,
native filesystem/stream APIs, SHA-256, and `Utf8JsonWriter`; no additional
JSON grammar, hashing implementation or dependency resolver is introduced.
The residual project contract is a schema-version-1, BOM-free UTF-8/LF JSON
record with ordinal normalized relative paths, byte lengths and lowercase
SHA-256. Absolute roots and timestamps are deliberately absent. Output must
be new and outside the quiescent input tree; traversal checks native reparse
attributes before recursion. `Test-PayloadManifestEquality.ps1` compares the
canonical producer outputs, not signed-container identity or build correctness.
The test reached the unimplemented producer (RED), then verified independent
fixed hashes/order/lengths (GREEN), distinct-root equality, an actual one-byte
same-length mismatch, and existing-output/self-inventory/native-junction refusals.
Two independent clean application builds, CI/SBOM/provenance and release
qualification remain separate obligations. Native API references:
[File.OpenRead](https://learn.microsoft.com/en-us/dotnet/api/system.io.file.openread),
[SHA256.HashData](https://learn.microsoft.com/en-us/dotnet/api/system.security.cryptography.sha256.hashdata),
and [Utf8JsonWriter](https://learn.microsoft.com/en-us/dotnet/api/system.text.json.utf8jsonwriter).

**Native workflow-validation source checkpoint (2026-10-15):**
Latest upstream release/API inspection selects YamlDotNet 18.1.0 and actionlint
1.7.12 (both MIT, tool-only, not application runtime dependencies). The standard
PowerShell runtime has no built-in YAML/Actions semantic validator. YamlDotNet's
representation model rejects duplicate mapping keys before conversion; native
scanner comment events retain version annotations without a parallel YAML
grammar. actionlint owns GitHub workflow/expression syntax. Residual repository
policy is explicit action pins, runner/permission/timeout/concurrency constraints,
PR secret-token prohibition and bounded artifact retention. No custom YAML or
Actions expression parser is introduced. Tool archives are exact-version/hash
pinned; cached DLL/EXE bytes are compared to their authenticated archive entries
before loading. NuGet repository signature and GitHub actionlint attestation
verification passed during qualification; the extracted MIT notices are retained.

The actual positive fixture passes both native consumers; duplicate-key and
undefined-context fixtures fail at their respective consumers. Producer/policy
stubs reached meaningful RED, then passed focused GREEN and archive/assembly
poison controls. Nine targeted workflow mutations fail. A separate relative-path
regression reached RED and now passes after binding paths through PowerShell's
provider, not the distinct process current directory. Setup-only failures (log
path, ABI version expectation, scanner constructor and invalid join operator)
are retained separately. These checks do not claim hosted CI, execution of any
workflow step, whole Task 17 completion, or application release qualification.
The default policy intentionally requires the five final workflows; fixture mode
qualifies the validator before those workflows are implemented and integrated.

Sources and reviewed contracts:
[YamlDotNet release](https://github.com/aaubry/YamlDotNet/releases/tag/v18.1.0),
[native duplicate-key handling](https://github.com/aaubry/YamlDotNet/blob/v18.1.0/YamlDotNet/RepresentationModel/YamlMappingNode.cs),
[actionlint release and attested assets](https://github.com/rhysd/actionlint/releases/tag/v1.7.12),
[YamlDotNet MIT notice](https://github.com/aaubry/YamlDotNet/blob/v18.1.0/LICENSE.txt),
[actionlint MIT notice](https://github.com/rhysd/actionlint/blob/v1.7.12/LICENSE.txt),
and [PowerShell/native working-directory distinction](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.management/set-location#notes).
The action pins below supersede the original research snapshot using current
official release tags resolved to full commits; all four action licenses remain
MIT, and their selected entry points use the hosted runner's Node 24 runtime.

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
- Create: `.github/workflows/scanner-fuzz.yml`
- Create: `.github/workflows/store-package.yml`
- Create: `.github/dependabot.yml`
- Modify: `scripts/Test-RepositoryPolicy.ps1`

**Incremental selected-input provenance checkpoint (2026-10-15):**
`New-BuildProvenance.ps1` observes the owning app project's native MSBuild
properties, selected executable file versions/SHA-256, committed modern project
NuGet locks, vcpkg manifest/baseline, current action commits, source cleanliness,
and actual hosted `ImageVersion` when hosted. Installation-root placeholders
preserve tool location scope without exposing a developer profile. The record
does not certify that a build or tests executed. `-RequireCleanSourceTree` refuses
a dirty checkout rather than presenting its HEAD as an exact build candidate.

`Test-BuildProvenance.ps1` reuses the platform strict-JSON reader before lossy
PowerShell conversion and validates original JSON through native `Test-Json`
against a closed local JSON Schema 2020-12 contract. Literal positive and six
negative controls pass: missing required fields with full native diagnostics,
wrong JSON boolean type, absolute profile path, duplicate member, stale action
commit and array root. The live observation test also passes against actual
MSBuild and selected binaries; local identity remains explicitly non-hosted.
The retained RED/GREEN logs and observed record are in the owning execution's
external Task 17 evidence group. This source checkpoint does not close hosted
execution, SBOM completeness, two-clean-build reproducibility, interactive
qualification or whole Task 17 acceptance.

Selected executable versions are observed independently of the selected toolset
directory name: servicing can change bytes inside that directory. The observed
compiler is 19.51.36260.0 and linker 14.51.36260.0 under selected directory
14.51.36231. Their hashes, not that directory name alone, identify this local
observation; release qualification must bind its own exact tools and artifacts.

**Incremental CI/analysis source checkpoint (2026-10-15):** The first three
workflows now declare the exact five-configuration build matrix, headless-only
x86/x64 execution, ARM64 compile-only status, clean-source provenance, x64 JPEG
ASan, native MSVC analysis, manual-build CodeQL and bounded scanner fuzzing.
Native YAML/action validation and the exact matrix/source-wiring contract pass;
hosted execution is not claimed. Protected packaging/interactive workflows,
scanner corpus minimization, the default five-workflow policy integration and
full Task 17 assurance remain open. Dependency caches exclude packages/signing
material and include architecture, configuration, toolset, baseline and locks.

**Remote execution policy amendment (user decision, 2026-10-15):** Every remote
workflow is manual-only through `workflow_dispatch`, including build/test,
CodeQL, scanner fuzzing and future protected package/interactive qualification.
No push, pull-request, schedule, release event, reusable-workflow call or chained
workflow event may start a remote run. A deliberate operator may select a ref
and dispatch qualification before release; no automatic run is a release gate.
The native YAML-based repository policy enforces this for every workflow. This
decision supersedes earlier automatic-trigger instructions and does not waive
any test, analysis, preservation, accessibility or package acceptance criteria.

`Invoke-StaticAnalysis.ps1` uses public native `RunCodeAnalysis` and
`CodeAnalysisTreatWarningsAsErrors`: compiler `/WX` alone did not fail the real
C6011 result-ingestion control. Positive fresh-report and actual C6011 failure
controls pass on the current driver. Dependency-header exclusions use the
documented `CAExcludePath` environment input, scoped to the actual NuGet root
and installed vcpkg tree; inherited exclusions are replaced and restored.
No private MSBuild property or warning suppression is used.

The first-party rebuild exposed a missing interprocedural BCrypt handle
contract, oversized test-stack buffers and a Catch2 optional-flow diagnostic.
SAL now documents the real handle lifetime; heap-backed buffers preserve the
same capacities and expectations; an explicit failing branch preserves the
native read-refusal oracle and establishes the optional precondition. Native
incremental analysis now passes after those repairs, and the focused runtime
digest regression passes 86 assertions. This is development feedback using
native invalidation, not a fresh final whole-tree rebuild. Default/CI analysis
still uses `Rebuild`; final candidate assurance retains that obligation. Existing
dependency LNK4099 missing-PDB warnings remain visible and separately recorded.

The affected x64 Release headless run completed every test report without
assertion failures but exposed a Presentation process-exit hang. A fixed-seed,
single recovery-observer case reproduced it twice within a 15-second owned-host
budget. Targeted instrumentation localized the wait to native `ShutdownQueue`
during static destruction, after the event loop and XAML close returned. The
fixture now releases its lazily created dispatcher from Catch2's native
`testRunEnded` callback, while the runtime is still operational. The same
single-case regression and full headless Presentation suite then exited zero;
all temporary instrumentation was removed. CI runs the bounded regression on
its actual x64 Release host before the ordinary suites. This repairs test-host
lifetime, not a desktop/application shutdown claim. Other unchanged headless
project reports from the preceding run remain separate retained evidence.

### Step 17.1: RED on immutable CI policy

- [ ] Extend repository policy to parse workflow YAML and reject mutable action tags, `windows-latest`, prerelease tools, unbounded permissions, missing timeouts, artifact retention over 30 days, and any secret passed to pull-request builds.
- [ ] Require `scripts/Invoke-Sbom.ps1` to consume the qualified `eng/artifact-sbom` adapter, readonly Go module graph and `eng/sbom-tools.json` pins for Syft SDK 1.54.1, SPDX Workgroup Tools 2.0.7 and the official SPDX 3.0.1 schema. Reject moving release URLs, mismatched validator digests, module replacements and `Microsoft.Sbom.Targets`, whose NuGet `Pack` integration does not describe an MSIX release drop.
- [ ] Require these reviewed action commits with a version comment:
  - `actions/checkout` v7.0.1 at `3d3c42e5aac5ba805825da76410c181273ba90b1`;
  - `actions/cache` v6.1.0 at `55cc8345863c7cc4c66a329aec7e433d2d1c52a9`;
  - `actions/upload-artifact` v7.0.2 at `cf430e030ddbb5b0abf93d22962f4752f3646cd9`;
  - `github/codeql-action` v4.38.3 at `24c54180a607b1449ed407dd24f251e4e9147c8d`.
- [ ] Require `windows-2025-vs2026`, least-privilege workflow permissions, job timeouts, and concurrency cancellation for superseded manually dispatched runs.
- [ ] Require every build/package job to capture runner `ImageVersion`, OS build, exact `VCToolsVersion`, full `cl.exe`/`link.exe` versions and paths, Windows SDK/MakeAppx versions, NuGet lock hashes, vcpkg baseline, action commits, and source commit. The runner label is selection policy, not immutable provenance.
- [ ] Implement `Test-BuildProvenance.ps1` before workflow provenance emission. Give it an empty or deliberately incomplete JSON record and observe RED diagnostics for every missing field; then make the workflow emit one versioned, schema-checked record whose executable/file paths are normalized without leaking the developer profile path.
- [ ] Run policy; expected RED names the absent workflows and the absent SBOM script/tool pin.

### Step 17.2: GREEN continuous integration matrix

**Unsigned packaging source checkpoint (2026-10-15):** The manual-only
`continuous-integration.yml` now defines an `unsigned-package` job depending on
policy, all build configurations, ASan and native analysis. It sequentially uses
the existing real package-build contract for Release x86/x64/ARM64, validates
each unsigned package, records clean-source provenance, composes a native bundle
and inspects its payloads. Its 14-day artifact contains the unsigned bundle,
provenance and diagnostics, not signing credentials or a release-approval claim.
The source-wiring test first failed for the absent job, then passed with native
YamlDotNet/actionlint validation. Trigger mutation controls still reject push,
PR, release, reusable-call, scheduled and mixed events. This does not dispatch
CI, prove the hosted image/toolchain or claim hosted packaging succeeded.

- [ ] On explicit manual qualification, restore qualified locked NuGet/vcpkg inputs, build Debug and Release for x86/x64, run all headless Domain/JPEG/Storage/Batch/Presentation-contract tests on x86/x64, compile Release ARM64, run x64 AddressSanitizer suites, run MSVC `/analyze`, validate manifests, and upload binary logs/test reports on failure. Do not run black-box UI Automation, Narrator, or Accessibility Insights on a GitHub-hosted service session. Do not register push, pull-request or scheduled triggers.
- [ ] Define a required separate interactive Windows VM/self-hosted workflow or protected qualification job for black-box UI Automation and assistive-technology checks. It begins with `Test-InteractiveUiEnvironment.ps1`, never has pull-request secrets, and publishes signed test evidence; a hosted headless pass cannot substitute for this gate.
- [ ] Use a separate package job for the unsigned x86/x64/ARM64 bundle. Package creation must not require Store credentials.
- [ ] Cache only content-addressed dependency/build inputs whose key includes vcpkg baseline, manifest hash, architecture, toolset, and configuration. Never cache signing material or staged packages.
- [ ] Require warnings as errors and fail on sanitizer findings, leaked handles, unhandled exceptions, or flaky-test retries.

### Step 17.3: GREEN CodeQL and fuzz workflows

**Native corpus checkpoint (2026-10-15):** The retained one-second regression
first completed the existing instrumented fuzz path and failed because no
minimized corpus was produced. Each invocation now owns a fresh UUID run,
preserves full inputs and native diagnostics, replays the complete corpus before
native `-merge=1`, then audits the result with a second native merge that must add
no inputs. Deliberately redundant synthetic seeds prove actual reduction rather
than copying everything; original inputs remain unchanged. Process deadlines,
two-process MSBuild concurrency and the documented native vcpkg Release mapping
bound this development path. No custom minimizer or coverage-prose parser is
introduced; [LLVM's corpus contract](https://llvm.org/docs/LibFuzzer.html#corpus)
owns feature selection. These bounded development passes do not establish the
final aggregate fuzz duration or authorize a remote CI run.

- [ ] CodeQL runs C++ analysis on a clean x64 build with `security-extended`; no high-severity result may remain open at release.
- [ ] Manually dispatched fuzz qualification runs the scanner target under AddressSanitizer for a bounded duration, minimizes new corpus inputs, uploads only non-user corpus/reproducers, and fails on crash, timeout, leak, or memory-limit violation. It has no scheduled trigger.
- [ ] A pull request adding a fuzzer reproducer first adds a named deterministic regression test that fails without the fix.

### Step 17.4: Pin SBOM generation and dependency updates

**Native bundle-inspection checkpoint (2026-10-15):**
`Expand-JpgSpinnerBundle.ps1` now uses the owning locked MSBuild targets to
select MakeAppx, then native `unbundle` and `unpack` with validation enabled
and overwrite disabled. It extracts the retained real x86/x64/ARM64 development
bundle into separate payload roots, reuses `PackageManifestPolicy.psm1` for
Store/AppContainer policy, checks bundle/declaration/package identity agreement,
and reuses `New-PayloadManifest.ps1` for non-following traversal and SHA-256
inventories outside the payloads. Native diagnostics remain alongside, not in,
the payloads. The first executable seam successfully unbundled but failed the
new test because it did not extract the inner payloads (observed RED); recursive
extraction and all three independent executable-hash assertions now pass.
Focused controls cover duplicate/missing architectures, resource packages,
unsafe/duplicate package filenames, identity/version disagreement, an actual
output-ancestor junction, existing output, and native refusal of a truncated
bundle without promoting partial output. The original container is unchanged.

This is **structural inspection of caller-qualified, quiescent inputs**, not a
hostile-archive sandbox, signature/hash authenticity verifier, installation test,
or release SBOM. Release integration must verify the exact outer signed bundle
before invoking it and enforce complete dependency coverage afterward. Microsoft
specifies that the outer bundle signature covers its inner packages; separate
inner signatures are not required. MakeAppx's semantic checks are limited, not
proof of installability. Do not mark the extraction/signing/release items below
complete from this source checkpoint. Sources:
[native extraction and validation limits](https://learn.microsoft.com/en-us/windows/msix/package/create-app-package-with-makeappx-tool),
[bundle signing boundary](https://learn.microsoft.com/en-us/windows/msix/package/signing-package-overview).

- [x] Qualify `eng/artifact-sbom` and `Test-SbomToolContract.ps1` against the retained synthetic payload, native installed vcpkg SPDX documents, resolved NuGet lock and independent expected-contract JSON. User-approved native SDK qualification passed on 2026-10-15: exact SPDX 3.0.1, payload SHA-256, package identities/membership, official schema and SPDX Workgroup semantic validation. Preserve the original Microsoft 4.1.5 failure; do not repair its output JSON or weaken the requirement. Test-first refusal cases cover empty payload, absent resolved NuGet/vcpkg evidence and output inside inventory input; retained overlay metadata protects document-local-ID composition. This is a backend checkpoint, not release integration or a compliant application SBOM.
- [ ] Implement `Invoke-Sbom.ps1` around that qualified adapter. Native Go owns the readonly SDK graph/checksums; use the exact schema and Workgroup validator identities in `eng/sbom-tools.json`. Acquire tooling only into an approved external tool cache, validate before execution and retain mismatched downloads as failed evidence or dispose recoverably when eligible. Never fall back to a different version or moving URL. Recheck latest applicable stable tooling and exact transitive rights/security before final release-tool adoption; the stable SDK's upstream graph contains prerelease/pseudo-version modules.
- [ ] Have the script recursively unpack the exact input `.msixbundle` and each contained architecture `.msix` into a newly created inspection directory. Reject path traversal, reparse points, duplicate output paths, an unexpected architecture, a package manifest that differs from the bundle declaration, or a signature/hash verification failure. Treat the extracted tree as read-only input and put the generated manifest outside it so the SBOM cannot inventory itself.
- [ ] Give the adapter the verified extracted payload tree and separate resolved architecture-specific component evidence, with explicit public artifact name and observed version. Use native NuGet lock cataloging and native vcpkg SPDX decoding, not a second manifest resolver. Rebind document-local package identities through the native typed model before encoding. Compare output against the complete installed build-component closure of the two application roots for each architecture and the application's resolved NuGet lock. A schema-valid SBOM that loses a required build component still fails. Do not describe this closure as the exact linked, deployed or instrumented runtime graph; qualify those separately under the approved scope contract below.
- [ ] Require root `@context` to equal `https://spdx.org/rdf/3.0.1/spdx-context.jsonld`, every emitted `CreationInfo.specVersion` to equal `3.0.1`, and every shipped payload file to have the expected SHA-256. Validate against the pinned official schema and SPDX Workgroup semantic verifier; preserve the SBOM beside the bundle and record the outer signed-bundle SHA-256 in provenance. Do not emit an obsolete-format duplicate without a named consumer requirement.
- [x] Run the maintained contract runner on the pinned native SDK and validators. GREEN proves output scope, 3.0.1 context/specVersion/schema, the expected fixture file hash, NuGet/libjpeg-turbo/Exiv2/Adobe XMP identities, membership and semantic validity. Actual signed-bundle dependency coverage, extraction and release integration remain pending.

**Reusable artifact-validation checkpoint (2026-10-15):**
`Test-ArtifactSbom.ps1` now shares the pinned official schema/semantic consumers
between fixture qualification and future release integration. A schema-valid
wrong payload digest passed the initial schema-only seam (observed RED at the
artifact gate); exact independent payload hashes/scope and membership now refuse
it. Controls also reject changed required-component/public versions, missing file
membership and duplicate JSON members before lossy conversion. The refactored
native contract runner passes all nine Go tests plus schema and semantic checks,
while preserving the independent fixed fixture hash oracle. This checks caller
requirements, not whether a caller's dependency list is complete; production
architecture-specific graph coverage and signed-bundle integration remain open.

**Architecture-qualified identity checkpoint (2026-10-15):** Required-component
entries may now include an exact native `packageUrl`, retaining vcpkg triplet
qualifiers without another URL parser. Name/version alone remains fail-closed
when ambiguous. A wrong-triplet requirement passed the previous validator
(observed RED); it now refuses, while a synthetic two-architecture document
passes the exact qualified requirement and both official validators. Non-string
URLs and unknown identity fields also refuse. This synthetic policy control is
not evidence that the actual three-architecture release graph was cataloged.
The [SPDX property](https://spdx.github.io/spdx-spec/v3.0.1/model/Software/Properties/packageUrl/)
and [native vcpkg SBOM contract](https://learn.microsoft.com/en-us/vcpkg/reference/software-bill-of-materials)
remain the authoritative identifier/installed-metadata boundaries.

**Validated native-generation checkpoint (2026-10-15):**
`New-ArtifactSbom.ps1` composes the maintained native SDK adapter, readonly Go
module/checksum verification, independent payload inventory and existing
`Test-ArtifactSbom.ps1` consumer gate. It publishes only after required component
identities, exact payload scope/hashes, official schema and Workgroup semantics
pass. A wrong required version reached the original seam and was incorrectly
published (observed RED); the candidate is now refused and retained separately.
The real fixture passes without changing its inputs or the caller's process
environment. A preservation assertion exposed imperfect environment restoration;
Go settings now apply only to owned child processes. Native argument lists,
repository-relative execution, two-worker compilation and finite child deadlines
bound generation without installing tools or resolving new dependencies.

Focused controls also reject existing output, output inside inventory inputs,
mismatched validator pins and an actual output-ancestor junction before tools or
output creation. The junction test reached the defective preflight before the
ancestor checks were added (observed RED). These ordinary, caller-owned snapshot
boundaries are not a hostile filesystem sandbox or release approval. Snapshot
preconditions and output invariants belong to this generation boundary; payload,
component and SPDX meaning remain owned by their reused validators. No new JSON,
SPDX, package-URL or dependency grammar is introduced.

Native Go verification initially refused a cached upstream zero-byte test fixture
whose archived filename ends in a space. Windows extraction had normalized its
name. Restoring that exact name changed no source bytes or expected checksum;
`go mod verify` then passed using the documented extended Win32 namespace. Only
verification uses that path form: native Go embed globbing rejects the prefix's
`?` during compilation. A cache with incorrect contents/names still fails; this
script never repairs a cache or suppresses verification. See Microsoft's
[file namespace contract](https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file)
and the [native Go verification contract](https://go.dev/ref/mod#go-mod-verify).

This source checkpoint does **not** finish `Invoke-Sbom.ps1`, actual complete
architecture-specific dependency coverage, signed-bundle generation or release
qualification. In particular, the installed vcpkg experimental package query
emits dependency JSON but exits nonzero. That failed result is retained, not
admitted as graph evidence; successful installed-package listing alone does not
prove dependency edges. Keep the release block until a supported native route
qualifies the actual graph rather than adding a shadow dependency resolver.

**Installed-closure composition checkpoint (2026-10-15):** The supported native
route is now qualified: standalone Microsoft vcpkg `2026-09-26` exports the
**already installed** closure of `exiv2` and `libjpeg-turbo`, with no version
resolution or installation. The Visual Studio edition correctly refuses
selective classic-mode export; it is not patched or replaced. The latest stable
standalone release and its bundle are digest-pinned in `eng/sbom-tools.json`;
the downloaded executable's Microsoft Authenticode signature verified, and its
MIT licence plus bundled native fmt/CMakeRC/curl notices are retained. This tool
is isolated in the external qualification cache, not added to the application or
system PATH. The optional vcpkg-artifacts JavaScript service is not executed.

`Export-JpgSpinnerVcpkgEvidence.ps1` retains exact exported target-triplet SPDX
documents and cross-checks their hashes against the original installed tree,
their identities against native `list --x-json`, and their count against native
target ownership lists. It preserves native package URLs without parsing them.
vcpkg, not repository code or a parsed console plan, owns dependency traversal.
Initial direct-root-only copying lost transitive ports (observed RED); tests now
retain seven target ports for each of x86, x64 and ARM64, excluding unrelated
Catch2 and separating host tools. Missing installed roots fail the actual native
exporter without promoting evidence; existing outputs and wrong executable pins
refuse before export. Native output ancestry/containment and process deadlines
are checked, and bulky raw exports have an explicit caller cleanup obligation.

Combining actual three-architecture evidence exposed another genuine RED:
Syft's default package hash deliberately excludes `PURL`, so otherwise equal
port packages from different triplets collapsed. The adapter now gives the
native `artifact.IDByHash` API the typed component **and its opaque package URL**,
then uses the existing public ID override and relationship rebinding. No SDK
patch, custom hash implementation, URL parser or JSON rewriting is introduced.
A focused two-triplet regression failed before the change; all ten native Go
tests now pass. The retained unsigned bundle's 186 extracted payload files and
38 required component identities (21 architecture-qualified native ports and 17
resolved NuGet packages) pass composition, exact hashes/membership, official
SPDX 3.0.1 schema and Workgroup semantic verification together.

This qualifies complete installed **build-component closure** for the two app
roots, not proof that every such package contributes linked runtime code. It
does not bind that current installed tree to a newly signed release bundle or
close clean-build provenance, final security/rights review or release approval.
The failed combined candidate and original native refusals remain evidence.
Spent executables and raw exports are recycled recoverably after their consumers
finish, with hashes/inventories and thin native metadata retained separately.
Sources: [native export](https://learn.microsoft.com/en-us/vcpkg/commands/export),
[Visual Studio distribution limitation](https://learn.microsoft.com/en-us/vcpkg/troubleshoot/build-failures#cannot-install-packages-using-classic-mode),
[immutable standalone tool release](https://github.com/microsoft/vcpkg-tool/releases/tag/2026-09-26),
[native package identity fields](https://github.com/anchore/syft/blob/v1.54.1/syft/pkg/package.go),
and [public content-hash API](https://github.com/anchore/syft/blob/v1.54.1/syft/artifact/id.go).

**Approved build-context scope amendment (2026-10-15):** The user approved
separating three independently evidenced claims rather than inferring runtime
composition from installed package membership:

1. **Build-context components:** the full native installed closure selected by
   vcpkg for `exiv2` and `libjpeg-turbo`, separately qualified for all three
   target triplets, plus the application's resolved NuGet lock. Preserve exact
   opaque native package URLs and all required identities. Keep host tools and
   unrelated test-only packages out of the application component requirements.
2. **Artifact bytes:** inventory every extracted architecture payload file and
   its SHA-256; verify the exact outer signed bundle before extraction, retain
   its selected SHA-256, and bind the unmodified native SPDX document and its
   independent payload/component evidence to that container. No generated SPDX
   rewriting or unsupported encoder changes are permitted to label this scope.
3. **Deployment/runtime:** retain separate installation, framework dependency,
   minimum-OS, physical ARM64, privacy/network and dynamic-loading evidence.
   Installed build packages do not prove static-linker contribution or every
   scenario-dependent runtime component. Neither a schema-valid document nor
   the new binding is a build-execution attestation or release approval.

This amends the erroneous "complete resolved runtime graph" SBOM criterion;
it does not relax preservation, exact dependency coverage, signing, security,
licensing, reproducibility or release gates. SPDX explicitly distinguishes
[build, deployed and runtime SBOM types](https://spdx.github.io/spdx-spec/v3.0.1/model/Software/Vocabularies/SbomType/).
Its runtime type requires instrumentation of running software; the selected
build-context inputs do not establish that claim. Corresponding Task 18 notices
must distinguish build-component rights from proof of actual redistribution,
and Tasks 17/20 retain deployment and runtime qualification independently.

**Release entry-point checkpoint (2026-10-15):** `Invoke-Sbom.ps1` composes the
existing native trust, extraction, inventory, generation and official validation
consumers. It requires the caller-selected exact bundle SHA-256 and frozen
component evidence/independent requirements, holds the bundle read-only across
the operation, and has no unsigned bypass. A real unsigned bundle reached native
extraction in the initial seam (observed RED); native SignTool now refuses it
before extraction or cataloging (GREEN). Focused preflight controls also reject
a wrong digest, an existing output and overlapping destinations without creating
work or changing the original bundle. The same-volume no-overwrite publication
boundary exposes one complete evidence directory only after all downstream
checks succeed. Its separately schema-constrained binding retains the exact
container, SBOM, payload inventory, build-component input inventory and required
component contract hashes; it explicitly makes no linkage, build-execution or
deployment/runtime attestation.

The downstream components already have real unsigned-bundle/three-architecture
qualification. A **positive end-to-end signed JPG Spinner release run is still
required**; this checkpoint does not pretend that an unsigned development bundle
meets it. No certificate was created, used or trusted, no package was installed,
and no remote workflow was dispatched. Final native tool rights/security,
build-input-to-build-execution binding, hosted workflow composition and whole
Task 17 acceptance remain open.

**Bounded official-schema consumer repair (2026-10-15):** Replaying the real
186-file/38-required-identity document exposed excessive memory use in the
PowerShell `Test-Json`/JsonSchema.Net 7.4.0 path (approximately 13 GB observed).
The owned producer was stopped; no document was promoted. Two isolated replay
controls, each with a 1 GiB GC heap cap, 768 MiB working-set refusal and 30-second
deadline, reproduced the resource refusal in about 1.7 seconds. Selecting the
library's public Boolean-only `Flag` output did not solve it. The retained
candidate, original schema and native SDK outputs remain unchanged; this is not
evidence attributing earlier machine crashes to the same cause.

Use SPDX's [recommended Ajv schema consumer](https://github.com/spdx/spdx-3-model/blob/develop/serialization/jsonld/validation.md#ajv)
instead of retrying that unbounded path. The latest stable native SDK, Ajv
8.20.0, is pinned with a complete npm integrity lock under
`eng/spdx-validation`; its five-package graph is release tooling, not an app
dependency. Restore only into an explicit external private tool tree with
`npm ci --ignore-scripts`, never globally or during validation. The unchanged
pinned official schema and native document passed in the qualification worker
at approximately 89 MB RSS; complete artifact/schema/Workgroup verification then
passed in about 2.4 seconds. Retain exact dependency notices and complete final
tool rights/security qualification; the bounded npm audit reported no known
advisories for this five-package graph, not a whole-application security review.

`Test-SpdxJsonSchema.ps1` uses the native draft-2020-12 compiler, unchanged
assertions and no coercion, defaults, removal, custom keywords, network schema
fetching or output rewriting. An owned hidden child has a 512 MiB V8 old-space
limit, 1 GiB working-set refusal and 30-second deadline; timeout/OOM/tool failure
never becomes validation success. The required tool-tree argument propagates
through generation, release entry points and maintained tests. Raw duplicate
members still fail before PowerShell conversion. Controls reject a wrong
context, unknown class, wrong JSON type and altered tool definitions while
preserving the original document. Small repository schemas may continue using
`Test-Json`; this fixes the measured large SPDX validation path only.

**Maintained build-context composition checkpoint (2026-10-15):**
`New-JpgSpinnerBuildComponentEvidence.ps1` replaces the operator-only composition
prototype with a maintained producer. It accepts exactly the three qualified
native exporter snapshots and the actual application NuGet lock, preserves
their bytes, reuses ordinal SHA-256 inventories for source/copy rechecks, and
promotes one fresh complete component-evidence directory. It consumes NuGet's
already resolved identities across every recorded target, including RID-only
packages; it neither restores packages nor resolves/traverses dependency edges.
Project references remain in the original lock, not the external package list.

After a setup-only missing-argument error was repaired, the direct-only seam
lost 13 transitive NuGet packages (observed behavioural RED). The corrected
producer retains all 21 triplet-qualified native and 17 resolved NuGet identities
(GREEN), including byte-exact original snapshots/lock. A synthetic RID-only
case additionally requires its own distinct package. Malformed/duplicate/
non-object JSON, inconsistent target content, missing architecture and missing
transitive native metadata refuse publication. The real composed output and
186-file unsigned development payload pass the native SDK, independent
requirements/hashes/membership, unchanged official schema and Workgroup gate.
The interrupted unbounded validation result remains failed, not retroactively
successful; subsequent bounded validation of its unchanged candidate is separate
evidence. [Operator guidance](../supply-chain-evidence.md) now documents both
development and signed-release routes and their independent proof boundaries.

**Native signature-verification checkpoint (2026-10-15):**
`Resolve-MsixToolchain.ps1` evaluates the locked owning app targets for MakeAppx
and selects its companion SignTool in that same SDK architecture directory,
not the unrelated legacy `SignToolPath` property or a PATH/newest-SDK fallback.
`Test-MsixSignature.ps1` holds the exact container read-only while hashing and
running native `verify /pa /all`, retains diagnostics, fails on every nonzero
exit, and bounds the owned process with no retry or trust-store changes. The
initial hash-only seam wrongly accepted the unsigned bundle (observed RED);
native verification now accepts an existing genuine Microsoft-signed runtime
MSIX and refuses both the unsigned app bundle and a well-formed tampered copy
with its original signature still present. Original inputs remain unchanged.
This qualifies the native trust boundary, not a signed JPG Spinner release or
Store submission. See [SignTool verification policies and exit codes](https://learn.microsoft.com/en-us/windows/win32/seccrypto/signtool)
and [MSIX signing guidance](https://learn.microsoft.com/en-us/windows/msix/package/sign-app-package-using-signtool).

- [ ] Dependabot version 2 monitors NuGet and GitHub Actions weekly, groups related Microsoft Windows packages, and never auto-merges.
- [ ] Include `Test-DependencyFreshness.ps1` in deliberately requested qualification for vcpkg/upstream release drift; do not add a scheduled remote CI trigger. An upgrade updates baseline/override/notice together and runs the entire release suite.

### Step 17.5: Protected Store packaging workflow

- [ ] Trigger Store packaging only by explicit `workflow_dispatch` into a protected environment. A signed release tag may identify the reviewed source, but must not itself start a workflow.
- [ ] Write `New-PayloadManifest.ps1` test-first. Given an unpacked package tree, it emits a path-ordinal, UTF-8 manifest of normalized relative path, byte length, and SHA-256 and rejects reparse points, duplicate normalized paths, or files outside the root. A one-byte fixture mutation must produce an observed RED mismatch before the final comparison is accepted.
- [ ] Rebuild twice from separate clean directories with immutable inputs, verify the source commit, recursively unpack both unsigned bundles, and require identical payload manifests. Record each container hash separately. Claim byte-identical unsigned MSIX/MSIXBUNDLE output only if the two independent container hashes also match under the locked toolchain; payload reproducibility is the mandatory contract.
- [ ] Sign the exact qualified container from protected secret material, verify that signed bundle, generate and validate the external SBOM from its recursively unpacked payloads, run WACK, and retain source, runner, toolchain, payload-manifest, unsigned-container, and signed-container provenance.
- [ ] Do not submit or stage rollout automatically. Publishing remains a deliberate Partner Center operation after qualification evidence is reviewed.

### Step 17.6: Verify and commit

- [ ] Validate every workflow locally with a YAML parser and repository policy, including rejection of automatic/mixed triggers; explicitly dispatch qualification for the selected candidate and require every hosted headless job green. Opening a pull request or pushing must not start a run. Separately dispatch the interactive workflow in its qualified environment and require its environment check plus UI suite green.
- [ ] Run `Test-BuildProvenance.ps1` against every job's record, `Test-SbomToolContract.ps1` against the pinned executable, and the two-clean-build payload-manifest comparison before accepting the workflow commit.
- [ ] Commit:

```powershell
git add scripts/Invoke-StaticAnalysis.ps1 scripts/Invoke-Sbom.ps1 scripts/Test-SbomToolContract.ps1 scripts/New-PayloadManifest.ps1 scripts/Test-BuildProvenance.ps1 tests/TestData/SbomContractFixture .github
git commit -m "ci: enforce build security and supply-chain gates"
```

## Task 18: Document the shipped system and remove secret-bearing telemetry

**Incremental documentation checkpoint (2026-10-15):** Public development docs
now describe safe defaults, exact output/backup roots, supported-format/resource
boundaries, module and transaction contracts, typed diagnostic fields, retention
and explicit export. They distinguish declared OS/device support, implemented
behavior and retained development evidence from unqualified release claims.
The release register explicitly leaves native analysis, final accessibility,
physical-device/upgrade, reproducibility, SBOM, legal, credential revocation and
rollout obligations open. Third-party notices are deliberately marked an
incomplete development inventory, not distribution clearance. The private
vulnerability-reporting route also needs maintainer enablement confirmation.

The structural documentation policy observed missing-required-document RED,
then current source GREEN; owned-copy missing-document and missing-section
controls pass. PowerShell's bundled native Markdown AST parsed all six documents
and every relative local target exists. This is not external-link liveness,
spelling, runtime privacy/traffic proof, exact transitive notice comparison or
whole Task 18 completion. No historical telemetry credential was revoked by
these source edits. Those steps remain mandatory below.

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
- [ ] Record libjpeg-turbo 3.2.0, Exiv2 0.28.9, Catch2 3.16.0 for test distributions where applicable, Microsoft Windows packages, the qualified Syft SDK 1.54.1 adapter and SPDX Workgroup Tools 2.0.7 as release tooling, and each transitive native dependency actually present in the bundle. Microsoft SBOM Tool CLI 4.1.5 is rejected qualification evidence, not adopted release tooling. Complete the native SDK's exact transitive rights/security inventory before final adoption or tool redistribution.
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

- [ ] Resolve the five existing `cpp/integer-multiplication-cast-to-long` alerts through this auditable deletion, not edits to the quarantined codec: [alert 1](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/1) names `JPG Spinner/jdmainct.c:186`; [alert 2](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/2), [alert 3](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/3), [alert 4](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/4) and [alert 5](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/5) name `JPG Spinner/jmemmgr.c:601-612`. Intake on 2026-10-15 found all five open against `c8151907ebf58a33f752de0dce04c32500963a0a`; these are not stale source locations. Prove the modern solution consumes maintained libjpeg-turbo instead, retain the legacy-absence regression and inspect a deliberately requested post-cutover CodeQL analysis before claiming GitHub resolution. Keep historical default-setup and new manual-build analysis categories distinct: a successful new category does not itself close old-category alerts. Do not hide the files with scanning exclusions or dismiss alerts merely because the runtime is retired. Remote CI remains manual-only; this checklist does not dispatch a workflow.

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
