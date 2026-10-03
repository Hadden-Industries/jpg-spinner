# JPG Spinner Modernization Design

- **Status:** Implementation-ready design
- **Date:** 2026-08-30
- **Target release:** 2.0.0.0
- **Implementation plan:** `docs/plans/2026-08-30-jpg-spinner-modernization.md`

## Executive decision

JPG Spinner 2.0 will be a clean, in-place Store upgrade built as a packaged, single-project MSIX application with WinUI 3, C++/WinRT, and the stable Windows App SDK 2.5.1. It will retain the existing Microsoft Store identity and AppContainer trust boundary so the installed product upgrades normally and keeps Store-managed deployment behavior.

The implementation will not retarget or wrap the current C++/CX UWP application. It will replace it with a deliberately layered C++20 system whose core transformation behavior is independent of WinUI. During construction, the legacy project may remain in the repository only as a behavioral reference. At cutover, the legacy project, C++/CX sources, vendored IJG JPEG 9a code, Application Insights beta package, and embedded instrumentation key will be removed in one auditable deletion. No bridge, compatibility facade, legacy adapter, dual runtime, or shim will be introduced.

Every production behavior will be developed test-first. The required cycle is RED, GREEN, REFACTOR: add one focused test, observe it fail for the expected missing-behavior reason, implement the smallest correct change, observe the focused and relevant regression tests pass, then improve structure while keeping the suite green. Scaffolding, package metadata, build configuration, and documentation will use executable validation checks rather than artificial unit tests.

## 1. Why whole-application replacement is the correct migration unit

### 1.1 Current repository evidence

| Concern | Repository evidence | Consequence |
|---|---|---|
| Retired application model | `JPG Spinner/JPG Spinner.vcxproj` targets Windows SDK 10.0.16299.0 and PlatformToolset v141; the application uses UWP and C++/CX | UWP receives maintenance support but is no longer the actively developed native Windows application platform |
| Obsolete codec | `JPG Spinner/jversion.h` identifies IJG JPEG 9a from 2014; codec implementation files are copied into the application | Security, correctness, SIMD, toolchain, and maintenance improvements from the active codec are absent |
| Unsupported instrumentation | `JPG Spinner/packages.config` pins `ApplicationInsights-CPP` 1.0.0-Beta and `pch.cpp` contains an instrumentation key | The application ships an abandoned beta dependency and a repository-visible secret-like identifier |
| Monolithic behavior | `Scenario_AfterPick.xaml.cpp` combines presentation, traversal, storage, JPEG transformation, metadata work, and telemetry in more than 1,600 lines | Correctness-critical behavior cannot be isolated, tested, fuzzed, or replaced safely |
| Architecture-specific gap | Store bundles cover x86, x64, and ARM32 but not ARM64 | The package does not match current Windows hardware distribution |
| No engineering safety net | There are no tests, automated builds, dependency lock, security analysis workflow, architecture documentation, or release checks | A toolchain or codec migration cannot be validated confidently |

Microsoft identifies WinUI 3 and the Windows App SDK as the successor path for UWP desktop applications, and its migration guidance explicitly supports migration through a new packaged project. See [Migrate from UWP to the Windows App SDK](https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/migrate-to-windows-app-sdk/migrate-to-windows-app-sdk-ovw) and [UWP migration guidance](https://learn.microsoft.com/en-us/windows/apps/develop/ai-assisted/migrate/uwp-to-winui).

### 1.2 Options considered

| Option | Advantages | Fundamental problems | Decision |
|---|---|---|---|
| Retarget the existing UWP/C++/CX project and replace only JPEG | Small initial diff | Retains the inactive application model, C++/CX, monolithic structure, and fragile test seams; postpones the expensive migration without reducing its risk | Rejected |
| Build an unpackaged or full-trust WinUI 3 application | Simple access to unrestricted Win32 APIs | Broadens authority unnecessarily, loses the intended AppContainer boundary, complicates Store continuity, and violates least privilege for a local image utility | Rejected |
| Build a clean packaged WinUI 3/C++/WinRT replacement with the same Store identity | Current platform, AppContainer isolation, normal Store updates, separable native core, ARM64, deterministic testing | Requires an explicit behavioral specification and a parallel build until cutover | Selected |

The selected option follows the [single-project MSIX guidance](https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/single-project-msix) and the [MSIX AppContainer model](https://learn.microsoft.com/en-us/windows/msix/msix-container). The package application declaration will use `uap10:TrustLevel="appContainer"` and `uap10:RuntimeBehavior="packagedClassicApp"`.

## 2. Goals, non-goals, and immutable constraints

### 2.1 Goals

- Correctly normalize all eight Exif orientation states with lossless JPEG DCT-coefficient transformations when the source coding process supports them.
- Preserve image fidelity and non-derived metadata unless a field must change to describe the transformed image truthfully.
- Make destructive output impossible without an explicit replacement choice and a verified backup.
- Preserve the existing Store identity and support update installation from 1.1.3.0 to 2.0.0.0.
- Support x86, x64, and ARM64 on Windows 10 version 22H2 build 19045 or later.
- Keep file processing inside AppContainer and request access only through user-selected files or folders.
- Provide deterministic, cancellable batch processing with honest progress and per-file results.
- Establish reproducible dependency resolution, continuous integration, static analysis, sanitizer coverage, fuzzing, accessibility verification, and a release checklist.
- Make domain language explicit enough that a type or operation communicates its invariant without requiring a reader to infer context.

### 2.2 Non-goals for 2.0

- No general-purpose image editor, resizer, recompressor, raw converter, or color-management pipeline.
- No cloud account, synchronization, advertising, behavioral analytics, or app-owned backend.
- No synthetic replacement Exif thumbnail. A stale embedded thumbnail is removed with its IFD instead.
- No C2PA claim generation, signing key, trust-list client, or update-manifest workflow. Provenance-aware derivative generation requires a separately designed security and identity model.
- No multi-picture JPEG/MPO mutation. MPF-bearing files are detected and rejected because transformation can invalidate embedded offsets.
- No pixel-domain fallback for a transformation that cannot be performed losslessly. The application reports why the source is unsupported.
- No parallel transformation queue until representative benchmarks demonstrate a user benefit within explicit memory bounds.
- No automatic replacement without a backup and no hidden edge trimming.

### 2.3 Constraints

- **TDD throughout:** production behavior begins with an observed failing test.
- **No subagents:** implementation is executed sequentially in the active task; review checkpoints are local verification steps, not delegated work.
- **No shims:** obsolete APIs and legacy abstractions are removed instead of emulated.
- **No secret-bearing telemetry:** the old Application Insights key and dependency are removed; the key must be revoked or rotated in its owning service.
- **No floating dependencies:** every package and CI action is pinned to a reviewed immutable version or commit.
- **No silent loss:** trimming, metadata removal, skipped files, and unsupported coding processes are visible in the review and result models.
- **No broad file authority:** the manifest rejects `broadFileSystemAccess`, known-folder library capabilities, `runFullTrust`, and unrelated restricted capabilities; source authority comes only from user-mediated picker grants.

## 3. Supported platform and pinned baseline

Versions below are the latest stable releases verified on 2026-08-30. Experimental and preview releases are intentionally excluded even when their numeric version is newer.

| Tool or package | Required baseline | Rationale and authority |
|---|---:|---|
| Visual Studio | Visual Studio 2026, MSVC 14.51, PlatformToolset v145, with the exact servicing tool directory pinned through `VCToolsVersion` after qualification | Current stable native toolchain; `PlatformToolset` selects the v145 family but does not freeze a particular 14.51 servicing installation. See [upgrading C++ projects to Visual Studio 2026](https://devblogs.microsoft.com/cppblog/upgrading-c-projects-to-visual-studio-2026/) and [MSVC 14.51 availability](https://devblogs.microsoft.com/cppblog/msvc-version-1451-available/) |
| C++ language mode | C++20 with `/std:c++20` and `/permissive-` | C++23 remains a preview switch in this toolchain; production must not depend on preview language behavior. See [MSVC C++23 support](https://devblogs.microsoft.com/cppblog/c23-support-in-msvc-build-tools-14-51/) |
| Windows App SDK | 2.5.1 | Latest stable Windows App SDK as of the design date; see [Windows App SDK downloads](https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/downloads) |
| Windows SDK Build Tools | 10.0.28000.2705 | Latest stable SDK build-tools package as of the design date; see [Windows SDK downloads](https://learn.microsoft.com/en-us/windows/apps/windows-sdk/downloads) |
| C++/WinRT | Microsoft.Windows.CppWinRT 3.0.260818.1 | Latest stable NuGet release as of the design date |
| SBOM generator | Microsoft SBOM Tool CLI 4.1.5; Windows x64 asset SHA-256 `625767b371b7fdd58f40f618b8a86da0247a33c89e419039c86b4edba1dad4b5` | Latest stable official generator as of the design date; the versioned executable can scan an arbitrary MSIX payload tree, unlike `Microsoft.Sbom.Targets`, whose supported target runs after NuGet `Pack`. See [Microsoft SBOM Tool 4.1.5](https://github.com/microsoft/sbom-tool/releases/tag/v4.1.5) |
| JPEG codec | libjpeg-turbo 3.2.0, vcpkg port revision 1 | Active codec with public TurboJPEG transform API and current SIMD/security maintenance; see [libjpeg-turbo releases](https://github.com/libjpeg-turbo/libjpeg-turbo/releases) |
| Metadata library | Exiv2 0.28.9 with XMP support | Current supported metadata implementation with 2026 security fixes; see [Exiv2 releases](https://github.com/Exiv2/exiv2/releases) and [Exiv2 security advisories](https://github.com/Exiv2/exiv2/security) |
| Native tests | Catch2 3.16.0 | Current vcpkg test-framework version at the pinned baseline |
| vcpkg baseline | `c748cb44f2a435fcf015c35225c9d5545fe0021c` | Immutable registry state refreshed on 2026-10-03 before Task 6; historical logs preserve earlier qualification versions |
| Package version | 2.0.0.0 | Major product migration while remaining a greater four-part MSIX version than 1.1.3.0 |
| Minimum Windows version | 10.0.19045.0 | Windows 10 22H2 plus maintained Windows 11 versions; no technical dependency on an older build |
| Target and tested maximum | 10.0.28000.0 | Current SDK contract and manifest `MaxVersionTested` |
| Architectures | x86, x64, ARM64 | x86/x64 retain upgrade compatibility; ARM64 replaces obsolete ARM32 |

The app must compile with a dynamic Universal CRT and statically linked vcpkg libraries using custom `x86-windows-static-md`, `x64-windows-static-md`, and `arm64-windows-static-md` triplets. This avoids separately packaged codec DLLs while retaining the platform-serviced CRT.

The MSIX is framework-dependent (`WindowsAppSDKSelfContained=false`). Its Windows App SDK framework dependency is deployed and serviced through the package/Store mechanism; the production app does not carry a private Windows App Runtime or invoke the unpackaged bootstrapper. A test executable that consumes Windows App SDK runtime types must instead run with test package identity or use the official test-only bootstrapper/auto-initializer; this test infrastructure is never included in the production package. This follows Microsoft’s [Windows App SDK deployment overview](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/deploy-overview) and [unpackaged deployment guidance](https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/deploy-unpackaged-apps).

Scaffolding uses the installed stable [Visual Studio C++ WinUI Blank App (Packaged) template](https://learn.microsoft.com/en-us/windows/apps/dev-tools/visual-studio). The public-preview Windows App Development CLI is not a production bootstrap dependency. The solution remains `JpgSpinner.sln`, because every required stable C++/MSBuild/Store tool supports it and the [newer solution format](https://devblogs.microsoft.com/visualstudio/new-simpler-solution-file-format/) does not yet add product capability; revisit only after the full packaging, analysis, and runner toolchain officially supports the newer format.

## 4. Standards baseline and semantic contracts

### 4.1 Standards

- JPEG DCT syntax and coding behavior are interpreted against [ITU-T T.81 / ISO/IEC 10918-1](https://www.itu.int/ITU-T/recommendations/rec.aspx?lang=en&rec=2633).
- Exif field meaning follows CIPA DC-008-Translation-2026, Exif 3.1, published 2026-01-30. Exif/XMP field correspondence follows CIPA DC-010-2026, published 2026-06-01. Both are listed by the [CIPA standards authority](https://www.cipa.jp/e/std/std-sec.html).
- XMP packet handling follows [Adobe XMP specifications](https://developer.adobe.com/xmp/docs/xmp-specifications/), including the current Part 2 specification for additional properties.
- Content Credentials detection follows the latest stable [C2PA 2.4 specification](https://spec.c2pa.org/specifications/specifications/2.4/), including JPEG JUMBF embedding. A transform changes the asserted asset hash, so version 2.0 rejects embedded or referenced Content Credentials instead of preserving an invalid signature or silently stripping provenance.
- IPTC Core/Extension and IIM preservation follows the latest stable [IPTC Photo Metadata Standard 2025.1](https://www.iptc.org/std/photometadata/specification/IPTC-PhotoMetadata-2025.1.html). Descriptive, rights, licensing, and AI-disclosure properties in XMP or Photoshop APP13 are retained unchanged; the orientation operation has no authority to reinterpret them.
- TIFF/Exif orientation values follow the authoritative [Adobe TIFF namespace definition](https://developer.adobe.com/xmp/docs/xmp-namespaces/tiff/).
- ICC payload preservation recognizes current [ICC.1:2022 profile format version 4.4](https://www.color.org/icc-1_specification/), while preserving older valid embedded profiles byte-for-byte.
- Accessibility acceptance uses [WCAG 2.2](https://www.w3.org/TR/WCAG22/) Level AA as a cross-platform heuristic plus Microsoft’s [Windows accessibility guidance](https://learn.microsoft.com/en-us/windows/apps/develop/accessibility).

### 4.2 Orientation vocabulary

The code must use directionally complete names. Raw numbers may appear only at parsing and serialization boundaries.

```cpp
namespace jpg_spinner::domain
{
    // Values are the Exif/TIFF Orientation tag encodings. The names describe where
    // the stored image's 0th row and 0th column appear in the intended display.
    enum class ExifOrientation : std::uint8_t
    {
        TopLeft = 1,
        TopRight = 2,
        BottomRight = 3,
        BottomLeft = 4,
        LeftTop = 5,
        RightTop = 6,
        RightBottom = 7,
        LeftBottom = 8,
    };

    enum class LosslessTransform
    {
        None,
        FlipHorizontal,
        Rotate180,
        FlipVertical,
        Transpose,
        Rotate90Clockwise,
        Transverse,
        Rotate270Clockwise,
    };
}
```

The required canonicalization mapping is exact:

| Exif value | Stored orientation name | DCT-coefficient transform | Output orientation |
|---:|---|---|---:|
| 1 | `TopLeft` | `None` | 1 |
| 2 | `TopRight` | `FlipHorizontal` | 1 |
| 3 | `BottomRight` | `Rotate180` | 1 |
| 4 | `BottomLeft` | `FlipVertical` | 1 |
| 5 | `LeftTop` | `Transpose` | 1 |
| 6 | `RightTop` | `Rotate90Clockwise` | 1 |
| 7 | `RightBottom` | `Transverse` | 1 |
| 8 | `LeftBottom` | `Rotate270Clockwise` | 1 |

The test corpus must distinguish transpose from transverse. Square, symmetric, or single-color fixtures cannot prove this mapping and are forbidden for orientation tests.

### 4.3 Transform completeness

JPEG coefficient transformations are exact only on complete minimum coded unit boundaries for axes affected by the transform. The application therefore exposes:

```cpp
enum class EdgeHandlingPolicy
{
    RequirePerfectCoefficientTransform,
    TrimPartialMinimumCodedUnits,
};
```

`RequirePerfectCoefficientTransform` is the default and rejects a request when the source dimensions and sampling geometry would require discarding partial edge units. `TrimPartialMinimumCodedUnits` is an explicit advanced choice whose UI states that edge pixels will be removed. Trimming must never occur as an implicit recovery action.

### 4.4 Scan organization

The code must not use a generic “progressive mode” Boolean. It exposes:

```cpp
enum class OutputScanOrganization
{
    PreserveSource,
    SequentialDct,
    ProgressiveDct,
};
```

`PreserveSource` is the default. Explicit conversion is allowed only when the selected coding process is supported and validation proves the output can be fully decoded.

## 5. Architecture

### 5.1 Dependency direction

The central rule is that policy points inward and platform details point outward:

```text
JpgSpinner.App (WinUI 3 presentation and composition root)
    ├── JpgSpinner.BatchProcessing
    │       ├── JpgSpinner.Domain
    │       ├── JpgSpinner.JpegTransformation
    │       └── ImageFileTransactionEngine interface
    ├── JpgSpinner.WindowsStorage
    │       └── JpgSpinner.Domain
    └── Windows App SDK / C++/WinRT

JpgSpinner.JpegTransformation
    ├── JpgSpinner.Domain
    ├── libjpeg-turbo public TurboJPEG API
    └── Exiv2 public API

Tests depend on public module interfaces and shared deterministic fixtures.
Production modules never depend on test projects or presentation types.
```

The storage and codec abstractions are capability interfaces at deliberate seams, not compatibility layers. Each seam is justified by a production adapter and a behaviorally useful deterministic test adapter.

### 5.2 Modules

| Module | Owns | Must not own |
|---|---|---|
| `JpgSpinner.Domain` | Value types, orientation mapping, transform planning, error taxonomy, resource policies | WinRT handles, UI strings, file I/O, codec calls |
| `JpgSpinner.JpegTransformation` | Bounds-checked marker inventory, lossless transform, metadata reconciliation, output validation | File replacement, folder traversal, XAML state |
| `JpgSpinner.WindowsStorage` | Source revision hashing, staging, verified backup, journaled replacement, and deterministic recovery from observable states | JPEG interpretation, localized messages, undocumented power-failure guarantees |
| `JpgSpinner.BatchProcessing` | Enumeration policy, sequential orchestration, cancellation boundaries, progress, summaries | Dialogs, concrete XAML controls, raw codec details |
| `JpgSpinner.App` | Folder picker, review experience, options, localized errors, accessibility, dependency composition | JPEG parsing, transaction state machines, business invariants |

### 5.3 Interface depth and seam discipline

Each module must hide more policy and mechanism than its callers learn. The deletion test is applied during review: deleting a module must force its complexity back into several callers; if deletion merely removes a pass-through, merge that shallow module into its caller.

| Module | External interface | Complexity hidden from callers | Adapter/test rule |
|---|---|---|---|
| `JpgSpinner.Domain` | Immutable values plus pure orientation/planning functions | Exif geometry, checked dimension math, MCU policy, exhaustive errors/findings | In-process; no adapter or hypothetical seam |
| `JpegImageAnalyzer` within JPEG Transformation | One `analyze` operation | Marker scan, Exif/XMP precedence, coding-process support, resource policy, transform planning | Concrete deep module; tests use real deterministic JPEGs |
| `JpegTransformationEngine` | One `createValidatedOutput` operation | TurboJPEG lifetime/parameters, coefficient transform, marker copy, metadata reconciliation, independent scan/full decode, metadata/ICC invariants, output digest, native-error translation | `LibJpegTurboTransformationEngine` production adapter plus deterministic batch-test adapter |
| `ImageFileTransactionEngine` | `execute` and `recoverIncompleteTransactions` | stage naming/ownership, handle lifetime, staged-byte hash verification, source recheck, destination uniqueness, backup, journal, replacement, recovery | `WindowsStorageImageFileTransactionEngine` production adapter plus deterministic batch-test adapter; fault operations remain an internal seam |
| `BatchProcessingCoordinator` | `analyzeBatch` and `processReviewedBatch` | safe traversal, stable ordering, progress accounting, one-at-a-time execution, cancellation, summary completeness | In-process module; module tests use real analyzer and deterministic engine/transaction adapters |

`JpegSegmentScanner`, `LibJpegTurboCoefficientTransformer`, `MetadataReconciler`, `JpegOutputValidator`, codec RAII handles, `RecoverableImageFileTransaction`, journal serialization, and fault-operation controls are internal seams. They are not exposed merely to make tests easy. Security-focused tests and fuzzers may exercise an internal seam from within its owning module, but application callers cannot depend on it. Tests of externally observable behavior cross the same module interface as production callers.

### 5.4 Naming rules

- Native namespace root: `jpg_spinner`; subnamespaces: `domain`, `jpeg`, `storage`, and `batch`.
- WinRT presentation namespace: `JpgSpinner.Presentation`.
- Types and enum members use PascalCase. Functions and local variables use camelCase. Private data members use lower
  camel case with a trailing `_`, matching the repository's enforced clang-tidy policy.
- Public WinRT/MIDL properties, methods, and events use PascalCase, including predicates such as `CanBeginProcessing`; native-only Boolean functions remain camelCase.
- Native abstract bases describe capabilities and do not receive a mechanical `I` prefix: `JpegTransformationEngine`, `ImageFileTransactionEngine`.
- Concrete adapter types identify the mechanism: `LibJpegTurboTransformationEngine`, `WindowsStorageImageFileTransactionEngine`.

`WindowsStorageImageFileTransactionEngine` identifies the native API mechanism.
AppContainer is the separately required process deployment/security environment,
not an isolation property conferred by `StorageFile` or by the adapter's name.
- Names such as `Manager`, `Helper`, `Utils`, `Data`, `Info`, `Item`, `Mode`, and `Handler` are prohibited unless that word is the precise domain term. Examples of correct replacements are `JpegSegmentScanner`, `SourceFileRevisionCalculator`, `BatchProcessingCoordinator`, and `ImageProcessingError`.
- Boolean names state a predicate: `isPerfectTransformAvailable`, `hasEmbeddedIccProfile`, `wasSourceModified`.
- Units appear in names for primitive quantities: `encodedFileLengthBytes`, `elapsedMilliseconds`, `maximumPixelCount`.

`ImageProcessingResult<TValue>` is a narrow domain discriminated result implemented from stable C++20 facilities. It has exactly one active value or `ImageProcessingError`, exposes no monadic compatibility facade, and is not a backport of `std::expected`. Tests must prove construction, move-only values, value/error access preconditions, and the never-empty invariant for the selected storage representation. A future C++23 migration evaluates `std::expected` directly rather than preserving a project-owned compatibility surface.

### 5.5 Comment policy

Comments are required liberally where they preserve intent:

- document every public interface contract and ownership rule;
- explain Exif orientation geometry, MCU completeness, marker size bounds, and metadata reconciliation decisions;
- state transaction recoverability invariants and why each recovery branch preserves validated artifacts;
- identify apartment and thread transitions around WinRT coroutines;
- explain privacy redaction and the mapping from internal errors to user-visible messages;
- explain why a malformed or unsupported source is rejected.

Comments must explain **why**, invariants, or failure consequences. They must not narrate syntax or repeat a correctly named operation. A comment and the behavior it describes are changed in the same commit.

## 6. End-to-end processing flow

```text
User selects source folder
        │
        ▼
Enumerate selected folder according to TraversalScope
        │ exclude app-owned output/backup roots
        ▼
Inspect bounds and JPEG marker inventory
        │ reject malformed, over-limit, MPF, or unsupported coding process
        ▼
Read Exif/XMP orientation and derive JpegTransformPlan
        │ report perfect-transform availability and output dimensions
        ▼
User reviews output, edge, scan, and replacement policies
        │
        ▼
Hash source into SourceFileRevision
        │
        ▼
Create a ValidatedJpegOutput on a background thread
        │ transform coefficients → reconcile metadata → independently validate
        ▼
Write a unique stage beside its final destination; request flush/close; verify staged SHA-256
        │
        ▼
Re-hash source and require unchanged SourceFileRevision
        │
        ├── CreateCorrectedCopy: move staged file to unique batch destination
        │
        └── ReplaceOriginalWithVerifiedBackup:
               copy and verify backup → persist journal state → request MoveAndReplaceAsync
        │
        ▼
Record per-file result, clean safe staging artifacts, retain backup
```

All codec work runs away from the UI thread by following the [C++/WinRT concurrency guidance](https://learn.microsoft.com/en-us/windows/apps/develop/cpp-winrt/concurrency). Coroutines accept parameters by value when lifetime can cross suspension, never call `.get()` on the UI thread, use `resume_background()` for blocking native work, and return through `DispatcherQueue` only for presentation updates.

## 7. Input safety and JPEG policy

### 7.1 Explicit resource limits

The first release uses conservative, named limits:

```cpp
struct JpegResourceLimits final
{
    std::uint64_t maximumEncodedFileLengthBytes;
    std::uint64_t maximumPixelCount;
    std::uint64_t maximumMetadataLengthBytes;
    std::uint32_t maximumProgressiveScanCount;

    [[nodiscard]] static consteval JpegResourceLimits production() noexcept
    {
        return {
            512ULL * 1024ULL * 1024ULL,
            268'435'456ULL,
            32ULL * 1024ULL * 1024ULL,
            100U,
        };
    }
};

struct TurboJpegResourceLimits final
{
    // TJPARAM_MAXMEMORY is expressed in mebibytes and limits codec
    // intermediate buffers; it is not an encoded-input-length limit.
    std::int32_t maximumIntermediateBufferMemoryMebibytes;

    [[nodiscard]] static consteval TurboJpegResourceLimits production() noexcept
    {
        return {512};
    }
};
```

These values are not hidden tuning knobs. `JpegResourceLimits` is a public Domain policy; `TurboJpegResourceLimits` is an internal adapter policy so TurboJPEG's unusual unit never leaks into application callers. Production composition always supplies their `production()` values. The application enforces `maximumEncodedFileLengthBytes` before codec parsing; it is independent of TurboJPEG's working-memory control. Focused tests may inject smaller immutable limits so boundary behavior can be proven without allocating hundreds of MiB. The values are logged by symbolic name, covered at the boundary and one unit beyond it, and documented in user-facing unsupported-file messages. Raising a production limit requires corpus evidence and memory measurements on the minimum supported device class.

Only one transform transaction runs at a time. This keeps peak codec memory bounded and makes cancellation, ordering, and file recovery deterministic. Folder analysis may use asynchronous I/O, but it does not create an unbounded task per file.

### 7.2 Preflight marker scanner

`JpegSegmentScanner` performs a bounded structural pass before libjpeg-turbo or Exiv2 receives bytes. It must:

- verify Start of Image, End of Image, marker framing, and whether any payload follows EOI;
- perform checked arithmetic for every two-byte marker length and buffer offset;
- inventory SOF coding process, dimensions, component sampling, SOS count, APPn segments, COM segments, ICC chunks, Exif, standard and extended XMP, MPF, APP11 JUMBF, and C2PA Content Credential references;
- enforce aggregate metadata and scan-count limits;
- detect duplicate or inconsistent ICC chunks;
- reject truncated, overlapping, impossible, or over-limit structures;
- make no attempt to interpret entropy-coded bytes beyond correctly finding marker boundaries;
- return immutable offsets into an owned input view, never dangling pointers.

This scanner is a security boundary and receives both deterministic malformed-input tests and continuous coverage-guided fuzzing.

### 7.3 Supported and rejected forms

Baseline, extended sequential, progressive, and arithmetic DCT JPEG are supported at the standard 8- and 12-bit lossy precisions when libjpeg-turbo reports the process as transformable. Grayscale, RGB, YCbCr, CMYK, and YCCK component organizations are retained without color conversion. Lossless predictive, hierarchical, invalid precision, and unknown SOF processes are rejected. MPO/MPF is rejected because its multi-image offsets and relationships cannot be preserved safely by a single-image transaction. Motion Photos and other payload-bearing data after EOI are rejected because transformation would invalidate XMP offsets or discard a second asset. C2PA Content Credentials are rejected because transformation invalidates their cryptographic asset binding; other JUMBF metadata is rejected because version 2.0 cannot prove its internal references remain truthful. No file is silently decoded and re-encoded in the pixel domain.

The transform engine uses the public TurboJPEG 3 API, including `tj3Transform`; it must not include `transupp.h`, internal libjpeg headers, or copied codec source. Defense-in-depth configuration sets `TJPARAM_STOPONWARNING=1`, `TJPARAM_MAXMEMORY=512` in TurboJPEG's mebibyte unit for intermediate buffers, `TJPARAM_MAXPIXELS=268435456`, `TJPARAM_SCANLIMIT=100`, and `TJPARAM_SAVEMARKERS=0`. Every `tjtransform` also sets `TJXOPT_COPYNONE`, so marker suppression is local to the operation even if context parameters or upstream defaults change. After reading the source header, each transform sets `TJXOPT_PROGRESSIVE` and `TJXOPT_ARITHMETIC` explicitly when the transform plan requires them; it never relies on mutable `TJPARAM_*` state surviving another header or transform call. The application’s bounds-checked reconciliation policy reassembles each permitted marker exactly once. The behavior is anchored in the official [libjpeg-turbo transform documentation](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/main/doc/usage.txt) and [TurboJPEG 3.2.0 interface](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/3.2.0/src/turbojpeg.h).

## 8. Metadata reconciliation

### 8.1 Preservation rule

Metadata is user data. The default rule is “preserve byte content and marker order unless the field is structurally unsafe or becomes false after transformation.” Specifically:

- Preserve an assembled ICC APP2 profile byte-for-byte and verify its SHA-256 hash after output assembly.
- Preserve unknown APPn and COM payloads in source order when their framing is valid and the output codec can retain them.
- Preserve non-derived Exif and XMP properties.
- Treat standard and extended XMP as an application-owned segment set. Preserve complete extended-XMP chunks byte-for-byte only when no field that must change is stored in the extension packet and the retained standard packet remains truthful. Otherwise reject before transformation with `ExtendedXmpMutationNotSupported`; do not partially rewrite, silently drop, or canonicalize the extension.
- Preserve IPTC IIM/Photoshop APP13 payloads and IPTC XMP properties, including rights and AI-disclosure fields, without semantic rewriting.
- Set Exif and XMP orientation to canonical value 1 after a successful transform.
- Update Exif `PixelXDimension`, `PixelYDimension`, `ImageWidth`, and `ImageLength` when present, plus their mapped XMP properties according to CIPA DC-010-2026.
- Remove derived previews when pixels are transformed: Exif IFD1 thumbnails, JFIF/JFXX thumbnails, XMP `xmp:Thumbnails`, and Photoshop IRB thumbnail resources. Preserve JFIF density/version fields and non-thumbnail Photoshop/IPTC resources. If a known preview container cannot be parsed and rewritten safely, reject the file; retaining a stale preview is semantically incorrect.
- Reject MPF rather than preserve offsets that may now point to invalid locations.
- Reject C2PA/JUMBF before transformation rather than retaining invalid authenticity/provenance assertions or removing them without consent.
- Reject malformed metadata rather than writing a partly repaired source without the user knowing.

Exiv2 is used for supported Exif/XMP interpretation and mutation; the application does not grow its own TIFF or RDF parser. The application supplies Exiv2 only isolated, owned Exif or standard-XMP payloads. It may supply a completely reassembled extended-XMP payload for read-only property inspection, but never asks Exiv2 to serialize that extension or the complete JPEG container. If the complete extension cannot be parsed unambiguously enough to prove that orientation, dimensions, and derived-thumbnail facts are unaffected, the transform is rejected. `JpegSegmentScanner` and the application-owned reconciler retain responsibility for marker framing, relative marker order, unknown APPn/COM payloads, ICC chunks, and extended-XMP chunk bytes. Before accessing IPTC 2025.1 properties whose namespace is not built into Exiv2 0.28.9, the metadata adapter registers the exact official namespace URI and prefix and tests the registration. The bounded marker scanner owns outer JPEG structural validation because it enforces application-specific aggregate limits before a general metadata library parses the source.

### 8.2 Output validation

A transformed byte sequence cannot become `ValidatedJpegOutput`, and is therefore not eligible for staging or commit, until the internal `JpegOutputValidator` proves all of the following:

1. The complete byte stream passes structural scanning and all resource limits.
2. The SOF coding process and scan organization match the transform plan.
3. Width and height match the planned dimensions.
4. A full decode into a bounded discard sink succeeds without warnings elevated to errors.
5. Exif and XMP orientation are absent or equal to canonical value 1 according to the chosen serialization rule.
6. Every present derived dimension field matches the encoded dimensions.
7. The ICC profile hash equals the source profile hash.
8. Required preserved marker payloads are present in the specified order.
9. No stale Exif thumbnail or MPF metadata remains.
10. Every retained extended-XMP chunk, GUID, full-length declaration, offset, and byte range satisfies the selected preservation policy.
11. The output SHA-256 hash is recorded for transaction recovery.

Coefficient-level tests, not merely decoded-pixel screenshots, prove that each lossless transform rearranges DCT coefficient blocks and signs correctly. Decoded comparisons may supplement these tests but cannot replace them.

## 9. Storage transaction and recovery design

### 9.1 User-visible output policies

```cpp
enum class OutputDisposition
{
    CreateCorrectedCopy,
    ReplaceOriginalWithVerifiedBackup,
};

enum class TraversalScope
{
    SelectedFolderOnly,
    SelectedFolderAndDescendants,
};
```

`CreateCorrectedCopy` is the default. It writes to `JPG Spinner Output/<batch-directory>/<relative-source-path>`. The batch directory name is a UTC timestamp followed by the first eight hexadecimal characters of a generated UUID; the underlying `BatchIdentifier` retains the full UUID.

Replacement is available only as `ReplaceOriginalWithVerifiedBackup`. Its backup path is `JPG Spinner Backups/<batch-directory>/<relative-source-path>`. There is no replace-without-backup state in the domain model or UI. A unique batch root prevents overwrite collisions. Recursive traversal excludes the exact app-owned output and backup roots after resolving their storage identity; string-prefix checks are insufficient.

### 9.2 Source revision

`SourceFileRevision` includes encoded length, last-modified time, and SHA-256. The hash is authoritative; length and timestamp permit fast diagnostics and clearer messages. The revision is captured before transformation and recalculated immediately before commit. A mismatch produces `SourceChangedAfterAnalysis`, leaves the original untouched, and safely removes only the transaction’s own staged file.

### 9.3 Staging, replacement, and recoverability

- Create a unique staging file named `.jpg-spinner-staged-<transaction-guid>.jpg` beside the final destination. Copy output stages inside its unique batch destination tree; replacement output stages beside the original so `MoveAndReplaceAsync` remains on the same volume. A non-destructive copy therefore does not require a temporary write beside each source file.
- Write the immutable `ValidatedJpegOutput` bytes, request the strongest supported flush, close every handle, hash the staged file through a new read handle, and require that hash to equal the validator-recorded SHA-256. A successful WinRT flush is not described as a physical-media durability guarantee.
- For copy output, move the hash-verified stage to its unique batch destination.
- For replacement, create the backup first, close it, hash it, and require its hash to equal the captured source hash.
- Persist and close the next journal snapshot before requesting the replacement boundary.
- Request replacement through `StorageFile.MoveAndReplaceAsync`, whose documented contract is move-and-replace behavior, not guaranteed power-fail atomicity or durable-media commitment. After any interruption, determine the observable outcome from source, stage, output, backup, and journal hashes rather than assuming whether the API completed. A Win32 alternative may replace this call only after an AppContainer capability prototype and the same fault-injection suite prove a stronger useful contract; `ReplaceFileW` is not assumed to provide an unsupported write-through guarantee.
- Never delete a verified backup automatically.

The supported guarantee is recoverability under the explicitly tested failure model, not immunity to physical-device loss. At every modeled interruption, the recovery procedure must retain at least one validated source-equivalent or committed-output artifact and must never delete the only validated copy. Qualification covers NTFS, supported removable filesystems, ReFS where the application is supported, and representative cloud-backed picker locations; unsupported provider behavior fails safely before replacement.

### 9.4 Journal state machine

`ImageFileTransactionJournal` is stored in local application data using `Windows.Data.Json`. Its states are monotonic:

```cpp
enum class ImageFileTransactionState
{
    TransactionInitialized,
    StagedOutputWritten,
    StagedOutputHashVerified,
    VerifiedBackupCreated,
    OutputCommitted,
    OwnedStagingArtifactsCleaned,
};
```

Every transition is idempotent and written as an immutable generation rather than replacing an existing journal file. The writer creates a uniquely named pending file, requests flush, closes it, reopens and validates its complete JSON, then publishes it under a never-before-used monotonically numbered filename. Recovery ignores incomplete pending files and selects the highest complete generation whose predecessor and artifact hashes are consistent; earlier valid generations remain until a verified terminal state. This protocol avoids making correctness depend on undocumented atomic journal replacement. A generation stores schema version, generation number, transaction ID, source token or path identity, output destination, source revision, staged-output hash, backup identity, output disposition, and state. Logs redact path components by default; the journal retains the minimum path identity required for recovery inside local app data.

The allowed paths are explicit. Copy transactions use `TransactionInitialized → StagedOutputWritten → StagedOutputHashVerified → OutputCommitted → OwnedStagingArtifactsCleaned`. Replacement transactions insert `VerifiedBackupCreated` between hash verification and commit. No other transition or backward transition is legal.

On startup, incomplete journals are recovered by comparing hashes:

| Observed original state | Recovery decision |
|---|---|
| Original hash equals captured source hash | Original was not replaced; remove only the owned stage when safe, keep any backup, and finish without changing the original |
| Original hash equals validated transformed hash | Commit succeeded; record `OutputCommitted`, remove only the owned stage when safe, and retain the backup |
| Original missing but a hash-verified stage and verified backup exist | Leave both, report `RecoveryConflict`, and give the user precise locations; do not guess |
| Original hash equals neither known hash | Leave original, stage, and backup untouched; report `RecoveryConflict` |

The rollback invariant is: the application may delete its own uncommitted stage, but never deletes a source or verified backup as rollback.

## 10. Error model

The core returns structured errors and never localized prose:

```cpp
enum class ImageProcessingErrorCode
{
    SourceAccessDenied,
    SourceChangedAfterAnalysis,
    DestinationAccessDenied,
    EncodedFileTooLarge,
    PixelCountLimitExceeded,
    MetadataLengthLimitExceeded,
    ProgressiveScanCountLimitExceeded,
    MalformedJpegStructure,
    MalformedImageMetadata,
    InvalidOrientationMetadata,
    UnsupportedJpegCodingProcess,
    UnsupportedJpegSamplePrecision,
    UnsupportedJpegComponentOrganization,
    MultiPictureJpegNotSupported,
    MotionPhotoNotSupported,
    UnsupportedTrailingPayload,
    ContentCredentialsWouldBeInvalidated,
    UnsupportedJumbfMetadata,
    UnsupportedEmbeddedPreviewMetadata,
    ExtendedXmpMutationNotSupported,
    PerfectCoefficientTransformUnavailable,
    InsufficientStorageSpace,
    OutputRelativePathCollision,
    StagingFileCreationFailed,
    StagingWriteFailed,
    StagingFlushFailed,
    StagedOutputHashMismatch,
    OutputValidationFailed,
    CorrectedCopyCommitFailed,
    BackupCreationFailed,
    BackupVerificationFailed,
    OriginalReplacementFailed,
    JournalPersistenceFailed,
    RecoveryConflict,
    Cancelled,
};
```

`ImageProcessingError` also carries the processing stage, an optional `std::error_code` or HRESULT projection, and safe diagnostic context. Presentation maps the enum to localized title, explanation, and remedy. Native library messages are never displayed verbatim because they may be unlocalized, overly technical, or path-bearing.

Non-fatal facts that require an explicit review use a separate, equally precise taxonomy:

```cpp
enum class JpegAnalysisFindingCode
{
    ExifXmpOrientationConflict,
    EmbeddedThumbnailRemovalRequired,
    PartialMinimumCodedUnitTrimRequired,
};
```

`JpegImageAnalysis` carries these findings together with their typed facts, such as the authoritative orientation or exact discarded-edge dimensions. Findings are not represented as strings or overloaded error codes.

## 11. Batch behavior and cancellation

- Candidate discovery recognizes `.jpg`, `.jpeg`, `.jpe`, and `.jfif` using an ordinal case-insensitive extension comparison, then requires a valid JPEG signature and scanner result. Output copies retain the source extension. The application does not inspect unrelated files or trust an extension as proof of format.
- Candidate enumeration is lazy and paged. The initial AppContainer implementation uses supported Storage APIs with pages no larger than 500 items. A production-shaped 10,000-file spike measures first-result latency, throughput, memory, cancellation, reparse-point handling, inaccessible descendants, removable media, and cloud placeholders. A Win32 enumeration path is selected only if that evidence demonstrates a material benefit and picker-granted AppContainer access is proven on every supported storage class; any such path obtains handles through documented [`IStorageItemHandleAccess` interop](https://learn.microsoft.com/en-us/windows/win32/api/windowsstoragecom/nf-windowsstoragecom-istorageitemhandleaccess-create) and never infers unrestricted authority from a path string. Benchmark folklore is not an interface decision.
- Recursive discovery does not follow directory reparse points. Every output relative path is derived from a source storage item proven to remain beneath the selected root; normalized string-prefix comparison alone is never used as a containment proof.
- Enumeration order is a stable ordinal comparison of normalized relative paths so tests and user-visible results are reproducible.
- Before processing begins, output relative paths are checked with conservative ordinal case-insensitive comparison. A collision is reported as `OutputRelativePathCollision` during review rather than failing after part of the batch commits. Unicode filenames remain user-visible unchanged; canonical path resolution and case folding are used only for comparison and never rewrite a name.
- Default traversal is `SelectedFolderOnly`; descendants require an explicit choice.
- Every candidate receives an immutable `JpegImageAnalysis` before the batch review is accepted.
- `BatchProcessingProgress` reports discovered, analyzed, eligible, completed, failed, skipped, and remaining counts; “percent” is shown only when the denominator is known.
- A batch has one active transformation transaction. This avoids codec memory multiplication and gives the file currently being committed exclusive treatment.
- Cancellation is observed between safe stages and between files. It does not attempt to abort native C code mid-call or interrupt replacement after the commit boundary begins.
- Cancellation before commit deletes only the owned stage. Cancellation after the commit boundary completes that file’s journal transition and then stops before the next file.
- `BatchProcessingSummary` records an explicit result for every discovered JPEG candidate. Silent omission is not permitted.

Terminal per-file outcomes use precise codes and carry a structured error only where applicable:

```cpp
enum class BatchFileOutcomeCode
{
    CorrectedCopyCreated,
    OriginalReplacedWithVerifiedBackup,
    NoOrientationNormalizationRequired,
    UnsupportedSourceSkipped,
    ProcessingFailed,
    CancelledBeforeTransformation,
    CancelledBeforeCommit,
};
```

An absent orientation tag has the same display semantics as `TopLeft`; it produces `NoOrientationNormalizationRequired` and no output file unless a future separately designed metadata-only operation is requested. Version 2.0 does not write unchanged copies merely to report success.

## 12. Presentation, accessibility, and localization

The single-window flow has six semantic states: source selection, analysis, review, processing, results, and recovery required. It does not recreate the old scenario/flyout structure.

- The source-selection state explains local-only processing and offers folder selection.
- Analysis shows indeterminate progress only before the candidate count is known and exposes cancellation.
- Review lists each source’s orientation, intended transform, output dimensions, perfect-transform eligibility, and unsupported reason. Advanced scan organization and edge handling are explicitly labeled.
- Results show completed, skipped, failed, and cancelled entries with a safe recovery instruction where relevant.
- Replacement requires selecting `ReplaceOriginalWithVerifiedBackup`; the UI displays the backup root before execution.

Every actionable control receives a programmatic name, role, state, and keyboard access. Logical focus follows visual order; focus is restored meaningfully after dialogs. Status is announced through an appropriate live region without repeated noise. The UI supports keyboard-only use, Narrator, high contrast, 200% text scaling, 400% effective zoom scenarios, and no information encoded only by color. Automated checks are followed by Microsoft’s recommended [accessibility release testing](https://learn.microsoft.com/en-us/windows/apps/design/accessibility/accessibility-testing).

Existing `en-US`, `en-GB`, and `ru` resources are migrated. Resource keys use stable semantics such as `Error_MalformedJpegStructure_Explanation`, not English sentence fragments. Missing-resource tests enumerate every `ImageProcessingErrorCode` and visible view-model state in every supported locale.

The C++/WinRT application declares `App` and `MainWindow` runtime classes in `App.idl` and `MainWindow.idl`; other projected presentation types live in focused IDL files. Every public projected member follows WinRT PascalCase conventions, including `CanSelectSourceFolder`, `CanBeginProcessing`, `CanCancelCurrentOperation`, and `CanReturnToSourceSelection`. Native implementation helpers remain camelCase. `x:Bind` compilation and WinMD inspection are executable contract checks for these names.

### 12.1 Fluent visual system and responsive layout

The application uses first-party WinUI controls and theme resources instead of recreating Windows chrome. The primary long-lived window uses `MicaBackdrop`; WinUI supplies the solid-color fallback on Windows 10, high contrast, disabled transparency, battery saver, and unsupported hardware. An integrated WinUI title bar contains only app identity and window-level commands. `InfoBar` communicates non-modal information, warnings, and errors; it replaces custom colored status strips. Content cards use Windows layer/card theme brushes, and the system light/dark theme remains authoritative. These choices follow Microsoft’s current [modern WinUI 3 application structure](https://learn.microsoft.com/en-us/windows/apps/develop/ui/windows-app-sdk-app-structure) and [Mica guidance](https://learn.microsoft.com/en-us/windows/apps/design/style/mica).

The workflow uses `Grid`, `ListView`, standard buttons, pickers, `ProgressBar`, `InfoBar`, and text styles from WinUI. It does not add a navigation pane for a single linear workflow, a third-party controls package, custom-drawn widgets, or icon-only actions. Segoe Fluent Icons may supplement labels but never replace accessible action text.

Layout responds to available window width, not monitor class. The small breakpoint below 640 effective pixels stacks review facts and actions; medium 640–1007 uses a compact list/detail arrangement; large 1008 and above exposes additional review columns without changing task order. This follows Microsoft’s [responsive design techniques](https://learn.microsoft.com/en-us/windows/apps/design/layout/responsive-design) and [current breakpoints](https://learn.microsoft.com/en-us/windows/apps/design/layout/screen-sizes-and-breakpoints-for-responsive-design). Spacing uses the Windows four-effective-pixel grid, text reflows without clipping, and no fixed height contains localizable prose.

The existing product metaphor is retained, but package artwork is regenerated from the highest-quality owned source to current [Windows app icon guidance](https://learn.microsoft.com/en-us/windows/apps/design/iconography/app-icon-design). Required scale/target-size assets are derived deterministically, inspected at native size in light/dark shell contexts, and validated by packaging tools. An AI-generated replacement brand is out of scope.

## 13. Privacy and diagnostics

The application has no network capability and sends no custom telemetry. It removes the Application Insights package and key. Crash and hang trends are obtained from the Store’s [Partner Center MSIX health report](https://learn.microsoft.com/en-us/partner-center/insights/msix-health-report), which reflects users’ Windows diagnostic-data settings.

A user-visible local diagnostic log contains application version, architecture, symbolic processing stages, symbolic error codes, durations, resource-limit names, and correlation IDs. It excludes image bytes, metadata values, full paths, user names, folder names, and instrumentation identifiers. A user must explicitly export diagnostics; no background upload exists.

## 14. Reproducibility and supply-chain policy

- `vcpkg.json` declares direct native dependencies and exact overrides. `builtin-baseline` is the reviewed commit `c748cb44f2a435fcf015c35225c9d5545fe0021c`.
- The exact Visual Studio 2026 WinUI C++ project shape must first prove that `Directory.Packages.props`, versionless `PackageReference`, lock-file generation, and clean locked restore produce the same graph for every architecture. When that executable proof passes, central management is used. If the exact `.vcxproj` shape cannot satisfy it, each project carries the same explicit immutable package versions and the policy verifier rejects drift; no compatibility package or restore shim is introduced.
- When the production-shaped NuGet proof passes, `packages.lock.json` files are committed and locked restore is used in continuous integration. Repository `NuGet.config` clears inherited sources, declares only the official HTTPS nuget.org v3 endpoint, maps every package to that source, contains no credentials, and is passed explicitly to restore commands so machine-wide configuration cannot alter the graph.
- GitHub Actions use immutable commit SHAs. Human-readable version comments accompany each SHA.
- Hosted jobs select `windows-2025-vs2026`, but the label is not treated as an immutable machine image; every job records runner `ImageVersion` and the exact toolchain described below.
- Dependabot covers NuGet and GitHub Actions with grouped, review-required pull requests. vcpkg freshness is checked by a scheduled script because Dependabot does not natively own the vcpkg baseline workflow.
- Before release integration, a fixed payload fixture runs the exact Microsoft SBOM Tool CLI 4.1.5 Windows asset and proves selector `SPDX:3.0`, output location, root `@context` `https://spdx.org/rdf/3.0.1/spdx-context.jsonld`, every emitted `CreationInfo.specVersion` value `3.0.1`, expected relationships, and validation against the official [SPDX 3.0.1 JSON schema](https://spdx.org/schema/3.0.1/spdx-json-schema.json). The release workflow downloads that same versioned asset, verifies SHA-256 `625767b371b7fdd58f40f618b8a86da0247a33c89e419039c86b4edba1dad4b5`, never invokes a moving `latest` URL, recursively unpacks the exact signed bundle and architecture packages into a fresh read-only inspection tree, and generates outside that tree. The outer signed-bundle SHA-256 is retained in release provenance.
- Clean builds produce a normalized unpacked-payload manifest containing relative path, byte length, and SHA-256. Reproducibility requires identical payload manifests under identical qualified inputs. Byte-identical unsigned MSIX/MSIXBUNDLE containers are claimed only if two independent clean builds prove that property; signed-container identity is the recorded hash of the exact qualified artifact.
- Release provenance records runner `ImageVersion`, operating-system build, exact `VCToolsVersion`, `cl.exe`, `link.exe`, Windows SDK, MakeAppx, package locks, vcpkg baseline, action commits, and payload/container hashes.
- The release generates `THIRD_PARTY_NOTICES.md`; licensing is checked before every dependency upgrade. The repository is GPL-3.0, and the selected Exiv2 GPL-2.0-or-later terms, corresponding-source disposition, build instructions, current [Microsoft Store Policies](https://learn.microsoft.com/en-us/windows/apps/publish/store-policies), and [Microsoft Publisher Agreement](https://learn.microsoft.com/en-us/legal/marketplace/msft-publisher-agreement) review must be recorded explicitly. A separate process or dynamic-loading boundary is not treated as a licensing shortcut.
- No update is auto-merged. A dependency bump must pass transform exactness, malformed corpus, metadata preservation, transaction fault injection, sanitizer, static analysis, package, and accessibility gates.

## 15. Test strategy

### 15.1 Test pyramid

| Layer | Primary proof |
|---|---|
| Domain unit tests | Exact eight-orientation table, dimension swaps, edge policies, error mapping, stable batch ordering |
| Codec module tests | Coefficient digests, scan organization, marker inventory, metadata preservation, full-decode validation |
| Storage module tests | Real temporary files plus fault injection at every journal boundary, hash-based recovery, source-change detection |
| Batch tests | Cancellation boundaries, progress accounting, app-owned-root exclusion, per-file outcome completeness |
| Presentation tests | Hosted headless tests for view-model transitions, resources, projected metadata, and accessibility contracts; interactive-lane UI Automation for the real control tree and focus behavior |
| Package tests | Manifest schema, identity equality, AppContainer trust, architecture bundles, upgrade installation |
| Security tests | Fuzzing, AddressSanitizer, static analysis, CodeQL, malformed corpus, resource-limit boundaries |
| Manual release checks | Narrator, keyboard-only, high contrast, text scaling, physical ARM64, Store update path, and storage-interruption simulations on qualified filesystems/providers |

### 15.2 Fixture rules

- Generate synthetic JPEG fixtures deterministically from explicit coefficient patterns and fixed metadata; do not rely on copyrighted photos or editor-dependent encodings.
- Hand-derive expected dimensions, coordinate mappings, coefficient permutations, and metadata hashes before implementation.
- Store only minimal binary regression samples whose precise provenance and expected property are documented in `tests/TestData/README.md`.
- A test name states the defect it would catch, such as `rotate90Clockwise_rejects_partialHorizontalMinimumCodedUnit_whenPerfectTransformIsRequired`.
- Test observable output or state, not which mock method was invoked. Use fault-injecting capability implementations only where reproducing an OS failure with a real file is impossible.
- A regression test must fail against the defective behavior before the remedy is accepted.

### 15.3 Required RED, GREEN, REFACTOR evidence

Each implementation commit records commands in its message or pull-request notes:

1. **RED:** focused test command and failure whose message proves the intended behavior is absent.
2. **GREEN:** the same focused command passes after the minimum implementation.
3. **REGRESSION:** the module suite and all transitive consumers pass.
4. **REFACTOR:** structural improvement, if any, followed by the same green commands.

Production code added before the corresponding observed RED must be reverted and recreated through the test-first cycle. Keeping the code and adding a test afterward is not an acceptable substitute.

## 16. Security build baseline

All first-party native projects use warnings as errors and enable `/sdl`, `/guard:cf`, and qualified `/Qspectre` settings on supported architectures. `/CETCOMPAT` is enabled and PE-verified for x64 only under the current linker contract; it is not passed to x86 or ARM64. Compiler settings are expressed as `ClCompile` item-definition metadata and linker settings as `Link` metadata in a late-imported shared target, then verified from evaluated MSBuild state or binary logs. Release builds enable link-time code generation and reproducible build flags; test and fuzz variants preserve symbols. The policy follows Microsoft’s [C++ build customization guidance](https://learn.microsoft.com/en-us/visualstudio/msbuild/customize-cpp-builds?view=visualstudio), [secure C++ build guidance](https://learn.microsoft.com/en-us/cpp/code-quality/build-reliable-secure-programs), [`/CETCOMPAT` reference](https://learn.microsoft.com/en-us/cpp/build/reference/cetcompat), and [buffer-overrun defense guidance](https://learn.microsoft.com/en-us/windows/win32/secbp/avoiding-buffer-overruns).

Continuous integration builds x86, x64, and ARM64. Headless native tests execute on x86 and x64; ARM64 is cross-compiled on every change and executed on physical ARM64 hardware before release. Black-box UI Automation, Narrator, Accessibility Insights, and focus traversal run in an isolated interactive Windows VM or self-hosted release lane, never under an assumed GitHub-hosted interactive desktop. AddressSanitizer runs the x64 codec, scanner, metadata, and transaction suites. CodeQL and MSVC `/analyze` must have no unresolved high-severity finding. The marker scanner’s x64 `/fsanitize=fuzzer` target has an installed-toolchain smoke test and retains every coverage-increasing or crashing input in a reviewed corpus.

## 17. Store continuity and rollout

The following values are immutable during migration:

- Package identity name: `HaddenIndustriesLtd.JPGSpinner`
- Publisher: `CN=42458E53-5B1F-4F49-97F4-ABE6B4A48BB3`
- Application identifier: `App`
- Store association and Store ID: the existing `Package.StoreAssociation.xml`, including Store ID `9NBLGGH3TVGW`

The new manifest increments the version to 2.0.0.0, declares AppContainer, targets build 28000, and sets minimum build 19045. Package display metadata may evolve, but identity and publisher cannot. Microsoft confirms that a packaged desktop application with the same Store identity can update the installed listing; see the [Windows application developer FAQ](https://learn.microsoft.com/en-us/windows/apps/get-started/windows-developer-faq).

Before Store submission, testing must install a signed 1.1.3.0 package, create representative app-local state and source folders, then install 2.0.0.0 as an update. It must prove package identity continuity, launch, user-selected file access, local-state migration or benign non-use, uninstall behavior, and no orphaned staging files.

The manual platform matrix covers current Windows 10 22H2 ESU, maintained Windows 11 24H2 and 25H2 systems, and Windows 11 26H1 hardware where available. The 26H1 entry is a supported new-device hardware cohort, not an assumed in-place upgrade path for existing 24H2 or 25H2 systems.

Release uses staged availability at 5%, then 25%, then 100%. Advancement requires the Partner Center health report to remain within the defined crash-free and hang-free thresholds and no evidence of source-file loss or backup failure. Any data-integrity incident stops rollout immediately; health thresholds never override correctness reports.

## 18. Acceptance criteria

JPG Spinner 2.0 is releasable only when all conditions hold:

- All eight Exif orientations pass coefficient-exact tests across representative 4:4:4, 4:2:2, and 4:2:0 sampling.
- Perfect transformations reject partial MCU edges; explicit trim tests prove only the documented edge is removed.
- Metadata tests prove ICC byte preservation, orientation canonicalization, derived-dimension updates, stale thumbnail removal, unknown-marker preservation, extended-XMP preservation/rejection policy, registered IPTC 2025.1 namespaces, and MPF rejection.
- Every malformed-input and resource-boundary test passes under AddressSanitizer.
- Fault injection at every transaction transition and qualified storage-provider boundary proves recoverability from observable hashes without claiming undocumented power-fail atomicity or physical-media durability.
- A changed source is never replaced using stale analysis.
- x86 and x64 tests pass; x86, x64, and ARM64 packages build; physical ARM64 smoke and transform suites pass.
- The new package upgrades an installed 1.1.3.0 package without changing Store identity.
- Keyboard, Narrator, high contrast, and text-scaling checks pass in all supported locales.
- No Application Insights reference, embedded key, C++/CX syntax, UWP XAML dependency, copied JPEG implementation, private `transupp` API, legacy adapter, or shim remains.
- CI actions and dependencies are immutable and current against the implementation-date stable release audit.
- NuGet resolution, effective MSBuild compiler/linker settings, exact `VCToolsVersion`, SBOM serialization, payload-manifest reproducibility, and runner provenance pass their executable policy fixtures.
- `README.md`, `SECURITY.md`, architecture documentation, privacy disclosure, third-party notices, SBOM, and Store listing accurately describe the shipped behavior.

## 19. Decision log

| Decision | Reason |
|---|---|
| WinUI 3 packaged single-project MSIX | Current Microsoft desktop UI path with Store continuity and the smallest packaging topology |
| AppContainer rather than medium integrity | Least privilege is sufficient for user-selected image processing |
| C++20 rather than preview C++23 | Stable, supported compiler contract is more important than newer syntax |
| Public TurboJPEG API rather than private libjpeg transforms | Stable supported surface and no copied internal code |
| Exiv2 rather than a custom TIFF/XMP parser | Metadata parsing is an untrusted-input security problem with mature standards complexity |
| Default create-copy output | Source preservation is safer than replacement |
| Mandatory verified backup for replacement | A transformation utility must retain validated recovery evidence across every supported and fault-injected interruption; physical storage-device survival is outside the software guarantee |
| Reject imperfect transform by default | “Lossless” cannot silently discard edge pixels |
| Remove stale thumbnail rather than regenerate it | Honest metadata without introducing a lossy thumbnail encoder or new quality policy |
| Reject MPO/MPF in 2.0 | Offset-bearing multi-image metadata needs a separately designed transaction and validator |
| Sequential transformations | Predictable memory, cancellation, and transaction behavior outweigh unproven throughput gains |
| Partner Center health plus local diagnostics | Adequate operational visibility without a custom telemetry channel |
| Parallel clean rewrite with hard cutover | Tests preserve behavior; shims would preserve obsolete architecture and multiply paths |
