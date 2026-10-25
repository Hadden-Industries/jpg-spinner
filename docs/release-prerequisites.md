# Release qualification prerequisites

This is the SLICE-003 availability register for the
[accepted release completion replan](plans/2026-10-25-release-completion-replan.md).
The [approved plan](plans/2026-08-30-jpg-spinner-modernization.md#task-20-qualify-release-2000-and-stage-the-store-rollout)
owns acceptance thresholds. This register identifies actual environments,
operators and the evidence still needed before the single final campaign.
An unassigned role or unavailable lane remains open; further builds cannot
supply a person's sign-off or a Store-trusted signature. Physical-device testing
is optional and nonblocking under the accepted amendment below.

## Observed availability — 2026-10-25

| Prerequisite                    | Actual observation                                                                                                                                                                                                                                                                                  | Next actor and evidence needed                                                                                                                                                                                                                                            |
| ------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Engineering host                | Local x64 Windows 11 Pro, native version `10.0.26300`; existing headless tools are usable. The edition/build string alone does not establish every named OS cohort.                                                                                                                                 | Engineer: use focused checks while editing; select one signed source identity before final qualification.                                                                                                                                                                 |
| Native Windows ARM64            | The deployment-only headless lane passed on signed `8183255a9e63c9d8282b526aee448b330440446a`; original JUnit and native OS/observer/PE identities are retained in the [hosted qualification record](implementation/2026-10-25-exiv2-hosted-qualification.md#native-arm64-execution).               | Engineer: hand off the exact retained result. Native packaged-app smoke belongs to an appropriate qualified desktop/VM lane and remains separate. No physical device is required.                                                                                         |
| Other OS lanes                  | No additional agreed Windows qualification lanes are identified. Existing VirtualBox registrations have not been adopted, started or treated as proof of guest OS/access availability.                                                                                                              | Windows qualification owner provides Windows 10 22H2 ESU and maintained Windows 11 24H2/25H2 desktop/VM lanes. Physical-device checks, including Windows 11 26H1 hardware, are optional and nonblocking. Deploy artifacts to guest lanes; do not build the product there. |
| Interactive operator            | Local desktop use is authorized for bounded final acceptance. Existing automated UI results are retained for their original source, not the future frozen candidate. The maintainer has accepted both human roles; see the canonical acceptance record below.                                       | Assigned maintainer performs the existing keyboard, Narrator, contrast, scaling and locale protocol against the exact candidate; engineering supplies the handoff inputs. Restore any temporary screen-saver setting afterward.                                           |
| GitHub protected packaging      | Following the explicit single-developer amendment, native GET confirms store-submission with required reviewer MaksymShostak (ID6131830), prevent_self_review=false and exactly the main branch allowed. Original zero-environment/HTTP404 observations remain retained.                            | Reviewer availability/configuration is resolved. Publish the compatible source guard before deliberately qualifying the hosted workflow; no run or job has been approved automatically.                                                                                   |
| Hosted interactive lane         | There are zero repository self-hosted runners, and no `interactive-qualification` environment exists. The separately authorized local final-acceptance route remains available; it does not imply a configured hosted lane.                                                                         | Maintainer chooses/provides a dedicated runner/operator only if this hosted route is to be qualified. Preserve its existing reviewer protection and isolation requirements.                                                                                               |
| Rights and corresponding source | Native notices, source preparation/reconstruction procedures and prior inventories exist. Current tool/module inputs and the final application artifact still need exact applicable rights/source evidence and qualified sign-off.                                                                  | Assigned maintainer performs licensing/corresponding-source review and records the decision; engineer assembles the exact technical inventory, delivery set and reconstruction evidence. No legal approval is inferred from a collector or successful package build.      |
| Historical credential           | Source retirement does not establish revocation or replacement by the credential owner. No credential value is inspected or published here.                                                                                                                                                         | Maintainer identifies the actual credential owner and supplies attributable revocation/replacement evidence through the appropriate private channel.                                                                                                                      |
| Trusted Store flight            | The installed historical Store baseline is retained. No agreed Partner Center operator, authorized flight or Microsoft-signed v2 candidate is established.                                                                                                                                          | Credentialed release owner prepares the deliberate selected-tester flight decision after qualification; prove the trusted same-identity upgrade before public release.                                                                                                    |
| Alert administration            | The [approved repair's native result](implementation/2026-10-25-codeql-result-dispositions.md#native-analysis-after-the-approved-repairs) fixes five findings without dismissal; the remaining current and historical observations remain open. No query suppression or analysis deletion occurred. | Maintainer deliberately authorizes any exact current-alert disposition and separate historical-configuration reconciliation; preserve original analysis evidence.                                                                                                         |

The maintainer has now accepted the two human roles in the acceptance record
below. This resolves their ownership, while their actual candidate-bound results
remain pending. Required OS lanes, historical credential action and the Partner
Center route retain their separate availability/evidence requirements. The
GitHub reviewer is resolved by the single-developer amendment below; neither
elapsed time nor a workflow name supplies a sign-off.

## Maintainer-owned acceptance

**Owner: Maksym Shostak. Assignment accepted: 2026-10-25.** The maintainer will
perform both the required human accessibility/localization checks and the
licensing/corresponding-source sign-off. This is an assignment, not a report
that either review has passed. No independent developer is required for these
maintainer decisions; their existing evidence and scope requirements remain.

| Acceptance record                | Authoritative checklist and inputs                                                                                                                                           | Current outcome                        |
| -------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------- |
| Human accessibility/localization | [Existing acceptance protocol](accessibility-release-checklist.md), covering the required OS lanes and locales against the exact deployed candidate                          | Pending execution and recorded results |
| Licensing/corresponding-source   | [Source closure, release binding and reconstruction/delivery procedure](corresponding-source.md), plus Task 20.6's exact dependency, notices, license and Store-terms review | Pending review and sign-off            |

The engineer prepares the frozen candidate's deployment artifact, full signed
source identity, exact artifact hashes and existing technical reports. For the
source review, also provide the resolved component/rights inventory, notices,
complete corresponding-source delivery set and actual reconstruction report.
Do not substitute a floating repository link, SBOM or historical preparation
checkpoint for those inputs. The maintainer performs and records the human
observations and final licensing/source decision; the agent does not sign for him.

For each acceptance record, retain the operator/date, full source commit, exact
reviewed artifact identities, the applicable checklist's required environment
and coverage details, original report references, unresolved findings and an
explicit outcome. Leave unperformed/unavailable checks pending. A later
candidate change requires only the affected reassessment, with its input scope
recorded; a source-owner assignment does not launch another build or full test
campaign. Record completed acceptance in the [release register](release-qualification.md)
before public release. These assignments grant no Store submission, publication
or workflow-dispatch authority.

## Why another Store run is premature

The current [Store workflow](../.github/workflows/store-package.yml) reads the
existing environment before its package job. HTTP failure refuses admission;
the package job depends on that successful admission. The maintained
[native-response predicate](../scripts/Test-ProtectedWorkflowAdmission.ps1)
requires the selected environment, a required reviewer, a native Boolean self-review setting
and a successful manual CI producer for the exact trusted source/run.
It does not create protection or accept a synthesized environment response.
The owner now explicitly permits sole-maintainer approval of a self-triggered
run. Configure `prevent_self_review=false`; this retains a deliberate approval
checkpoint without requiring an independent developer.

The original absence is retained as historical evidence. The subsequent
[single-maintainer checkpoint](implementation/2026-10-25-maintainer-self-review.md)
configures Store approval and verifies its native readback. The compatible
source predicate must be published before attempting the workflow; the older
remote source still requires self-review prevention. Existing static protection
controls and settings readback remain separate from actual hosted execution,
which still needs qualification.

## Admission to the final campaign

The engineer freezes one clean, signed source candidate after the consolidated
engineering changes and authoritative version audit. Before launching its full
campaign, this register must name the required operators and available lanes,
and identify the intended evidence for every gate. Source publication and each
remote dispatch retain their scoped authorization requirements.

Use the existing native build/test/package and Microsoft tooling against that
candidate. Record source, exact artifact hashes, actual architecture/OS,
operator, command/report and outcome. Reuse earlier observations only within
their real input scope; a compile result is not native ARM64 execution.
Do not rerun software because HISEW asks for refreshed bookkeeping.

The [accepted device amendment](implementation/2026-10-25-native-arm64-qualification.md)
removes the physical-device prerequisite. The engineer owns the native hosted
ARM64 headless lane; no hardware purchase, AWS provisioning or Codespaces setup
is needed for it. Qualified VM/desktop lanes can also supply their applicable
interactive, package and trusted-upgrade evidence. Those results must still be
established independently; the source change does not mark them passed.

## Evidence and resource checkpoint

Read-only lane/runner and exact Store-environment observations are retained under
HISEW inventory `f47722c6cb1c4f378a8af54f2a73a871`. No desktop input, VM launch,
repository setting mutation, install, workflow dispatch or publication occurs in
this availability assessment.

The finished tooling qualification has 83 operator originals and 39 selected
working originals independently retained through native HISEW readback. Consumer
release checkpoint `167786d11159459e98e11d13cf53b37e` for inventory
`7f4f797785ad40c09993b22cb27e31d5` reports complete preservation, no problems
and about 2.79 GB of spent working material. Its module/build caches have no
remaining qualification consumer. Preserve that directory while the known
[HISEW cleanup admission blocker](https://github.com/Hadden-Industries/software-engineering-workflow/issues/197#issuecomment-6395313372)
remains unresolved; reassess through native disposition after the fix. No
alternate deletion route, unverified reclamation claim or product test rerun is
introduced to work around it.
