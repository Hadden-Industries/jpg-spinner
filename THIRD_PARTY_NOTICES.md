# Third-party dependency qualification

## Qualification status

This is a development inventory, **not complete distribution notices or legal
clearance**. Release is blocked until every shipped direct/transitive component,
required verbatim notice, corresponding source and license obligation has been
compared with the exact package payload. Do not distribute this draft as if that
work were complete. The [release register](docs/release-qualification.md) names
the outstanding legal and supply-chain gates.

## Native components

| Component | Reviewed version / role | Upstream terms to retain |
| --- | --- | --- |
| [libjpeg-turbo](https://github.com/libjpeg-turbo/libjpeg-turbo/tree/3.2.0) | 3.2.0, port revision 1; image runtime | Actual packaged BSD-style and IJG notices, not a shortened SPDX label |
| [Exiv2](https://github.com/Exiv2/exiv2/tree/v0.28.9) | 0.28.9, port revision 1; metadata runtime | Selected GPL-2.0-or-later terms; qualify combination under repository GPL-3.0 |
| [Adobe XMP Toolkit SDK](https://github.com/adobe/XMP-Toolkit-SDK/tree/v2025.03) | v2025.03; qualified external XMP backend | Exact upstream notices and local overlay patches |
| [Catch2](https://github.com/catchorg/Catch2/tree/v3.16.0) | 3.16.0; tests only | BSL-1.0; not an application runtime dependency |

The actual transitive native closure is determined by resolved vcpkg installation
metadata and shipped payloads, not this list of direct names alone. Retain every
native port's copyright file and source/patch locator. Static linkage is not an
exemption from obligations. No loader or process boundary is a licensing remedy.

## Windows packages

[`Directory.Packages.props`](Directory.Packages.props) and committed project
`packages.lock.json` files identify direct/transitive versions. Current direct
inputs are Windows App SDK 2.5.1, C++/WinRT 3.0.260818.1, Windows Implementation
Library 1.0.260126.7 and Windows SDK BuildTools 10.0.28000.2705.

Use each exact package's license/redistribution terms and compare which framework,
runtime or build-only files actually ship. Do not label all Microsoft packages
MIT or describe SDK build tooling as application runtime. WIL's reviewed MIT
notice is retained in [docs/licenses](docs/licenses/Microsoft.Windows.ImplementationLibrary.LICENSE.txt).
The remaining complete exact-payload notice inventory is pending.

## Engineering tools

Microsoft SBOM Tool 4.1.5 is release tooling under its upstream MIT license, not
an application runtime. Its SPDX/dependency fixture currently fails; it is not
qualified simply because its executable hash matches. YamlDotNet 18.1.0 and
actionlint 1.7.12 are MIT-licensed workflow validation tools. Their pins live in
[`eng/workflow-verification-tools.json`](eng/workflow-verification-tools.json);
the full YamlDotNet notice is retained under `eng/licenses` and actionlint's
downloaded archive includes its upstream notice. Pinned GitHub Actions and
CodeQL licensing/usage terms are separate from the generated application.

New tooling must pass version, immutable identity, native consumer and rights
qualification before adoption. No prospective SBOM alternative is adopted by
mentioning it in a development decision.
