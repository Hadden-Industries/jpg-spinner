# JPG Spinner 2.0 modernization — Task 1 implementation log

Date: 2026-08-31

Last updated: 2026-09-01

Scope: reproducible toolchain, dependency policy, effective compiler/linker policy, and freshness reporting

## Decisions established by executable evidence

- Visual Studio discovery is read-only in `Resolve-MSBuildToolchain.ps1`; the separately named
  `New-MSBuildToolchainLock.ps1` is the only operation permitted to create an absent lock, and it refuses replacement.
- The persistent lock omits the Visual Studio Installer product build because that informational value is not the
  enforced invariant. The resolver still emits it for build provenance; the lock records only values resolution checks.
- The repository selects the stable Visual Studio 18 release channel, v145, exact MSVC directory `14.51.36231`, the
  64-bit MSVC host tools, Windows SDK `10.0.28000.0`, and minimum Windows build `10.0.19045.0`. Guarded
  `UseEnv=false` keeps VC include and library directories under MSBuild/toolset control instead of replacing them with
  calling-shell state.
- Effective verification observes repository-selected toolchain values instead of supplying the values it is meant to
  verify. MSBuild command-line `/p:` values are immutable global properties during evaluation, so the probe passes only
  `Configuration`, `Platform`, and evidence-output paths. Every evidence build also passes `-noAutoResponse`, preventing
  ambient `MSBuild.rsp` or parent-directory `Directory.Build.rsp` files from injecting global properties. Repository
  policy loads `Microsoft.Build.dll` from the same selected Visual Studio instance as `MSBuild.exe` and uses
  `ProjectRootElement.Properties`, whose documented traversal includes `Choose` branches, to require every
  case-insensitive early selection property exactly once in the canonical modern-policy-conditioned root
  `PropertyGroup`.
- C++ task metadata lives in `Directory.Build.targets`, after `Microsoft.Cpp.props`; early selection values live in
  `Directory.Build.props`. Structured `SpectreMitigation=Spectre` is an early selection value because Microsoft defines
  it to emit `/Qspectre` and redirect the runtime-library search path. The distinction is checked structurally and
  through actual CL/LINK command lines plus MSBuild's evaluated property output. LINK searches command-line `/LIBPATH`
  directories before the `LIB` path that MSBuild derives from `LibraryPath`; the verifier models that combined order
  and requires the architecture-specific Spectre runtime to precede the ordinary locked-toolset runtime.
- `JpgSpinnerModernCppBuildPolicyEnabled` defaults to true before the guarded selection values are evaluated. The
  current C++/CX project declares false in its `Globals` group before `Microsoft.Cpp.Default.props`; all shared
  selection and item-definition groups honor that boundary because Microsoft documents `/ZW` as incompatible with
  `/std:c++20` or later. MSBuild's own `-getProperty`/`-getItem` evaluation proves the legacy C++/CX items receive no
  C++20 metadata while the modern probe still does. This is a temporary project-scope decision during parallel
  replacement, not a runtime shim; the exception disappears with the legacy project at cutover.
- Debug uses `/Zi`, because empirical compilation demonstrated that MSVC rejects Edit and Continue (`/ZI`) together
  with Control Flow Guard (`/guard:cf`).
- Final x64 executables require CFG plus CET-compatible PE metadata. Verification follows Microsoft's prescribed
  `dumpbin /headers /loadconfig` contract and separately requires the CFG image characteristic, `CF Instrumented`,
  `FID table present`, and CET compatibility; a header advertisement alone is insufficient. First-party builds select
  the exact x86, x64, or ARM64 `lib\spectre` directory through the MSBuild property; dependency libraries receive CFG and Spectre compiler
  mitigations through custom triplets but do not receive the final-image-only `/CETCOMPAT` option. Repository policy
  treats each triplet as executable CMake and uses the selected Visual Studio instance's bundled CMake `json-v1` trace
  as the semantic boundary. Exactly six executed two-argument `set(...)` records are allowed, each with its canonical
  variable name and exact approved value; every other command, variable, duplicate, or missing execution fails closed.
- Effective command-line checks select exactly one task command beginning with the locked `cl.exe` or `link.exe`
  path and containing the expected probe operand; FileTracker and other rendered diagnostic lines are not merged into
  the option stream. The rendered argument string is split by Microsoft's C/C++ process-argument rules. Only the
  documented interchangeable `-` and `/` option specifiers are normalized: CL option-name matching remains
  case-sensitive, option-specific arguments retain the grammar the locked compiler actually consumes, LINK matching
  remains case-insensitive, and exact lowercase `/link` ends CL option processing. In particular, v145 accepts case
  variants of the compiler `/guard:cf[-]` argument without accepting a differently cased `guard` option name.
  Within that grammar, MSVC's left-to-right precedence requires the rightmost family member. Later `/std:c++17`,
  `/permissive`, `/W0`, `/WX-`, `/sdl-`, `/guard:cf-` or `/guard:CF-`, `/Qspectre-`, `/Zc:__cplusplus-`, Release `/Od`
  or `/GL-`, linker `/GUARD:NO`, Release `/LTCG:OFF`, or x64 `/CETCOMPAT:NO` cannot be mistaken for compliance; the
  same applies to documented dash-prefixed spellings. Release `/LTCG`, `/LTCG:STATUS`, `/LTCG:NOSTATUS`, and
  `/LTCG:INCREMENTAL` are accepted as the current documented enabling states, while deprecated or unknown arguments
  remain noncompliant. CL or LINK `@command-file` arguments fail closed because this evidence path does not capture
  their expanded tokens. The verifier pins MSBuild's working directory to the repository under test so relative tool
  inputs cannot resolve against the caller's directory. Release `/O2` is explicit in both structure and the observed
  command line.
- `/utf-8` does not join those rightmost-precedence families. The locked v145 compiler reports D8016 whenever
  `/source-charset:` or `/execution-charset:` is combined with `/utf-8`, in either order. Policy therefore requires the
  exact `/utf-8` option and emits a targeted incompatibility diagnostic for every separate charset form instead of
  mischaracterizing the compiler contract as last-option-wins.
- The effective-policy child process retains one canonical `Path` entry but removes ambient `CL`, `_CL_`, `LINK`,
  `_LINK_`, `UseEnv`, `INCLUDE`, `EXTERNAL_INCLUDE`, `LIB`, and `LIBPATH`. Those documented variables can inject
  compiler/linker options or replace MSBuild's VC directories outside repository review and therefore sit outside the
  verifier's trust boundary. MSBuild's own `-getProperty` output independently proves the exact effective Windows SDK,
  observes `UseEnv`, and supplies the final `LibraryPath` rather than relying on diagnostic-log assignments.
- Visual Studio's documented C++ EditorConfig surface does not define identifier-naming rules. Portable whitespace
  remains in `.editorconfig`; standard clang-tidy identifier checks enforce authored native `jpg_spinner` naming.
  `src/JpgSpinner.App/.clang-tidy` inherits that policy but precisely exempts the projected `JpgSpinner` namespace,
  requires PascalCase public/protected WinRT methods, retains lower-camel private methods, and excludes generated headers.
- The vcpkg manifest root, direct dependencies, and overrides are closed schemas, not minimum subsets: the root may
  contain only the reviewed identity, builtin-baseline, dependency, override, and optional current configuration
  members, while only libjpeg-turbo, Exiv2, and Catch2 are approved graph entries. Root `features` and
  `default-features` are rejected because vcpkg defines them as dependency-activation surfaces that could add edges
  without altering the reviewed direct sets. Each dependency and override also has an exact closed property set, JSON
  types are checked before PowerShell comparison can coerce them, and Exiv2's sole `xmp` feature must be represented as
  an array.
  PowerShell 7.6 runs on .NET 10, so `JsonObjectMemberValidation.psm1` delegates duplicate detection to
  `JsonSerializerOptions.AllowDuplicateProperties=false` before PowerShell's last-member-wins conversion. Repository
  policy, Visual Studio discovery, the toolchain-lock resolver, CMake trace ingestion, and file/live freshness inputs
  all consume that same platform boundary. The original `System.Text.Json.JsonException` remains the inner exception,
  preserving its structured path, line, and byte-position evidence without a repository-owned JSON tree walker.
- Repository JSON readers distinguish an absent optional file from a successfully parsed JSON `null`. Every policy
  surface defined as an object—`.vsconfig`, the toolchain lock, the vcpkg manifest, and a present standalone vcpkg
  configuration—must have an object root before a consumer can inspect its fields.
- The manifest's immutable builtin baseline is the sole vcpkg resolution authority. Policy inspects the standalone
  `vcpkg-configuration.json` form, the current embedded `configuration` form, and the older embedded
  `vcpkg-configuration` spelling; it rejects alternate default/additional registries and port/triplet overlays. A
  standalone configuration file cannot coexist with either embedded representation, even when both objects are empty.
- NuGet merges `packageSources` and `disabledPackageSources` as independent configuration collections. The repository
  therefore clears both collections, declares nuget.org as the sole source, and permits no disabled-source entry; a
  machine- or user-level disablement cannot silently make the reviewed sole source unavailable.
- Generated build evidence remains ignored, but versioned release `*.spdx.json` files beneath `artifacts/release/`
  remain visible for review. The policy tests planned sidecar and nested Microsoft SBOM Tool output paths directly.
- Dependency freshness first requires every normalized exact pin—including a nonzero vcpkg port revision—to exist in
  the package authority that must restore it. This prevents a syntactically valid future or mistyped pin from appearing
  current merely because no published version sorts after it. The report then distinguishes an actionable version
  available through an approved package authority from upstream release-channel lag. It compares exact vcpkg port
  revisions after equal upstream versions, renders nonzero revisions as `#N`, never edits manifests, and never
  recommends an unrestorable overlay or source shim. A successful channel-lag report states only that no newer version
  is actionable through the approved package authorities; it does not contradict its upstream-release notice. Live metadata
  uses `Invoke-WebRequest` only to obtain textual response content, then runs the same strict platform parser used for
  repository and snapshot files before any PowerShell object conversion. Each authority declares its raw root schema:
  vcpkg and NuGet metadata are objects, while GitHub release collections are arrays. Separate connection and
  response-data read timeouts prevent a connected endpoint from stalling the gate indefinitely.

## TDD evidence

Every production script or policy change began with a focused failing check. Environmental failures were not accepted
as RED evidence; the harness was corrected until it reached the intended missing-behavior assertion.

| Cycle | RED evidence | Minimum implementation | GREEN evidence |
|---|---|---|---|
| Repository policy | Separate missing-file diagnostics for root configuration | Structured JSON, XML, EditorConfig, constrained YAML, triplet, and ignore-policy checks | `Test-RepositoryPolicy.ps1` exits 0 and reports only parsed evidence |
| JSON object roots | Required `.vsconfig`, toolchain-lock, manifest, and optional standalone configuration files containing JSON `null` were treated as absent and passed | Require the expected `JsonValueKind` in the raw `System.Text.Json` document while retaining a distinct absent-file sentinel | Every null-root fixture fails before PowerShell conversion with a file-specific object-root diagnostic; valid objects remain green |
| Toolchain resolution | Resolver contract failed because the script was absent | Stable `vswhere` selection, exact lock enforcement, and x86/x64/ARM64 Hostx64 tool paths | Stable VS `18.9.12112.369`, MSVC `14.51.36231` resolved |
| Immutable lock creation | Creation contract failed because the creator was absent | Race-safe `FileMode.CreateNew`, durable flush, and refusal to replace | Exact lock created in a temporary repository; second creation rejected |
| Effective build policy | Six builds succeeded but reported 86 missing language, warning, hardening, reproducibility, optimization, and CET requirements | Shared props/targets plus explicit x64 host selection and Debug `/Zi` | Debug/Release x86, x64, ARM64 command lines green; x64 PE headers advertise CFG and CET |
| Legacy C++/CX policy scope | MSBuild evaluated `CompileAsWinRT=true` items with `LanguageStandard=stdcpp20`, reproducing Microsoft's prohibited `/ZW` combination | One default-on `JpgSpinnerModernCppBuildPolicyEnabled` boundary, a pre-import false declaration in the legacy project, and guards on every shared props/targets policy group | Evaluated legacy compile items contain no `stdcpp20`; the modern probe retains it; removing a guard or re-enabling the legacy project fails policy |
| Repository-selected MSBuild toolchain | An invalid `PlatformToolset`, an automatic `Directory.Build.rsp`, and later or conditional duplicate selection declarations passed because globals masked evaluation and a root-only XPath missed `Choose` | Remove toolset/version/SDK globals, pass `-noAutoResponse`, and use the selected instance's `ProjectRootElement.Properties` with MSBuild's case-insensitive property identity | Invalid and auto-response-masked selections reach MSBuild and fail; direct, differently cased, and `Choose`-nested duplicates receive targeted diagnostics; the clean matrix remains locked |
| CFG load-configuration evidence | Header-only fixture text advertised CFG and CET without proving compiler instrumentation or a function-ID table | Parse anchored DUMPBIN mitigation fields and invoke `/headers /loadconfig` | Header-only evidence keeps its advertisement but cannot satisfy `CF Instrumented` or `FID table present`; real Debug/Release x64 images satisfy all facts |
| Spectre runtime selection and precedence | One isolated build retained raw `/Qspectre` but selected no mitigated runtime directory; another placed the ordinary locked-toolset runtime before a still-present Spectre directory | Structured `SpectreMitigation=Spectre`; combine ordered LINK `/LIBPATH` values with evaluated `LibraryPath`; compare exact normalized locked-toolset paths | Both hostile fixtures are rejected; all six valid builds emit `/Qspectre` and search the matching `lib\spectre` runtime before ordinary `lib\<architecture>` |
| Effective Windows SDK selection | A project-local pre-import property changed the effective SDK to another installed version while the verifier continued reporting green | Query `WindowsTargetPlatformVersion` with locked MSBuild's `-getProperty` and require `10.0.28000.0` exactly | The alternate installed SDK receives a targeted diagnostic in every matrix entry; the repository-selected SDK remains green |
| Ambient MSVC inputs and VC directories | Hostile `CL`, `_CL_`, `LINK`, `_LINK_`, `UseEnv`, `INCLUDE`, `EXTERNAL_INCLUDE`, `LIB`, and `LIBPATH` values changed options or prevented MSBuild from reconstructing toolset/SDK directories | Guarded repository `UseEnv=false`, structural uniqueness/value enforcement, and a case-insensitive child-environment exclusion set while preserving one canonical `Path` | The hostile macro, override, and directory sentinel never reaches the six build logs; an explicit `UseEnv=true` mutation is rejected; the valid matrix remains green |
| Effective option families | Required tokens remained present before later `/std:c++17`, `/permissive`, `/W0`, `/WX-`, `/sdl-`, `/guard:cf-`, `/Qspectre-`, `/Zc:__cplusplus-`, Release `/Od` or `/GL-`, `/GUARD:NO`, and `/CETCOMPAT:NO` | One ordered family validator requiring the rightmost effective member, plus explicit Release `Optimization=MaxSpeed` | Every later conflict receives a targeted semantic diagnostic; inverse `/sdl- /sdl` is accepted; the clean six-build matrix observes Release `/O2` |
| UTF-8 representation | Separate `/source-charset:` and `/execution-charset:` forms produced only downstream D8016 failures and no policy-owned explanation | Require `/utf-8` and reject either separate charset family in both option orders using the same CL token grammar | Slash- and dash-prefixed incompatible forms receive targeted diagnostics before the clean matrix remains green |
| MSVC option specifiers and case | A later `-std:c++17` was ignored, `/wx-` incorrectly disabled case-sensitive CL `/WX`, dash-prefixed exact `-utf-8` was reported missing, and invalid `/UTF-8` incorrectly satisfied the required exact CL option | Normalize only a leading option specifier; apply ordinal CL and ordinal-ignore-case LINK comparisons to exact and family options | Dash-prefixed CL and LINK conflicts name the original effective tokens; valid `-utf-8` passes; invalidly cased CL lookalikes cannot override or satisfy policy |
| Option-specific CFG and LTCG states | The locked compiler consumed rightmost `/guard:CF-` but the whole-token ordinal matcher ignored it; valid `/LTCG:STATUS` was rejected as different from bare `/LTCG` | Separate family membership from semantic acceptance: preserve the case-sensitive `guard` name while matching its consumed `cf[-]` argument, and enumerate current LTCG enabling states | Mixed-case compiler CFG disablement fails all six configurations; `STATUS`, `NOSTATUS`, and `INCREMENTAL` pass; `OFF` receives a targeted Release diagnostic |
| CL `/link` boundary | Compiler `/guard:cf` and `/Brepro` lookalikes placed only after `/link` satisfied compiler policy | End exact and family compiler-option inspection at exact case-sensitive `/link`; preserve build-failure evidence long enough to inspect emitted task commands | Both compiler requirements receive targeted diagnostics even when the downstream CL invocation also fails |
| MSVC task-command and command-file evidence | A quoted CL command file hid `-std:c++17`; a LINK command file hid `-GUARD:NO -CETCOMPAT:NO`; repository-relative LINK input resolved against the caller's directory; broad rendered-log matching could include non-tool lines | Select one locked-executable task line, split its argument string by Microsoft C/C++ rules, reject every parsed `@` argument, and set the child working directory explicitly | Quoted CL and relative LINK command files fail with tool-specific diagnostics across the six-build matrix; the clean matrix remains green |
| Closed vcpkg triplets | A triplet retained approved tokens before `/guard:cf-`, `/Qspectre-`, and `/GUARD:NO`, appended `list(APPEND ...)`, and hid apparent declarations inside a multiline bracket comment | Execute the selected Visual Studio CMake in script mode and validate its versioned JSON trace as exactly six canonical two-argument `set(...)` records | Altered values, the executed `list`, commented-out records, malformed scripts, and temporary-trace leaks all fail; all three reviewed triplets remain green |
| clang-tidy naming boundary | The WinRT scope had no policy capable of accepting `winrt::JpgSpinner::implementation` and PascalCase ABI methods | Inherited app-scoped naming overrides plus generated-header exclusion | LLVM 22.1.3 accepts both native and WinRT fixtures while a deliberately nonconforming generated projection is excluded |
| Reviewed release artifacts | The planned `artifacts/release/2.0.0.0/JpgSpinner.spdx.json` was hidden by `artifacts/` | Narrow directory traversal and `*.spdx.json` negations, with quiet exit-status queries | Planned sidecar/nested SBOMs are visible; an ordinary build-policy log remains ignored |
| Exact vcpkg sets | An isolated manifest added `zlib` as a fourth valid-shaped direct dependency and policy accepted it | Normalize string/object entries, reject unnamed/additional dependencies and overrides, then require each approved name exactly once | The extra dependency fails with a targeted diagnostic; the approved manifest remains green |
| Closed vcpkg manifest root | Root `features` plus `default-features` added an unreviewed `zlib` edge while the direct dependency and override sets remained unchanged | Require the manifest root's exact reviewed property set, including optional current `configuration`, before graph-entry validation | Both dependency-activation members receive targeted unapproved-property diagnostics; the reviewed manifest remains green |
| Exact vcpkg entry semantics | Approved names retained unreviewed `platform`, `host`, `features`, or `port-version` fields; string `"false"`, scalar `"xmp"`, and non-string names/versions also survived coercion | Exact per-package property sets, explicit JSON string/Boolean/list checks, and an exact one-string Exiv2 feature array | Every behavior-bearing field and malformed representation receives a targeted diagnostic; the approved object shapes remain green |
| Unique JSON object members | PowerShell erased earlier duplicate dependency, registry, toolchain-lock, and override-version members before their direct consumers inspected only the final values | Require PowerShell 7.6/.NET 10 and set the platform `JsonSerializerOptions.AllowDuplicateProperties` contract to `false` before object or hashtable conversion | Repository, resolver, freshness, and CMake-trace controls reject reviewed-looking duplicate members with a retained `JsonException` cause; clean JSON remains green without a custom recursive walker |
| Live JSON ambiguity and shape | `Invoke-RestMethod` converted official responses before strict validation; callers could also receive a wrong root kind, and a connected server could stall indefinitely between body reads | Fetch textual content with `Invoke-WebRequest`, enforce authority-specific raw root kinds, then convert; map an explicit wrapper timeout to `OperationTimeoutSeconds` | Loopback duplicate and wrong-root responses fail at the raw boundary, a valid object preserves its structure, and a header-only stalled response terminates within the configured read timeout |
| vcpkg resolution authority | Standalone, current embedded, and legacy embedded configurations replaced registries and added port/triplet overlays without policy failure | One closed authority validator across both supported representations; reject the legacy alias instead of accepting a compatibility spelling | Each default registry, additional registry, overlay port, and overlay triplet mutation fails; the implicit builtin baseline remains green |
| vcpkg configuration representation | Empty standalone and embedded configuration objects coexisted without an alternate-authority diagnostic | A presence-based exclusivity assertion independent of configuration contents | Dual representations fail even when both objects are empty; each representation remains valid on its own |
| Enabled NuGet authority | A repository `disabledPackageSources` entry disabled the sole reviewed nuget.org source while the package-source and mapping checks remained green | Independently clear inherited disabled sources and require that collection to contain only `<clear />` | A local disablement receives a targeted diagnostic; the isolated enabled nuget.org configuration remains green |
| Policy reporting | Success output claimed three Task 2 NuGet versions that no manifest yet declared | Report values parsed from the current toolchain lock and vcpkg overrides only | Focused report test rejects unsupported evidence and passes the corrected output |
| Policy negative controls | An isolated valid copy accepted missing modern-policy guards, legacy C++/CX re-enablement, missing structured Spectre policy, appended `/std:c++latest`, disabled Release optimization, ARM32, alternate vcpkg authorities, or an extra dependency | Exact project-scope, structured-property, floating-language, optimization, architecture, authority, and closed-set rejection | Every isolated mutation fails with its targeted diagnostic |
| Dependency freshness | Contract first failed because the verifier was absent; later controls exposed upstream-only handling, loss of registry `port-version`, unavailable exact pins that sorted as current, contradictory success wording, last-member-wins parsing, and permissive property access on wrong-root metadata | Exact authority-membership checks before ordering, stable comparison across official sources, platform duplicate rejection, object/array root contracts, `#N` parsing/reporting, snapshot seam, immutable inputs, and an actionable-versus-channel-lag distinction | NuGet and vcpkg reject absent exact pins, `3.2.0#1` outranks `3.2.0`, a published exact `#N` pin passes, duplicate and wrong-root metadata fail, prereleases are ignored, channel lag remains informational without a contradictory success claim, and input hashes remain unchanged |

Three harness conditions were corrected before accepting RED evidence:

1. The automation host exposed both `PATH` and `Path`; the child-only process environment is normalized because
   MSBuild's case-insensitive CL-task dictionary rejects duplicate names.
2. The workspace sandbox denied native MSBuild FileTracker access to installed tool paths. Build probes were rerun with
   the required read/build access; repository outputs remained under ignored `artifacts/`.
3. `git check-ignore --verbose` reports a matching negation rule, whereas `--quiet` supplies the documented ignored/not
   ignored status. Policy now uses the quiet exit code and requests verbose output only to explain an ignored path.

## Verified commands and observed outcomes

```powershell
pwsh -NoProfile -File scripts/tests/Test-MSBuildToolchainResolution.ps1
# PASS: resolved stable Visual Studio 18.9.12112.369 with MSVC 14.51.36231
# and rejected ambiguous lock JSON.

pwsh -NoProfile -File scripts/tests/Test-JsonObjectMemberValidation.ps1
# PASS: strict JSON validation enforces root kinds and delegates duplicate-member
# detection to System.Text.Json before conversion.

pwsh -NoProfile -File scripts/tests/Test-MSBuildToolchainLockCreation.ps1
# PASS: created immutable MSVC 14.51.36231 toolchain lock and rejected replacement.

pwsh -NoProfile -File scripts/tests/Test-RepositoryPolicyReporting.ps1
# PASS: repository-policy success reporting is limited to configuration that the verifier actually parsed.

pwsh -NoProfile -File scripts/tests/Test-ModernCppBuildPolicyScope.ps1
# PASS: evaluated MSBuild metadata excludes the legacy C++/CX project from the modern
# C++ build policy while retaining C++20 for the modern build-policy probe.

pwsh -NoProfile -File scripts/tests/Test-RepositoryPolicyNegativeControls.ps1
# PASS: repository policy rejects weakened or duplicate build settings, unscoped modern C++ policy,
# legacy C++/CX re-enablement, non-object JSON roots, executable triplet mutations, alternate vcpkg
# authorities, duplicate JSON members, root dependency-activation surfaces, disabled NuGet sources,
# and unreviewed dependency semantics.

pwsh -NoProfile -File scripts/tests/Test-DependencyFreshnessContract.ps1
# PASS: freshness verification requires authority-published exact pins, compares vcpkg port
# revisions, ignores prereleases, rejects ambiguous JSON, distinguishes actionable upgrades
# from release-channel lag, and never mutates manifests.

pwsh -NoProfile -File scripts/tests/Test-StrictJsonWebRequest.ps1
# PASS: live JSON response bodies enforce root shapes, reject duplicate members before
# PowerShell object conversion, and time out stalled body reads.

pwsh -NoProfile -File scripts/tests/Test-DumpbinImageMitigationMetadata.ps1
# PASS: dumpbin mitigation parsing distinguishes image advertisements from CFG instrumentation
# and function-table evidence.

pwsh -NoProfile -File scripts/tests/Test-EffectiveBuildPolicyNegativeControls.ps1
# PASS: effective build policy consumes repository-selected toolchain values, rejects alternate SDK
# selection, ambient MSVC inputs, compiler-only or shadowed Spectre runtime selection, and incompatible
# charset forms; it validates ordered CL/LINK option streams, option-specific case rules, semantic LTCG
# states, both option specifiers, /link boundaries, and fail-closed command files.

pwsh -NoProfile -File scripts/tests/Test-ClangTidyNamingBoundaries.ps1
# PASS: clang-tidy accepts native and C++/WinRT naming boundaries while excluding generated projections.

pwsh -NoProfile -File scripts/tests/Test-GitIgnoreReviewedArtifacts.ps1
# PASS: reviewed locks, fuzz inputs, and release SBOMs are visible while generated artifacts remain ignored.

pwsh -NoProfile -File scripts/Test-RepositoryPolicy.ps1
# Repository policy verified: Visual Studio 2026/v145, MSVC 14.51.36231, C++20,
# Windows SDK 10.0.28000.0, libjpeg-turbo 3.2.0, Exiv2 0.28.8+xmp, and Catch2 3.16.0.

pwsh -NoProfile -File scripts/Test-EffectiveBuildPolicy.ps1
# Effective build policy verified for Debug/Release x86, x64, and ARM64 with VCToolsVersion
# 14.51.36231 and Windows SDK 10.0.28000.0; MSBuild controls VC directories and each architecture
# searches its Spectre-mitigated runtime before the ordinary MSVC runtime, while x64 PE images
# advertise CFG/CET and their load configurations report CF instrumentation and function-ID tables.

pwsh -NoProfile -File scripts/Test-DependencyFreshness.ps1
# Dependency release-channel notice: exiv2: upstream GitHub offers stable 0.28.9, but it is not
# yet present in the official vcpkg registry (latest 0.28.8); the manifest remains on the newest
# version that the selected official registry can resolve.
# No newer dependency versions actionable through the approved package authorities were found.
# Checked official vcpkg registry. Upstream-only release-channel lag, if any, is reported above.
# Repository manifests were not changed.
```

The installed vcpkg client `2026-05-27-d5b6777d666efc1a7f491babfcdab37794c1ae3e` also completed `--dry-run`
resolution for `x86-windows-static-md`, `x64-windows-static-md`, and `arm64-windows-static-md`. Each graph selected
libjpeg-turbo `3.2.0`, Exiv2 `[core,xmp]` `0.28.8`, and Catch2 `3.16.0` from the immutable manifest inputs.

## Primary specifications consulted

- [Microsoft PowerShell approved verbs](https://learn.microsoft.com/powershell/scripting/developer/cmdlet/approved-verbs-for-windows-powershell-commands)
- [Microsoft C++ EditorConfig properties](https://learn.microsoft.com/visualstudio/ide/cpp-editorconfig-properties)
- [clang-format style options](https://clang.llvm.org/docs/ClangFormatStyleOptions.html)
- [Microsoft `/Qspectre` compiler and mitigated-library guidance](https://learn.microsoft.com/en-us/cpp/build/reference/qspectre?view=msvc-170)
- [Microsoft compiler command-line syntax, option specifiers, and CL case rules](https://learn.microsoft.com/en-us/cpp/build/reference/compiler-command-line-syntax?view=msvc-170)
- [Microsoft order of CL options](https://learn.microsoft.com/en-us/cpp/build/reference/order-of-cl-options?view=msvc-170)
- [Microsoft `/source-charset` and `/utf-8` equivalence](https://learn.microsoft.com/en-us/cpp/build/reference/source-charset-set-source-character-set?view=msvc-170)
- [Microsoft compiler Control Flow Guard enable/disable semantics](https://learn.microsoft.com/en-us/cpp/build/reference/guard-enable-control-flow-guard?view=msvc-170)
- [Microsoft Control Flow Guard binary verification](https://learn.microsoft.com/en-us/windows/win32/secbp/control-flow-guard)
- [Microsoft CL command-file ordering and `/link` line semantics](https://learn.microsoft.com/en-us/cpp/build/reference/cl-command-files?view=msvc-170)
- [Microsoft C/C++ process-argument parsing rules](https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments?view=msvc-170)
- [Microsoft CL environment variables](https://learn.microsoft.com/en-us/cpp/build/reference/cl-environment-variables?view=msvc-170)
- [Microsoft C++ MSBuild `UseEnv` behavior](https://learn.microsoft.com/en-us/cpp/build/reference/msbuild-visual-cpp-overview?view=msvc-170)
- [Microsoft linker option order and environment variables](https://learn.microsoft.com/en-us/cpp/build/reference/linking?view=msvc-170)
- [Microsoft LINK `/LIBPATH` search precedence](https://learn.microsoft.com/en-us/cpp/build/reference/libpath-additional-libpath?view=msvc-170)
- [Microsoft `/LTCG` arguments and effective states](https://learn.microsoft.com/en-us/cpp/build/reference/ltcg-link-time-code-generation?view=msvc-170)
- [Microsoft MSBuild global-property semantics](https://learn.microsoft.com/en-us/visualstudio/msbuild/msbuild-glossary?view=visualstudio)
- [Microsoft `Directory.Build.props`/`.targets` scope and import order](https://learn.microsoft.com/en-us/visualstudio/msbuild/customize-by-directory?view=visualstudio)
- [Microsoft evaluated MSBuild item/property output](https://learn.microsoft.com/en-us/visualstudio/msbuild/evaluate-items-and-properties?view=visualstudio)
- [Microsoft C++ target-platform-version property](https://learn.microsoft.com/en-us/cpp/build/reference/general-property-page-project?view=msvc-170)
- [Microsoft MSBuild automatic response files and `-noAutoResponse`](https://learn.microsoft.com/en-us/visualstudio/msbuild/msbuild-response-files?view=visualstudio)
- [Microsoft `ProjectRootElement.Properties` traversal semantics](https://learn.microsoft.com/en-us/dotnet/api/microsoft.build.construction.projectrootelement.properties?view=msbuild-18-netcore)
- [Microsoft `/ZW` compatibility contract](https://learn.microsoft.com/en-us/cpp/build/reference/zw-windows-runtime-compilation?view=msvc-170)
- [CMake `json-v1` trace format](https://cmake.org/cmake/help/latest/manual/cmake.1.html#cmdoption-cmake-trace-format)
- [Microsoft PowerShell 7.6 on .NET 10](https://learn.microsoft.com/en-us/powershell/scripting/whats-new/what-s-new-in-powershell-76?view=powershell-7.6)
- [Microsoft PowerShell `Invoke-WebRequest` timeouts](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.utility/invoke-webrequest?view=powershell-7.6)
- [Microsoft PowerShell `Invoke-RestMethod` automatic deserialization](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.utility/invoke-restmethod?view=powershell-7.6)
- [Microsoft PowerShell duplicate-key conversion behavior](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.utility/convertfrom-json?view=powershell-7.6)
- [Microsoft .NET 10 `JsonSerializerOptions.AllowDuplicateProperties`](https://learn.microsoft.com/en-us/dotnet/api/system.text.json.jsonserializeroptions.allowduplicateproperties?view=net-10.0)
- [clang-tidy configuration inheritance and header filtering](https://clang.llvm.org/extra/clang-tidy/)
- [clang-tidy identifier naming](https://clang.llvm.org/extra/clang-tidy/checks/readability/identifier-naming.html)
- [Git `check-ignore` exit-status contract](https://git-scm.com/docs/git-check-ignore)
- [Microsoft vcpkg manifest mode](https://learn.microsoft.com/en-us/vcpkg/concepts/manifest-mode)
- [Microsoft `vcpkg.json` reference](https://learn.microsoft.com/en-us/vcpkg/reference/vcpkg-json)
- [Microsoft `vcpkg-configuration.json` reference](https://learn.microsoft.com/en-us/vcpkg/reference/vcpkg-configuration-json)
- [Microsoft vcpkg package-name and overlay resolution](https://learn.microsoft.com/en-us/vcpkg/concepts/package-name-resolution)
- [Microsoft vcpkg versioning reference](https://learn.microsoft.com/en-us/vcpkg/users/versioning)
- [Microsoft vcpkg versioning troubleshooting: versions absent from a registry](https://learn.microsoft.com/en-us/vcpkg/users/versioning-troubleshooting)
- [Microsoft vcpkg registry version-database format](https://learn.microsoft.com/en-us/vcpkg/maintainers/registries)
- [Microsoft NuGet configuration-file collections and `disabledPackageSources`](https://learn.microsoft.com/en-us/nuget/reference/nuget-config-file)
- [Microsoft NuGet hierarchical configuration and collection-specific `<clear />`](https://learn.microsoft.com/en-us/nuget/consume-packages/configuring-nuget-behavior)
- [Official vcpkg Exiv2 version registry](https://github.com/microsoft/vcpkg/blob/master/versions/e-/exiv2.json)
