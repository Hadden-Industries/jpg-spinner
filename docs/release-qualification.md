# JPG Spinner 2.0 release qualification

Status: **NOT QUALIFIED / NOT AUTHORIZED FOR RELEASE**. This register is a
maintained handoff, not a waiver. A source checkpoint, signed Git commit, native
build, installed package and Store submission are different outcomes. The
[approved plan](plans/2026-08-30-jpg-spinner-modernization.md)
owns full acceptance; nothing here replaces its thresholds.

The [2.0 release-note draft](releases/2.0.0.md) is maintained as an unreleased
communication artifact. Its existence does not authorize installation,
distribution, cutover, certification or Store submission.

The [2026-10-24 Adobe static-library packaging qualification](implementation/2026-10-24-adobe-static-debug-information.md)
repairs missing compiler PDBs without dropping debug information or suppressing
linker warnings. Native Debug/Release package and independent public-client
links pass for x86/x64/ARM64; runtime initialization passes only on x86/x64.
The product's restored x64 JPEG suite passes all 165 testcase elements against
the revised recipe. This focused evidence does not qualify the final application
matrix, ARM64 runtime, independent package pair, SBOM, signed upgrade or release.

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

| Gate                                 | Current evidence boundary / remaining proof                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                | Accountable role                        |
| ------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | --------------------------------------- |
| Locked builds                        | Local native x86/x64/ARM64 package checkpoints exist; final exact clean-source and hosted matrix remain                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    | Build maintainer                        |
| Tests and static analysis            | Native incremental analysis and retained x64 headless suites pass; a reproduced exit-time dispatcher hang is repaired with a bounded regression; final clean-candidate analysis/CodeQL/ASan remain                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         | Engineering owner                       |
| Parser fuzzing                       | Native replay, corpus minimization and a zero-growth merge audit pass bounded development controls; required final aggregate qualification remains                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         | Engineering owner                       |
| Metadata/preservation                | Retained synthetic cases do not replace final dependency revision qualification                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            | Image-processing owner                  |
| Storage/recovery                     | Retained identity/backup/journal/refusal cases; final supported-provider and interruption matrix remain                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    | Storage owner                           |
| AppContainer                         | Actual grant/denial development cases exist; final packaged candidate and capability/traffic checks remain                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 | Windows qualification owner             |
| UI/accessibility/localization        | [The 2026-10-17 native picker/startup repair](implementation/2026-10-17-native-picker-focus-continuity.md) retains actual RED/GREEN and passes all six x64 Release automated UI cases (178 assertions) through the native console development entry point. Foreground-probe refusals remain distinct; that console run is not the separately qualified final acceptance driver. Earlier x64 Debug reachability passed 15 assertions with owned-fixture removal readback. Final candidate, Narrator, high contrast, keyboard, scaling and locale acceptance remain                                                                                                                                                                                                                                                                          | Accessibility owner                     |
| Dedicated interactive workflow       | Deployment-only, manual same-main CI input authentication, protected-environment admission and automated-only signed-result source wiring pass local contracts; live environment/runner setup and deliberate hosted execution remain pending                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               | Windows qualification owner             |
| OS/device matrix                     | Minimum OS, approved long-path-disabled refusal lane and physical ARM64 remain                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             | Windows qualification owner             |
| Independent build comparison         | Native v2 validation/comparison of both original independent Release x86/x64/ARM64 sets at signed source `d29b594df3f821a0771a47a420643bbde494f35f` passes the approved WinMD/consequential-block/whole-File-order policy. Executables agree; complete payload/container bytes do not. Raw originals and v1 failures remain. Final source verification/reviews, hosted execution and exact final release candidate remain; no full byte-reproducibility claim                                                                                                                                                                                                                                                                                                                                                                              | Build maintainer                        |
| SBOM                                 | Native build-context composition retains 21 triplet-qualified ports and 17 resolved NuGet identities. [Exact retained-bundle development qualification](implementation/2026-10-17-independent-bundle-version.md#exact-retained-bundle-sbom-development-qualification) passes actual unsigned bundle `2026.2.0.0` payload/component rechecks, official SPDX 3.0.1/Ajv 8.20.0 and Workgroup validation, with a retained exact submission binding. This does not qualify the subsequent UI repair or Catch2 3.16.1 build context. Release trust/digest refusals remain; exact final clean-build/hosted binding, signed-route integration and final tool rights/security qualification remain; no compliant release SBOM                                                                                                                       | Supply-chain owner                      |
| Package identity/update              | [Native installed Store baseline](implementation/2026-10-17-published-store-baseline.md): main 1.1.2.0 x64, bundle 2015.616.2316.3521, matching authored name/publisher/application ID, Store / Ok registration. [Native packaging qualification](implementation/2026-10-17-independent-bundle-version.md) passes for fixed bundle 2026.2.0.0 with unchanged application 2.0.0.0; this is not a new build, exact final SBOM or signed upgrade. Actual signed update, bundle ordering/restore and all-architecture runtime qualification remain                                                                                                                                                                                                                                                                                             | Release owner                           |
| Signing/WACK                         | Native selected-SDK signature checks accept a genuine Microsoft-signed dependency and refuse unsigned/tampered controls without trust changes; no final trusted signed JPG Spinner bundle/WACK approval claimed                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            | Credentialed release owner              |
| Store-submission contract            | Approved 2026-10-15: Microsoft signs after certification. Separate submission schema/producer and signed-route preservation controls pass. Following retained timeout/extraction diagnosis, literal native cache preparation passes integrity and real Store SBOM integration passes exact payload/component, official validation, binding and complete publication checks for the retained bundle; certification/release approval remain pending. Protected workflow/clean-build provenance, WACK, deliberate submission and post-certification artifact evidence remain pending; signed sideload verification is unchanged                                                                                                                                                                                                               | Release owner                           |
| Licensing/source                     | [Native release-tool notice collection](implementation/2026-10-16-native-release-tool-notice-qualification.md) preserves original documents and exposes a dual-license classifier limitation. All 160 external contributing Windows Go modules have native report entries; non-Go coverage and maintained adoption remain unqualified. Native/NuGet notice-byte checks pass. Actual unsigned Debug x64 and Release x86/x64/ARM64 MSIX packages preserve all 24 application/build-input notice assets at signed source `a086ec2dee7e0e226edc7158c8eccb771cdee57d`; native manifest/framework/artwork and source/target/payload controls pass. Final frozen-candidate inclusion, actual binary contribution, notice sufficiency, GPL/Exiv2 compatibility, corresponding source and current Store terms still need qualification/legal review | Maintainer and qualified legal reviewer |
| Retired telemetry                    | New source must remain transport-free; historical credential revocation is a private credentialed-owner action, not a source-edit claim                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    | Resource owner                          |
| Legacy deletion / open CodeQL alerts | Alerts [1](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/1), [2](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/2), [3](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/3), [4](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/4) and [5](https://github.com/Hadden-Industries/jpg-spinner/security/code-scanning/5) identify retained IJG 9a source. Task 19 must delete that source after accepted cutover proof; require deliberate post-cutover CodeQL evidence before claiming the alerts resolved. No exclusion, dismissal, codec fork or compatibility shim substitutes for removal.                                                                                                                                            | Engineering owner                       |
| Resource hygiene                     | Remove eligible owned scratch after consumers finish; preserve required evidence and unresolved ownership                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  | Execution owner                         |
| Final reviews                        | Only final consolidated task candidates; narrow repair follow-ups; earlier reviews cover only their frozen revisions                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       | Engineering owner                       |
| Store/rollout                        | Submission and 5%/25%/100% observation gates not performed or authorized by this document                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  | Credentialed release owner              |

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
