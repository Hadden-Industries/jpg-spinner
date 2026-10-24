# Clean-source package and recovery qualification

Status: local qualification of signed source
`c716931f53b0f84f03dda58aa09b312a2ac351a2`; **not release approval**. This
checkpoint follows the [native library-directory repair](2026-10-24-native-library-directory-selection.md)
and the owner's explicit waiver of the mandatory 24-hour fuzz campaign. It does
not waive the maintained bounded regression or the remaining release gates.

## Source and execution boundary

The maintained `Invoke-IndependentPackageBuildComparison.ps1` verifies the
selected source signature and clean authoring checkout before creating two
independent native clones with `--no-local --no-hardlinks --no-checkout`.
Both select the exact commit above. Neither takes application build output from
the authoring checkout. Locked native dependency restoration remains supported;
this is not a claim of a disconnected source rebuild or an empty machine-wide
package cache.

The existing controlled-path driver builds each clone at the same canonical
source path, then retains each completed checkout separately. This removes a
build-path variable without rewriting packages, metadata or executable bytes.
Architectures run sequentially with two MSBuild workers; child-only vcpkg
concurrency is also two. No desktop, signing, installation, trust change, Store
submission, remote CI dispatch, security review or external review is performed.

The official dependency freshness result in `native-dependency-freshness.log`
under operator group `b70f634d955345ee838ae8f64e56c9db`, produced on 2026-10-24
at 05:52:47 UTC, reports no newer version actionable through the approved package
authorities. It covers the revised Adobe `2025.03#2` root-pin/schema contract
recorded in `7edc40f4`; the subsequent library-path and CI/documentation changes
do not alter dependency pins. The earlier `resume-dependency-freshness.log`
precedes that root-pin repair and is not reused for this claim. Neither report
supplies qualified legal review or final release-tool rights clearance.

## Independent Release package sets

Both clones complete native Release application/package builds for x86, x64 and
ARM64: six actual builds. Native SDK package selection, bundling, validation,
extraction and the maintained closed comparison complete successfully. Source
signature verification and comparison satisfaction are true. The original
container hashes are:

| Build set | Original unsigned bundle SHA-256                                   |
| --------- | ------------------------------------------------------------------ |
| First     | `5a35fb9b0f3a7587c63c869017b3c779bd76dc44e65058b6caef85a3eca02c04` |
| Second    | `617f575d73f482c23054a601ef79e26b8a275d526412a48952bc42062d2b3e2f` |

The approved comparison permits only the qualified app WinMD identity variance,
directly consequential block-map hash/compressed-size variance and whole-File
sibling ordering. Every other difference remains a refusal. Both complete
payload-byte identity and complete container-byte identity are **false**. This
result satisfies that bounded comparison contract; it is not full byte
reproducibility, authenticated hosted provenance or approval to distribute.

Native binlog replay discloses MSB8029 in these disposable roots because their
intermediate/output directories are under Windows' temporary directory.
Microsoft's [MSB8029 guidance](https://learn.microsoft.com/en-us/visualstudio/msbuild/errors/msb8029?view=visualstudio)
requires output locations outside that directory to avoid incremental-build
issues. The diagnostic is retained and unsuppressed. Successful build exit,
closed package comparison and passing functional tests do not turn these runs
into warning-free final qualification. The earlier authoring-worktree matrix
outside the temporary directory retains its distinct zero-warning result.
For a later deliberately selected warning-free release lane, allocate an
independent build workspace outside the operating-system temporary directory;
do not hide the diagnostic by altering `TEMP`/`TMP` or its warning policy.

Raw original bundles, both complete payload manifests, native comparison report,
architecture provenance, SDK streams and all six original package-build binlogs
remain under operator group `864a71908a214ba88bbb6b63e2cf4c74`. The binlogs are
copied after those builds finish and before subsequent solution builds can
overwrite their ordinary output locations.

## Additional clean-source solution and headless lanes

The maintained `Invoke-Build.ps1` builds the full modern solution in each
retained clone for the following lanes. These calls follow the independent
package builds; the clones began with clean source and no borrowed application
output, but subsequent solution calls can reuse their own valid native outputs.
They are not represented as a new empty-output rebuild per call.

| Configuration | Architecture | First clone | Second clone | First-clone headless testcases |
| ------------- | ------------ | ----------- | ------------ | ------------------------------ |
| Debug         | x86          | Exit 0      | Exit 0       | 378                            |
| Debug         | x64          | Exit 0      | Exit 0       | 378                            |
| Release       | x86          | Exit 0      | Exit 0       | 378                            |
| Release       | x64          | Exit 0      | Exit 0       | 378                            |

All four first-clone `HeadlessAll` invocations return zero. Their 20 original
JUnit reports contain 1,512 testcase executions in total, covering 378 unique
cases across the four lanes, with no failure, error or skipped elements. They
exclude the deliberately separate packaged-runtime, UI Automation and
assistive-technology lanes. The 300-second outer deadline is not extended and
no failed/flaky retry is used. Actual storage discovery includes 10,000 native
file candidates and cancellation observations; those raw measurements are not
the full reference-hardware/ETW performance baseline.

The x64 JPEG ASan result and final bounded scanner regression at the unchanged
runtime/build/test input boundary remain those in the
[native-library checkpoint](2026-10-24-native-library-directory-selection.md#final-supported-headless-results).
Neither is rerun to refresh bookkeeping or to revive the waived campaign.

The final native `Invoke-StaticAnalysis.ps1` run uses `ModernSolution`, the
`Rebuild` target and one MSBuild worker. It completes against the same
first-party runtime/build/C++ test inputs in the authoring worktree. Only the
CI/source-contract/documentation edits described here differ from the signed
source; no application source or dependency pin changes. Microsoft's native
binlog replay reports zero warnings and zero errors. All 96 original native
analysis XML reports, the original binlog and both streams are retained. This is
the integrated compiler analysis route, not a security review or CodeQL run.

## Corrected future hosted build placement

The protected manual Store workflow also used `RUNNER_TEMP` for its independent
C++ outputs. It now derives a run-/attempt-specific sibling directory from the
documented [GitHub checkout workspace](https://docs.github.com/en/actions/reference/workflows-and-actions/variables).
Native `Split-Path`/`Join-Path` perform path composition; there is no new path
parser, hardcoded runner drive, environment masking or warning suppression.
The existing producer still rejects existing or source-overlapping outputs.

The workflow source contract first reaches RED on the original temporary-root
selection. After the correction, native YAML/actionlint/PowerShell validation
and all nine retained source-refusal controls pass, including a return to
`RUNNER_TEMP` and a build directory nested in the checkout. This is a bounded
source-composition test, not a second filesystem policy engine or an actual
hosted execution claim. Manual-only triggers, admission/protection, source
signature trust and exact artifact-evidence roles remain unchanged. The previous
local MSB8029 results remain historical warnings; this change does not relabel
them or trigger another application build matrix.

Final formatting exposes two older indentation-dependent negative-control
mutators. Their failed precondition remains retained. Native representation-
model serialization is also rejected because it drops the mandatory original
action-version comments. The two mutations now use YamlDotNet's actual block
nodes and source marks to insert only the intended field/step while preserving
all remaining source and comments. The pinned assembly's mark/style APIs are
checked directly; the final native consumer refuses all nine mutants at their
intended boundaries. Fixture/API diagnostic failures remain separate from the
passing product matrix and are not hidden as flaky product retries.

## Exact first-bundle SBOM

The established native vcpkg exporter captures the actual first clone's x86,
x64 and ARM64 installed dependency states. The maintained composition combines
21 triplet-qualified native identities with 17 resolved NuGet identities. It
retains original installed SPDX documents, the exact lock, closed component
contract and before/after/copy inventories. Catch2 remains a test dependency,
not an application root inferred into the runtime inventory.

`New-StoreSubmissionSbom.ps1` consumes the exact first bundle hash above. Its
native Syft SDK producer passes the unchanged official SPDX 3.0.1 schema through
Ajv 8.20.0 and the SPDX Workgroup semantic verifier. The independent evidence
gate verifies exact payload membership/hashes and all 38 required component
identities before publishing `submission-sbom/`. Subsequent readback matches
the selected bundle and all four evidence hashes in `binding.json`.

The original `manifest.spdx.json` SHA-256 is
`f2fe0f750e029ec180ae98c62b90e8b140a27b28da38542e084a5f922f6ae26e`.
The binding identifies outer bundle `2026.2.0.0`, existing Store product
`HaddenIndustriesLtd.JPGSpinner` and the three architecture payloads. It states
`buildExecution: not-attested`, `staticLinkage: not-inferred`, separately pending
deployment/runtime qualification, Microsoft-managed signing after certification
and pending certification/release approval. Do not replace these limits with
claims about a signed artifact, static contribution or hosted authentication.

The maintained package-content checks also pass against all three actual
first-bundle payloads: 24 exact application/build-input notice assets and 55
independently decoded PNG dimension/plate controls per architecture. These
checks retain original notice bytes; they do not establish legal sufficiency,
complete binary contribution or live shell rendering.

The producer uses private Go 1.27.2, the already qualified SDK/validator pins and
existing Java/Node executables. The absent historical private Go copy is not
reported as reused: the official archive is restored and checked against
SHA-256 `1314008898bd40df77af4b014f777f08873dbdfbcd3d92308728ee03304fe04f`.
The system Go installation and persistent environment are unchanged. Native Go
module-integrity and original validator/build streams are retained separately
from regenerable tool and module caches.

## Native interruption and recovery

The maintained `Invoke-TransactionTerminationQualification.ps1` completes 88
real termination/recovery cases against the first clone's x64 Debug storage
test executable, SHA-256
`aa223d68db9a6f5c9004a72b871559eca7b13fd0ce43bb0d7520267825befdc0`.
Run `b226dd67-08f3-438c-a46b-8de827bc94bf` reports `completed: true`; every
recovery child returns zero. The driver retains all 176 original child/recovery
JUnit streams and its receipt before removing its exclusive fixture directory.
Readback confirms that exact directory absent. Intentional killed-child exit
`-1` is the tested interruption, not an unreported successful child exit.

These results cover the driver's actual native NTFS transaction boundaries.
They do not assert physical-media durability, undocumented power-failure
atomicity, other filesystems/providers or physical ARM64 execution.

## Resource disposition and remaining gates

The independent build workspace is declared in inventory
`51451a699ee24998858665e3bea38c09`. Both matrices, package/SBOM consumers and all
owned producer sessions finish before recording their release. Native
source-recovery/ownership remains unresolved. Its finite allowance rises from 24 to
32 GiB before the second clone's Debug output, based on 21,133,328,931 observed
logical bytes after the first Debug lanes and about 4.6 GB expected additional
Debug output. An allowance is not a claim of measured final usage or reclaimed
physical capacity.
After both matrices finish, direct source measurement is 31,170,730,657 logical
bytes, still within the 32 GiB allowance. Native group-wide ownership observation
remains unresolved for multiply linked members; finished consumers do not
authorize bypassing that check.
Inventory checkpoint `1f1410cf68f44db1a43888155ea763cf` retains all original
groups and maps 28 current binlog/JUnit originals to independent results.
Held resource `57dd3012169c4efda452f5b8344d84f1`, disposition
`16894ca7f74645efbb2124ff7c923799`, records released consumers, the exact native
ownership/recovery blocker, next actor and 2026-10-31 reassessment. A new product
run is not its release condition.

The SBOM workspace consumer is released. Native promotion retains 79 original
members in two bounded calls, but complete group observation refuses multiply
linked members without identifying the offending path. Checkpoint
`14462768ac7c4aa882e28661d029b793` therefore remains unknown, not verified.
Held resource `20f4203e5a8e4a14bebc2419dd56fea7`, disposition
`f0684e556b7c4e078a9f8edb6f3cad7b`, preserves the source, owner, next actor and
2026-10-31 reassessment. The [maintainer observation](https://github.com/Hadden-Industries/software-engineering-workflow/issues/197#issuecomment-6382372715)
records exact receipts and distinguishes an unresolved safety condition from a
proven engine defect. No custom deletion route or false preservation claim is
used. Earlier blocked working groups remain intact.

The [release register](../release-qualification.md) still owns final device/OS,
human accessibility/localization, performance, source/rights, authenticated
hosted qualification, trusted Store upgrade, WACK, certification and deliberate
distribution/rollout decisions. Current owner restrictions prohibit security
and external reviews; outstanding requirements are not converted into passes.
