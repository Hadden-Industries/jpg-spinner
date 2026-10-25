# Release-candidate retention

## Approved rule

Retain at most one current release-candidate archive. Keep the latest successfully
archived candidate available until a complete, verified successor replaces it;
then automatically retire the superseded candidate's archive. Elapsed time alone
does not retire the current candidate. This replaces the proposed rule of keeping
every candidate until an explicit retirement decision.

"Latest" means the accepted successful replacement, not the most recent build
attempt. A failed build, incomplete upload or failed readback leaves the existing
candidate available. Before the first successful archive there may be no current
candidate; a failed attempt must not be presented as one.

## Candidate and replacement boundary

One candidate archive contains its exact candidate bundle and available
comparison inputs, SBOM, provenance/attestations, checksums, qualification reports
and corresponding-source material. Its manifest records the required membership
for that archive's stage and explicitly identifies unavailable or pending
qualification, human and licensing/source evidence. Archiving a candidate for
review does not require those later acceptances to have passed. Multiple
architectures and the two required independent builds are evidence for that one
candidate, not separate retained release candidates. Its manifest identifies the
exact source, producer runs, selected submission artifact and remaining gates.
Existing ordinary-QA output alone does not establish that this complete Store
candidate has been assembled or qualified.

Use an unpublished GitHub draft release for the current candidate's durable
review/download location, with independently retained required evidence under
the existing HISEW contracts. Serialize replacement through the native workflow
concurrency controls and use GitHub's supported release/asset operations:

1. Stage the successor under a distinct identity, preserving the current
   candidate throughout upload and validation.
2. Verify complete declared membership, exact source/producer binding, the
   producer's required technical checks and actual retained asset lengths/digests. Read back the
   archived bytes before treating the successor as available.
3. Accept the verified successor as current and retire the previous candidate's
   owned unpublished archive and bulk payloads through the native disposal
   operations. Retain the small source/hash/decision/removal record and any
   independently required evidence under its actual obligations.

Temporary staging may briefly coexist with the current archive. This overlap
prevents a failed replacement from losing the only usable candidate; it is not
permission to accumulate historical candidate archives. Interrupted transitions
retain their exact identities and next actor for reconciliation. An unknown
outcome or failed retirement remains an unresolved transition, not a claim that
the single-candidate steady state has been restored.

## Other retention and release boundaries

Actions artifacts remain short-lived transfer copies. Their one-day expiry is
acceptable only when required candidate/evidence preservation is completed and
verified before expiry; it must not impose a one-day deadline on human review.
Automatic durable archival still needs implementation and hosted qualification
before that handoff can be claimed. Do not launch another build to compensate
for a missing archive without separately resolving its actual outcome.

Published releases, supported-version recovery assets, required failure/analysis
evidence, source history and independent legal/incident/consumer holds have their
own retention contracts. Candidate replacement grants no deletion authority over
those objects or unrelated draft releases. Release candidates may be superseded
without deleting the product's published release history.

Blocked local temporary directories remain intact under the existing HISEW
cleanup instruction until its native process is fixed. This rule supplies no
alternate deletion route or clean-resource claim. Reassess and retire obsolete
payloads through the existing native retention/disposal machinery when admitted;
do not retain every old candidate under an unsubstantiated evidence label.

Keeping the current candidate available permits review later without an
automatic rebuild. Retention does not grant publication, certification,
installation, Store submission or rollout authority, and changed release facts
still require their affected reassessment.

## Native guidance

- [GitHub draft releases and asset preparation](https://docs.github.com/en/repositories/releasing-projects-on-github/managing-releases-in-a-repository).
- [GitHub release/asset API](https://docs.github.com/en/rest/releases/releases),
  including separate draft and published states.
- [Native release deletion](https://cli.github.com/manual/gh_release_delete);
  candidate-archive retirement does not require its optional Git-tag cleanup.

The [release register](release-qualification.md) records actual qualification and
the [accepted completion replan](plans/2026-10-25-release-completion-replan.md)
retains its existing execution and approval boundaries.
