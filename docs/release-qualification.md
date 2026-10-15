# JPG Spinner 2.0 release qualification

Status: **NOT QUALIFIED / NOT AUTHORIZED FOR RELEASE**. This register is a
maintained handoff, not a waiver. A source checkpoint, signed Git commit, native
build, installed package and Store submission are different outcomes. The
[approved plan](plans/2026-08-30-jpg-spinner-modernization.md)
owns full acceptance; nothing here replaces its thresholds.

## Gate register

| Gate | Current evidence boundary / remaining proof | Accountable role |
| --- | --- | --- |
| Locked builds | Local native x86/x64/ARM64 package checkpoints exist; final exact clean-source and hosted matrix remain | Build maintainer |
| Tests and static analysis | Native incremental analysis and retained x64 headless suites pass; a reproduced exit-time dispatcher hang is repaired with a bounded regression; final clean-candidate analysis/CodeQL/ASan remain | Engineering owner |
| Parser fuzzing | Bounded development runs do not establish required final aggregate duration or corpus minimization | Engineering owner |
| Metadata/preservation | Retained synthetic cases do not replace final dependency revision qualification | Image-processing owner |
| Storage/recovery | Retained identity/backup/journal/refusal cases; final supported-provider and interruption matrix remain | Storage owner |
| AppContainer | Actual grant/denial development cases exist; final packaged candidate and capability/traffic checks remain | Windows qualification owner |
| UI/accessibility/localization | Some real picker/layout evidence exists; final Narrator, high contrast, keyboard, scaling and locale acceptance remain | Accessibility owner |
| OS/device matrix | Minimum OS, approved long-path-disabled refusal lane and physical ARM64 remain | Windows qualification owner |
| Reproducibility | Payload manifest contract passes fixtures; two independent clean builds and exact manifests remain | Build maintainer |
| SBOM | Pinned Microsoft CLI fails the SPDX 3.0.1 fixture and omits vcpkg output; backend qualification/design decision remains; no compliant release SBOM | Supply-chain owner |
| Package identity/update | Unsigned native packages/bundle exist; trusted 1.1.3.0 upgrade and all-architecture runtime qualification remain | Release owner |
| Signing/WACK | No final trusted signed bundle/WACK approval claimed | Credentialed release owner |
| Licensing/source | Exact shipped dependency notices, GPL/Exiv2 compatibility, corresponding source and current Store terms need qualified legal review | Maintainer and qualified legal reviewer |
| Retired telemetry | New source must remain transport-free; historical credential revocation is a private credentialed-owner action, not a source-edit claim | Resource owner |
| Legacy deletion | Pending accepted cutover proof; no compatibility shim is permitted | Engineering owner |
| Resource hygiene | Remove eligible owned scratch after consumers finish; preserve required evidence and unresolved ownership | Execution owner |
| Final reviews | Only final consolidated task candidates; narrow repair follow-ups; earlier reviews cover only their frozen revisions | Engineering owner |
| Store/rollout | Submission and 5%/25%/100% observation gates not performed or authorized by this document | Credentialed release owner |

Every gate needs an actual date, operator, exact command/report locator, result
and immutable candidate identity before changing to qualified. A missing field
is pending, not an implied approval. Required final fuzz duration, health sample
counts, observation windows and zero-integrity-incident rules remain those in
Task 20. Never invent health observations for an unreleased application.

## Evidence identity

Archive source commit and clean-tree proof, NuGet/vcpkg locks, runner ImageVersion,
actual selected executable paths/versions/hashes, SDK, provenance, unpacked payload
manifests, unsigned/signed container hashes, SBOM and validation reports, tests,
analysis/fuzz logs, security/review scope, WACK, performance, accessibility,
upgrade and legal sign-offs. Preserve failures as well as successes.

Local development raw evidence is external to Git under the owning HISEW
execution; public source checkpoints link the implementation/design contracts.
Do not commit credentials, signing material, user photographs or generated
packages. Do not claim an input observation proves execution. Do not delete a
held reproducer or verifier workspace because a source commit was made.

## Release authorization

This register grants no signing, secret access, Store submission, rollout or
remote publication permission. The existing Store identity must be retained.
After all gates, obtain deliberate authorization for the exact candidate and
destination. Any source/backup/recovery integrity report pauses rollout regardless
of aggregate crash rate. Use Store health and opt-in local diagnostics; do not
add telemetry as a release remedy.
