# Retained native dependency notices

Status: development provenance, **not complete distribution notices or legal
clearance**. The [release register](../../release-qualification.md) and
[third-party inventory](../../../THIRD_PARTY_NOTICES.md) retain the open gates.

## Exact inputs and roles

The copyright assets below were copied without editing from native
`share/<port>/copyright` files preserved before generated dependency cleanup on
16 October 2026. SHA256 comparisons agree across x86, x64, ARM64 and the x64
AddressSanitizer installations. The seven non-test notices also agree with the
retained native export used for the independent-build Store SBOM qualification.
These are observations of resolved build inputs, not static-link contribution
proof or an approved release package inventory.

| Port / resolved version | Source identity | Verbatim installed notice |
| --- | --- | --- |
| adobe-xmp-core 2025.03#1 | [581c41213ddcee1fbc72cbb532531102a6617a25](https://github.com/adobe/XMP-Toolkit-SDK/tree/581c41213ddcee1fbc72cbb532531102a6617a25), with [repository patches](../../../vcpkg-ports/adobe-xmp-core) | [Combined Adobe/RSA notice](adobe-xmp-core.copyright.txt) |
| exiv2 0.28.9#1 | [v0.28.9](https://github.com/Exiv2/exiv2/tree/v0.28.9), with [repository patches](../../../vcpkg-ports/exiv2) | [Packaged COPYING](exiv2.copyright.txt) |
| expat 2.8.5 | [R_2_8_5](https://github.com/libexpat/libexpat/tree/R_2_8_5) | [Combined packaged notice](expat.copyright.txt) |
| inih 62#1 | [r62](https://github.com/benhoyt/inih/tree/r62) | [Packaged notice](inih.copyright.txt) |
| libjpeg-turbo 3.2.0#1 | [3.2.0](https://github.com/libjpeg-turbo/libjpeg-turbo/tree/3.2.0) | [Packaged license summary and BSD notice](libjpeg-turbo.copyright.txt) |
| libspng 0.7.4#1 | [v0.7.4](https://github.com/randy408/libspng/tree/v0.7.4) | [Combined packaged notice](libspng.copyright.txt) |
| zlib 1.3.2#2 | [v1.3.2](https://github.com/madler/zlib/tree/v1.3.2) | [Packaged notice](zlib.copyright.txt) |
| Catch2 3.16.1; tests only | [v3.16.1](https://github.com/catchorg/Catch2/tree/v3.16.1) | [BSL-1.0 notice](catch2.copyright.txt) |

The registry baseline is the committed `builtin-baseline` in
[vcpkg.json](../../../vcpkg.json). Port revisions are packaging revisions, not
different upstream release tags. Native vcpkg status supplies versions; its
binary SPDX `versionInfo` is an ABI hash and must not be presented as a version.
Several native SPDX license fields say `NOASSERTION`; they are not clearance.

The libjpeg-turbo packaged file references, but does not contain, the IJG README.
The separate [complete README.ijg](libjpeg-turbo.README.ijg.txt) was retrieved
from the selected [upstream 3.2.0 file](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/3.2.0/README.ijg).
The acknowledgement required by its binary-distribution terms appears in the
top-level third-party inventory. This is not a claim that a source offer is ready.

## Preservation and verification

Microsoft documents [vcpkg's native copyright installation](https://learn.microsoft.com/en-us/vcpkg/maintainers/functions/vcpkg_install_copyright)
and [triplet-specific SPDX metadata](https://learn.microsoft.com/en-us/vcpkg/reference/software-bill-of-materials).
The copyright generator can concatenate whole source files. In particular,
Adobe's asset includes `MD5.h`, and libspng's includes `spng.c`; those source
sections are intentional upstream packaged bytes, not application compilation
inputs. Do not trim them, summarize their licenses or normalize their newlines.
The `.gitattributes` exception preserves the installed copyright bytes.

[Test-DocumentationPolicy.ps1](../../../scripts/Test-DocumentationPolicy.ps1)
owns the independently retained SHA256 expectations and is called by repository
policy. It uses PowerShell's [native file hashing](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.utility/get-filehash?view=powershell-7.6),
not a license classifier or SPDX parser. Refresh the assets and expected hashes
together from newly qualified packages when dependencies change; never accept a
changed asset by hashing that edit and calling the result independently verified.

The owned-copy controls verify a valid baseline and precise refusals for missing
or edited notice bytes. Hash equality proves preservation of selected content,
not sufficiency of that content, final binary membership, GPL compatibility or
Store terms. Microsoft/NuGet and engineering-tool transitive notice closure,
verbatim distribution packaging, corresponding source/build instructions and
qualified legal review remain mandatory before release. No licensing boundary
is inferred from static linkage, process separation or component labels.
