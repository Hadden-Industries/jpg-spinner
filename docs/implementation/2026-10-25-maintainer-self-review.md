# Required maintainer approval with self-review

The owner explicitly permits self-review because this repository has one
developer. This amends the human reviewer-separation requirement in the
[modernization plan](../plans/2026-08-30-jpg-spinner-modernization.md#step-175-protected-store-packaging-workflow)
and [release prerequisite register](../release-prerequisites.md). A required
reviewer still deliberately approves the job; that reviewer may be the person
who triggered it. No independent-person approval is claimed.

## Native contract

GitHub documents [`prevent_self_review`](https://docs.github.com/en/rest/deployments/environments#create-or-update-an-environment)
as a Boolean controlling whether the initiating user may approve their job.
Its [environment settings guidance](https://docs.github.com/en/actions/how-tos/deploy/configure-and-manage-deployments/manage-environments#creating-an-environment)
makes prevention optional, separately from required reviewer membership.

The shared `Test-ProtectedWorkflowAdmission.ps1` schema now requires a genuine
Boolean rather than `const: true`. It accepts `false` for sole-maintainer
approval and continues to accept `true` when another reviewer is available.
Both protected roles use this same predicate. Missing, string, numeric or null
settings still fail. Nonempty required reviewers, exact role, successful manual
CI producer, trusted repository/main branch and exact source/run checks remain.
There is no custom JSON parser, coercion, policy bypass or automated job approval.

## Focused RED/GREEN

The revised native-REST-shaped fixture first fails against the original guard:
a valid required-reviewer environment with `prevent_self_review=false` is
refused. After the schema change, both roles accept self-review and retain the
stronger separation option: four positive cases. All 38 refusal cases pass,
including wrong source/run/workflow/role, automatic event, fork/branch,
unprotected or empty reviewer rules, malformed Boolean forms, excessive input,
array root and duplicate members. These are focused contract tests, not a new
security review, workflow execution or final release campaign.

## Actual Store environment

The existing native GitHub session identifies `MaksymShostak`, user ID `6131830`,
with administration permission on `Hadden-Industries/jpg-spinner`. A fresh GET
first confirms the earlier HTTP404; no existing protection is overwritten.
The GitHub connector exposes no environment-configuration operation, so the
documented native REST API is used through the existing authenticated CLI.

The explicitly requested sole-maintainer policy is configured for
[`store-submission`](https://github.com/Hadden-Industries/jpg-spinner/settings/environments):

- Required reviewer: `MaksymShostak`, ID `6131830`.
- `prevent_self_review`: native Boolean `false`.
- Custom deployment policy: exactly the `main` branch, with no tag rule.

Fresh native GETs independently read back the reviewer, Boolean setting and
complete one-rule branch list. Original requests, HTTP404, mutation responses
and readback responses are retained. No workflow is dispatched, no pending job
is approved, no signing/Store operation occurs, and no source is pushed.

The changed predicate also consumes the unfiltered native record for the
original successful CI run `40176250565`, source `0d78e54b`, together with the
real self-review-enabled environment response. Its exact source/run/role check
passes. This is response-consumer integration against that original run, not
new CI execution or evidence for the current candidate.

The compatible source guard must be published before attempting the Store
workflow: the older remote predicate still requires prevention. Settings
readback is not hosted workflow qualification. The optional
`interactive-qualification` environment and dedicated runner are not created;
their source admission predicate now supports the same maintainer-approved
self-review rule when that route is provided.

## Evidence and remaining gates

Raw evidence is retained under HISEW inventory
`d1b28cb795b343a4875d538b0dc815e8`. No disposable build, dependency installation
or fixture directory is created: the regression uses in-memory JSON and the
native API bodies/logs live in the configured evidence store.

Native PowerShell formatting/parsing and all warning/error analyzer rules pass
for both changed scripts. The consolidated focused test retains four positive
admissions and 38 refusals, including both cross-role controls. Selected
Markdown formatting, documentation policy and whitespace checks pass; full
verification remains separate.

This resolves the independent-GitHub-reviewer prerequisite for Store approval.
It does not resolve physical ARM64/OS lanes, human accessibility/localization,
legal/source-delivery review, credential-owner action, a deliberate trusted
Store flight or public release authorization. Full verification stays reserved
for the frozen final candidate; no HISEW bookkeeping rerun is introduced.
