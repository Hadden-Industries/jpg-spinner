# Approved package-payload comparison contract — 2026-10-15

Status: **approved for bounded qualification, not release-qualified**.
This explicitly reduces the former complete-payload byte-equality guarantee.
Unequal payloads do not become reproducible; every other release gate remains.

## Decision and closed policy

The user approved retaining Microsoft's shipping WinMD producer and all metadata
while qualifying this read-only comparison. A second explicit approval includes
compressed `Block/@Size` on the same changed WinMD blocks: the retained real pair
changes from 5,169 to 5,170 compressed bytes. Uncompressed `File/@Size` and
`LfhSize` remain strict. The [native alternative failed preservation](2026-10-15-native-winmd-merger-qualification.md)
and remains rejected, not adopted.

1. Build the same signature-verified clean commit independently twice. Preserve
   every original container, exact raw payload manifest and toolchain/provenance
   record. Native MakeAppx must validate and unpack both original containers
   without `/nv` before package comparison.
2. Require identical relative-path membership and byte equality for every file
   except the exact per-architecture `JpgSpinner.winmd` and `AppxBlockMap.xml`
   paths. No extension glob or dependency WinMD qualifies.
3. For the app WinMD, require equal lengths/layout and every byte equal except
   its four-byte COFF `TimeDateStamp` and the 16-byte GUID selected by the Module
   row's MVID handle. Use .NET `PEReader`/raw `MetadataReader`, not a private
   PE/ECMA parser, fixed build-specific offsets or GUID-byte search. Refuse
   malformed/absent/ambiguous/overlapping identities and every difference outside
   those fields. Table/member/attribute changes are not permitted.
4. Use the platform XML reader for block maps. Permit changed `Hash` and compressed
   `Size` only on corresponding blocks of the already-qualified app WinMD whose
   content hashes change. Native package validation binds each block map to its
   actual compressed container. Keep file name, uncompressed length, local-header
   size, block count/order, unrelated files/blocks and all other values strict.
   A size change on an unchanged-hash block or other XML change is a refusal.
5. Compare read-only: no artifact patching, rewriting, filtering, omission or
   repacking. Preserve both raw manifests and hashes. A comparison-specific
   representation is evidence only, never a substitute shipping artifact.
6. Report separately whether this closed comparison is satisfied, complete
   payload bytes are identical, and container bytes are identical. List actual
   permitted variances and raw identities. Never emit unconditional
   `payloadsEqual=true` for an exception-qualified pair.

The strict `Test-PayloadManifestEquality.ps1` remains unchanged. Rename the
native two-build entry point to `Invoke-IndependentPackageBuildComparison.ps1`
and its evidence report to reflect the narrower guarantee. No compatibility
alias is required.

## Qualification before adoption

Use native-writer fixtures and the retained SDK pair. Accept identical files
and isolated timestamp/MVID variance; refuse changed type/member/attribute
content, unrelated PE bytes, malformed metadata, extra/omitted files, unrelated
block-map changes and size changes on unchanged-hash blocks. Prove originals
remain unchanged. Missing tools or fixture setup failures are not behavioural
RED. Native APIs own formats; local code owns only the explicit comparison rule.

After focused controls pass, integrate native-container comparison and run the
independent builds from a clean signed checkpoint. The old six-build pair still
has EXE absolute PDB references: this amendment does not allow them or make that
failed run pass. The existing symbol-path fix needs new-build confirmation.

WACK, manual-only hosted CI, applicable native signature trust, exact Store SBOM,
rights/security, accessibility, devices/upgrades, private credential revocation,
legacy cutover, deliberate submission and observed rollout remain separate.
No release, trust-store change, push or workflow dispatch is authorized here.

## WinMD-only source checkpoint

`Test-WinmdBuildIdentityVariance.ps1` now uses the platform PE/metadata readers
and byte-interval comparison without changing either input. This narrow app
inspection is bounded to 16 MiB and the actual SDK's single-MVID GUID-heap layout;
larger, edit-and-continue or ambiguous identity layouts refuse qualification.

The native PE/metadata writer supplies independent fixtures. Identical data
passes the original strict seam; changing only build identities first produces
behavioural RED. The native-located implementation then passes identity-only,
timestamp-only and MVID-only cases, reports real values/raw hashes and preserves
both originals. A same-length type-name change, missing MVID and malformed image
are refused. This does not test every malformed metadata encoding.

The retained identical-input SDK merger replay and the WinMD pairs from both
original clean builds in arm64/x64/x86 pass the narrow comparison, explicitly
with `ByteIdentical=false`. The old EXEs are not accepted by this WinMD helper;
their PDB-path differences remain outside the policy. No new application build,
block-map or complete-package acceptance is claimed by this source checkpoint.

## Block-map predicate source checkpoint

`Test-JpgSpinnerBlockMapVariance.ps1` is a read-only comparison predicate for
already native-validated snapshots, not original-container validation. The
platform XML reader owns syntax, namespace and attribute selection; its line
coordinates locate the selected native SDK literal UTF-8 attribute values.
All intervening original bytes, including unrelated XML formatting/comments,
remain strict. No tree or artifact is rewritten. Unqualified lexical forms,
noncanonical native scalar values, missing app entries and DTDs refuse; input
inspection is bounded to 1 MiB. This is not a general XML comparison library.

Native-writer fixture comparison first reaches RED under strict byte equality,
then accepts the approved hash/compressed-size changes, including a size digit-
count change. It refuses size changes on unchanged-hash blocks, another file's
hash, local-header size and an unrelated comment, and proves original fixture
preservation. A PowerShell-only span-method dispatch error was setup failure,
not behavioural RED; the supported ordinal string API fixes that invocation.
The shared native WinMD builder supplies inputs, not expected decisions.

These synthetic XML cases alone do not establish container hash or compression
validation; that obligation belongs to the native-container route below.

## Native-container comparison source checkpoint

`Compare-JpgSpinnerBundlePayload.ps1` now calls the existing owning-SDK
`Expand-JpgSpinnerBundle.ps1` for both original containers, with MakeAppx
validation enabled. Both inputs remain read-shared only throughout inspection.
It retains complete canonical payload inventories, checks exact ordinal path
membership, and applies the two predicates only to the six closed architecture
paths. No dependency WinMD, executable, PRI or unrelated file is excluded.
All other file lengths/hashes remain strict. A refusal retains diagnostics but
publishes no success comparison report.

The platform PE/metadata writer plus actual MakeAppx pack/bundle operations
supply newly generated synthetic fixture containers from copies of a retained
development payload. Identical originals first pass; the identity-only pair
then fails the original strict manifest comparison (behavioural RED). The
approved closed comparison accepts that pair with three actual identity/block
variances and both byte-identity flags false (GREEN). Native generated compressed
WinMD blocks change from 467 to 464 bytes in this fixture. Changed ordinary data,
extra/omitted files and a malformed original container are refused without
publishing success. The test checks original source/control container hashes.
These are package-comparison controls, not application activation or independent
clean-build qualification. No shipping input was repacked or changed.

`Test-NativePackageBlockIntegrity.ps1` separately characterizes the actual SDK's
hash-consumption boundary. A valid native fixture package extracts first; native
ZIP/XML writers then create an intentionally invalid owned copy with a validly
encoded but false WinMD block hash, retaining its payload and valid ZIP CRC.
MakeAppx unpack refuses with `APPX_E_BLOCK_HASH_INVALID` (`0x80080207`), and the
original package hash remains unchanged. This passing existing-behavior control
is not manufactured RED or a new local MSIX validator. Microsoft's
[lazy file-stream validation contract](https://learn.microsoft.com/en-us/windows/win32/api/appxpackaging/nf-appxpackaging-iappxfile-getstream)
explains why complete native sequential extraction, rather than merely reading
manifest syntax, matters here.

The detailed `package-payload-comparison.json` reports separate acceptance,
payload-byte identity and container-byte identity, complete raw hashes and
actual identity/block fields. Inventory and inspection references are report-
relative so candidate promotion does not leave stale staging paths. A replay
against the retained native fixture pair verifies those references and truthful
flags after that representation change.

The original retained two-build pair is also replayed through the native route
and still refuses unrelated executable block-map differences without success
promotion. The amendment does not retroactively qualify that failed run.

`Invoke-IndependentPackageBuildComparison.ps1` replaces the obsolete entry
point without an alias. It retains signature-verified clean source selection,
two independent clones, sequential three-architecture Release builds and two
MSBuild workers. It reuses the established bounded artifact-process runner and
invokes the native-container comparison once after both bundles exist, avoiding
duplicate extraction. Its `independent-package-build-comparison.json` links the
detailed report without converting accepted variance into byte equality.
Cheap wrong-source/existing/nested-output refusal controls pass before builds.

## New clean-build counterevidence

Two independent clean, signature-verified checkouts of source
`9e23722b30afaf827c1ac3df59aa682452ac8e82` now each build Release x86/x64/ARM64
and produce native-valid bundles with actual per-architecture provenance. Each
Release image passes the native basename-only PDB-reference check. The closed
comparison nevertheless refuses, correctly: executable, WinMD and block-map
hashes differ in every architecture. No success report/output is promoted.

Native PEReader inspection of x64 shows differences in code/read-only/data/
relocation sections, not just build-identity fields. Both PDB references are
`JpgSpinner.pdb`. Twenty-seven current clone-root strings remain in read-only
data, including C++/WinRT error-origin information. That is a concrete varying
input, not permission to exclude more bytes or proof that it is the only cause.
The next minimized qualification uses Microsoft's documented
[`WINRT_NO_SOURCE_LOCATION`](https://learn.microsoft.com/en-us/windows/apps/develop/cpp-winrt/macros#winrt_no_source_location)
Release configuration, consistently across files/static libraries, while
preserving Debug information, HRESULTs, the app's diagnostic contract and all
WinMD metadata. No new compiler-path shim or binary patch is proposed.

The manual CI source now includes the two narrow native-writer predicates and
real package-comparison/false-block-hash controls. The hosted source-contract
test first refuses their absence, then passes after wiring. Native YAML/
actionlint and automatic/mixed-trigger refusal controls pass; no workflow is
dispatched. The false-block-hash control also passes on the new real clean x64
application package, not only the synthetic fixture.

Independent application comparison qualification and the remaining Store
workflow integration are still required; this source checkpoint does not
complete Task 17 or any certification, legal, device, release or hosted gate.

## Release error-origin configuration checkpoint

The minimized `WinrtErrorSourceLocationProbe` uses the actual qualified
C++/WinRT projection and native error-origin/WIL-interoperation callback. Its
first Release build succeeds but execution returns 3 because source file/line
information is present (behavioural RED). Root Release item definitions now
select Microsoft's documented `WINRT_NO_SOURCE_LOCATION` before projection
headers, consistently for modern files/static libraries. No internal compiler
path option, vendor patch, binary rewriting or expanded byte exclusion is used.

`Test-WinrtErrorSourceLocationPolicy.ps1` then builds from two distinct owned
source directories through the real root MSBuild policy. Both Release consumers
preserve the expected `E_ACCESSDENIED` through origination, callback and exception,
omit optional file/line information and produce equal executable hashes. A
separate Debug consumer retains nonempty file and line origins. All pass in the
bounded native probe (GREEN). No application diagnostic field, HRESULT, PDB
symbol or WinMD table/member is removed. These results qualify this configuration
mechanism, not the whole application or the dependency's absolute source paths.
Microsoft's configuration contract requires consistent settings within each
linked module; Debug and Release remain independently built graphs.

The manual CI packaging lane uses its actual prebuilt projection to run this
control. Its source-contract check first refuses missing wiring, then passes
after the call is added. The prior complete application comparison remains
failed. A new clean signed-source pair is still required before qualification.

## Controlled build environment checkpoint — 2026-10-16

The next complete pair at signed source
`e02695a9d85d318364a4edcd963a164436d650f1` also builds all architectures, but
still refuses executable differences. All three actual WinMD pairs pass the
closed identity predicate. The rebuilt x64 image no longer contains its current
clone-root string in ASCII/UTF-16; both x64 images have equal printable read-only
string sets, but code/data/relocation bytes still differ. The optional-origin
configuration corrected one varying input, not the whole application outcome.

Microsoft's native `LINKREPRO`/`LINKREPROFULLPATHRSP` captures the retained x64
inputs without a local command/PE parser. Compiler outputs are already up to date;
the original-context incremental capture reproduces the original executable
hash. Two direct consumptions of the unchanged vendor-generated response file
produce the same executable hash as each other, including every section. Their
hash differs from the original context, so this is not proof of arbitrary-path
invariance or sole causation. Original containers, payload snapshots and source
executable bytes stay unchanged. Missing capture-directory and SDK `mt.exe`
setups are retained as setup failures; selecting the native evaluated SDK search
path in the child fixes the latter without changing the host environment.

The next implementation controls the absolute source path as a recorded build-
environment input. [Reproducible Builds' environment definition](https://reproducible-builds.org/docs/perimeter/)
explicitly includes the build directory; its
[build-path guidance](https://reproducible-builds.org/docs/build-path/) discusses
this option when paths affect outputs. This is not an extra byte exception:
executables and every other closed-policy byte remain strict. Do not claim
arbitrary-path reproducibility, retroactively accept a prior failed pair, patch
binaries, use a path-mapping shim or discard metadata.

`IndependentSourceBuildPair.psm1` owns the native Git/filesystem lifecycle, while
the caller owns compilation. Each pass creates a genuinely independent
`--no-local --no-hardlinks` checkout at one specified ordinary path, without prior
intermediates. After its completed build/clean-source check, the entire checkout
is retained separately by a checked same-volume native directory move. Only then
can a new independent directory occupy the source path. No checkout, original
artifact or user data is deleted or reused. A failed/dirty build or retention
failure stops the pair and preserves diagnostics; no unrelated process is killed.

Real native Git tests first exercise both independent passes at differing paths
and reach RED on the required controlled-path observation. Selecting one path
then passes fresh-intermediate/object-store, complete separate-retention,
exact-commit, source-change and early-failure controls (GREEN). The test replaces
only expensive compiler work, not clone/retention decisions. It also exercises
the actual closure/native-invoker pattern: a forced private dependency reload
initially hides the caller's command (setup failure), and reusing the current
dependency instance fixes that loading defect. No private function extraction,
test-only bypass or second process runner is introduced.

The main producer records the actual source path and both retained checkout
identities with its detailed comparison, preserving native source signature,
clean-tree, architecture, provenance and all output-acceptance gates. The manual
CI source contract first refuses missing lifecycle coverage, then passes its
new call. A full application pair under this explicit environment remains
required; the fast native lifecycle test is not that proof.

## Authoritative and practice anchors

- [PE/COFF format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#coff-file-header-object-and-image):
  timestamp is four bytes at offset four within the native COFF header.
- [.NET GUID heap indices](https://learn.microsoft.com/en-us/dotnet/api/system.reflection.metadata.ecma335.metadatatokens.getheapoffset)
  are one-based indices of 16-byte GUIDs, not byte offsets;
  [native heap locations](https://learn.microsoft.com/en-us/dotnet/api/system.reflection.metadata.ecma335.metadatareaderextensions.getheapmetadataoffset)
  are relative to the metadata start.
- [Block](https://learn.microsoft.com/en-us/uwp/schemas/blockmapschema/element-block)
  and [File](https://learn.microsoft.com/en-us/uwp/schemas/blockmapschema/element-file)
  distinguish compressed block length from uncompressed file length.
- [Reproducible Builds comparison guidance](https://reproducible-builds.org/docs/plans/)
  calls for an explicit reviewed protocol for exclusions; its
  [definition](https://reproducible-builds.org/docs/definition/) retains the
  distinction from byte-identical specified artifacts.
