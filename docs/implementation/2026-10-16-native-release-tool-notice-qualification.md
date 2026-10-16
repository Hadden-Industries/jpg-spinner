# Native release-tool notice collection qualification

## Decision and scope

Status on 2026-10-16: bounded native collection and Windows Go-module report
coverage are demonstrated; non-Go input coverage, maintained integration and
rights clearance are **not qualified**.
This advances [Task 18](../plans/2026-08-30-jpg-spinner-modernization.md#task-18-document-the-shipped-system-and-remove-secret-bearing-telemetry),
not application runtime dependencies or release approval. The source baseline is
signed commit `904bb2385a3f1b4e6e39833eea7e5697be0756eb`. NuGet notice
preservation is a separate input inventory, not qualification of this tool.

The desired outcome is a reproducible collection of original notices for the
[native SBOM producer](../../eng/artifact-sbom/go.mod), without inventing a
license classifier, module resolver or legal conclusion. Go's own dependency
inventory identifies inputs but does not collect their notices. Syft's component
inventory is not a replacement for original terms. Manually selecting filenames
risks omitting subordinate notices. The experiment therefore uses a maintained
native collector, retains its actual output and independently checks preservation.

The selected collector is [google/go-licenses v2.0.1](https://github.com/google/go-licenses/releases/tag/v2.0.1),
the latest stable release checked on 2026-10-16. Its own
[tagged guidance](https://github.com/google/go-licenses/blob/v2.0.1/README.md)
states that it is not an officially supported Google product. Its public
`save` and `report` commands collect documents and produce a library-level CSV;
they do not qualify every non-Go dependency. No maintained product pin or new
application dependency is introduced by this experiment.

## Reproduction identity

The tool was built with the already qualified Go 1.27.2 runtime in an isolated,
bounded tool workspace. No system-wide installation or persistent configuration
change was made. Native acquisition used the exact release, not floating latest:

```text
go install -p=2 github.com/google/go-licenses/v2@v2.0.1
```

Source commit: `3e084b0caf710f7bfead967567539214f598c0a2`.
Native module checksum: `h1:ti+9bi5o7DKbeeg5eBb/uZTgsaPNoJaLCh93cRcXsW8=`.
Native go.mod checksum: `h1:efibo0EDNGkau6AIMOViGW+rTNPudhxX9rCxtfw5zKE=`.
Locally built executable SHA256:
`CCC617D26DFE2EEA382D0E6BE59130825F2B3C485B7F7EC1FAD677E14FB764AD`.
These identify this build, not a maintainer-published Windows executable.

The actual project input identities remained unchanged:

- [go.mod](../../eng/artifact-sbom/go.mod):
  `0F4C94A27430E60C989DBED5AB8328FF28A629925350A6E74CB5CBA268FAC2F7`.
- [go.sum](../../eng/artifact-sbom/go.sum):
  `77851DA9D4CEB54ADD0FC3037E6A5D362D66618863AE02F0E1620A233EF856F6`.

## Actual observations

An importing fixture used a real local dependency with a full MIT license.
Native collection returned zero and preserved that license's exact SHA256.
An otherwise equivalent unlicensed dependency produced a native nonzero
unknown-license refusal. Reusing the positive output directory without `--force`
also refused and left the collected license unchanged. These characterize
existing native behavior; they are not new product RED/GREEN test chronology.

Against the actual `eng/artifact-sbom` module, the maintained
[process boundary](../../scripts/ArtifactToolProcess.psm1) ran both commands with
bounded concurrency/memory, an existing qualified module cache, offline module
resolution and read-only module inputs:

```text
go-licenses save . --ignore github.com/Hadden-Industries/jpg-spinner/eng/artifact-sbom --save_path <fresh-owned-output>
go-licenses report . --ignore github.com/Hadden-Industries/jpg-spinner/eng/artifact-sbom
```

Both commands returned zero. The native CSV contains 179 library rows; the
collection contains 970 files totaling 5,574,544 logical bytes. Library rows are
not module counts or an assertion that every locked module contributes code.
Only the owned main module was ignored; no third-party package was excluded to
make collection succeed.

The unmodified report classifies `github.com/spdx/tools-golang` as GPL-2.0. That
selected module's original [LICENSE.code](https://github.com/spdx/tools-golang/blob/v0.6.0-rc4/LICENSE.code)
instead offers an Apache-2.0 OR GPL-2.0-or-later choice. Preserve both artifacts.
The classifier label must not silently select a license, erase the alternative
or be presented as a GPL-only rights conclusion.

Native warnings identify non-Go and assembly inputs; these require separate
review. The initial composed dependency-comparison command was refused by the
command guard before execution; that failed attempt remains historical. The
owner subsequently requested simpler explicit commands and authorized running
them. The maintained bounded process runner completed native `go list -deps`
with the same module template, offline cache and read-only module inputs.
PowerShell's native line reader and CSV reader compared the saved output:
all 160 distinct external contributing module identities have entries among
the 179 native license-report rows, with no uncovered module and nonempty-input
checks. This covers the selected Windows Go graph, not every source file, other
build targets, embedded assets or the collector's non-Go dependency blind spots.
No guard exception/configuration change, custom parser or output-rewriting shim
was required. Module membership is not legal clearance or final tool adoption.

## Evidence and remaining work

The native notice output was packed and extracted with .NET ZipFile. The
maintained [payload-manifest producer](../../scripts/New-PayloadManifest.ps1)
verified identical relative paths, lengths and hashes for all 970 files before
and after extraction. The 1,577,934-byte archive has SHA256
`21C8E38EFBE27556662AB3853142BB62B24E0703620E0818A5CB9024E845A392`.
It is retained externally, not committed as a generated binary.

External inventory `b2848f1d05f9459e9ac1597d7fc5b8bb` identifies original raw
logs, fixtures, manifests, archive and promotion state under execution
`352c2077-8e60-441f-ac25-4b13a115a082`. A promotion intent or summary is not a
successful byte-readback receipt. Initial broad observations exceeded the native
bound; their failed results remain. Released collector installation and cache
directories, totaling 265,208,438 logical bytes, were subsequently recycled.
After narrowing that working footprint, native promotion checkpoint
`38d5e963faec42f3afbd3e22fcbd9c63` verified all 19 selected source-to-evidence
mappings with no missing evidence. Deduplicated retained payloads total
1,783,538 logical bytes; this is preservation evidence, not collection coverage.
Evidence and required inputs were not removed, and no reclaimed-capacity claim
is made. The Task 18 owner reassesses the retained group after coverage and
non-Go/dual-license review, not at an arbitrary future date.

Before maintained integration, resolve non-Go/embedded/standard-library input
obligations and repeat the module comparison for any newly selected build target.
Then qualify exact original terms and distribution obligations, including any
source delivery, with the accountable rights reviewer. Do not treat collector
success, an SPDX label, this document or a signed source commit as that approval.
The [release register](../release-qualification.md) remains not qualified.

## Native non-Go input inventory (2026-10-17)

At signed source `ff14e258181d54e78469040f62166d11f3e489cd`, an offline native
`go list -deps -mod=readonly -p=2 -f <native-template> .` inspection of the same
unchanged `eng/artifact-sbom` module narrows the collector's non-Go blind spot.
Go's [documented Package fields and template renderer](https://pkg.go.dev/cmd/go#hdr-List_packages_or_modules)
own target selection and report generation; no custom import graph, JSON stream
parser, license classifier or application dependency was introduced.

The explicit retained Go 1.27.2 executable was used, not the unrelated system
Go 1.27.0 found during read-only discovery. Native environment output confirms
`windows/amd64`, `CGO_ENABLED=0`, the existing qualified module cache and a fresh
owned loader cache. `GOPROXY=off`, `GOSUMDB=off`, `GOTOOLCHAIN=local`, `GOWORK=off`
and `GOENV=off` prevent network resolution/toolchain switching; loading uses
read-only module mode, two native processes and a 90-second process deadline.
No compiler, linker, generator, package installation, app launch or CI ran.
Both original module-file hashes above remained unchanged.

The native report selects 48 packages with assembly, native-source/header or
embedded-file fields: 34 standard-library packages and 14 external packages
from nine modules. Forty-four packages have assembly fields; two have embedded
files. These are package-loader input declarations, **not linker-contribution
proof**: a header listed by the loader need not be compiled in this target.

| External input owner | Selected non-Go/embedded material |
| --- | --- |
| `github.com/klauspost/compress` v1.20.1 | Assembly in cpuinfo, huff0, zstd/xxhash, zstd and s2 |
| `github.com/pierrec/lz4/v4` v4.1.26 | lz4block assembly |
| `github.com/cespare/xxhash/v2` v2.3.0 | Assembly |
| `golang.org/x/crypto` v0.57.0 | poly1305 and chacha20poly1305 assembly |
| `golang.org/x/sys` v0.48.0 | CPU assembly |
| `github.com/minio/minlz` v1.0.1 | Assembly |
| `github.com/envoyproxy/protoc-gen-validate` v1.3.3 | `validate.h` declaration |
| `google.golang.org/protobuf` v1.36.12 | Embedded `editions_defaults.binpb` |
| `github.com/anchore/syft` v1.54.1 | Embedded `dictionary/data/cpe-index.json` |

Upstream [protobuf source](https://github.com/protocolbuffers/protobuf-go/blob/v1.36.12/internal/editiondefaults/defaults.go)
declares its embedded binary. Syft's [tagged generation guidance](https://github.com/anchore/syft/blob/v1.54.1/syft/pkg/cataloger/internal/cpegenerate/README.md)
identifies the CPE dictionary's NVD origin and offline generation. Preserve and
assess those data origins and their applicable rights separately; the enclosing
module's license label alone is not a complete data-rights decision. No NVD
request or index regeneration was performed.

Original Go runtime `LICENSE`, `PATENTS`, and its vendored x/crypto and x/sys
licenses were copied without re-encoding and independently hash-matched.
The three license copies have SHA-256
`911f8f5782931320f5b8d1160a76365b83aea6447ee6c04fa6d5591467db9dad`;
the original patent grant has
`96f408bfae65bf137fc2525d3ecb030271c50c1e90799f87abf8846d8dd505cc`.
These are release-tool inputs, not notices falsely assigned to the image app.

External inventory `cf6934efd4d64567928ad00892f5635b`, execution
`352c2077-8e60-441f-ac25-4b13a115a082`, retains the wrapper, native target/output
streams and original copied terms. Retain the earlier collector's warnings and
dual-license counterevidence. This inspection does not establish complete
standard-library/embedded-data notice coverage, rights clearance, final binary
contribution or maintained collector adoption. The Task 18 owner must assess
the identified original files/data and repeat target-specific inventory when
the native runtime, dependency graph or target changes. Release the fresh loader
cache after evidence preservation; do not remove the shared qualified inputs.
