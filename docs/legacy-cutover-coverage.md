# Legacy cutover: outcome coverage and remaining proof

Status: **coverage map, not cutover approval**. The modern implementation exists
in parallel with the quarantined historical tree. Do not delete that tree until
[Task 19's pre-cutover gates](plans/2026-08-30-jpg-spinner-modernization.md#task-19-cut-over-in-one-auditable-change-and-delete-the-legacy-implementation)
have actual passing evidence. A test declaration is not an execution result;
retained development runs do not qualify a different release candidate.

## Preserved user outcomes

The links identify maintained executable tests, not copied legacy implementation.
Read the named cases in the linked files when selecting a bounded regression.

| Outcome                                                   | Maintained test boundary                                                                                                                                                                                                                                                                                                | What it can establish / remaining limit                                                                                                                                                       |
| --------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Select a source folder; cancel without losing review      | [Actual picker cases](../tests/JpgSpinner.Presentation.Tests/WindowStateAutomationTests.cpp), especially “A real picked folder reaches a non-destructive eligible review”; [grant retention](../tests/JpgSpinner.Presentation.Tests/SourceFolderAccessRetentionTests.cpp)                                               | Actual picker/UI Automation requires the approved interactive lane; headless grant tests cannot replace it.                                                                                   |
| Direct or recursive traversal with honest exclusions      | [Native Storage collection](../tests/JpgSpinner.WindowsStorage.Tests/WindowsStorageImageSourceCollectionTests.cpp); [native recursive review](../tests/JpgSpinner.Presentation.Tests/MainWindowViewModelTests.cpp)                                                                                                      | Real paged Storage enumeration, containment, exclusions and review counts; actual AppContainer grant qualification is separate.                                                               |
| All eight Exif orientations                               | [Orientation mapping](../tests/JpgSpinner.Domain.Tests/ExifOrientationTests.cpp); [coefficient transformer](../tests/JpgSpinner.JpegTransformation.Tests/LibJpegTurboCoefficientTransformerTests.cpp); [public engine](../tests/JpgSpinner.JpegTransformation.Tests/JpegTransformationEngineTests.cpp)                  | Independently derived coefficient fields and resulting orientation, not visual plausibility or decoding alone. Final format/precision corpus remains required.                                |
| Perfect transformation by default; explicit edge loss     | [Domain MCU-edge planning](../tests/JpgSpinner.Domain.Tests/JpegTransformPlannerTests.cpp); [native coefficient edges](../tests/JpgSpinner.JpegTransformation.Tests/LibJpegTurboCoefficientTransformerTests.cpp); [exact review consent](../tests/JpgSpinner.Presentation.Tests/MainWindowViewModelTests.cpp)           | Exact source-edge loss, refusal without required consent and review invalidation. Actual readable disclosure remains an accessibility/picker check.                                           |
| Preserve, sequential or progressive output choice         | [Three-state planning](../tests/JpgSpinner.Domain.Tests/JpegTransformPlannerTests.cpp); [output scan organization](../tests/JpgSpinner.JpegTransformation.Tests/LibJpegTurboCoefficientTransformerTests.cpp)                                                                                                            | Explicit output policy with preserved entropy coding; identity and transformed image cases are distinct.                                                                                      |
| Preserve metadata consistently with transformed pixels    | [Metadata reconciliation](../tests/JpgSpinner.JpegTransformation.Tests/MetadataReconcilerTests.cpp); [independent output validation](../tests/JpgSpinner.JpegTransformation.Tests/JpegOutputValidatorTests.cpp); [Extended XMP policy](../tests/JpgSpinner.JpegTransformation.Tests/ExtendedXmpPolicyTests.cpp)         | Exact preservation/removal/refusal semantics; unsupported mutation is refused rather than flattened or silently dropped.                                                                      |
| Create corrected copies without modifying sources         | [Real copy transactions](../tests/JpgSpinner.WindowsStorage.Tests/ImageFileTransactionTests.cpp), including exact validated bytes, nested paths, existing destinations and substituted identities                                                                                                                       | Real native effects and source-byte preservation, not a mock's success code. Provider and long-path prerequisites still apply.                                                                |
| Replace only with independently verified retained backup  | [Real replacement and backup controls](../tests/JpgSpinner.WindowsStorage.Tests/ImageFileTransactionTests.cpp); [backup disclosure](../tests/JpgSpinner.Presentation.Tests/MainWindowViewModelTests.cpp)                                                                                                                | Backup hash/flush/close/reopen, stale revision refusal and replacement uncertainty. No unbacked replacement path is preserved as a feature.                                                   |
| Recover interrupted work without losing originals/backups | [Immutable recovery](../tests/JpgSpinner.WindowsStorage.Tests/ImageFileTransactionRecoveryTests.cpp); [recovery catalog](../tests/JpgSpinner.WindowsStorage.Tests/ImageFileTransactionRecoveryCatalogTests.cpp); [process termination](../tests/JpgSpinner.WindowsStorage.Tests/TransactionProcessTerminationTests.cpp) | Modeled native interruptions and deterministic identity/hash outcomes; no undocumented power-failure durability guarantee.                                                                    |
| Cancel and stop at safe boundaries                        | [Batch cancellation](../tests/JpgSpinner.BatchProcessing.Tests/BatchProcessingCoordinatorTests.cpp); [window close](../tests/JpgSpinner.Presentation.Tests/CooperativeWindowCloseTests.cpp); [presentation cancellation](../tests/JpgSpinner.Presentation.Tests/MainWindowViewModelTests.cpp)                           | Cancellation does not relabel committed output, admit another transaction or discard unresolved recovery.                                                                                     |
| Understand localized controls, errors and results         | [Native PRI localization](../tests/JpgSpinner.Presentation.Tests/LocalizationCompletenessTests.cpp); [accessibility declarations](../tests/JpgSpinner.Presentation.Tests/AccessibilityContractTests.cpp); [result states](../tests/JpgSpinner.Presentation.Tests/MainWindowViewModelTests.cpp)                          | Resource completeness/provenance and truthful state projection for en-US, en-GB and ru. Human translation, Narrator, contrast, focus and scaling are not established by those headless tests. |
| Report each source outcome deterministically              | [Batch terminal outcomes](../tests/JpgSpinner.BatchProcessing.Tests/BatchProcessingCoordinatorTests.cpp); [observable presentation results](../tests/JpgSpinner.Presentation.Tests/MainWindowViewModelTests.cpp)                                                                                                        | Exactly one terminal result per candidate, ordinal discovery order and distinct ordinary refusal/uncertain-commit behavior.                                                                   |

## Deliberately retired behavior

| Historical surface                                                              | Replacement or reason for removal                                                                                                                                                                              |
| ------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Application Insights transport, package and embedded instrumentation credential | No replacement transport. Local redacted diagnostics require explicit export. A credentialed owner must separately revoke/rotate the historical credential; source removal cannot perform or prove revocation. |
| Obsolete settings flyouts and C++/CX/UWP XAML implementation                    | Native WinUI commands, policy controls and accessible review. Outcomes above survive; framework implementation and old UI shape do not.                                                                        |
| ARM32                                                                           | The approved architecture set is x86, x64 and ARM64. Cross-compilation alone does not prove native ARM64 operation.                                                                                            |
| Silent or ambiguous destructive correction                                      | Corrected copies by default; explicit backed replacement, exact trim consent and deterministic recovery. Unsupported/uncertain operations refuse.                                                              |
| Vendored IJG 9a, copied codec internals and private transform helpers           | Maintained libjpeg-turbo public APIs behind the tested transform boundary. Delete the quarantined codec at cutover; do not retain a wrapper, fork or scanning exclusion.                                       |
| Unsupported MPO/MPF, signed/JUMBF or payload-bearing trailing-data mutation     | Explicit scanner/engine refusal; no flattening into an apparently preserved JPEG.                                                                                                                              |

## Candidate-bound cutover evidence

For the selected immutable pre-cutover candidate, retain full test-selection
results, native binlogs, JUnit reports, ASan, static analysis and package/manifest
validation before deletion. The owner's approved 2026-10-24 sequencing amendment
moves only the actual trusted same-identity Store-upgrade smoke to the
pre-public-release gate. Retirement must still wait for every other pre-cutover
technical check, including the actual picker acceptance scenarios. Preserve the
signed Git baseline and installed Store app; no test package may masquerade as a
trusted upgrade. Microsoft documents the
[unsigned/signed identity restriction](https://learn.microsoft.com/windows/msix/package/unsigned-package)
and the supported [package-flight boundary](https://learn.microsoft.com/windows/apps/publish/package-flights).
The [installed Store observation](implementation/2026-10-17-published-store-baseline.md)
identifies main version 1.1.2.0 and distinct bundle version 2015.616.2316.3521;
neither identity matching nor baseline installation is an upgrade pass. Record the
operator, command, architecture, environment and raw report locator; do not turn
this mapping into a blanket passed checklist. The
[release register](release-qualification.md) records the outstanding device,
upgrade, accessibility, privacy, legal and credential-owner obligations.

The [resumed picker acceptance](implementation/2026-10-24-cutover-preparation.md#resumed-picker-acceptance-2026-10-24)
now supplies an actual passing result for both application-review scenarios on
the retained native candidate: two JUnit cases, suite counter 124, no failures,
errors or skips, and unchanged synthetic source bytes. This clears that input
lane gap without promoting the earlier timeout to success. The full cutover,
post-deletion verification and release gates remain separate.

Then tighten repository policy and prove its legacy-tree RED, delete the exact
Git-tracked legacy solution/tree in one separate auditable commit, repair stale
references, and obtain the post-cutover system GREEN. Preserve historical design
descriptions as history, not runnable legacy paths. Deliberate post-cutover
CodeQL evidence must reconcile the five existing alerts and their original
analysis category before claiming GitHub closure. Nothing here authorizes a
remote run, alert dismissal, signing, installation, Store submission or release.
