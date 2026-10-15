# Third-party dependency qualification

## Qualification status

This is a development inventory, **not complete distribution notices or legal
clearance**. Release is blocked until every shipped direct/transitive component,
required verbatim notice, corresponding source and license obligation has been
compared with the exact package payload. Do not distribute this draft as if that
work were complete. The [release register](docs/release-qualification.md) names
the outstanding legal and supply-chain gates.

## Native components

This software is based in part on the work of the Independent JPEG Group.

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

The [retained native notice inventory](docs/licenses/native/README.md) now
includes the exact installed copyright files for seven native build-context
ports, plus the test-only Catch2 notice and libjpeg-turbo's upstream IJG README.
This preserves notice text; it does not prove every installed library contributes
to the application, package those notices into an MSIX, deliver corresponding
source, or complete final binary/terms comparison. Transitive libspng and zlib
are retained conservatively as resolved inputs, not asserted runtime features.
Adobe's combined notice includes the RSA Data Security, Inc. MD5 Message-Digest
Algorithm notice; the committed overlay patches remain separately inspectable.

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

Syft SDK 1.54.1, go-sync 0.1.2 and stereoscope 0.3.4 use Apache-2.0 and serve
the isolated release-engineering adapter, not the application runtime. The
[Apache-2.0 license text](eng/licenses/Anchore.Apache-2.0.txt) is retained. SPDX
Workgroup Tools 2.0.7 is an Apache-2.0 qualification tool. Their exact pins and
validator digests are in `eng/sbom-tools.json`; native Go module checksums are
retained under `eng/artifact-sbom`. The SDK's upstream transitive graph includes
prerelease/pseudo-version modules: retain its native rights inventory and notices
before redistributing a built tool; do not imply stable-only transitive or legal
clearance. Microsoft SBOM Tool 4.1.5 failed the retained contract and is not the
selected backend. YamlDotNet 18.1.0 and
actionlint 1.7.12 are MIT-licensed workflow validation tools. Their pins live in
[`eng/workflow-verification-tools.json`](eng/workflow-verification-tools.json);
the full YamlDotNet notice is retained under `eng/licenses` and actionlint's
downloaded archive includes its upstream notice. Pinned GitHub Actions and
CodeQL licensing/usage terms are separate from the generated application.

The immutable workflow pins include actions/download-artifact 8.0.2 and
actions/attest 4.2.2. Their exact MIT notices are retained in
[download-artifact](eng/licenses/actions-download-artifact.LICENSE.txt) and
[attest](eng/licenses/actions-attest.LICENSE.txt). They run only as external
workflow tooling; neither is an application runtime dependency. Source contracts
and local tool tests do not establish a successful hosted action invocation.

The Store evidence source also pins actions/setup-go 7.0.0 and
actions/setup-java 6.1.0. Both exact revisions use the
[retained MIT notice](eng/licenses/actions-setup-runtimes.LICENSE.txt). Their
Go 1.27.2 and Temurin 25.0.4.1+1 inputs are external engineering runtimes, not
application payload dependencies. Go's archive carries its BSD-style notice;
Temurin's exact distribution notices and signature verification remain with the
selected runtime. Native local qualification is not hosted installation proof
or legal clearance for redistribution of a tool/runtime distribution.

The large official SPDX schema uses [Ajv](https://github.com/ajv-validator/ajv/tree/v8.20.0)
8.20.0 through its public draft-2020-12 API, as recommended by SPDX. This is
external release tooling, not an application runtime dependency. The complete
native npm integrity lock is under `eng/spdx-validation`. Exact upstream notices
are retained for [Ajv](eng/licenses/spdx-validation/ajv.LICENSE.txt),
[fast-deep-equal](eng/licenses/spdx-validation/fast-deep-equal.LICENSE.txt),
[fast-uri](eng/licenses/spdx-validation/fast-uri.LICENSE.txt),
[json-schema-traverse](eng/licenses/spdx-validation/json-schema-traverse.LICENSE.txt)
and [require-from-string](eng/licenses/spdx-validation/require-from-string.LICENSE.txt).
The locked versions are 8.20.0, 3.1.3, 3.1.8, 1.0.0 and 2.0.2 respectively;
fast-uri is BSD-3-Clause and the other four are MIT. These notices do not clear
the separately qualified Syft graph or the application's distribution rights.

New tooling must pass version, immutable identity, native consumer and rights
qualification before adoption. Backend fixture qualification is not a release
SBOM or final rights approval for the complete tool/application distribution.
