# Release-tool freshness coverage

The existing freshness checker inspected the runtime vcpkg and NuGet manifests
but omitted the canonical GitHub Action and SBOM-tool pins. Its successful
runtime result therefore did not establish that all release tooling was current.
The [release completion replan](../plans/2026-10-25-release-completion-replan.md)
requires this gap to close before freezing a candidate.

## Native authority and shared policy

The checker now reads `eng/github-actions-lock.json` and the adopted Syft SDK
and SPDX Workgroup verifier versions in `eng/sbom-tools.json`. It reuses the
existing strict JSON reader, stable-version ordering, exact published-pin check
and update report. No manifest is rewritten. Runtime and tooling checks share
one GitHub release-selection function.

GitHub documents separate [release listing and pagination](https://docs.github.com/en/rest/releases/releases#list-releases).
The repository's newest release can be an unrelated product tag: the CodeQL
Action repository also publishes CodeQL bundles. The checker consumes stable
release tags through the existing version policy instead of treating
`/releases/latest` as the newest Action version.

PowerShell's [native web response](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.utility/invoke-webrequest)
provides parsed relation links. The shared transport follows that dictionary;
it adds no Link-header or JSON parser. Every raw response page must pass the
same .NET duplicate-member, root-kind and depth checks before PowerShell
conversion. Results are emitted only after successful completion. The finite
page allowance refuses an unfinished list rather than publishing partial success.
Existing connection/body deadlines remain category-driven.

## Focused proof

The retained RED contract misses an available CodeQL Action update with the old
checker. The extended contract then passes current pins, detects newer Action,
SDK and verifier releases, ignores drafts/prereleases and unrelated bundle tags,
rejects non-string version pins and non-Boolean release flags, rejects duplicate
Action members and wrong metadata roots, and preserves input hashes.

Real loopback HTTP responses prove successful multi-page traversal, duplicate
rejection on a later page and refusal when a next page remains at the configured
bound. Existing raw-root and stalled-body controls also pass. The fixture's
TCP accept wait now contains a cancellable PowerShell boundary; an earlier RED
attempt needed owned-child teardown before reporting its missing-parameter
failure. That intervention is retained and is not described as an unattended pass.
The fixture reuses the JSON transport readiness budget and one polling interval.

The final focused checks use native PowerShell parsing/formatting and
PSScriptAnalyzer. Three heuristic rules are excluded for this scope: collective
`Metadata`/`ObjectMembers` names; mandatory private fixture lifecycle operations
that must not acquire `ShouldProcess` prompts; and job parameters explicitly
bound by `Start-Job -ArgumentList`, which the runspace heuristic misidentifies.
Other warning/error rules remain enabled; no product validation is suppressed.

Actual live authority reads identify Action 4.39.1 and Syft SDK 1.55.0 while the
maintained pins still select 4.38.3 and 1.54.1. The live gate correctly reports
both updates with exit 1. Their native adoption is a separate qualified change;
this checker repair neither adopts them nor starts another workflow or scan.

Original metadata, RED/GREEN diagnostics and the existing private candidate are
tracked in HISEW inventories `f47722c6cb1c4f378a8af54f2a73a871` and
`7f4f797785ad40c09993b22cb27e31d5`. Required evidence is retained before native
disposal; blocked cleanup groups remain intact under the existing HISEW issue.
