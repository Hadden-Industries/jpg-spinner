# Cutover preparation and candidate-bound verification

Status: preparation checkpoint, **not source retirement or release approval**.
The legacy tree and installed Store application remain unchanged. This record
binds existing product verification to signed source
`906de8542b744238e8792375b2a71b1375d623ca` and separately describes the subsequent
test-tooling changes. It does not turn earlier evidence into proof of a later
application candidate.

## Approved sequencing and remaining boundary

On 2026-10-24 the owner approved separating legacy-source retirement from the
actual trusted same-identity Store-upgrade test. All other pre-cutover technical
checks must pass before deletion. Only that upgrade test moves to the
pre-public-release gate, using a Microsoft-signed candidate from a separately
authorized package flight. The signed Git baseline and installed Store app must
survive unchanged. No submission, flight, installation, trust change or public
rollout is authorized by this decision.

Microsoft documents both the
[unsigned/signed package identity restriction](https://learn.microsoft.com/windows/msix/package/unsigned-package)
and the supported [package-flight mechanism](https://learn.microsoft.com/windows/apps/publish/package-flights).
These are distinct distribution contracts, not reasons to impersonate a trusted
update with an unsigned test package.

## Product evidence at source 906de854

Native commands ran sequentially with bounded build concurrency, not in response
to repeated HISEW bookkeeping notices. Original logs, binlogs and JUnit reports
are retained in external evidence inventory
`96aa0e75761e4d9cb0abdda363d825c6`, under
`C:/Users/maksy/.hi/w/e/operator/b70f634d955345ee838ae8f64e56c9db`.
The evidence filenames begin with `candidate-906de854-`.

| Boundary                       | Actual result                                                                                                               | Limit                                                                                  |
| ------------------------------ | --------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------- |
| Native builds                  | x64 Debug/Release, x86 Debug/Release and ARM64 Release exit 0                                                               | ARM64 cross-build is not ARM64 runtime qualification.                                  |
| Headless suites                | Each of the four x86/x64 configuration runs contains 378 testcase elements across five suites; no failures, errors or skips | 1,512 testcase executions, not 1,512 distinct cases.                                   |
| MSVC analysis                  | Modern solution analysis rebuild exits 0                                                                                    | Not a CodeQL run or independent security review.                                       |
| AddressSanitizer               | Native JPEG selection: 95 cases, 838,563 assertions; exit 0                                                                 | Tag-selected sanitizer scope, not all 165 JPEG suite cases.                            |
| Repository and manifest policy | Both exit 0                                                                                                                 | Does not qualify certification, installation or update.                                |
| Automated desktop acceptance   | Six cases, 178 assertions; no failures, errors or skips; exit 0                                                             | Does not include the operator-assisted picker workflows or human accessibility checks. |
| Operator-assisted acceptance   | Bounded attempt terminates with exit 124                                                                                    | Unqualified input lane, not a product pass. No retry-until-green.                      |

The analyzer and sanitizer share ordinary output locations with other build
configurations. In particular, the sanitizer replaces the ordinary x64 Release
JPEG test output. The retained per-run reports identify what actually ran; do
not retrospectively identify every report with the executable currently at that
path.

Screen-saver suppression was temporary and its original enabled state was
restored and read back after both desktop attempts. Computer Use could display
the picker but could not admit its separately hosted input target; the refused
click sent no folder text. That attempt did not qualify either picker-driven
scenario. The later, explicitly requested bounded retry below supplies their
actual result; it does not relabel this timeout as a product pass.

### Resumed picker acceptance, 2026-10-24

The owner explicitly requested another Computer Use attempt. A fresh native
selection enumerated exactly the two `[automation][operator-assisted]` cases;
the broader `[operator-assisted]` selection also includes a separate packaged
source-access controller and is not the same test boundary. The preceding
broader attempt again reached its 300-second timeout and remains retained as
unqualified input-lane evidence, not a product failure or a passing test.

After resetting Computer Use, coordinate input into the separately hosted
`PickerHost.exe` was still refused. The supported keyboard operation did reach
the visibly focused Folder edit. Only each native fixture's synthetic source
path was entered, followed by ordinary Enter operations to navigate and confirm
that folder. Native UI Automation performed Analyze, review checks, picker
cancellation and trim-consent assertions. Start was never invoked. No production
folder hook, private picker notification or capability change was introduced.

The exact two cases pass with process exit 0, two JUnit `testcase` elements and
suite attributes `tests=124`, `failures=0`, `errors=0`, `skipped=0`. Catch2's suite
counter here must not be described as 124 distinct test cases. Both source-byte
preservation oracles pass. The operator-approved local desktop lane remains
distinct from an isolated VM or human assistive-technology qualification.

The application, presentation-test sources, shared build policy and dependency
inputs are unchanged between signed source `906de854` and checkpoint `d9157fe9`.
The actual exercised Release x64 binaries are identified independently:

| Artifact                            | SHA-256                                                            |
| ----------------------------------- | ------------------------------------------------------------------ |
| `JpgSpinner.Presentation.Tests.exe` | `12e7247bcc9183b94c6f92167eba33694a4a4b1867bfaa4305d65b3aa7e25af5` |
| `JpgSpinner.exe`                    | `2ba7db38d1131d9130fbac1e2c182bdd7e8df7a1186bc4d4e514a89d5a88daf5` |
| Retained picker JUnit report        | `d8625e83b50e7a9b6322e2b96dfe3eec587eeccaef136512d4bb443f82c23fd1` |

Original results are retained in the same external evidence group as
`resume-focused-picker-acceptance.log` and
`resume-focused-picker-acceptance.junit.xml`; the preceding timeout remains in
`resume-picker-acceptance.log`. The copied operator driver selects only the two
application-review cases and retains the original suppression/restoration
contract. Native readback confirms screen-saver active=1, secure=0, timeout=300.
Both successful-run UUID package registrations and synthetic working fixtures
are removed by the native fixture lifecycle. An extra isolated app instance
opened during activation recovery was closed; no owned app/test process remains.

The earlier timed-out registration was separately removed using the owner's
exact one-time command-guard approval. The installed Store baseline remains
`HaddenIndustriesLtd.JPGSpinner` 1.1.2.0, Store / Ok. Its app identity, files and
trust were not changed. The two previously blocked cleanup groups remain intact
pending the native HISEW fixes tracked in issues 197 and 187.

The stable-only dependency freshness check also reports no actionable newer
versions through the approved authorities; its original output is retained as
`resume-dependency-freshness.log`. No package pin changed. These focused results
clear the recorded picker input gap, not Task 20's device, accessibility, legal,
certification, security or deliberately authorized distribution gates.

## Maintained retirement policy: deliberate RED

`Test-RetiredImplementationAbsence.ps1` now checks the exact legacy root/solution,
copied codec filenames and explicit retired framework/telemetry source strings.
At the current checkout it fails on **only** `JPG Spinner.sln` and `JPG Spinner`.
The policy is deliberately not wired into the parallel-tree baseline's passing
repository gate yet; wiring belongs to the atomic cutover.

A valid synthetic source fixture and seven independent forbidden fixtures pass
the maintained controls. The positive control includes a filename with spaces
and ordinary C++ XOR. The first draft exposed an unrelated PowerShell scalar
`Count` error; it was corrected before interpreting the controls. The actual
checkout RED is a retirement-policy result, not that setup failure.

Reuse decision: [Git's native NUL-delimited inventory](https://git-scm.com/docs/git-ls-files)
owns tracked/non-ignored file selection. Direct root existence checks also
reject an ignored or empty retired directory. Existing MSBuild/effective-tool
policy and native C++20 compilation continue to own SDK and language semantics.
This lexical retirement gate does not implement a C++ parser, ban every caret
character, or claim to complete every Task 19 language/naming/documentation check.
Microsoft explicitly documents the
[/ZW and C++20 incompatibility](https://learn.microsoft.com/cpp/build/reference/zw-windows-runtime-compilation).
The legacy coexistence opt-out remains necessary until the actual cutover.

## Fuzz output ownership

The maintained fuzz driver now accepts an existing, empty, absolute caller-owned
`RunDirectory`. The default still creates a fresh UUID beneath the product fuzz
root. Explicit directories are checked before toolchain resolution: occupied
paths and redirected ancestors are refused, with prior evidence unchanged.
No HISEW dependency is introduced into the product test runner; the workflow
caller reserves the group and owns preservation/disposal through HISEW.

The corpus-minimization test supplies that exact reservation instead of finding
a new directory by comparing shared-root siblings. Its pre-change API-contract
run failed because the driver lacked `RunDirectory`; no fuzzer ran in that RED.
After the change, native fuzzing, full-corpus replay, native merge, zero-growth
coverage audit and unchanged duplicate input checks pass. An occupied-directory
negative control also passes without changing its sentinel. No custom corpus
minimizer, parser, output-rewriting shim or weaker fuzz limit is introduced.

The prepared driver's final 60-second-budget run exits 0: libFuzzer reports
438,020 executions over 61 seconds, followed by successful full-corpus replay,
native merge and a zero-growth audit (zero new files/features/coverage edges).
The final run contains 2,671 full-corpus files (2,617,234 bytes) and 231 minimized
files (70,475 bytes). Its twelve original streams/control files were independently
promoted by HISEW in inventory `78996691a11e44d5a8a8fe6b153e3110`, checkpoint
`3f1697dd80de4364bc6e1dc8d48dbff8`. This is the bounded pre-cutover fuzz check,
not Task 20's long-duration release campaign. Application sources and dependency
pins are unchanged by this preparation slice; the earlier source-906 matrix is
reported at its actual boundary, not rerun for these test-tooling/document edits.

The consolidated ordinary review found one caller integration gap: the manual
scanner workflow supplied `RUNNER_TEMP`, outside its always-uploaded artifact
root. The workflow now reserves regression output beneath
`artifacts/fuzz/scanner/regression`, so failure diagnostics survive even when a
regression failure prevents the later fuzz step. The native YAML source-wiring
contract reaches the specific missing-upload-root RED and then passes with the
corrected caller. Native actionlint/workflow policy also passes for that changed
workflow. This is local source validation, not a hosted run; its sole trigger
remains `workflow_dispatch`.

## Resource disposition

The exact timed-out registration
`JpgSpinner.UI.CE58129F-87EB-49D9-923E-95529DAB6AE0_2.0.0.0_x64__mc2sj62c1fkda`
was removed with the owner's one-time DCG approval. Native enumeration then found
no `JpgSpinner.UI.*` registration. The installed Store main version remains
1.1.2.0, `SignatureKind=Store`, `Status=Ok`.

HISEW inventory `1290cb0c8a7a46418195f91850964a25`, checkpoint
`80e7d9eed08e4baaa7430020bf0292e4`, independently preserves the temporary package
manifest and synthetic JPEG. Both groups have verified native coverage. Their
initial disposition publication failed with **Native project ownership bound
exhausted**, without removal. The owner's later publication succeeded. Its
two-location declaration and paraphrased authority did not satisfy the native
single-location/exact-authority contract; those were agent input mistakes,
recorded with the diagnostic gap in
[HISEW issue 199](https://github.com/Hadden-Industries/software-engineering-workflow/issues/199).

After explicit owner authorization, append-only single-location corrections
preserved that original record, both inventory groups and all evidence mappings.
Both exact fixture roots were then recycled through HISEW's public native route.
Fresh readback finds both absent; the Store app remains 1.1.2.0 / Store / Ok.
Recycling is recoverable removal, not proof of reclaimed physical disk capacity.

The first operation, `78a95bd4-dc12-424a-862a-5210abde9ba9`, has a retained native
Recycle Bin receipt, but higher-level reconciliation still refuses equivalent
forward-/backslash spellings. The native recycling inspector reports
`native-result-recorded`; one resource reconciliation exits 1 with `Native
recycling result is unestablished`. That source was not removed again. The
[concrete path bug evidence](https://github.com/Hadden-Industries/software-engineering-workflow/issues/179#issuecomment-6376487551)
includes the separate second-source preparation `KeyError` and all original
operation records. Next actor: HISEW maintainer fixes the typed-path reader, then
the owning coordinator reconciles that same first operation without rewriting
its context or repeating recycling.

For the second source, an inventory successor changed only its locator spelling
to native Windows separators, preserving both groups and independently verified
bytes. Operation `279bf0dd-bb1e-4662-b9ba-bd945f3e8ffe` then returned
`removed-confirmed`, `actualMode=recycled`, with absence/recovery readback and
terminal record `2132593de37140859b3076681c3fdb79`. The earlier pre-effect
preparation and all original spellings remain retained. This operational input
correction is not a durable HISEW fix and did not involve any engine patch,
private recycler, irreversible fallback or unrelated test rerun.

The earlier Adobe working graph remains held by native observation failures,
not by a need for further product verification. Its original selected evidence
and child-refinement history remain retained; no alternate deletion route is
authorized. Next actor: owning coordinator when the native observation blocker
is resolved. Do not rebuild merely to update its bookkeeping.

New retirement/fuzz fixtures are confined to preallocated working inventory
`3e4aca5d5e2f469a80893fa80f64b042`; preserve the focused raw results, release the
consumer and use native HISEW disposal after this preparation checkpoint.

## Delivery limits

No security or external review, remote CI dispatch, push, Store submission,
package flight or trusted upgrade was performed. Task 19's actual deletion and
post-cutover GREEN remain pending. Task 20's physical-device, minimum-OS, human
accessibility/localization, long-duration fuzzing, legal, certification and
deliberately authorized distribution gates remain separate and unmet where no
actual evidence exists.
