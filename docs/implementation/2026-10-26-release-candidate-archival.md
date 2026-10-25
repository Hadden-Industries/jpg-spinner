# Single-candidate archival implementation

## Accepted scope and native boundaries

Continue the accepted R2 modernization execution from signed `3e33bd139eaf`,
implementing the [approved retention rule](../release-candidate-retention.md).
The application, dependency graph and existing QA result at `8183255a` remain
unchanged. This slice adds no security/external review or automatic CI trigger.

The protected Store workflow will archive the original `store-submission-evidence`
artifact ZIP after its separate attestation job succeeds. Use the GitHub CLI and
native REST release/asset operations; do not repack containers or implement an
archive, JSON, URL or command-line parser. The existing strict JSON boundary owns
raw response admission. Native GitHub artifact and release-asset SHA-256 digests
must match the actual downloaded bytes.

Before attestation, reuse `New-PayloadManifest.ps1` to record the candidate's
complete path/length/SHA-256 membership. Produce it outside the quiescent evidence
root, then include it as a separately attested JSON subject without attempting
to hash itself. A candidate context records source/producer/QA identities, the
selected submission bundle and the still-pending release acceptance register.
This does not reimplement the existing inventory or assert later acceptance.

Use a separate trusted-main hosted archival job with only the required contents
write and Actions read permissions. It has no compiler, dependency installation,
OIDC/signing permission or application execution. Native workflow concurrency
serializes candidate replacement without cancelling an in-progress replacement.
The shared qualification policy owns its budget.

Each archive operation uses an exact source/run/attempt identity and a reserved
candidate tag namespace. Never overwrite an existing candidate's assets. Create
and verify the successor before retiring only superseded owned unpublished
candidate drafts. Published or unrelated releases remain outside that scope.
Failures and unknown outcomes retain the previous candidate and report the
unresolved transition; staging is not proof of replacement.

GitHub requires contents write for release operations. Its native release API
also restricts targeting a different workflow revision: the ordinary Actions
token cannot gain Workflows write. Keep this job in the candidate's own protected
workflow and bind its exact source; introduce no personal-token workaround or
weakened source comparison.

For draft readback, use the authenticated paginated release list: GitHub's tag
lookup promises a published release. Refuse an existing tag before creation,
because the provider ignores `target_commitish` when that tag already exists.

## Focused proof

1. Extend the shared artifact-process boundary with an explicit raw-output file
   contract. An independent invalid-UTF-8/NUL byte fixture must survive exactly;
   a nonzero child must retain its partial bytes and stderr. Existing text and
   teardown contracts remain covered. The initial missing-parameter failure is
   a deliberate compile-time/public-API contract RED, not a behavioral claim.
2. Exercise the real archival policy against independently authored provider
   responses: wrong source/run/digest/length/state, duplicate members, published
   release, unrelated draft, and failed replacement must not authorize retirement.
   Synthetic provider responses establish local control behavior, not live
   GitHub qualification.
3. Extend the existing native YamlDotNet/actionlint/PowerShell workflow checks
   and negative controls for archival ordering, serialized replacement and the
   isolated write-capable role. Run only affected checks while editing.
4. Format/lint the exact consolidated commit scope and create a signed checkpoint.
   No full application verification is repeated for workflow bookkeeping.
   Source publication and actual hosted/native release operations retain their
   scoped approval and final-candidate requirements.

The first native process test workspace has a one-MiB allocation. Retain actual
raw results through HISEW and use its normal disposition process. Previously
blocked directories remain intact; allocation or a passing test grants no
alternate cleanup route.

## Authoritative guidance

- [Release API and permissions](https://docs.github.com/en/rest/releases/releases).
- [Release-asset digests and native endpoints](https://docs.github.com/en/rest/releases/assets).
- [GitHub CLI release creation](https://cli.github.com/manual/gh_release_create),
  [upload](https://cli.github.com/manual/gh_release_upload) and
  [deletion](https://cli.github.com/manual/gh_release_delete).
- [.NET stream copying](https://learn.microsoft.com/en-us/dotnet/api/system.io.stream.copytoasync)
  preserves bytes without text decoding.

## Local checkpoint

The implementation preserves the original ZIP through the shared native process
boundary, creates a distinct draft, checks provider metadata and actual uploaded
asset bytes, and only then retires the previously observed owned drafts. A
failed retirement/readback produces no success receipt. Published releases,
unrelated drafts and Git tags are excluded. `-WhatIf` performs no provider or
filesystem operation.

Observed RED checks included the missing binary-output API, missing archival job,
the draft-list/root mismatch and absent candidate membership wiring. The final
focused local checks pass:

- 16 archival transition cases plus the side-effect-free `WhatIf` control;
- eight invalid hosted-context refusals before remote I/O;
- real native-child raw-byte success/failure/stderr/existing-file controls and
  the existing text-process success/failure/timeout lifecycle regression;
- native YamlDotNet/actionlint/PowerShell Store composition and 18 source refusal
  cases, including cancellation, ordering, role and manifest controls;
- affected global workflow, CI wiring, attestation-role, manual-trigger and
  canonical qualification-policy regressions;
- the unchanged payload manifest's independent path/length/SHA-256 vectors.

A read-only native GitHub CLI download also preserved artifact `12613801516`
exactly: 16,915 bytes with SHA-256
`ad6e837c28dd36ae2e9f197ce46d09e8c75e220c6c15abf4a189346fffd16b3c`.
This proves that byte-transfer integration for an existing artifact; it does not
qualify release creation, asset upload or predecessor retirement on GitHub.

Retain the small archive-operation receipt through HISEW before its Actions
diagnostic copy expires. Required evidence keeps its independent obligations;
the current candidate ZIP remains available at the draft release for later
review. Local tests use the existing allocated fixture group; its justified
three-MiB budget includes final native refusal copies and retained evidence.
Blocked native cleanup remains a recorded hold, not a successful disposal.

Hosted qualification of automatic replacement remains pending until its actual
results are recorded. This implementation record is not release authorization.
