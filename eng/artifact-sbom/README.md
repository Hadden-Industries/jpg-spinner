# Artifact SBOM composition

This release-engineering command composes an explicit payload inventory and
resolved component evidence through [Syft 1.54.1's public SDK](https://github.com/anchore/syft/tree/v1.54.1/syft).
It does not parse NuGet/vcpkg manifests itself, rewrite generated JSON, extract
MSIX containers, sign packages, install tools or become an app dependency.

The caller supplies already-verified, read-only inspection roots. Only payload
files receive file entries and SHA-256 digests. Syft's NuGet lock cataloger and
SPDX decoder consume resolved component evidence separately. Native vcpkg SPDX
element IDs are document-local: the adapter obtains native content-addressed
package identities and rebinds package relationships before native SPDX encoding.
Discovery files are not promoted into shipped payload files.

The command requires nonempty payload, resolved NuGet packages, installed vcpkg
SPDX 2.3 documents, explicit public artifact name/version, and a fresh output file
outside both input roots. Native exclusive creation preserves existing outputs.
A failed write may leave a diagnostic partial file; callers must never promote
it as a valid SBOM. Caller-owned inputs must remain unchanged during inventory.
The 60-second context and two-worker file executor bound development execution;
qualification of actual release workloads remains pending.

## Qualification

Go 1.27.0 was available and used locally; `go.mod` declares that minimum language
version. Use native Go module integrity, the checked-in `go.sum` and readonly
module resolution. Do not patch vendor sources or silently upgrade the SDK's
transitive graph. Its graph includes upstream prerelease/pseudo-version modules;
the stable top-level SDK version does not establish a stable-only transitive
graph or final release-tool rights/security clearance.

The PowerShell producer preserves both native streams for a successfully
terminated deadline failure and separately refuses unresolved process exit or
stream completion. The deadline is not extended and timed-out integrity is not
accepted. [`Test-ArtifactToolProcessLifecycle.ps1`](../../scripts/tests/Test-ArtifactToolProcessLifecycle.ps1)
exercises this actual helper with owned success, failure and timeout children;
it runs no SDK compilation, dependency download or package generation.

From this directory, with caller-selected external Go caches:

```powershell
go test -mod=readonly -p=2 -count=1 -timeout=90s -v .
```

Tests use synthetic payload and retained native component metadata under
[the fixture](../../tests/TestData/SbomContractFixture/README.md).
The stronger [contract runner](../../scripts/tests/Test-SbomToolContract.ps1)
also emits through the command boundary and requires pinned official SPDX
schema and Workgroup semantic verification. It requires explicit Go, Java,
schema, verifier JAR, cache and fresh external output paths; it installs nothing.
Immutable validator identities are in [the pins](../sbom-tools.json).

The reusable [artifact validator](../../scripts/Test-ArtifactSbom.ps1) consumes
an unmodified native document, a separate `New-PayloadManifest.ps1` inventory,
an independent JSON component contract with nonempty `requiredPackages`
(`name`/`version` pairs with an optional exact native `packageUrl`), and explicit
public artifact name/version. Component entries reject unknown fields and
non-string URL values. It checks
exact payload scope/hashes, SBOM membership and required components before the
official semantic verifier. The fixture runner additionally checks its payload
against the independently fixed hash vector, rather than letting two generated
inventories define their own oracle. Negative controls accept a schema-valid
wrong hash only in the initial RED seam, then require refusal of wrong hashes,
missing membership, changed component/public versions and duplicate members.

The component contract states required identities, not an exhaustive dependency
resolver. Its producer must independently establish complete architecture-specific
runtime coverage; this validator cannot prove a caller supplied every dependency.
Name/version-only requirements must be unambiguous within the selected inventory.
When multiple architectures share that pair, require their exact native package
URLs, including triplet qualifiers. The validator compares emitted identifiers;
it does not parse, reorder or reconstruct URLs. A synthetic two-architecture
control rejects the ambiguous pair and accepts the qualified identity through
both official validators; a wrong-triplet requirement fails. This follows the
[SPDX packageUrl contract](https://spdx.github.io/spdx-spec/v3.0.1/model/Software/Properties/packageUrl/)
and [vcpkg's triplet-separated installed metadata](https://learn.microsoft.com/en-us/vcpkg/reference/software-bill-of-materials).
Full multi-architecture release composition and dependency evidence remain open.

For an already-qualified inspection tree, the command syntax is:

```powershell
go run -mod=readonly -p=2 . -payload <payload-root> -components <evidence-root> -name JpgSpinner -version <observed-package-version> -output <fresh-external-file>
```

This syntax alone is not the release pipeline. Bundle signature/identity checks,
safe recursive extraction, complete architecture-specific dependency coverage,
payload-manifest comparison and release provenance remain Task 17 obligations.
No compliant application-release SBOM is claimed by fixture qualification.

The SDK, go-sync and stereoscope use [Apache-2.0](../licenses/Anchore.Apache-2.0.txt); retain their exact notices and
the native transitive rights inventory before redistributing a built tool.
First-party adapter code remains under the repository's GPL-3.0 license.
