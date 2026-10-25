# Supplied CodeQL result dispositions

This preserves the original static triage for SLICE-002 of the [accepted release replan](../plans/2026-10-25-release-completion-replan.md).
The [authorized native analysis](https://github.com/Hadden-Industries/jpg-spinner/actions/runs/40179840428)
ran on `435b87b36bc3fe63f40fa09132945edc6edf83bc` with CodeQL 2.27.3 and produced
29 results. Five additional open alerts belong to the historical default category
at `e6c17b6`. Local triage uses signed `057fc66cda925e0e6a37d08b83746e630ef0897b`;
product/test source, native dependency configuration and build controls have no
diff from the reported manual-analysis revision. No new scan, security review,
runtime validation, delegation or external review was performed.

## Scoped outcome

**Subsequent fix-first decision (2026-10-25):** The maintainer approved
[durable Exiv2 source repairs](2026-10-25-exiv2-source-repairs.md) before any
administrative dismissal. The original static triage and raw reports below
remain historical. Its blanket "no source patch" recommendation is superseded:
absence of a demonstrated product exploit does not settle build correctness or
the best native buffer contract. Keep the proposed batch dismissals unexecuted;
new local repair evidence does not itself change GitHub's alert state. The later
authorized native analysis below supplies the actual post-repair state.

## Native analysis after the approved repairs

The [completed native CodeQL run](https://github.com/Hadden-Industries/jpg-spinner/actions/runs/40221237300)
analyzed signed `74f168b3912bd1bedf706e3baf0167f9c5edd1c9` with CodeQL 2.27.3.
Analysis `2023185530`, category `cpp-security-extended`, reports 24 results from
95 rules. GitHub marks **#9, #10, #12, #13 and #14** fixed without dismissal at
`2026-10-25T19:43:40Z`. These are the five actual source-repaired findings.

Old #11 closed while [#35](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/35)
opened for the same FileIo expression at `basicio.cpp:413`. Only the vcpkg
source-directory hash changed; original and revised source bytes have the same
SHA-256, `89d465f195320eb683d7b20f58c3089b05596edd282e10af9cd62bf366feae44`.
This is an observed relocation, not another repaired source defect. The original
product-boundary assessment still applies to that unchanged observation; the
upstream FileIo operation is not declared fixed.

The actual post-analysis snapshot has 29 open alerts: 24 current manual results
and five historical default-configuration results. No alert dismissal, query
suppression or analysis deletion occurred. The
[hosted qualification record](2026-10-25-exiv2-hosted-qualification.md)
identifies the original provider responses, native traced-build evidence and
independently preserved diagnostics. This section reconciles those existing
results; it introduces no new review or scan. The original rows and managed
triage below retain their historical inputs and meaning.

## Original static assessment

Positive static evidence defeats the specific product-vulnerability claims for
all 29 current results under the pinned configuration. The five historical
locations are obsolete in current v2 source after signed retirement `e1eb2361`.
That does not declare their original claims false or validate an installed legacy
binary. No confirmed or needs-review queue remains for this bounded collection.
Charset dispositions have medium static confidence; the other claim-specific
conclusions have high static confidence.

No source patch, vendor fork, custom parser or query exclusion is justified by
these results. GitHub's original open states remain unchanged. Administrative
false-positive dispositions and historical-configuration retirement require a
separate deliberate maintainer decision. No release approval follows from triage.

## One disposition per input

All rows retain the original static assessment above. Separate generated instances and
Debug/Release probes remain separate inputs in their original order.

| Alert                                                                            | Exact reported location                                                                                                                | Positive counterevidence                                      |
| -------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------- |
| [34](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/34) | `vcpkg_installed/x64-windows-static-md/vcpkg/blds/adobe-xmp-core/src/02a6617a25-0c7e259fce.clean/source/UnicodeConversions.cpp:1419`   | Endian halfword selector is bounded to offset 0/1.            |
| [33](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/33) | `artifacts/obj/x64/Release/JpgSpinner.WindowsStorage.Tests/Generated Files/winrt/base.h:4295`                                          | Intentional ABI size-field store.                             |
| [32](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/32) | `artifacts/obj/x64/Release/JpgSpinner.Presentation.Tests/Generated Files/winrt/base.h:4295`                                            | Intentional ABI size-field store.                             |
| [31](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/31) | `artifacts/obj/x64/Release/JpgSpinner.WindowsStorage/Generated Files/winrt/base.h:4295`                                                | Intentional ABI size-field store.                             |
| [30](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/30) | `src/JpgSpinner.App/Generated Files/winrt/base.h:4295`                                                                                 | Intentional ABI size-field store.                             |
| [29](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/29) | `vcpkg_installed/x64-windows-static-md/vcpkg/blds/catch2/src/v3.16.1-fdb3c14baa.clean/src/catch2/catch_config.cpp:251`                 | Operator-selected Bazel test output.                          |
| [28](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/28) | `tests/JpgSpinner.WindowsStorage.Tests/TransactionProcessTerminationTests.cpp:184`                                                     | Synthetic output oracle under the parent-owned fixture.       |
| [27](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/27) | `tests/JpgSpinner.WindowsStorage.Tests/TransactionProcessTerminationTests.cpp:183`                                                     | Synthetic source oracle under the parent-owned fixture.       |
| [26](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/26) | `tests/JpgSpinner.WindowsStorage.Tests/TransactionProcessTerminationTests.cpp:182`                                                     | Synthetic camera input under the parent-owned fixture.        |
| [25](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/25) | `tests/JpgSpinner.WindowsStorage.Tests/TransactionProcessTerminationTests.cpp:275`                                                     | Generated corrected-copy readback.                            |
| [24](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/24) | `tests/JpgSpinner.WindowsStorage.Tests/TransactionProcessTerminationTests.cpp:273`                                                     | Generated backup readback.                                    |
| [23](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/23) | `tests/JpgSpinner.WindowsStorage.Tests/TransactionProcessTerminationTests.cpp:266`                                                     | Selected synthetic source recheck.                            |
| [22](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/22) | `tests/JpgSpinner.WindowsStorage.Tests/TransactionProcessTerminationTests.cpp:231`                                                     | Owned interrupted-stage snapshot.                             |
| [21](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/21) | `tests/JpgSpinner.WindowsStorage.Tests/TransactionProcessTerminationTests.cpp:221`                                                     | Selected synthetic source snapshot.                           |
| [20](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/20) | `tests/JpgSpinner.WindowsStorage.Tests/TransactionProcessTerminationTests.cpp:219`                                                     | Independent synthetic output oracle.                          |
| [19](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/19) | `tests/JpgSpinner.WindowsStorage.Tests/TransactionProcessTerminationTests.cpp:218`                                                     | Independent synthetic source oracle.                          |
| [18](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/18) | `vcpkg_installed/x64-windows-static-md/vcpkg/blds/adobe-xmp-core/src/02a6617a25-0c7e259fce.clean/XMPCore/source/XMPUtils.cpp:2373`     | Assignment count checked before return; failure throws.       |
| [17](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/17) | `vcpkg_installed/x64-windows-static-md/vcpkg/blds/adobe-xmp-core/src/02a6617a25-0c7e259fce.clean/XMPCore/source/XMPUtils.cpp:2398`     | Assignment count checked before return; failure throws.       |
| [16](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/16) | `vcpkg_installed/x64-windows-static-md/vcpkg/blds/libspng/src/v0.7.4-2d78cb2862.clean/spng/spng.c:3276`                                | Existing pass transition preserves the seven-element bound.   |
| [15](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/15) | `vcpkg_installed/x64-windows-static-md/vcpkg/blds/libspng/src/v0.7.4-2d78cb2862.clean/spng/spng.c:3276`                                | Existing pass transition preserves the seven-element bound.   |
| [14](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/14) | `vcpkg_installed/x64-windows-static-md/vcpkg/blds/exiv2/src/v0.28.9-64e6a3bd87.clean/src/convert.cpp:1484`                             | Native conversion creates UTF-16 in owned byte storage.       |
| [13](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/13) | `vcpkg_installed/x64-windows-static-md/vcpkg/blds/exiv2/src/v0.28.9-64e6a3bd87.clean/src/convert.cpp:1515`                             | Native conversion consumes even-length UTF-16 bytes.          |
| [12](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/12) | `vcpkg_installed/x64-windows-static-md/vcpkg/blds/exiv2/src/v0.28.9-64e6a3bd87.clean/src/convert.cpp:1505`                             | Native sizing consumes the same even-length UTF-16 bytes.     |
| [11](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/11) | `vcpkg_installed/x64-windows-static-md/vcpkg/blds/exiv2/src/v0.28.9-64e6a3bd87.clean/src/basicio.cpp:413`                              | FileIo transfer is absent from product payload APIs.          |
| [10](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/10) | `vcpkg_installed/x64-windows-static-md/vcpkg/blds/exiv2/x64-windows-static-md-rel/CMakeFiles/CMakeScratch/TryCompile-ps8e3x/src.cxx:6` | Trusted CMake diagnostic probe, not shipped app code.         |
| [9](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/9)   | `vcpkg_installed/x64-windows-static-md/vcpkg/blds/exiv2/x64-windows-static-md-dbg/CMakeFiles/CMakeScratch/TryCompile-j64xbj/src.cxx:6` | Separate Debug instance of the same configure probe.          |
| [8](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/8)   | `vcpkg_installed/x64-windows-static-md/vcpkg/blds/zlib/src/v1.3.2-c19a0bfd23.clean/zutil.c:302`                                        | Native stream initialization bounds allocation operands.      |
| [7](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/7)   | `vcpkg_installed/x64-windows-static-md/vcpkg/blds/libjpeg-turbo/src/3.2.0-6292819c57.clean/src/jdmainct.c:164`                         | Product component count is restricted to 1, 3 or 4.           |
| [6](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/6)   | `vcpkg_installed/x64-windows-static-md/vcpkg/blds/libjpeg-turbo/src/3.2.0-6292819c57.clean/src/jdlhuff.c:234`                          | Predictive-lossless coding is rejected before native use.     |
| [5](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/5)   | `JPG Spinner/jmemmgr.c:611`                                                                                                            | Source retired; original historical claim not declared false. |
| [4](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/4)   | `JPG Spinner/jmemmgr.c:609`                                                                                                            | Source retired; original historical claim not declared false. |
| [3](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/3)   | `JPG Spinner/jmemmgr.c:603`                                                                                                            | Source retired; original historical claim not declared false. |
| [2](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/2)   | `JPG Spinner/jmemmgr.c:601`                                                                                                            | Source retired; original historical claim not declared false. |
| [1](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/1)   | `JPG Spinner/jdmainct.c:186`                                                                                                           | Source retired; original historical claim not declared false. |

## Source and boundary evidence

The root [security policy](../../SECURITY.md#threat-model) was resolved for the
affected test file, generated header and existing artifact/vendor ancestors.
Its native image/metadata boundary is separate from trusted build/test operands.
No scoped SECURITY.md applies.

- **Adobe #34/#18/#17:** the pinned [Unicode loop](https://github.com/adobe/XMP-Toolkit-SDK/blob/581c41213ddcee1fbc72cbb532531102a6617a25/source/UnicodeConversions.cpp#L1397)
  selects a two-byte half of the current four-byte unit with initialized offset
  zero/one before advancing a whole input unit. The
  [integer converters](https://github.com/adobe/XMP-Toolkit-SDK/blob/581c41213ddcee1fbc72cbb532531102a6617a25/XMPCore/source/XMPUtils.cpp#L2356)
  check the assignment count; the included native macro actually throws on failure.
- **Generated headers #33–#30:** pinned C++/WinRT 3.0.260818.1 places a pointer
  before a 32-bit size. The proxy skips exactly that pointer and writes the size;
  changing the offset to a whole object would corrupt the intended ABI field.
  The exact local header is retained as SHA-256
  `61ff0d209ce00849c638be58a4e4d9551c30e7e441d8f2f487c65e2f169823d7`.
  Hosted `artifacts/obj` copies are absent locally; unchanged pins and the
  reported expression support static comparison, not invented hosted hashes or
  fresh ABI runtime qualification.
- **Test paths #29–#19:** the [parent harness](../../scripts/Invoke-TransactionTerminationQualification.ps1)
  creates an exclusive UUID fixture and sets each child's exact environment.
  Native children retain the caller's token, with no shell, alternate credentials
  or elevation. The [hidden tests](../../tests/JpgSpinner.WindowsStorage.Tests/TransactionProcessTerminationTests.cpp)
  consume deterministic synthetic inputs/recovery artifacts. No image-to-environment
  or app-to-test dispatch exists. Catch2's Bazel output is likewise a deliberate
  operator test operand; the app never invokes `Catch::Session`.
- **libspng #16/#15:** [subimage construction and pass progression](https://github.com/randy408/libspng/blob/v0.7.4/spng/spng.c#L3257)
  preserve `pass <= last_pass <= 6`. The last pass returns before increment;
  the skip loop increments only below that last pass. Its first array read is
  already bounded. Public row-info access returns a copy. The product also
  exposes no first-party PNG loader.
- **Exiv2 #14–#11:** the [Windows functions](https://github.com/Exiv2/exiv2/blob/v0.28.9/src/convert.cpp#L1471)
  perform actual native charset conversion using owned byte storage. Even UTF-16
  byte lengths become code-unit counts; output is sized before conversion.
  Microsoft's [forward](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar)
  and [reverse](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte)
  specifications define those units. The forward helper advertises more capacity
  than needed, but its unchanged input writes the queried count inside allocated
  bytes. This conclusion does not license using that count with different input.
  The separate FileIo precondition is absent: [MetadataReconciler](../../src/JpgSpinner.JpegTransformation/src/internal/MetadataReconciler.cpp)
  gives Exiv2 TIFF/XMP bytes, not filesystem paths or Storage authority.
- **Configure probes #10/#9:** [FindFilesystem.cmake](https://github.com/Exiv2/exiv2/blob/v0.28.9/cmake/FindFilesystem.cmake#L110)
  compiles or runs its small check according to cross-compilation status.
  Its narrow format/wide-path mismatch is a real diagnostic defect on the build
  process's own current directory. It is not linked into the app or exposed to
  image input. Do not mislabel it as always compile-only or as correct formatting.
- **zlib #8:** [stream guards](https://github.com/madler/zlib/blob/v1.3.2/deflate.c#L434)
  bound window bits to 15 and memory level to 9 before deriving sizes. Inflate/
  infback allocate one fixed state or a bounded window; copies preserve settings.
  Reviewed dynamic products fit unsigned arithmetic: at most 32,768 × 5 or
  65,536 × 2. The overflowing operand pair is not established by the generic
  allocator expression.
- **JPEG #7/#6:** the [scanner](../../src/JpgSpinner.JpegTransformation/src/internal/JpegSegmentScanner.cpp)
  accepts component counts 1/3/4 before native use, making the pointer-list
  intermediate at most eight. It rejects predictive-lossless coding before
  transformation/output decode. Lossless coefficient rotation does not select
  the reported predictive-lossless `jdlhuff` path.
- **Historical #5–#1:** the current tree contains neither the legacy source nor
  its solution. Original claims/analyses remain retained. Source obsolescence
  supplies no new conclusion about the old installed binary or trusted upgrade.

## Retained evidence and next step

The complete `triage-finding/v0` record contains 34 unique items in original
order under managed collection
`artifacts-1d52fbd832d59b2be4d62c6612cd34ef43c7a1d8efdd11229425b0d431bf202b`,
file `findings/2026-10-25-codeql-static-triage.json`, SHA-256
`e604a3807d0338eee42a0c8e74179268fd4d11f7c43d4fa03d445817664d3188`.
Its detailed Markdown and native source-identity manifest accompany it. Original
SARIF/API/job evidence remains in HISEW inventory
`e758767c80b6414887db035479dfdcbf`, checkpoint
`4e67670772e846588a1b235152148905`; exact source responses are tracked by
`956e226e2d28470bb67358531d382c15`, native checkpoint
`ee51b0765f4d44a1a05e3f97d096cea8`. Native readback verifies all 25 selected
source/provenance members. Saved-record reconciliation separately verifies all
34 original identities/order/locations and all 23 exact source hashes; it is
data reconciliation, not another security review or runtime validation.

No alert dismissal, analysis deletion, query suppression or new CodeQL dispatch
occurs. Continue the [release prerequisite register](../release-prerequisites.md)
before freezing the final candidate.
