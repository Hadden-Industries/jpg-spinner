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
