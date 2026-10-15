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
