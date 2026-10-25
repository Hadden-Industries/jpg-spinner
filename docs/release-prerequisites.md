# Release qualification prerequisites

This is the SLICE-003 availability register for the
[accepted release completion replan](plans/2026-10-25-release-completion-replan.md).
The [approved plan](plans/2026-08-30-jpg-spinner-modernization.md#task-20-qualify-release-2000-and-stage-the-store-rollout)
owns acceptance thresholds. This register identifies actual environments,
operators and the evidence still needed before the single final campaign.
An unassigned role or unavailable lane remains open; further builds cannot
supply a person's sign-off, a physical device or a Store-trusted signature.

## Observed availability — 2026-10-25

| Prerequisite                    | Actual observation                                                                                                                                                                                                                           | Next actor and evidence needed                                                                                                                                                                                                                |
| ------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Engineering host                | Local x64 Windows 11 Pro, native version `10.0.26300`; existing headless tools are usable. The edition/build string alone does not establish every named OS cohort.                                                                          | Engineer: use focused checks while editing; select one signed source identity before final qualification.                                                                                                                                     |
| Physical ARM64                  | No physical Windows ARM64 device/access owner has been identified. Native compile/package evidence is already retained. ARM64 project configurations and the test-suite selector already exist; no speculative test infrastructure is added. | Maintainer identifies the device/operator. Deploy the exact built artifacts, then run the required native transform, transaction, recovery, batch and packaged-app smoke suites on that device. Record device/OS and artifact identities.     |
| Other OS lanes                  | No additional agreed Windows qualification lanes are identified. Existing VirtualBox registrations have not been adopted, started or treated as proof of guest OS/access availability.                                                       | Windows qualification owner provides Windows 10 22H2 ESU and maintained Windows 11 24H2/25H2 lanes; include Windows 11 26H1 hardware where available, as the plan specifies. Deploy artifacts to guest lanes; do not build the product there. |
| Interactive operator            | Local desktop use is authorized for bounded final acceptance. Existing automated UI results are retained for their original source, not the future frozen candidate. Human accessibility/localization operator is unassigned.                | Maintainer identifies the operator. Run the plan's keyboard, Narrator, contrast, scaling and locale acceptance against the exact candidate in the available OS lanes. Restore any temporary screen-saver setting afterward.                   |
| GitHub protected packaging      | Native repository environment enumeration returns zero environments. Exact `GET environments/store-submission` returns HTTP 404. No environment was created or changed.                                                                      | Maintainer supplies an agreed independent GitHub user/team. Prepare the exact environment configuration for deliberate approval, including the existing contract's required reviewer and self-review prevention.                              |
| Hosted interactive lane         | There are zero repository self-hosted runners, and no `interactive-qualification` environment exists. The separately authorized local final-acceptance route remains available; it does not imply a configured hosted lane.                  | Maintainer chooses/provides a dedicated runner/operator only if this hosted route is to be qualified. Preserve its existing reviewer protection and isolation requirements.                                                                   |
| Rights and corresponding source | Native notices, source preparation/reconstruction procedures and prior inventories exist. Current tool/module inputs and the final application artifact still need exact applicable rights/source evidence and qualified sign-off.           | Maintainer identifies a qualified reviewer; engineer assembles the exact technical inventory and corresponding-source artifact for that review. No legal approval is inferred from a collector or successful package build.                   |
| Historical credential           | Source retirement does not establish revocation or replacement by the credential owner. No credential value is inspected or published here.                                                                                                  | Maintainer identifies the actual credential owner and supplies attributable revocation/replacement evidence through the appropriate private channel.                                                                                          |
| Trusted Store flight            | The installed historical Store baseline is retained. No agreed Partner Center operator, authorized flight or Microsoft-signed v2 candidate is established.                                                                                   | Credentialed release owner prepares the deliberate selected-tester flight decision after qualification; prove the trusted same-identity upgrade before public release.                                                                        |
| Alert administration            | [Static dispositions](implementation/2026-10-25-codeql-result-dispositions.md) cover all 29 current and five historical results. GitHub states/analyses remain unchanged.                                                                    | Maintainer deliberately authorizes any exact current-alert disposition and separate historical-configuration reconciliation; preserve original analysis evidence.                                                                             |

Availability questions for the device lanes, human/legal operators and independent
GitHub/Partner Center owners have been presented to the maintainer. No answer,
elapsed wait or existing workflow name establishes an owner or approval.

## Why another Store run is premature

The current [Store workflow](../.github/workflows/store-package.yml) reads the
existing environment before its package job. HTTP failure refuses admission;
the package job depends on that successful admission. The maintained
[native-response predicate](../scripts/Test-ProtectedWorkflowAdmission.ps1)
requires the selected environment, a required reviewer, self-review prevention
and a successful manual CI producer for the exact trusted source/run.
It does not create protection or accept a synthesized environment response.

The current absence is therefore an unmet operational prerequisite, with a
fail-closed source path already implemented. No new package build, weaker
predicate, automatic workflow trigger or fabricated protection fixture is a
repair for missing agreed reviewers. Existing static protection controls remain
source evidence; actual hosted execution/protection still needs qualification.

## Admission to the final campaign

The engineer freezes one clean, signed source candidate after the consolidated
engineering changes and authoritative version audit. Before launching its full
campaign, this register must name the required operators and available lanes,
and identify the intended evidence for every gate. Source publication and each
remote dispatch retain their scoped authorization requirements.

Use the existing native build/test/package and Microsoft tooling against that
candidate. Record source, exact artifact hashes, actual architecture/OS,
operator, command/report and outcome. Reuse earlier observations only within
their real input scope; a compile result is not physical ARM64 execution.
Do not rerun software because HISEW asks for refreshed bookkeeping.

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
