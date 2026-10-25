# JPG Spinner 2.0 release qualification

Status: **NOT QUALIFIED / NOT AUTHORIZED FOR RELEASE**. This register is a
maintained handoff, not a waiver. A source checkpoint, signed Git commit, native
build, installed package and Store submission are different outcomes. The
[approved plan](plans/2026-08-30-jpg-spinner-modernization.md)
owns full acceptance; nothing here replaces its thresholds.

The [2026-10-25 release completion replan](plans/2026-10-25-release-completion-replan.md)
defines the accepted execution order: consolidate the current engineering
batch and supplied-alert dispositions, establish device/human/legal/Store-flight
prerequisites, freeze one candidate, then run its final qualification campaign.
It preserves existing acceptance and authorization boundaries; it introduces no
new scan, automatic workflow or release waiver.

The [qualified tooling refresh](implementation/2026-10-25-qualified-tooling-refresh.md)
adopts Syft SDK 1.55.0 after native identity/integrity, independent fixture and
official SPDX validation, artifact refusals and preservation of all 38 real
component identities against the retained three-architecture payload. It also
updates the immutable CodeQL Action pin and selected Go workflow patch. This
closes that engineering slice without claiming a new application build,
additional CodeQL analysis, hosted Store execution or final release approval.

The [supplied CodeQL dispositions](implementation/2026-10-25-codeql-result-dispositions.md)
complete static source/boundary triage of 29 current and five historical inputs.
No product patch is justified by those claims; historical source obsolescence
is distinct from declaring the original findings false. GitHub states and
analyses remain unchanged. The [release prerequisites](release-prerequisites.md)
record the available host and outstanding qualification-lane and human owners.
The initial absence of Store protection is retained as history; the subsequent
[single-maintainer amendment](implementation/2026-10-25-maintainer-self-review.md)
configures that approval gate. Establish the remaining resources before the
frozen candidate's full campaign.

The maintainer now owns the human accessibility/localization checks and the
licensing/corresponding-source sign-off, as recorded in the canonical
[acceptance record](release-prerequisites.md#maintainer-owned-acceptance).
Their owner-availability gaps are resolved. Both outcomes remain pending until
the maintainer records results for the exact candidate; engineering supplies
the deployment, source-delivery and technical evidence inputs.

The maintainer's [2026-10-25 device amendment](implementation/2026-10-25-native-arm64-qualification.md)
accepts native Windows ARM64 VM execution and makes physical-device testing
optional and nonblocking. Manual CI now defines a separate deployment-only ARM64
runtime job using the same run's prebuilt binaries. Local focused source,
architecture and installer controls pass; that hosted job has not run yet.
Native packaged-app smoke and general interactive/accessibility, OS, performance,
legal and trusted Store-upgrade evidence remain separate requirements, eligible
for their appropriate qualified VM/desktop lanes. A headless pass supplies none
of those other results automatically.

GitHub Actions storage is a short-lived transfer service, not the release
evidence archive. All explicit artifact uploads use the native minimum
`retention-days: 1` and `compression-level: 9`. Retrieve the exact candidate's
required packages, reports, provenance and attestations into the approved
evidence store before that deadline; expiry does not waive any release gate.
Keep artifact IDs, source/run identity and digests with the retained originals.
CI dependency caching requires the explicit manual `cache_dependencies` boolean,
which defaults to false. This reduces shared cloud storage; it does not disable
native dependency integrity checks or local build caches. Workflow triggers
remain manual-only. When removing existing cloud artifacts, verify the retained
original archives first; remove reconstructible cloud caches by their exact IDs.
GitHub may update its usage meter after the deletion itself has completed.

This follows the short-lived producer-transfer pattern used in
[`json-quality` CI](https://github.com/Hadden-Industries/json-quality/blob/main/.github/workflows/ci.yml)
and the explicit one-day transfer retention in
[`owlapi` maintenance](https://github.com/Hadden-Industries/owlapi/blob/main/.github/workflows/maintenance.yml).
The native action documents
[minimum retention and compression](https://github.com/actions/upload-artifact#inputs);
GitHub documents [artifact deletion](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/remove-workflow-artifacts).

The [2.0 release-note draft](releases/2.0.0.md) is maintained as an unreleased
communication artifact. Its existence does not authorize installation,
distribution, cutover, certification or Store submission.

The [WinRT probe symbol-identity repair](implementation/2026-10-25-winrt-probe-symbol-identity.md)
addresses the final failure in manual attempt `40127045938` at signed `887bec0adf5b`.
All build lanes, ASan, analysis and interactive-input publication passed; packaging
reached a source-root-sensitive fixture namespace after native bundle controls.
The unchanged focused probe passes after stabilizing that fixture's qualified
names. The hosted attempt remains failed and its later packaging controls remain
unexecuted; another dispatch requires deliberate authorization.

The [published-alert remediation](implementation/2026-10-25-published-security-alert-remediation.md)
updates Ajv's locked `fast-uri` dependency to the compatible patched release.
All four advisory regressions and ordinary URI/Ajv controls pass after failing
against the old lock. The five historical CodeQL locations were already deleted
by signed source retirement; their absence gate and refusal controls pass.
After owner-authorized publication of signed `0d78e54b`, GitHub reports all four
Dependabot alerts fixed without dismissal. The separately authorized native
CodeQL run and historical configuration reconciliation remain distinct from this
dependency fix and from complete release qualification.

The [successful hosted-QA checkpoint](implementation/2026-10-25-hosted-qa-completion.md)
at exact signed `0d78e54b` completes all ten jobs: the five build lanes, source
policy, x64 ASan/analysis, input attestation and native unsigned packaging.
The four x86/x64 headless lanes contain 1,513 passing testcase executions with
no JUnit failure/error/skip; the extra x64 Release case exercises shutdown.
Actual package/control checks pass for all three architectures. ARM64 remains
compile/package evidence, and an upstream unused CMake option warning remains
disclosed. Original archives and native identities are retained before spent
cloud-copy removal; this is not final device, signed-upgrade or release proof.

The owner-approved [qualification timeout budgets](implementation/2026-10-24-qualification-timeouts.md)
supersede earlier workstation-sized process and workflow ceilings. All waits
remain finite; deliberate deadline-refusal tests and product assertions remain.
This policy does not relabel prior attempts or authorize another remote run.

The [hosted QA prerequisite repair](implementation/2026-10-24-hosted-qa-prerequisite-repair.md)
addresses the failed manual run at `8efb4d5`: its provenance fixture lacked two
adopted action pins, and the hosted image lacked the selected SDK 28000 inputs.
The owner-authorized follow-up at signed `9e7ac782` successfully installed that
SDK and compiled all five matrix configurations; ARM64, ASan and MSVC analysis
jobs passed. The attempt still failed because the COMDAT probe ran outside an
SDK-equipped lane and the unpackaged presentation host lacked deployed Windows
App Runtime packages. The follow-up moves the probe and adds Microsoft's pinned
native runtime deployment to executable test lanes. Local read-only identity
and source/refusal checks remain distinct from actual hosted deployment and
successful full QA. Both prior attempts remain failed; another remote run
requires deliberate authorization.

The [2026-10-24 Adobe static-library packaging qualification](implementation/2026-10-24-adobe-static-debug-information.md)
repairs missing compiler PDBs without dropping debug information or suppressing
linker warnings. Native Debug/Release package and independent public-client
links pass for x86/x64/ARM64; runtime initialization passes only on x86/x64.
The product's restored x64 JPEG suite passes all 165 testcase elements against
the revised recipe. This focused evidence does not qualify the final application
matrix, ARM64 runtime, independent package pair, SBOM, signed upgrade or release.

The [native dependency-refresh repair](implementation/2026-10-24-native-dependency-refresh.md)
addresses the later discovery that the actual ARM64 installation still consumed
Adobe revision 1. The actual public-package probe reaches LNK4099 RED and
revision-2 GREEN, with root-manifest pin alignment. An additional all-warning
MSBuild gate was rejected after actual ARM64 qualification exposed the SDK's
CS1668 library-search-path warning; it is retained as counterevidence. Final
warning-free application builds and desktop acceptance remain
separate; a successful build exit alone does not qualify either.

The repair's final supported headless checkpoint has exit-0 Release x86/x64/ARM64
builds, 378 passing headless cases on each of x86/x64 and a passing policy-control
component follow-up. MSB4121/LNK4099 are absent from the final build logs, while
SDK inline-task CS1668 warnings remain documented. This is not a complete
warning-free clean-source matrix, native ARM64 run, desktop acceptance or release.

The same repair's [current-source unsigned packaging checkpoint](implementation/2026-10-24-native-dependency-refresh.md#current-source-unsigned-packaging)
at signed `7edc40f4` passes actual SDK-selected Release x86/x64/ARM64 component
validation, native three-architecture bundling/extraction and clean-tree local
provenance. All four original containers and 39 supporting records are natively
retained before confirmed recoverable removal of the 802,706,055-byte workspace.
These are local incremental outputs; exact-bundle SBOM, independent clean-pair
comparison, hosted authentication, deployment and release remain unqualified.

On 2026-10-24 the owner waived the mandatory 24-hour aggregate fuzz campaign as
disproportionate. Final qualification retains the maintained 60-second scanner
mutation budget, full-corpus replay, native minimization and zero-growth merge
audit. The manual scanner workflow uses that budget. This is an explicit
reduction in duration-based assurance, not a completed long campaign; malformed
inputs, resource limits, deterministic regressions, sanitizers and storage/recovery
requirements remain. Other outstanding gates and review restrictions are unchanged.

The subsequent [native library-directory repair](implementation/2026-10-24-native-library-directory-selection.md)
resolves the CS1668 producer boundary through Microsoft's early C++ property
import hook, preserving ASan/libFuzzer path extensions and Spectre precedence.
All five supported local build lanes report zero warnings/errors, all four
x86/x64 Debug/Release headless lanes pass 378 cases each, the actual JPEG ASan
run passes, and the bounded scanner regression/replay/minimization/audit passes.
The normal JPEG test output is restored after ASan. This closes the recorded
local warning issue; it does not qualify an independent clean-source package
pair, exact final-bundle SBOM, device/desktop acceptance or release.

The subsequent [clean-source qualification checkpoint](implementation/2026-10-24-clean-source-qualification.md)
at signed `c716931f53b0f84f03dda58aa09b312a2ac351a2` passes two independent
native Release x86/x64/ARM64 package sets and the approved closed comparison.
Both complete payload/container byte-identity results remain false. The exact
selected first bundle passes native component/payload composition and official
SPDX validation for all 38 required identities; its original hash and binding
are retained. All three actual payloads preserve 24 exact notice assets and pass
55 native artwork controls each. The first clone's x64 Debug executable also
passes 88 real termination/recovery cases, with producer-owned fixture removal
confirmed. These local results do not supply hosted authentication, device/OS,
human accessibility, legal/source delivery, trusted upgrade, certification or
distribution approval.

The owner approved bounded qualification of the
[closed native metadata/block-map comparison amendment](implementation/2026-10-15-package-payload-comparison-contract.md).
It reduces the former unconditional byte-equality guarantee; original bytes and
raw identity results remain. Native controls and the separately reported v2
evaluation of the retained independent build pair now pass; final frozen-source,
hosted and release-candidate qualification remain. No other release gate changes.

At signed source `9e23722b30afaf827c1ac3df59aa682452ac8e82`, native synthetic
container controls pass the approved identity-only policy and refuse unrelated
changed/extra/omitted files or false native block hashes. New independent clean
three-architecture build sets complete, and their basename-only PDB checks pass,
but complete application comparison still refuses differing executable bytes.
Native inspection identifies clone-root strings in C++/WinRT error-origin data;
the next bounded native configuration qualification is not yet acceptance.
The [comparison checkpoint](implementation/2026-10-15-package-payload-comparison-contract.md)
retains the exact distinction. Historical failures are not relabelled.

The subsequent controlled-path pair at signed source
`b9cd6b9ade93621d5c135b2bd6932443bf6e45c5` also completes all six native builds,
but still refuses unrelated differences. ARM64/x86 executables are byte-identical;
x64 differs by 163 bytes despite matching application objects and recorded LINK
commands. Native XML inspection exposes reordered unchanged asset entries in the
block maps. No new exception is accepted and no qualified output is published.
Bounded native packaging/LTCG controls remain investigation, not release proof.

The protected manual Store workflow now has locally validated source composition
and seven negative controls for role, ordering, digest and privilege boundaries.
Its source remains fail-closed on any unsuccessful native comparison. The restored native
tools and current Go 1.27.2 pass the maintained SDK fixture, official SPDX schema
and semantic verifier; actual hosted action/protection/attestation execution,
exact successful clean-build binding and certification remain pending.

## Gate register

The [approved Catch2 3.16.1 overlay qualification](implementation/2026-10-17-catch2-native-overlay.md)
passes native Release builds for x86/x64/ARM64, all 378 headless cases on both
x86 and x64, focused x64 incremental analysis and closed-authority/refusal
controls. The original upstream source and unchanged BSL-1.0 notice are retained.
One bounded final-acceptance attempt at signed source
`e6c17b62658bb7197a220deee3e3982a0dd2e04c` returns 78 because Windows cannot
establish an unlocked desktop; no product test executes. Catch2 stays test-only;
physical ARM64 runtime, final desktop/OS/upgrade acceptance and exact final
SBOM/clean-build/hosted evidence remain pending.

On 2026-10-24 the unchanged signed-source Release x64 candidate passes the
maintained interactive acceptance driver for six automated UI cases and 178
assertions, without failures/errors/skips. Native preflight qualifies the
operator-approved local desktop. Temporary screen-saver suppression is restored
and native package enumeration confirms no test registration remains. The
[resumed acceptance record](implementation/2026-10-17-catch2-native-overlay.md#resumed-automated-acceptance-2026-10-24)
retains exact executable identity, raw reports and the earlier unavailable
attempt. Operator-assisted picker, human accessibility, device, upgrade and
release qualification are not inferred from these automated results.

For this continuation the owner's explicit instruction prohibits security and
external reviews, reserves full verification for the final consolidated
candidate, and forbids bookkeeping-driven test reruns. Those restrictions do not
turn outstanding security, legal, device or distribution gates into passes.

At signed source `d29b594df3f821a0771a47a420643bbde494f35f`, both new native
three-architecture sets finish with byte-identical executable pairs and accepted
WinMD-only identity variance. Identical SDK upload file maps and actual `/np`
commands still produce reordered block-map `File` entries. The comparison
correctly refuses them and publishes no qualified bundle. The exact first-bundle
Store-SBOM test passes with all 38 required component identities and official
validation; hosted authentication, comparison acceptance, certification and
release are separate, still pending. See the
[historical failure and subsequent approved v2 qualification](implementation/2026-10-15-package-payload-comparison-contract.md#order-only-qualification-and-adoption-2026-10-16).

The owner-approved whole-File-order extension now passes a separate native
validation/comparison of those same originals. It records comparison satisfaction,
both raw byte-identity results as false, exact original hashes, one qualified
WinMD block and actual File-order variance per architecture. Native negative
controls still refuse integrity, membership, per-file and envelope changes.
Historical failure reports are unchanged; neither this result nor the SBOM is
hosted attestation, final frozen-source review or release authorization.

The [legacy outcome coverage map](legacy-cutover-coverage.md) links supported
behaviors to maintained executable tests and names deliberately retired
surfaces. It is selection guidance, not pre-cutover execution/upgrade approval.

The [2026-10-24 cutover preparation checkpoint](implementation/2026-10-24-cutover-preparation.md)
records the source-bound native matrix, analysis, sanitizer and automated UI
results without relabelling the timed-out operator-assisted picker attempt.
The owner's sequencing amendment moves only the actual trusted same-identity
Store upgrade to the pre-public-release gate. All other pre-cutover checks still
apply. That preparation checkpoint left the legacy source and installed Store
application unchanged.

The [resumed two-case picker acceptance](implementation/2026-10-24-cutover-preparation.md#resumed-picker-acceptance-2026-10-24)
now passes on the same application/presentation source boundary: native exit 0,
two JUnit case elements, suite counter 124, no failures/errors/skips and unchanged
synthetic sources. Computer Use supplies only the real keyboard folder grants;
native UI Automation owns review, cancellation and trim-consent assertions.
The earlier broad-selection timeout is preserved, not relabelled. Screen-saver
state is restored and successful-run test registrations are gone. This clears
that recorded pre-cutover picker gap, not human accessibility, native device,
trusted upgrade or release qualification. Source retirement remains a separate
auditable change.

Signed Task 19 source-retirement checkpoint `e1eb2361` removes exactly 143 tracked files in
`JPG Spinner.sln` and `JPG Spinner/`, including C++/CX, the copied IJG codec,
obsolete XAML/assets and beta telemetry package configuration. The maintained
repository policy now enforces their absence and has independent reintroduction
controls through its public entry point. Signed baseline `ed87e990` preserves
the deleted source; the installed Store app is unchanged. Post-cutover technical
results are recorded with the
[cutover evidence](implementation/2026-10-24-cutover-preparation.md). This deletion
does not resolve GitHub analysis records, revoke the historical credential or
qualify distribution.

| Gate                                 | Current evidence boundary / remaining proof                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               | Accountable role                        |
| ------------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------- |
| Locked builds                        | At signed c716931f, two independent Release x86/x64/ARM64 application/package sets and eight Debug/Release x86/x64 full-solution invocations exit0. Their unsuppressed MSB8029 temporary-root diagnostic means they are not warning-free final qualification. The authoring-worktree five-lane matrix separately has zero warnings/errors. Protected Store source now selects a checkout-sibling build root; deliberate hosted execution remains pending.                                                                                                                                                                                 | Build maintainer                        |
| Tests and static analysis            | The four first-clone Debug/Release x86/x64 headless lanes each pass 378 testcase elements without failures/errors/skips or retries. Native ModernSolution /analyze Rebuild at unchanged c716931f first-party runtime/build/C++ test inputs completes with zero warnings/errors and 96 retained native XML reports. JPEG ASan passes 14 transform cases / 836,957 assertions. The supplied CodeQL run is recorded above; final-candidate native ARM64 VM execution and other named runtime gates remain pending. Physical-device testing is optional and nonblocking.                                                                      | Engineering owner                       |
| Parser fuzzing                       | Signed-source c716931f retains the passing 60-second native regression: 826,304 executions in 61 seconds, complete replay/minimization and zero-growth merge audit, with raw output under operator/a60716381c0e41b399cdd38d705ba39b. Owner waived the 24-hour campaign on 2026-10-24; it is neither required nor claimed completed. Preserve deterministic failures and other resource/sanitizer/recovery requirements.                                                                                                                                                                                                                   | Engineering owner                       |
| Metadata/preservation                | Retained synthetic cases do not replace final dependency revision qualification                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           | Image-processing owner                  |
| Storage/recovery                     | The exact first-clone x64 Debug storage executable passes 88 real termination/recovery cases; all recovery children exit0, all 176 original JUnit reports and receipt remain, and producer-owned fixture absence is confirmed. Existing identity/backup/journal/refusal tests pass in all four headless lanes. This does not assert power-failure durability, other providers/filesystems or physical ARM64 qualification.                                                                                                                                                                                                                | Storage owner                           |
| AppContainer                         | Actual grant/denial development cases exist; final packaged candidate and capability/traffic checks remain                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                | Windows qualification owner             |
| UI/accessibility/localization        | [The 2026-10-17 native picker/startup repair](implementation/2026-10-17-native-picker-focus-continuity.md) retains actual RED/GREEN and passes all six x64 Release automated UI cases (178 assertions) through the native console development entry point. Foreground-probe refusals remain distinct; that console run is not the separately qualified final acceptance driver. Earlier x64 Debug reachability passed 15 assertions with owned-fixture removal readback. Final candidate, Narrator, high contrast, keyboard, scaling and locale acceptance remain                                                                         | Accessibility owner                     |
| Dedicated interactive workflow       | Deployment-only, manual same-main CI input authentication, protected-environment admission and automated-only signed-result source wiring pass local contracts; live environment/runner setup and deliberate hosted execution remain pending                                                                                                                                                                                                                                                                                                                                                                                              | Windows qualification owner             |
| OS/device matrix                     | Minimum OS, approved long-path-disabled refusal lane and physical ARM64 remain                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            | Windows qualification owner             |
| Independent build comparison         | [Exact signed-source c716931f qualification](implementation/2026-10-24-clean-source-qualification.md) passes both original independent Release x86/x64/ARM64 package sets and approved v2 comparison. Raw complete payload/container bytes differ; exact hashes and original manifests remain. MSB8029 in the temporary build roots is disclosed, so these are not warning-free final qualification. Authenticated hosted execution, device/OS and distribution gates remain; no full byte-reproducibility claim.                                                                                                                         | Build maintainer                        |
| SBOM                                 | [Exact c716931f first-bundle qualification](implementation/2026-10-24-clean-source-qualification.md#exact-first-bundle-sbom) passes native installed-state/lock composition, all 38 required identities, exact payload membership/hashes, unchanged official SPDX 3.0.1 schema/Ajv 8.20.0 and Workgroup semantics. Selected unsigned bundle SHA-256 is 5a35fb9b0f3a7587c63c869017b3c779bd76dc44e65058b6caef85a3eca02c04. Local binding is not hosted attestation, static-linkage proof or complete deployed/runtime coverage. Signed-route integration, authenticated hosted binding and final tool rights/security qualification remain. | Supply-chain owner                      |
| Package identity/update              | [Native installed Store baseline](implementation/2026-10-17-published-store-baseline.md): main 1.1.2.0 x64, bundle 2015.616.2316.3521, matching authored name/publisher/application ID, Store / Ok registration. [Native packaging qualification](implementation/2026-10-17-independent-bundle-version.md) passes for fixed bundle 2026.2.0.0 with unchanged application 2.0.0.0; this is not a new build, exact final SBOM or signed upgrade. Actual signed update, bundle ordering/restore and all-architecture runtime qualification remain                                                                                            | Release owner                           |
| Signing/WACK                         | Native selected-SDK signature checks accept a genuine Microsoft-signed dependency and refuse unsigned/tampered controls without trust changes; no final trusted signed JPG Spinner bundle/WACK approval claimed                                                                                                                                                                                                                                                                                                                                                                                                                           | Credentialed release owner              |
| Store-submission contract            | Microsoft supplies the Store signature after certification. Existing source/schema/producer, retained-bundle SBOM and refusal controls pass within their recorded scopes. The [single-maintainer amendment](implementation/2026-10-25-maintainer-self-review.md) allows self-review and natively configures Store approval for MaksymShostak on main. Publish the compatible guard, then deliberately qualify hosted execution, authenticated provenance, WACK, submission and post-certification evidence. No run is started or automatically approved. Signed sideload verification is unchanged.                                       | Release owner                           |
| Licensing/source                     | The exact c716931f selected bundle preserves all 24 expected application/build-input notice assets in x86/x64/ARM64 payloads; native byte and artwork checks pass. [Native release-tool notice collection](implementation/2026-10-16-native-release-tool-notice-qualification.md) retains original documents and 160 contributing Windows Go-module report entries, with dual-license/non-Go limitations. Actual binary contribution, notice sufficiency, GPL/Exiv2 compatibility, complete corresponding-source assembly/reconstruction/delivery, exact SDK terms and qualified legal approval remain pending.                           | Maintainer and qualified legal reviewer |
| Retired telemetry                    | New source must remain transport-free; historical credential revocation is a private credentialed-owner action, not a source-edit claim                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   | Resource owner                          |
| Legacy deletion / open CodeQL alerts | Signed e1eb2361 retires the affected legacy source. [Static dispositions](implementation/2026-10-25-codeql-result-dispositions.md) preserve one result per 29 current and five historical inputs. Positive source/boundary evidence defeats the current claims; historical obsolescence does not declare the original findings false. GitHub open states/analyses remain unchanged; deliberate administrative reconciliation and final-candidate qualification remain pending. No query exclusion, vendor fork or new scan is introduced.                                                                                                 | Engineering owner / maintainer          |
| Resource hygiene                     | Remove eligible owned scratch after consumers finish; preserve required evidence and unresolved ownership                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 | Execution owner                         |
| Final reviews                        | Only final consolidated task candidates; narrow repair follow-ups; earlier reviews cover only their frozen revisions                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      | Engineering owner                       |
| Store/rollout                        | Submission and 5%/25%/100% observation gates not performed or authorized by this document                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 | Credentialed release owner              |

Every gate needs an actual date, operator, exact command/report locator, result
and immutable candidate identity before changing to qualified. A missing field
is pending, not an implied approval. Required final fuzz duration, health sample
counts, observation windows and zero-integrity-incident rules remain those in
Task 20. Never invent health observations for an unreleased application.

## Evidence identity

At source checkpoint `db669828941ce99a1e8582f9463c978fddaa88e7` on 2026-10-16,
`Invoke-TestSuite.ps1 -Project HeadlessAll -Configuration Release -Architecture x64`
completed against the existing Release executables: 378 JUnit test-case elements
across five suites, with no failure, error or skipped elements. All five raw
reports and the exercised executable hashes are retained under external inventory
`0e3dfdc53a4649ad99eda39e3be5c104`, execution
`352c2077-8e60-441f-ac25-4b13a115a082`. No compiler, deployment or desktop session
ran. This is not a fresh build or final-candidate/full-verification acceptance.
The [SDK terms inventory](licenses/nuget/README.md) separately records retrieval
of the declared external RTF, preserving its 2018 EULA identity and leaving exact
applicability and qualified legal review unresolved.

The [corresponding-source procedure](corresponding-source.md) now defines native
source/patch/registry preparation, binary binding, independent reconstruction
and recipient delivery checks. Maintained structural negative controls pass;
they do not qualify an assembled source artifact or discharge legal obligations.

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

The approved SBOM scope distinguishes build-context components, exact signed
artifact bytes and separately qualified deployment/runtime behaviour. See the
[supply-chain evidence procedure](supply-chain-evidence.md) for maintained
native producers, bounded validation, exact hash binding and remaining claims.
Do not infer static-linker contribution or instrumented runtime coverage from
installed package membership, or call input observations a build attestation.

## Release authorization

This register grants no signing, secret access, Store submission, rollout or
remote publication permission. The existing Store identity must be retained.
After all gates, obtain deliberate authorization for the exact candidate and
destination. Any source/backup/recovery integrity report pauses rollout regardless
of aggregate crash rate. Use Store health and opt-in local diagnostics; do not
add telemetry as a release remedy.
