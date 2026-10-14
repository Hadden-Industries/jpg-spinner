# Packaged JPG Spinner artwork

The generated PNGs preserve the repository's existing orange/white JPG Spinner
brand. Generation inputs and exact provenance live in
[`assets-source`](../../../assets-source/README.md), outside the shipping payload.
Run `scripts/New-PackageArtwork.ps1` to reproduce these variants and
`scripts/tests/Test-PackageArtwork.ps1` to decode/check them.

Following [Microsoft's current icon-construction guidance](https://learn.microsoft.com/en-us/windows/apps/design/iconography/app-icon-construction),
the package includes 100/200/400% scale variants for its manifest assets and all
14 app-list target sizes in default, unplated and light-unplated qualifier forms.
The existing brand's orange plate is deliberately retained for both themes.
The input masters are not vectors; larger required sizes use native bicubic
upsampling and do not claim additional artwork detail or universal byte identity.

Actual shell appearance, high contrast and Store qualification remain release
checks, not consequences of decoding the PNGs successfully. No template logo or
unused lock-screen resource belongs in the shipping asset inventory.
