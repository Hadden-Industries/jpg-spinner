# Build-context SBOM evidence

This is release-engineering guidance, not permission to sign, publish or release.
JPG Spinner 2.0 remains [not release-qualified](release-qualification.md).

## Latest integrated development evidence (2026-10-16)

The actual independent pair at signed source
`d29b594df3f821a0771a47a420643bbde494f35f` completes all six native builds.
All three executable pairs are byte-identical, but reordered native block-map
file entries remain outside the approved comparison and correctly refuse
qualification. Serial MakeAppx `/np` is present in the actual SDK command;
mapping-file inputs agree. No qualified drop or complete-payload reproducibility
is claimed. The [comparison checkpoint](implementation/2026-10-15-package-payload-comparison-contract.md#actual-integrated-pair-outcome-and-next-decision-2026-10-16)
retains that historical result and the subsequently approved order-only policy.

Under the owner-approved v2 extension, a separate native validation/comparison
of the same original bundle pair now passes: only complete File sibling order
and the already qualified WinMD fields/consequences differ. The new report keeps
both raw byte-identity results false and records actual File orders. No source
rebuild, shipping-package rewrite or historical-result replacement occurs.

The completed first checkout supplies new native triplet exports and its actual
resolved NuGet lock: 38 required component identities. The real Store-submission
integration passes against that set's exact original bundle SHA-256
`3d83dbaf7ba8d9dac8fb4d9c92b6253cc0ada67b4e6af78eda1e041c8879d78f`,
including official SPDX schema/semantic validation and complete five-file
evidence publication. It preserves original bytes and records certification,
release approval and runtime qualification as pending. This is exact local
clean-build artifact evidence, not hosted provenance, a passing independent
comparison, Store acceptance or whole Task 17 completion.

## What each result establishes

| Result                               | Claim                                                                                            | Not established                                                                          |
| ------------------------------------ | ------------------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------- |
| Native installed vcpkg export        | Installed build closure of `exiv2` and `libjpeg-turbo`, separately for each target triplet       | Which static-library objects were linked or executed                                     |
| Application `packages.lock.json`     | Resolved direct/transitive package identities across all recorded targets                        | Every package is redistributed, or a build ran with this lock                            |
| Extracted payload inventory          | Exact relative filenames, lengths and SHA-256 values                                             | Container authenticity, installation or runtime behaviour                                |
| Native SPDX plus official validation | Format/semantic validity and membership matching independent required identities and file hashes | Completeness of arbitrary caller requirements or release approval                        |
| Release `binding.json`               | Hash linkage between the selected signed container and the exact SBOM/input-evidence files       | Build-execution attestation, linkage analysis or deployed/instrumented runtime inventory |

The approved scope is **build context plus exact artifact bytes**. SPDX
[distinguishes build, deployed and runtime inventories](https://spdx.github.io/spdx-spec/v3.0.1/model/Software/Vocabularies/SbomType/).
Retain installation/framework, supported-OS/device, privacy, dynamic-loading,
security, licensing and reproducibility evidence independently. Build-context
membership must not be used to invent redistribution or licensing conclusions.

## Prepare native component inputs

Use the exact qualified standalone vcpkg distribution pinned in
[`eng/sbom-tools.json`](../eng/sbom-tools.json), not a tool discovered from PATH
or a patched Visual Studio edition. Its native `export --raw` selects the
installed closure; repository code does not traverse a second dependency graph.
See Microsoft's [export contract](https://learn.microsoft.com/en-us/vcpkg/commands/export).

Run [`Export-JpgSpinnerVcpkgEvidence.ps1`](../scripts/Export-JpgSpinnerVcpkgEvidence.ps1)
sequentially for `arm64`, `x64` and `x86`, supplying the matching explicit
installed root, a qualified `VcpkgRoot`, and fresh external evidence/working
directories. The repository's installed-root layout is
`vcpkg_installed/<architecture>-windows-static-md`; each such root contains its
same-named target directory. Do not accidentally pass their common parent.
Keep the installed state quiescent. Retain native diagnostics, original SPDX
documents and ownership-list evidence. Recycle generated raw exports only after
their consumers finish; no automatic deletion occurs in the producer.

Pass the three resulting `ComponentEvidenceRoot` values as a hashtable with
exact keys `arm64`, `x64`, `x86` to
[`New-JpgSpinnerBuildComponentEvidence.ps1`](../scripts/New-JpgSpinnerBuildComponentEvidence.ps1),
together with the application lock (`src/JpgSpinner.App/packages.lock.json`)
and fresh external `OutputDirectory`/`WorkingDirectory`. Do not substitute a
test-project lock. The script preserves original native metadata/lock bytes,
checks copied and source snapshots, includes direct/transitive and RID-only
resolved package identities, and excludes owned-source `Project` entries from
external package requirements. It does not run NuGet restore. NuGet's own
[lock-file guidance](https://learn.microsoft.com/en-us/nuget/consume-packages/package-references-in-project-files#locking-dependencies)
owns resolution; the complete original lock remains a native cataloger input.

The composer returns `ComponentEvidenceRoot` and `ComponentContractPath` for
generation. Native-export provenance and actual clean-build binding remain
caller obligations; arbitrary JSON supplied by a caller is not proof that a
native export or application build ran.

## Prepare the bounded schema validator

SPDX [recommends Ajv for its JSON Schema](https://github.com/spdx/spdx-3-model/blob/develop/serialization/jsonld/validation.md#ajv).
The measured PowerShell `Test-Json`/JsonSchema.Net large-document path consumed
excessive memory; do not reintroduce it for the SPDX gate. Small repository
schemas still use the platform consumer.

Stage the committed `eng/spdx-validation/package.json` and `package-lock.json`
unchanged in a **fresh, owned external tool directory**. With the qualified Node
runtime and npm, restore there using:

```powershell
npm ci --prefix $schemaValidatorToolsDirectory --ignore-scripts --no-audit --no-fund
if ($LASTEXITCODE -ne 0) { throw 'Native locked validator restore failed.' }
```

`$schemaValidatorToolsDirectory` must already identify that explicit staged
directory; this example does not choose a disk location or grant acquisition
authority. Do not install globally, restore inside the payload, or resolve a
moving dependency version. Keep the upstream notices with any redistributed
tool copy; see [third-party qualification](../THIRD_PARTY_NOTICES.md).
Validation itself never installs tools or fetches remote schemas. Its owned
child uses a 512 MiB V8 old-space limit, 1 GiB working-set refusal and 30-second
deadline. Limit/tool/validator failures never become success.

## Restore the reviewed verification tools

[`Restore-SbomVerificationTools.ps1`](../scripts/Restore-SbomVerificationTools.ps1)
composes the already-qualified native tools, rather than inventing an archive,
dependency or schema implementation. Supply a frozen `DownloadDirectory` with
`vcpkg.exe`, `vcpkg-standalone-bundle.tar.gz`, `spdx-json-schema.json` and
`tools-java.zip`, acquired from the exact URLs in
[`eng/sbom-tools.json`](../eng/sbom-tools.json). All four digests must match that
single authority before output creation or execution. Original inputs stay held
read-only throughout restoration. Supply an explicit selected Node executable
and its npm CLI path; they are caller-owned toolchain inputs, not downloads
selected by this script.

The fresh external output retains Microsoft's native standalone bundle and
notices, the exact Workgroup verifier and schema, and unchanged copied npm locks.
Windows `tar`, .NET ZIP extraction and native `npm ci --ignore-scripts --no-audit
--no-fund` own their formats. No system-wide installation, global setting, module
resolution, cache repair or application compilation occurs. Partial failures stay
diagnostic; an existing destination is refused, not repaired or overwritten.

The real-archive provisioning control first reaches an explicitly unimplemented
restoration boundary (RED), then passes actual native extraction/npm restoration,
changed-tool-before-effects and reused-destination refusals (GREEN). Original
download hashes and staged/source locks stay unchanged. This is not SBOM,
application-build, hosted-execution or release qualification.

## Portable source-signature trust

[`eng/source-commit-allowed-signers`](../eng/source-commit-allowed-signers)
contains only the maintainer's existing reviewed public SSH signing identity,
not a private key or new signer. It makes the same native Git trust decision
available on an ephemeral hosted runner without depending on a personal file.
Changes to this file are trust-policy changes requiring review in protected main;
a file from an arbitrary checkout is not an independent trust anchor.

Use Git's documented [`gpg.ssh.allowedSignersFile`](https://git-scm.com/docs/git-config#gpgsshallowedSignersFile)
configuration and [`verify-commit`](https://git-scm.com/docs/git-verify-commit),
not a repository signature parser. The maintained native test verifies the real
signed source with command-scoped trust and confirms that an empty policy refuses
it, without changing any Git configuration. The protected workflow may configure
only its own disposable source checkout before the existing producer verifies
the exact commit. This is configuration qualification, not invented behavioural
RED, signature creation or proof that remote protection is configured.

## Prepare the native Go module cache

Stage unchanged copies of `eng/artifact-sbom/go.mod` and `go.sum` in an owned
external provisioning directory. Use an explicit private module/build cache,
the qualified Go executable, `GOTOOLCHAIN=local`, `GOWORK=off`, `GOENV=off`, empty
`GOFLAGS` and `GOMAXPROCS=2` in the **provisioning child only**. Do not change the
user's persistent environment or restore into the application payload.

On Windows, use the absolute extended namespace for `GOMODCACHE` **during
extraction as well as integrity verification**: `\\?\C:\...\module-cache`, or
`\\?\UNC\server\share\...` for a qualified UNC destination. Use backslashes
after the prefix. Microsoft's [namespace contract](https://learn.microsoft.com/windows/win32/fileio/naming-a-file)
preserves literal filename spelling. Ordinary extraction can remove a trailing
space from upstream test fixtures; adding the prefix only when verifying an
already damaged extraction cannot repair it.

[`Restore-GoModuleCache.ps1`](../scripts/Restore-GoModuleCache.ps1) provides this
bounded preparation boundary. Supply the module directory, explicit Go
executable, fresh disjoint module/build cache paths, fresh external working
directory and approved proxy. It copies both locks, uses only child environment
values, selects literal Windows extraction, retains separate diagnostic streams,
and refuses changed source/staged locks or pre-existing output roots. Native
download and integrity operations retain their 180-second deadlines without
retry. It returns ordinary absolute cache paths for subsequent offline builds;
it never repairs an existing cache or installs a tool.

In that bounded child, with the approved proxy/checksum database, run native
`go -C <staged-lock-directory> mod download -json`, then `go ... mod verify`.
Go owns [module authentication, extraction and cache integrity](https://go.dev/ref/mod#module-cache).
Do not use `all` unnecessarily: for this Go-versioned module, the unqualified
download selects its explicit requirements; `all` expands a substantially larger
graph. Preserve diagnostics, observe child exit, enforce the existing operation
deadlines and compare both staged locks with their unchanged source files.
No missing dependency, changed checksum or timeout is success. Native `verify`
checks downloaded members; it does not prove every dependency needed by a later
build is present. The subsequent offline, read-only build remains required.

Keep the literal prefix out of **compilation**: Go's embed glob handling treats
its `?` as a pattern character. Compilation uses the ordinary absolute spelling
of the same cache tree; the artifact producer already selects the literal
spelling for integrity verification. Do not rename individual upstream files,
rewrite hash records, disable integrity checks or modify vendor source. If an
owned generated cache is malformed, preserve the failed evidence and quarantine
the affected leaf before native extraction from its authenticated archive; never
silently repair a shared or unowned cache.

[`Test-SbomModuleCacheNamespace.ps1`](../scripts/tests/Test-SbomModuleCacheNamespace.ps1)
requires an authentic cached `go-mtree v0.7.0` archive and a short owned Windows
output root. With networking disabled, ordinary native extraction reproduces the
integrity failure and literal native extraction preserves the trailing-space
fixture and passes. It leaves both diagnostic cases for retention decisions.
This bounded environment regression is not application qualification.
[`Test-GoModuleCacheRestore.ps1`](../scripts/tests/Test-GoModuleCacheRestore.ps1)
exercises the maintained preparation entry point against an authenticated local
proxy snapshot, checks literal filename/lock/host-setting preservation and
refuses existing outputs. Removing literal extraction in an isolated copy causes
native integrity refusal. The initial fixture's newly promoted direct module
needed its own authenticated `go.mod` checksum; that setup correction does not
change the production graph or relax the unchanged-lock acceptance condition.

`ArtifactToolProcess.psm1` owns the shared bounded child/diagnostic contract for
preparation and generation. The consumers pass explicit directories/environment
values; maintained tests import the real boundary rather than extracting a
private function through a syntax tree. Native namespace and lifecycle controls
remain separate from complete SDK and real-container integration qualification.

## Generate and qualify

[`New-ArtifactSbom.ps1`](../scripts/New-ArtifactSbom.ps1) accepts already qualified
payload/component snapshots, the independent component contract, explicit public
name/version, private Go caches, pinned schema/Workgroup tool paths and the
`SchemaValidatorToolsDirectory`. It checks native Go integrity and builds the
adapter with two workers, then publishes only after file hashes/membership,
required packages, unchanged official schema and Workgroup checks succeed.
Use this route for clearly labelled unsigned **development qualification**;
it is not a substitute for signed-release acceptance.

[`Invoke-Sbom.ps1`](../scripts/Invoke-Sbom.ps1) is the release entry point. Supply
the exact `BundlePath` and independently selected `ExpectedBundleSha256`, the
frozen component inputs and native tool/cache arguments above, and fresh
same-volume output/working directories. It holds the container read-only,
verifies native Windows signature trust **before extraction**, validates all
three package identities, generates the external native SPDX document, rechecks
inputs and promotes one complete evidence directory without overwriting.
There is no unsigned bypass. No certificate/trust or installation effects occur.

Successful output contains only `manifest.spdx.json`, `payload.json`,
`build-component-inputs.json`, `required-components.json` and `binding.json`.
The binding uses the [closed repository schema](../eng/release-sbom-binding.schema.json);
it does not modify the native SPDX document. Keep this directory beside the
exact signed bundle and actual clean-build/release provenance.

Run [`Test-ReleaseSbom.ps1`](../scripts/tests/Test-ReleaseSbom.ps1) only with a
genuinely signed, qualified JPG Spinner input and protected-release authority.
That positive end-to-end release run remains pending. Unsigned/native component
tests and real signature-refusal controls cannot satisfy it. Remote CI remains
manual-only, and this guidance does not dispatch a workflow.

## Prebuilt interactive inputs

For the Store-only distribution distinction and independent native builds, see
[the approved Store route below](#approved-store-only-distinction).

The x64 Release CI producer stages only its existing application and presentation
test-host directories with [`New-InteractiveTestDrop.ps1`](../scripts/New-InteractiveTestDrop.ps1).
It copies every ordinary native output file, preserves assets/dependencies and
compares inventories before/after copying. It does not compile on the test VM.
The schema-bound `build-context.json` binds the selected commit and complete
payload inventory. [`Test-InteractiveTestDrop.ps1`](../scripts/Test-InteractiveTestDrop.ps1)
re-inventories actual bytes, rejects extra binary roots and returns the payload
root for existing deployment/test scripts. Hashes alone establish no publisher.

A separate hosted trusted-main publisher waits for policy, build, ASan and
analysis. It uses GitHub's native [generic attestation action](https://github.com/actions/attest/tree/1e69f48acb82d1966a394da916b4c1698aa569d6)
with the input-context predicate, then publishes `interactive-test-drop`.
Signing permissions are absent from compiler and self-hosted desktop jobs.
The [download action](https://github.com/actions/download-artifact/tree/9000827ccba6bdab643e8b6fd33ac0654aef8333)
must fail on digest mismatch. Consumers must independently verify the context's
platform attestation, source commit/ref and signer workflow with native
[`gh attestation verify`](https://cli.github.com/manual/gh_attestation_verify)
before running any downloaded executable. No custom signature verifier is used.

This attestation authenticates **inputs**, not UI execution or acceptance. A
successful hosted run, protected dedicated test lane and separately recorded
executed-test evidence remain pending. Local sentinel/copy tests and native YAML
validation cannot satisfy those gates. All workflows remain manual-only.

The [interactive workflow](../.github/workflows/interactive-qualification.yml)
requires a successful manually dispatched CI run for the **same main commit**.
It reads unfiltered native [workflow-run metadata](https://docs.github.com/en/rest/actions/workflow-runs#get-a-workflow-run)
and the [existing environment](https://docs.github.com/en/rest/deployments/environments#get-an-environment),
requiring nonempty required reviewers and a native Boolean self-review setting
before scheduling the dedicated runner. The owner's 2026-10-25 amendment permits
the sole maintainer to approve their own run: configure `prevent_self_review=false`
and retain deliberate reviewer approval. A `true` value remains admissible when
another reviewer is available; strings, numbers, null and omission are refused.
Referencing an environment name alone does not
configure its protection. An absent, unreadable or unprotected environment
refuses admission. Repository administrators must separately configure/review
reviewer membership, bypass policy and the dedicated runner; source changes here
do not create those live settings or prove VM isolation.

The [self-review checkpoint](implementation/2026-10-25-maintainer-self-review.md)
records the separately authorized live `store-submission` configuration and its
native readback. It does not establish a hosted interactive lane or approve a run.

The runner verifies the input attestation, re-inventories payload bytes and uses
the prebuilt host through `Invoke-TestSuite.ps1`; its native environment probe
runs first. No compilation, toolchain installation, signing key or trust-store
change occurs on that runner. The fixed selection
`[ui-automation]~[operator-assisted]` is bounded; unavailable desktops fail with 78. The workflow does not suppress or change the dedicated host's screen saver.
The lane owner must provide its ordinary unlocked interactive test session.

The native Catch2 [JUnit reporter](https://github.com/catchorg/Catch2/blob/v3.16.0/src/catch2/reporters/catch_reporter_junit.cpp)
counts assertions in the suite's `tests` attribute and can emit sections as
`testcase` elements. The result uses `assertionCount` and `reportedCaseCount`,
not an invented executed-test count. The platform XML reader prohibits DTDs and
is size-bounded; zero execution, failures, errors, skips and stale timestamps
refuse publication. A separate hosted job checks strict schema/run identities,
input/report digests and the input attestation before signing the native report
with the automated-execution predicate. The record explicitly leaves operator
acceptance pending. The protected runner and observed process result supply the
execution claim; a cryptographic attestation alone cannot prove an honest runner.

Actual dispatch/deployment, Narrator/high-contrast/scaling/keyboard and
operator-assisted picker acceptance remain independent pending gates. The
workflow's local contract tests neither run UI actions nor satisfy those gates.

## Approved Store-only distinction

The shared [`Test-ProtectedWorkflowAdmission.ps1`](../scripts/Test-ProtectedWorkflowAdmission.ps1)
requires an explicit current environment role: `interactive-qualification` or
`store-submission`. It reuses the native JSON/schema consumer for the same
successful, same-commit, trusted-main manual CI producer and reviewer-protection
contract; it does not duplicate an evolving REST grammar. A protected environment
for the other role is not interchangeable. The former UI-only entry point is
renamed with its consumers, without an alias. Positive native-response-shaped
fixtures and producer/type/duplicate/root/protection/cross-role refusal controls
pass for both roles. These local predicates do not authenticate caller-written
JSON, configure a GitHub environment, schedule a runner or prove hosted execution.
The [Store evidence workflow](../.github/workflows/store-package.yml) composes
these roles without dispatching or configuring them. Only an explicit manual run
on `main` can reach admission. The protected compiler job has a read-only token;
it authenticates the exact signed source, creates two fresh native build sets,
requires the closed comparison and selects the first original unsigned bundle.
The selected retained build's actual installed roots and unchanged application
lock supply native component exports and the exact-container submission SBOM.
Existing caches are never repaired; tools and native Go modules restore into
fresh private external directories. Failure prevents the successful input drop.

A separate hosted publisher downloads only the same run's successful evidence
with digest-mismatch refusal. It has no checkout, compiler, dependency restore
or application execution. The reviewed native attestation action's
[default SLSA mode](https://github.com/actions/attest/tree/1e69f48acb82d1966a394da916b4c1698aa569d6)
authenticates both original containers, all JSON evidence and observed runtime
versions; no local signature format is implemented. A consumer must use native
[`gh attestation verify`](https://cli.github.com/manual/gh_attestation_verify)
with this repository, exact Store signer workflow, source digest, main ref and
hosted-runner requirement before trusting a downloaded subject. Signed provenance
does not prove an honest builder or supply certification/installation evidence.

Source composition uses immutable setup-go 7.0.0 and setup-java 6.1.0 action
revisions, verified against their official release/commit APIs on 2026-10-16.
The exact MIT notice is retained in
[`eng/licenses`](../eng/licenses/actions-setup-runtimes.LICENSE.txt). They are
external engineering tools, not app runtime packages. Native Go 1.27.2 was the
then-current stable patch from the official Go distribution; its authentic
Windows archive and complete BSD-style notice were retained. It passes the actual
SDK fixture and restored official schema/semantic validators. Temurin
25.0.4.1+1 is the current patch of the applicable newest LTS line; setup-java's
documented multi-field syntax is used with mandatory fresh-download signature
verification. The locally installed matching Java runtime passes the native
semantic verifier. Hosted installation/action execution remains unqualified.

Native YAML/actionlint/PowerShell syntax and protected-order/permission/role/
digest negative controls validate source wiring, not live protection, package
comparison success, hosted execution, WACK or certification. The actual clean
pair still refuses unrelated differences, so no qualified Store artifact or
release authorization is claimed. The workflow never submits or stages rollout.

The subsequent owner-approved
[closed package-payload comparison contract](implementation/2026-10-15-package-payload-comparison-contract.md)
is a bounded-qualification amendment to the earlier unconditional equality gate.
Only native WinMD timestamp/MVID and directly consequent block hash/compressed-
size differences can qualify. Keep original bytes, metadata, raw hashes and
native package validation. The strict equality helper is unchanged. Historical
failures below are not retroactive passes; a permitted-variance comparison is
not complete-payload byte reproducibility.

The 2026-10-15 approval separates the unsigned Store-submission artifact from
signed sideload distribution. [Microsoft supplies the Store signature](https://learn.microsoft.com/en-us/windows/apps/publish/publish-your-app/msix/app-package-requirements#code-signing-for-microsoft-store-submissions)
after certification. The protected Store route must authenticate its exact
reproducible submission bundle and separately labelled SBOM/provenance; it must
not claim a trusted installed or Store-delivered package.
[`New-StoreSubmissionSbom.ps1`](../scripts/New-StoreSubmissionSbom.ps1) is the
separate submission-artifact producer. It shares native bundle inspection,
cataloging, bounded official validation and complete-directory publication with
the signed route, but emits the distinct closed
[Store-submission binding](../eng/store-submission-sbom-binding.schema.json).
Certification and release approval remain explicitly pending. Its caller must
authenticate the actual clean-build provenance and selected container; matching
a caller-provided digest alone does not establish publisher trust.
`Invoke-Sbom.ps1` remains the signed-distribution entry point described above,
not an unsigned Store switch. Neither route waives
WACK, upgrade/device/accessibility, legal, deliberate submission or rollout gates.

The public entry points fix their respective artifact roles; callers cannot
select a signature bypass on the signed route. Native PowerShell AST and JSON
Schema controls reject a wrong role, cross-role binding, missing contract,
unqualified runtime/approval claims and bad JSON types. Real unsigned input is
still refused by native trust before signed-route extraction. Store workflow
composition, authenticated hosted execution and final artifact qualification
remain pending. The real unsigned Store integration reached the existing native
Go module-integrity check and exceeded its unchanged 180-second deadline before
cataloging, official validation or publication. The owned process was terminated
and its absence checked; partial inspection plus module-identity/definition
diagnostics are retained. No completed integrity result or passing integration
is claimed. Diagnose this bounded failure before another expensive attempt; do
not increase the deadline, suppress integrity verification or infer success from
the earlier SDK fixture results.

The timeout diagnostic boundary has a separate test-first correction: the
producer now observes owned-process exit after requesting tree termination,
bounds redirected-stream completion, and saves both completed streams before
reporting a deadline failure. Its regression reproduces the lost-stream defect
with a five-second owned child, then checks success, nonzero exit and timeout
without compiling the SDK. The original 180-second module-integrity deadline
and all acceptance conditions are unchanged. A bounded local diagnostic verifies
a minimal cached dependency in under one second, while full offline module-list
inspection refuses missing cache entries. Neither result qualifies the complete
SDK cache or resolves the retained full integration timeout; complete native
dependency restoration/integrity remains required before another producer run.

The subsequent bounded diagnosis established a separate extraction cause:
the restored `go-mtree v0.7.0` directory lacked the archive's trailing-space
filename. The minimal native ordinary/literal extraction control above passes.
After quarantining only that owned malformed leaf and letting native Go extract
the unchanged authenticated archive through the literal namespace, the complete
restored cache passes `go mod verify`; source and staged locks are unchanged.
The earlier timeout remains failed historical evidence, not a retroactive pass.

The changed-condition real Store integration now passes
[`Test-StoreSubmissionSbom.ps1`](../scripts/tests/Test-StoreSubmissionSbom.ps1)
for the retained unsigned bundle SHA-256
`020643305c8a902fbe12e2f9f14451886d7f2b6dac1c2a7b4a8b9f346dd8b9e3`
and all 38 independent required component identities. Native SDK generation,
exact payload/component checks, unchanged official SPDX schema/Ajv and Workgroup
validation, input preservation, binding hashes and complete five-file publication
pass. The binding keeps certification and release approval pending. This proves
the submission-artifact producer, not clean-build provenance, byte reproducibility,
protected hosted execution, WACK, Store certification or a signed release.

[`Invoke-IndependentPackageBuildComparison.ps1`](../scripts/Invoke-IndependentPackageBuildComparison.ps1)
owns two independent native Git clones of one exact signature-verified clean
commit. It sequentially builds Release x86/x64/ARM64 with two MSBuild workers,
retains each actual architecture/toolchain provenance record, bundles/unpacks
with the owning SDK and compares complete payload inventories under the
[approved closed identity-variance contract](implementation/2026-10-15-package-payload-comparison-contract.md).
Its `independent-package-build-comparison.json` links the detailed native-container
comparison report and retains separate comparison-satisfied, payload-byte-identity
and container-byte-identity results. A permitted WinMD variance cannot become a
byte-identical-payload claim. Working/output directories must be fresh,
disjoint and outside the source checkout. Failed native logs and partial work
remain diagnostic, not published qualification. The first complete real run at
signed source `1cafa5b680f014d1c6808f2a309851f25b9e6201` built all three
architectures twice, but canonical equality refused nine executable/WinMD/block
map paths. Nothing was promoted as reproducible; both native bundle snapshots
and comparison evidence are retained for diagnosis. Do not remove those paths
from the manifest or rewrite their bytes merely to obtain equality. Use an
explicitly allocated short workspace with adequate
disk headroom, and recycle released clones only after retaining their required
results and provenance.

### Release symbol-path checkpoint (2026-10-15)

Native `DUMPBIN /headers` and .NET `PEReader` inspection of the retained pair
identified an absolute clone-local PDB reference in each executable. The x64
WinMD differs only at the COFF timestamp and module-version identifier (MVID).
These are separate producer inputs; fixing one does not qualify the other.

Release links now use Microsoft's documented
[`/PDBALTPATH:%_PDB%`](https://learn.microsoft.com/en-us/cpp/build/reference/pdbaltpath-use-alternate-pdb-path)
to embed the PDB filename without changing where symbols are actually written.
[`Test-PeProgramDatabaseReference.ps1`](../scripts/Test-PeProgramDatabaseReference.ps1)
uses the platform PE/CodeView reader, requires exactly one symbol reference,
and refuses directory-bearing references without repeating the private path
in its diagnostic. Successful Release application builds inspect their actual
output; Debug retains ordinary local debugging paths.

The native build-policy probe first reproduced the absolute-reference refusal
(RED). After the option change, independently placed probe outputs were
byte-identical and the positive/absolute-reference negative control passed
(GREEN). This is a bounded probe result, not a clean application rebuild or
whole-package reproducibility proof. The original six application builds and
their WinMD timestamp/MVID failure remain retained; no package bytes are patched,
filtered or normalized. Metadata reproducibility and final payload equality
remain release blockers.

The subsequent approved
[native WinMD merger qualification](implementation/2026-10-15-native-winmd-merger-qualification.md)
rejects Microsoft's `windows-metadata` 0.100.0 for this purpose: its public merge
API produces identical files but removes properties, events and method metadata.
The platform-reader preservation oracle fails. The portable Rust experiment
changes no application build input; this SDK is not adopted release tooling.
Retain the existing merger, all shipping metadata and the strict complete-payload
gate rather than wrapping incomplete output with custom reconstruction code.
