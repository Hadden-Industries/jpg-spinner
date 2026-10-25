# Hosted QA completion — 2026-10-25

The deliberately authorized [manual QA run 40176250565, attempt 1](https://github.com/Hadden-Industries/jpg-spinner/actions/runs/40176250565)
completed successfully on 2026-10-25 at 09:33:22 UTC. It built signed source
[`0d78e54bc27018856742647a9e3c4098fea8a1da`](https://github.com/Hadden-Industries/jpg-spinner/commit/0d78e54bc27018856742647a9e3c4098fea8a1da).
This closes the ordinary hosted-QA failure repaired by the
[WinRT probe namespace change](2026-10-25-winrt-probe-symbol-identity.md).
Earlier failed runs remain failed.

## Executed scope

All ten jobs succeeded: source policy; five native build configurations;
AddressSanitizer; MSVC native analysis; interactive-input publication; and
unsigned package construction/inspection. ARM64 was compiled and packaged on an
x64 runner; no ARM64 runtime or physical-device result is inferred.

The original headless JUnit reports contain these testcase elements:

| Native lane | Reports | Testcase executions | Failures / errors / skips |
| ----------- | ------- | ------------------- | ------------------------- |
| x86 Debug   | 5       | 378                 | 0 / 0 / 0                 |
| x86 Release | 5       | 378                 | 0 / 0 / 0                 |
| x64 Debug   | 5       | 378                 | 0 / 0 / 0                 |
| x64 Release | 6       | 379                 | 0 / 0 / 0                 |

The x64 Release lane additionally ran the single presentation-shutdown case.
The total is 1,513 executions across lanes, not 1,513 distinct tests. The ASan
and analysis jobs passed separately; their archives have no JUnit XML, so their
results are not derived from the table.

## Actual package checks

The native producer built and inspected all three unsigned application
packages. Each passed the exact Store/AppContainer manifest, framework/payload,
55 artwork controls and 24 exact notice-asset checks. Provenance records bind
the clean source, selected tools and locks. The reported hosted image is
`windows-2025-vs2026`, version `20261021.278.1`, with selected SDK
`10.0.28000.0` and VC tools `14.51.36231`.

The same job passed native three-architecture bundling/extraction, the closed
comparison's synthetic membership/variance refusals, actual false-block-hash
rejection, Release/Debug WinRT-origin controls, native Store-component
construction and release/Store-SBOM preflight refusals. These are actual
container/control executions. They do not produce a second independent build
set or qualify an exact final-bundle SBOM.

The retained original bundle is 8,660,928 bytes, SHA-256
`10a94f6722e2442c8bbe500a5f0f4fa7447b431c4e3f110efa04436d9f070fe8`.
It is unsigned. The diagnostic ZIP is artifact `12593334664`, 11,040,384 bytes,
SHA-256 `21fa0fd81d164b9b17db41148993e84e2fa7e946620c55302cacd2f5611d3d2f`.
The package log includes an upstream vcpkg/CMake warning that
`NEON_INTRINSICS` is unused in the ARM64 port's CMake configuration. A successful
job is not a claim that every dependency-producer diagnostic is absent.

## Authenticated inputs and retained evidence

GitHub's native attestation verifier accepted the published interactive
`build-context.json`, binding the exact source/ref, CI workflow, custom
interactive-input predicate and hosted runner. Strict native JSON/schema checks
and the payload-manifest digest also pass. This authenticates the input context;
it does not establish complete extracted-payload validation, package deployment,
desktop acceptance or release approval. No desktop was used for this run's
headless qualification or its evidence readback.

Original diagnostic archives, native API run/job/artifact records, the final
package log, attestation bundle and verification result are retained through
HISEW inventory `5e3e4fc1357f4790b47c80d41bd8aecc`, under accepted execution
`1f8e9d2c-38ef-4f91-a9d3-d114332d547e`. Native preservation checkpoint
`86de514fd3774bbf900c72412fc2562f` verifies the final originals; successor
`8a42c68c6bcd4184ba3f3e25e43a063e` records completed producers/consumers and the
bounded evidence allowance. SHA-256 and byte-length readback precedes removal
of spent cloud copies. GitHub then reports zero artifacts for this run.

Blocked local temporary groups remain held under the existing
[HISEW cleanup issue](https://github.com/Hadden-Industries/software-engineering-workflow/issues/197#issuecomment-6395313372).
Their required evidence was retained; this result grants no bypass of native
cleanup admission.

## Remaining qualification

The later workflow-only SDK repair is a separate source checkpoint and native
CodeQL run. This ordinary QA result is specific to `0d78e54b`; it is not a run
on that later revision. The [release register](../release-qualification.md)
continues to require final source-bound package/SBOM/provenance evidence,
physical ARM64 and OS/provider testing, performance, human accessibility and
localization, trusted Store upgrade, certification, legal/source delivery and
deliberate distribution approval. No new remote dispatch or publication is
authorized by this record.
