# Build-context SBOM evidence

This is release-engineering guidance, not permission to sign, publish or release.
JPG Spinner 2.0 remains [not release-qualified](release-qualification.md).

## What each result establishes

| Result | Claim | Not established |
| --- | --- | --- |
| Native installed vcpkg export | Installed build closure of `exiv2` and `libjpeg-turbo`, separately for each target triplet | Which static-library objects were linked or executed |
| Application `packages.lock.json` | Resolved direct/transitive package identities across all recorded targets | Every package is redistributed, or a build ran with this lock |
| Extracted payload inventory | Exact relative filenames, lengths and SHA-256 values | Container authenticity, installation or runtime behaviour |
| Native SPDX plus official validation | Format/semantic validity and membership matching independent required identities and file hashes | Completeness of arbitrary caller requirements or release approval |
| Release `binding.json` | Hash linkage between the selected signed container and the exact SBOM/input-evidence files | Build-execution attestation, linkage analysis or deployed/instrumented runtime inventory |

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
