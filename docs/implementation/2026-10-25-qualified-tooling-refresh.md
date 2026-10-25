# Qualified release-tool refresh

This checkpoint implements SLICE-001 of the
[accepted release completion replan](../plans/2026-10-25-release-completion-replan.md).
The [freshness-coverage repair](2026-10-25-release-tool-freshness.md) is already
signed as `f722cc9858e2`. This change adopts the two observed stable tool
updates and the current Go patch for the protected manual workflow. It changes
release engineering inputs, not application runtime dependencies.

## Native SDK selection and preservation

The official [Syft 1.55.0 release](https://github.com/anchore/syft/releases/tag/v1.55.0)
is selected through native `go get github.com/anchore/syft@v1.55.0`, following
[Go's module-management guidance](https://go.dev/doc/modules/managing-dependencies).
Go updates the dependency graph and checksum file; no replacement, broad `-u`,
vendor patch, custom parser or JSON repair is used. Native module identity is
`v1.55.0`, sum `h1:IeBZJrF/g75ba3k0ITQ8v4NCVGiGsWU3Wa1P/eSAFa0=`.
`go mod verify` passes before and after execution. The adapter source and public
SDK API usage are unchanged.

The private native candidate passes all ten adapter tests. The official
document qualification first refuses the old 1.54.1 pin against the new module,
then passes the unchanged independent fixture, exact SPDX 3.0.1 schema,
Workgroup Tools 2.0.7 semantics and artifact refusal controls with the new pin.

Actual application coverage also passes. The original hosted-QA archive and
bundle are independently verified before native SDK extraction. The exact
unsigned bundle remains SHA-256
`10a94f6722e2442c8bbe500a5f0f4fa7447b431c4e3f110efa04436d9f070fe8`.
The SDK consumes its native-inspected x86/x64/ARM64 payload plus the previously
preserved real component snapshots and independent 38-identity contract. This
includes Adobe XMP `2025.03#2` and Exiv2 `0.28.9#1` with exact triplet-qualified
package URLs. Every required identity, payload membership/hash, official schema
and semantic check passes. The unmodified native output's SHA-256 is
`0ca45007b214d19a60ad066157e0566d214168aa5d63f0e22ba30083c34739c9`.

Those original component files are recovered by exact native HISEW
content-addressed references from inventory `6fa37531e0944f4cb5381e259e8e29d1`,
checkpoint `14462768ac7c4aa882e28661d029b793`. The held working directory is not
copied, repaired or deleted. This avoids another large binary export while
preserving the real, independently fixed input oracle.

These results qualify the SDK/adapter composition. They do not attest a new
application build, static-linker contribution, deployed runtime coverage,
signed upgrade or final release. Exact final-candidate evidence remains open.

## Action pin, provenance fixtures and runtime selection

The official [CodeQL Action 4.39.1 release](https://github.com/github/codeql-action/releases/tag/v4.39.1)
and annotated tag resolve to immutable commit
`9b3254085494b55a0f7c4c2b3d2d52cc712ce370`. Both workflow steps and the canonical
Action lock use that identity. The release changes the default CodeQL bundle
to 2.27.3; the earlier authorized analysis already reported that CLI version.
The wrapper update is source-qualified here; no new analysis or dispatch occurs.

After changing the canonical Action pin, the old provenance fixture fails its
valid-record check because it duplicated the previous SHA. The positive fixture
now reads valid identities from the single canonical lock. Its independently
mutated stale, missing, unexpected and substituted identities still refuse.
All nine provenance refusal cases pass. No fixture claims that its placeholder
compiler/runner values are observations.

Official [Go distribution metadata](https://go.dev/dl/?mode=json) identifies
1.27.3 as stable. The protected Store workflow selects that patch. Compatibility
qualification above used the already installed Go 1.27.0, which satisfies the
module's minimum language directive; that directive stays 1.27.0. Hosted
execution of the selected 1.27.3 runtime remains separate. No system installation
or administrator-consent bypass occurs. The contract runner and README derive
their Go test deadline from the canonical qualification policy.

The Apache-2.0 license bytes match the retained shared license. The new upstream
`COPYRIGHT` file is retained verbatim under `eng/licenses`. Transitive rights
inventory, actual tool redistribution and qualified legal review remain release
obligations; this source update does not supply those approvals.

## Evidence and next work

The adopted maintained contract runner passes all ten native Go tests, the
independent fixed-hash/component fixture, official schema and Workgroup semantic
validation. Native YAML/actionlint and protected Store composition checks pass
for the two changed workflows; their steps are not executed. Native PowerShell
formatting/parsing and scoped analysis pass for the changed scripts, with the
same documented analyzer heuristics excluded as in the freshness checkpoint.
Selected Markdown/JSON/YAML formatting, documentation policy and whitespace
checks pass. The final live freshness check reports no actionable newer version
through the approved package authorities. These are focused integration checks,
not the final release verification campaign.

HISEW inventories `7f4f797785ad40c09993b22cb27e31d5` and
`f47722c6cb1c4f378a8af54f2a73a871` track the private candidate and original
RED/GREEN, integrity, official-validation and actual-component evidence.
Preserve required originals before native cleanup and keep blocked directories
intact. Full verification stays reserved for the frozen final candidate.
The next engineering workstream is the supplied CodeQL-result disposition;
device/human/legal/Store-flight availability remains a separate critical path.
