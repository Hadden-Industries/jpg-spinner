# Task 6: external XMP backend qualification — 2026-10-03

## Status and authority

**Patched candidate passes isolated and production Debug/AddressSanitizer
qualification; final milestone checks remain pending.** Task 6 remains uncommitted and its
required full verification is incomplete. This is development evidence, not a
release or milestone-completion claim. The initial failed candidate results below
are retained; the approved patch results follow them.

The user approved bounded qualification of Exiv2 0.28.9 with Adobe XMP Toolkit
SDK v2025.03, retaining the public Exiv2 API. Adoption was conditional on the
namespace reproducer, preservation tests and sanitizer checks passing. The user
subsequently approved retrying the guard-blocked build with the verified literal
CMake executable; the guard remained enabled.

Baseline HEAD: `89ab452dd3c2fd4a0a13a045d691b323ce6571fe`.
HISEW execution: `cb290bb9-5f95-4b46-8af8-fbed85498c1b` (R2).

## Initial candidate and isolated integration (historical)

- Adobe tag `v2025.03`, commit
  `581c41213ddcee1fbc72cbb532531102a6617a25`; native XMPCore static build only.
- Exiv2 0.28.9 and Expat 2.8.5, reusing the qualified dependency sources.
- MSVC v145 / 14.51.36231; Windows SDK 10.0.28000.0; Debug x64, dynamic CRT.
- SDK BSD-3-Clause and bundled third-party notices remain with the research
  checkout. Distribution notice/package integration has not been completed.
- All candidate outputs stay under `artifacts/research/task-6`. No production
  manifest, lock, triplet, library or build-policy file was replaced.

Integration observations, distinguished from behavioral test failures:

1. Corrected a stale source path and quoted PowerShell's dotted CMake values.
2. Configured the SDK's native Expat build with `XML_GE=1`, the upstream default.
   Adobe compiles these sources as C++, so the definition belongs in its C++
   extra-flags setting as well as the C setting. Parser source was unchanged.
3. Propagated isolation properties into CMake's native try-compile projects;
   otherwise repository-wide compiler policy leaked into dependency probes.
4. Supplied Exiv2's native `XmpSdk` package discovery contract with a real static
   imported target, Windows/static definitions and its real Expat dependency.
5. Selected the external SDK's headers ahead of bundled SYSTEM headers, including
   its matching template implementation and MD5 declaration. Mixing versions
   caused compile and link failures.
6. In a separate Exiv2 research source copy, changed one MD5 call from the const
   accessor to the mutable accessor of the existing owned buffer, matching the
   selected Adobe API. This is a candidate source patch, not unmodified-upstream
   qualification. No wrapper, custom parser, or production-cache edit was used.
7. Linked the isolated test harness with the actual production and test sources,
   Catch2, and the existing JPEG/PNG/zlib dependency closure.

## Observed behavioral results

Use `[jpeg][metadata] --rng-seed 1287842626` on each executable.

| Candidate | Result | Failing boundary |
| --- | --- | --- |
| Existing bundled backend | 21 cases: 20 passed, 1 failed; 295 assertions: 294 passed | `Exiv2::XmpProperties::registeredNamespaces` throws `Fatal namespace map problem` after the combined fixture sequence |
| External Adobe backend, original assertions | 21 cases: 20 passed, 1 failed; 297 assertions: 296 passed | French rights text is replaced by the default-language rights text |
| External backend, strengthened input oracle | 21 cases: 20 passed, 1 failed; 284 assertions: 283 passed | The native Exiv2 decode already loses the French text, before reconciliation |

The strengthened maintained test checks both literal language values immediately
after decoding. It does not redefine expected preservation to match the parser.
The source fixture and expected French text remain unchanged.

A smaller isolated native probe then called Adobe `ParseFromBuffer` and
`GetLocalizedText` directly, and separately called Exiv2 decode on the same
literal packet. Both returned the wrong French value (6 assertions, 2 failed,
seed 4013164110). Thus the observed loss is not caused by our reconciliation
writer or Exiv2's language-alternative extraction.

## Root cause and specification boundary

Adobe `XMPCore/source/XMPCore_Impl.cpp`, `NormalizeLangArray`, explicitly replaces
the non-default entry with `x-default` when the array has exactly two entries.
Its comment describes compatibility with older Adobe applications. The function
is reached during parsing and serialization. This is intentional upstream
normalization behavior, not evidence that our source bytes have been preserved.

The [official XMP language-alternative definition](https://developer.adobe.com/xmp/docs/xmp-namespaces/xmp-data-types/)
requires unique language qualifiers and requires an existing `x-default` entry
to be first. It **recommends**, rather than requires, repeating the default value
under its actual language. The fixture satisfies the mandatory conditions but
does not follow that recommendation. That distinction does not authorize us to
silently overwrite existing rights information. Adding an English item just to
make this test pass would conceal the retained preservation counterexample.

Other authorities:

- [Adobe SDK release](https://github.com/adobe/XMP-Toolkit-SDK/releases/tag/v2025.03).
- [Pinned normalization implementation](https://github.com/adobe/XMP-Toolkit-SDK/blob/v2025.03/XMPCore/source/XMPCore_Impl.cpp).
- [Exiv2 native external-SDK integration](https://github.com/Exiv2/exiv2/blob/v0.28.9/src/CMakeLists.txt).
- [Expat native configuration](https://github.com/libexpat/libexpat/blob/R_2_8_5/expat/CMakeLists.txt).

## Disposition and retention

The external backend fixes the retained namespace sequence but fails the
preservation gate. Do not adopt it, suppress the failure, relax the oracle, or
claim sanitizer qualification. Sanitizers were not run on this failing candidate.

At this initial decision point, a further native-library behavior patch or a
different native API strategy required explicit approval. The user subsequently
approved the narrowly described patch below. No upstream issue or patch has been
published; that is a separate external action.

## Approved native preservation patch

After receiving the explanation of the exact native behavior, maintenance cost,
Exiv2 integration adjustment, and conditional adoption gate, the user replied
"I approve" on 2026-10-03. This authorizes implementing and testing the patch;
it does not waive any qualification or production integration check.

The retained patch is
[`preserve-localized-text.patch`](../../vcpkg-ports/adobe-xmp-core/preserve-localized-text.patch),
SHA-256 `c9c00209390ac1e540574815554b387061873374cdbe0d0a3889d79375a746c3`.
It removes only the two-entry value-copy operation from `NormalizeLangArray` and
corrects its explanatory comment. Default-item ordering, existing qualifier
checks, namespace handling and public APIs remain unchanged. The retained name
still denotes language-array normalization; no new API or alias is introduced.

Test-first evidence on the isolated candidate:

1. Replayed `[native-rights-probe] --rng-seed 4013164110` before mutation:
   6 assertions, 2 failed, at both direct Adobe and Exiv2 preservation boundaries.
2. Applied the native patch and rebuilt XMPCore and the actual-source harness.
   The combined metadata/native probe passed: 305 assertions in 22 cases,
   seed 1287842626. This includes the previously failing namespace sequence.
3. The complete JPEG harness passed: 837852 assertions in 66 cases, same seed.
4. Added maintained characterization coverage for default-first, default-last,
   and three-entry language alternatives. Literal French/default values are
   checked before serialization, and serialized default-first ordering is
   checked independently. These are passing characterization checks, not an
   invented additional RED cycle.
5. Rebuilt and ran the complete harness after that test addition: **837879
   assertions in 67 cases passed**, seed 1287842626, in both **Debug x64** and
   **Release x64 with AddressSanitizer**. The native diagnostic probe accounts
   for one case; the remaining cases compile the repository's current tests.
6. The sanitizer build instruments Adobe XMPCore (including its Expat sources),
   Exiv2's two native targets, and the application/test sources, and links the
   existing instrumented dependency graph. Generated compile settings were
   inspected for `/fsanitize=address`. No sanitizer finding was reported.
   The matching MSVC runtime was made available through process-local PATH only.
7. `git apply --check --reverse` succeeds against the patched SDK checkout,
   confirming the retained patch matches the tested native source change.

The ASan Exiv2 configure initially failed because its native filesystem probe
could not load the ASan runtime (Windows status `0xc0000135`). Providing the
selected compiler's runtime directory and rerunning the native probe resolved
that environment failure. It was not a behavioral RED or a waived check.

### Native package integration

The two approved ports now reside under `vcpkg-ports/`. vcpkg applies their
retained patches and installs real CMake package exports; the Adobe port links
the registry's Expat package rather than compiling a second bundled XML parser.
Both Debug and Release native package builds and vcpkg post-build validation
passed. The installed-package qualification harness passed 837879 assertions in
67 cases. The actual repository MSBuild Debug x64 project then passed 837873
assertions in 66 cases (before the subsequent analyzer regression addition),
seed 1287842626. No isolated research binary was copied into the production graph.

The policy permits only the exact ordered two-path overlay array. Its negative
controls pass for scalar/object/null/empty values, reordered/duplicated paths,
extra paths, wrong case and wrong element type. Existing alternate-authority,
strict JSON and mutually exclusive representation controls also pass.

Freshness contract RED: the old verifier rejected a selected local Exiv2 #1
because Microsoft's registry contained only #0. GREEN separates the selected
local revision from upstream source-version ordering, rejects disagreement with
the local manifest, and reports newer official Adobe releases. The complete
freshness contract suite and live official-source check pass. This follows
[vcpkg overlay precedence](https://learn.microsoft.com/en-us/vcpkg/concepts/overlay-ports),
not an assumption that a root override controls an overlay's version.

Task 6 analyzer follow-up RED: extension-only orientation 6 was accepted as an
identity image (seed 3146454621, one failed assertion). The analyzer's existing
extension gate now rejects extension-held orientation before accepting a
provisional identity plan. The affected metadata suite passes 331 assertions in
23 cases, seed 1287842626. This preserves the no-partial-extension-rewrite policy.

### Analyzer and reconciliation follow-up evidence

These are the same accepted R2 Task 6 slice (steps 6.2–6.4), not new product
scope. The oracle is the plan's no-silent-loss rule, native libjpeg transform
geometry and format-defined metadata types/boundaries. Focused tests exercise
the real analyzer/reconciler and literal/native-validated fixtures.

| Boundary | Observed RED | GREEN or characterization |
| --- | --- | --- |
| Thumbnail-removal review finding | Analyzer returned zero findings for a literal RGB JFXX preview; seed 917427617 | Reconciliation now returns owned bytes plus the actual preview-removal effect; the analyzer projects the typed finding. Exif, XMP, JFIF/JFXX and Photoshop share this result contract. |
| Single-component transform geometry | Relative 2x2 factors produced a 16x16 plan; seed 3543153388 | Uses the codec's 8x8 single-component unit. Native coefficient read proves the altered grayscale fixture still has 6x3 blocks. |
| Malformed XMP dimensions | `forty-eight` was silently overwritten; seed 1142732770 | Existing native-value integer validation rejects malformed dimensions before rewriting. |
| Aggregate output metadata budget | A framed comment exceeded the supplied budget but succeeded; seed 3368250660 | Check occurs before appending serialized bytes and reports the exact observed/maximum byte counts. |
| Nested JFIF in JPEG-coded JFXX | Reconciliation accepted the forbidden nested marker; seed 3631113986 | Reuses scanner-proven marker ranges to reject nested JFIF/JFXX identifiers. |
| New domain error diagnostic | `MetadataPreservationFailed` rendered as invalid; seed 1177429834 | Focused domain diagnostics pass 40 assertions in two cases. |

The source for grayscale geometry is
[libjpeg-turbo 3.2.0 `jtransform_request_workspace`](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/3.2.0/src/transupp.c);
the nested-preview rule comes from
[JFIF 1.02, JPEG-coded thumbnail extension](https://www.w3.org/Graphics/JPEG/jfif.pdf).
The latest focused metadata pass before final maximum-payload characterization
was 376 assertions in 27 cases, seed 1287842626. Initial test-compilation mistakes
(a guessed member name, missing include, and unused seam parameter) were corrected
before observing the behavioral RED results above; they are not counted as RED.

After maximum-payload characterization and formatting, the focused metadata
suite passes **384 assertions in 28 cases**, seed 1287842626. The production
ARM64 Debug cross-build also succeeds, including both native overlay ports.
This host has not executed the ARM64 binaries.

### Focused independent-assessment follow-up

The read-only independent assessment inspected staged tree
`aa8fa7e8cbc1864fbc725173cce850f5ae4b1bbe`. Its raw output and candidate patch
are retained under `artifacts/research/task-6/`. No second broad review was run.

- Freshness checking now accepts the same standalone vcpkg configuration as
  repository policy. The new positive control failed before the fix; the
  freshness contract suite passes afterward. Embedded and standalone
  representations remain mutually exclusive.
- An opaque MakerNote with a real thumbnail demonstrated that intrusive native
  TIFF rewriting could be accepted without proving offset safety (RED seed
  657232599). Reconciliation now rejects that combination. The native Exiv2
  interpretation distinguishes recognized MakerNotes; a Canon serial-number
  characterization confirms preservation through thumbnail removal. This does
  not qualify every camera format or justify ignoring layout-dependent fields.
- A combined fixture exercises actual APP1 framing with Exif, XMP, four IPTC AI
  properties, rights, previews, Photoshop IIM, split ICC, a comment and an opaque
  marker. Native namespace lookup resolves XML prefix aliases. An incorrect
  prefix in the draft test was corrected; it was not a production preservation
  failure or behavioral RED.
- Overlay recipe contents are controlled by source review and native vcpkg
  source-hash, patch-application and build checks. The repository policy checks
  the approved overlay selection; it does not authenticate every recipe byte.

After these follow-ups and formatting, the x64 Debug metadata suite passes
**444 assertions in 31 cases**, seed 1287842626. This focused result does not
replace final governed verification or sanitizer verification of the new edits.

### Final acceptance still pending

Governed run `868ef69a-e34a-4c54-8089-092114dc1548` stopped in HISEW's Windows
process observer with `win32process.QueryFullProcessImageName` unavailable.
Policy completed; the build capture and remaining checks did not establish a
complete run. Native recovery subsequently established producer quiescence and
preserved this failed result. The local HISEW repair and test evidence were
handed off in issue 116; that repair is paused, not installed. Task 6 resumed at
execution generation 3 on the user's request. Do not bypass verification or
modify the installed engine directly. Task 6 remains uncommitted.

The resumed production AddressSanitizer suite passed **837979 assertions in 74
cases**, seed 2790692799, including the MakerNote and combined-marker follow-ups.
This supersedes the earlier sanitizer results for those source edits.

The scripted build also needed bounded MSBuild worker lifetime: its parallel
invocation left node reuse at Microsoft's default (workers remain alive after
the build). A native PowerShell-AST argument contract failed before adding
`-nodeReuse:false`. This follows Microsoft's scripted-build guidance and changes
worker retention, not build outputs or verification acceptance. The contract is
static evidence; the governed real build must independently establish completion.

The production sanitizer graph also passed `[jpeg]`: 837866 assertions in 66
cases, seed 2699698556, before the subsequent preview-reporting and validation
changes. That result qualifies the native backend but is not final evidence for
the later source edits. Other architectures, Store packaging and remaining
Task 6 requirements are not established by that result. Complete the applicable checks before committing
the metadata milestone. Do not mark the HISEW execution complete here.

Task owner: this implementation session. Retain the ignored SDK/source copies,
isolated package configuration, CMake caches and native probe for the decision
and reproducer. Reassess/remove their disposable build outputs after the next
backend decision is validated; retain the evidence and maintained regression
test. The pre-existing untracked `tmp/` remains untouched.
