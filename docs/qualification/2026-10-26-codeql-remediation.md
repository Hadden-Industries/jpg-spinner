# CodeQL remediation and remaining diagnostics

## Evidence boundary

The manually dispatched [CodeQL run 40367943262](https://github.com/Hadden-Industries/jpg-spinner/actions/runs/40367943262)
completed successfully on source `76f8e468be446a7a5978d4ecefc1572eaf9dbf85`.
Analysis `2030274848`, category `cpp-security-extended`, has nine results,
down from 24 in the preceding analysis. Fifteen findings disappeared through
source/configuration repairs, with no alert dismissals or query exclusions.

The subsequent local zlib repair is signed checkpoint
`6e8e6d6914058a81e1205c1efa834440020d42b6`. It is not included in that remote
analysis. Neither a passing workflow nor the local tests below establish that
every GitHub alert is resolved. No further scan, publication, historical-analysis
retirement, Store submission or release is implied by this document.

## Confirmed allocator defect: alert 8

The installed zlib 1.3.2 allocator multiplied two `unsigned` arguments before
passing the result to `malloc(size_t)`. Two requests whose mathematical products
exceed the process's addressable range wrapped into tiny successful allocations.
The installed-library reproducer failed on the original archive while its normal
allocation and binary codec controls passed.

The repair passes the element count and size separately to the native `calloc`
implementation. Microsoft's [CRT contract](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/calloc?view=msvc-170)
defines array allocation and allocation failure. This avoids narrow multiplication
at the call site and adds no custom allocator, parser or compatibility shim.

The overlay retains Microsoft's pinned port recipe, upstream version/archive
hash, license and native package interfaces. Only its local revision changes to
`1.3.2#3`. Root pin, overlay identity, JSON types and source-release freshness are
checked independently; a private packaging revision is not an upstream release.

The reproducer queries Windows' process address limit before attempting either
large request. It refuses an unsupported host rather than risk a real multi-GB
allocation. This demonstrates the native allocator defect, not a crafted-photo
exploit or universal application reachability.

Validation includes:

- Original installed allocator: RED; normal allocation/codec control passed.
- Repaired installed allocator: both impossible requests refused; normal control
  and compression/decompression round trip passed.
- Affected x64 Debug build and 186 JPEG/metadata cases: passed.
- Freshness contract and scoped native dependency policy negative controls:
  passed, including refusal of the original port revision and missing overlay.

## JPEG arithmetic diagnostics: alerts 6 and 7

These remain open. No overflow was reproduced for accepted JPEG input, and no
vendor source change is claimed for them.

For alert 6, the private entropy decoder receives the remaining MCU count from
[`jddiffct.c`](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/3.2.0/src/jddiffct.c).
[`jdinput.c`](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/3.2.0/src/jdinput.c)
checks image dimensions and sampling factors before configuring the scan. The
selected library defines `JPEG_MAX_DIMENSION` as 65,500 and `MAX_SAMP_FACTOR` as
four. Under those admitted-input invariants, the reported count-times-MCU-width
subexpression is at most 262,000, within its unsigned type.

For alert 7, [`jdmaster.c`](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/3.2.0/src/jdmaster.c)
selects scaled DCT sizes between one and 16. With sampling factors at most four,
the row-group value is conservatively bounded by 64. The integer-only part of
the reported pointer-list extent is bounded by `2 * 64 * (16 + 4) = 2560`;
the subsequent `sizeof` multiplication uses the wider allocation type.

The native codec probe decodes three maintained fixture sampling modes at every
scale from 1/8 through 16/8. It also encodes/decodes a maximum-width, two-row
lossless image and exercises premature entropy termination with independent row
guards. Valid lossless output must preserve exact pixels; malformed entropy is
checked for bounded recovery, without inventing a pixel-fidelity guarantee.

These are bounded source arguments and native behavioral controls, not proof
against arbitrary corruption of private decoder state. Exact instruction/branch
coverage was not collected. Retain the diagnostics and those limits rather than
apply an unneeded source patch solely to lower an alert count.

## PNG pass diagnostic: alerts 15 and 16

These remain open. In libspng 0.7.4, [`calculate_subimages`](https://github.com/randy408/libspng/blob/v0.7.4/spng/spng.c)
sets the last nonempty pass from a loop over seven subimages. In
`update_row_info`, reaching that last pass returns end-of-image before the next
increment. Otherwise the increment cannot exceed the last pass. Each further
increment is guarded by the same upper bound. Thus the reported array access
precedes a local check but is constrained by an earlier invariant in the examined
path.

The native probe encodes and progressively decodes interlaced RGBA images of
sizes 1, 2, 3, 8 and 17. It verifies actual pass/row ranges, bounded completion
and exact pixels, covering sparse and fully populated Adam7 layouts. It follows
the upstream [progressive-decoding example](https://github.com/randy408/libspng/blob/v0.7.4/examples/example.c),
including end-of-image returned by either row-information or row decoding.

No PNG parser was added. These tests characterize existing native behavior;
they do not claim that reordering a condition fixed a reproduced vulnerability.

## Generated WinRT helper: alerts 30 through 33

The latest stable `Microsoft.Windows.CppWinRT` package checked for this work is
`3.0.260818.1`; Microsoft's observed [native array implementation](https://github.com/microsoft/cppwinrt/blob/30ac599beedc03d0f38f5fc84c390ee2f8551204/strings/base_array.h)
retains the same operation. `array_size_proxy` advances one pointer-sized word
to update the receive-array count within a `com_array` object. It is locating an
internal count field, not advancing to the next complete `com_array` object.
This explains the size mismatch reported by the general pointer-scaling rule.

The existing native characterization cases exercise real Windows Runtime byte
and GUID receive arrays, including replacement of a previously populated byte
array and independent verification of counts, bytes, interface IDs and boxed
values. Do not replace this with a replica of Microsoft's layout arithmetic.
Both x64 Debug and Release executions passed the two cases and nine assertions;
the Release run uses the optimized configuration relevant to this analysis.

No generated `base.h`, SDK package cache, analyzer query or runtime shim was
changed. The ABI tests are behavioral evidence for the reported instantiations,
not a universal proof about every WinRT type or platform. A supplier/analyzer
resolution remains separate from changing this application's implementation.

## Retired configuration: alerts 1 through 5

These reference the deleted `JPG Spinner/` sources under analysis key
`dynamic/github-code-scanning/codeql:analyze`, category `/language:c-cpp`.
Its last analysis is based on historical source `e6c17b6`; the current maintained
workflow uses a different analysis key and category. Current repository policy
confirms that the retired sources remain absent.

No deleted source is restored to satisfy those historical records. They need
supported obsolete-configuration administration, not another application patch.
No historical analyses were deleted or alerts dismissed during this work.

## Reproduction and ownership

The maintained entry points are:

- [Allocator probe](../../scripts/tests/Test-ZlibAllocation.ps1).
- [Codec boundary probe](../../scripts/tests/Test-CodecBoundaries.ps1).
- [Adobe conversion probe](../../scripts/tests/Test-AdobeXmpConversions.ps1).
- [WinRT byte/GUID cases](../../tests/JpgSpinner.WindowsStorage.Tests/WinrtArrayProjectionTests.cpp).

The three standalone probes share
[one native runner](../../scripts/NativeDependencyProbe.psm1), selected MSVC
toolchain, vcpkg package discovery and existing process/time-budget machinery.
They consume installed packages with manifest restoration disabled and use a
fresh caller-owned output directory. They do not modify Windows settings or
installed apps.

Original logs, including unsuccessful harness attempts, are held under native
HISEW inventory `66c6ae4a10e646f38ab171702773647d`, originating execution
`1f8e9d2c-38ef-4f91-a9d3-d114332d547e`. Required original evidence must be
promoted/read back before spent build outputs are disposed through the native
resource lifecycle. An uncertain or blocked cleanup is not a passing result.

The next implementation decision must distinguish a reproducible defect from
an already guarded vendor operation. No dismissal, filter, generated-header edit
or altered scan identity should substitute for that decision.
