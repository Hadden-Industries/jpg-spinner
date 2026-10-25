# Exiv2 repairs and native ARM64 hosted qualification

## Source and executed scope

The [approved source repairs](2026-10-25-exiv2-source-repairs.md) are published
in signed source
[`74f168b3912bd1bedf706e3baf0167f9c5edd1c9`](https://github.com/Hadden-Industries/jpg-spinner/commit/74f168b3912bd1bedf706e3baf0167f9c5edd1c9).
The following signed
[`8183255a9e63c9d8282b526aee448b330440446a`](https://github.com/Hadden-Industries/jpg-spinner/commit/8183255a9e63c9d8282b526aee448b330440446a)
changes only workflow inputs, their controls and documentation. It adds the
[generated manifest omitted from the first ARM64 transfer](2026-10-25-arm64-manifest-fixture.md);
the production C++ source and dependency recipe remain those analyzed at
`74f168b3`.

| Deliberately authorized run                                                                               | Exact source                               | Observed outcome                                                                                                                                            |
| --------------------------------------------------------------------------------------------------------- | ------------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [First manual QA, 40221234193](https://github.com/Hadden-Industries/jpg-spinner/actions/runs/40221234193) | `74f168b3912bd1bedf706e3baf0167f9c5edd1c9` | Failed: the prebuilt ARM64 transfer omitted the application manifest required by one headless test. Other successful results retain their actual scope.     |
| [Native CodeQL, 40221237300](https://github.com/Hadden-Industries/jpg-spinner/actions/runs/40221237300)   | `74f168b3912bd1bedf706e3baf0167f9c5edd1c9` | Completed successfully at 19:45:56 UTC on 2026-10-25; five source-repaired alerts are fixed without dismissal.                                              |
| [Final manual QA, 40227978479](https://github.com/Hadden-Industries/jpg-spinner/actions/runs/40227978479) | `8183255a9e63c9d8282b526aee448b330440446a` | Completed successfully at 21:12:37 UTC on 2026-10-25: all eleven jobs passed, including native ARM64 headless execution and unsigned package qualification. |

Each workflow was dispatched once under its own recorded authority. The first
failed run was not rerun or relabeled successful. The CI-only repair did not
launch another CodeQL scan.

## Native ARM64 execution

The deployment-only job `121285015927` consumed the same run's
`arm64-headless-test-inputs` artifact `12613257630`. Its original archive
digest is
`bfc1779d963a36beacf15f2bceed292ec9f8b7fd15b40c8f6f9fcc2e116c37a4`.
The native download action enforced digest equality. No compiler or desktop
interaction ran in this job.

The original reports contain these testcase elements:

| Report              | Testcase executions | Failure/error elements |
| ------------------- | ------------------- | ---------------------- |
| Batch processing    | 15                  | 0                      |
| Domain              | 38                  | 0                      |
| JPEG transformation | 186                 | 0                      |
| Presentation        | 62                  | 0                      |
| Windows Storage     | 98                  | 0                      |
| Total               | 399                 | 0                      |

These are reported testcase executions, not assertion counts or a claim of 399
independent requirements. The presentation report includes the previously
failing case, `The isolated test-package manifest conforms to its native Windows consumer`.

For all five executables, the native identity report records PE machine,
observer architecture and OS architecture as `Arm64`. It records Windows
`10.0.26200`, image `win11-vs2026-arm64`, version `20261022.194.1`, run attempt
1 and the exact `8183255a` source identity. The Microsoft-signed ARM64 Windows
App Runtime 2.5.1 installer passed the pinned digest/signature check and deployed
successfully.

The original runtime-evidence ZIP is artifact `12613801516`, 16,915 bytes,
SHA-256
`ad6e837c28dd36ae2e9f197ce46d09e8c75e220c6c15abf4a189346fffd16b3c`.
Its JUnit and native machine/runner records are independently preserved.
This establishes the headless native-execution requirement; packaged-app smoke,
other OS/provider lanes, performance and human acceptance retain their own
requirements.

## Unsigned package qualification

Job `121286257880` produced and inspected the x86, x64 and ARM64 unsigned
packages. Each passed the native manifest/framework/payload checks, 55 artwork
controls and 24 notice-asset checks. All three original package provenance
records identify clean signed source `8183255a9e63c9d8282b526aee448b330440446a`.
Native bundling/extraction, synthetic payload-comparison refusals, false
block-hash rejection, WinRT-origin preservation, Store-component construction
and release/Store-SBOM refusal controls also passed.

The original unsigned bundle is 8,663,308 bytes, SHA-256
`38ec9ac0d3ca9e3af404d508b0b58ffdad5de84ff3ec9766c5c19044ccb08b01`.
It remains inside the preserved original diagnostic ZIP, artifact `12613993331`,
11,041,030 bytes, SHA-256
`109a7ef64f4c148585192f20e6c7ff4ee98ed6fc08b5218c06c00c7dcc11cea3`.
The retained log still records the previously observed ARM64 libjpeg-turbo port
warning that `NEON_INTRINSICS` is unused; successful qualification does not
declare the dependency producer entirely diagnostic-free.

This ordinary QA run did not produce a second independent build set or an exact
final-bundle SBOM. Native packaged-app smoke, the required OS/provider and
performance observations, human accessibility/localization, WACK, trusted Store
upgrade, licensing/source sign-off and deliberate distribution decisions remain
under their existing release gates.

## Source repair result

CodeQL 2.27.3 analysis `2023185530`, category `cpp-security-extended`, evaluated
95 rules and reported 24 results without an analysis error or warning.
GitHub records alerts **#9, #10, #12, #13 and #14** as fixed at
`2026-10-25T19:43:40Z`, with no dismissal. The traced build log confirms both
Exiv2 patches were applied before analysis.

The old FileIo observation #11 closed while #35 opened at the same
`basicio.cpp:413` expression under the revised vcpkg source-directory hash.
Both actual source files have SHA-256
`89d465f195320eb683d7b20f58c3089b05596edd282e10af9cd62bf366feae44`.
That relocation is not a sixth repaired source defect. The existing application
boundary uses Exiv2's in-memory TIFF/XMP payload APIs; this result does not claim
the upstream FileIo operation itself was repaired. The
[original static assessment and current-state reconciliation](2026-10-25-codeql-result-dispositions.md)
retain that distinction.

The post-analysis snapshot has 29 open alerts: 24 in the current manual
configuration, including #35, and five in the historical default configuration.
No alert dismissal, query suppression or analysis deletion was performed.
The read-only Dependabot snapshot is empty; no new dependency alert was
dismissed.

## Input publication and evidence boundaries

Job `121286257897` validated the prebuilt interactive inputs and published
[attestation 59693901](https://github.com/Hadden-Industries/jpg-spinner/attestations/59693901)
for `build-context.json`, SHA-256
`ad5c7cb125efaecefd87294a5e0109283370855ee8a8c568f79776eda4265eb5`.
This checkpoint records successful producer execution, not an independent local
attestation-verifier result, deployment or desktop acceptance. The one-day
`interactive-test-drop` archive expires on 2026-10-26 at 20:45:38 UTC; required
downstream inputs must be retained before expiry.

HISEW inventory `4ff2e26686144d8aa31e64d45d5ced87` retains both campaigns'
original compressed diagnostics, actual native API records, the provider's
returned SARIF subset, and the selected decoded job logs. Original ZIPs were
checked against their native GitHub SHA-256 digests before independent native
preservation. Checkpoint `88810d6b21424de9a4b5f13134ff4774` includes the successful
ARM64 runtime and input-publication evidence; successor
`8053780f75bf46ef9dee7765c91a7956` includes the original final container, completed
run, package log and cloud-disposal receipts. A decoded UTF-8/LF log is identified
as that representation; it is not claimed to reproduce the server's original
log-container bytes. No entire dependency environment or compiler tree is
archived.

After fresh original/independent-copy hash and length readback, the nine spent
diagnostic cloud copies from the two completed earlier runs were removed through
GitHub's native artifact API. Every deletion returned HTTP 204; independent
listing confirms those IDs are absent. Their 14,216,825 provider-reported bytes
are removed from artifact storage, subject to GitHub's usage-meter delay. The
original reports, workflow runs, CodeQL analyses, older unpreserved input
archives and current acceptance inputs remain retained; this is not a claim of
complete cloud cleanup or local disk reclamation.

Blocked local working groups remain intact under
[HISEW issue #197](https://github.com/Hadden-Industries/software-engineering-workflow/issues/197).
No alternate cleanup route, new security/external review, desktop interaction,
extra workflow dispatch or HISEW-driven software rerun occurs here. The
[release register](../release-qualification.md) and
[maintainer-owned acceptance](../release-prerequisites.md#maintainer-owned-acceptance)
continue to separate completed engineering evidence from pending release gates.
