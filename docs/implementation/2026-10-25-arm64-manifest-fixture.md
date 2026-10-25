# ARM64 headless deployment manifest fixture

The first native ARM64 VM campaign for signed candidate
`74f168b3912bd1bedf706e3baf0167f9c5edd1c9` fails exactly one headless case:
`The isolated test-package manifest conforms to its native Windows consumer`.
Its existing file-presence assertion cannot find the sibling generated
`JpgSpinner.App/AppxManifest.xml`. The transfer selects only `*.Tests/**`,
so the consumer's application-manifest input is absent. All twenty charset
sections and the remaining JPEG tests pass on the native ARM64 host; native
OS, observer and executable identities are independently recorded as ARM64.

The failure is retained in manual QA run
[40221234193](https://github.com/Hadden-Industries/jpg-spinner/actions/runs/40221234193),
job `121261156158`, artifact `12610069671` (SHA-256
`55651ece72250cc4ebd59d1f9a65593d9ac2e9a8d323dddb5b603fd3424fb857`).
Packaging correctly remains blocked. CodeQL's separately authorized run
continues against the unchanged production source.

## Bounded repair and proof

Transfer the actual generated application manifest alongside the existing test
hosts/dependencies, preserving their sibling layout and PDB exclusion. Use the
precise artifact designation `arm64-headless-test-inputs` consistently at its
producer and consumer. Keep compilation in the existing build job, and retain
the same-run source identity and native digest-mismatch refusal.

First strengthen the existing source-wiring contract so the incomplete transfer
fails before changing the workflow. Then add an omission control for this exact
fixture. Reproduce the missing-file failure with the existing native x64 package
fixture test in a small deployment layout; provide only the genuine generated
manifest and rerun that selected test. This isolates an architecture-independent
input omission without rebuilding, controlling the desktop, registering a
package, removing coverage or inventing another manifest parser. Original RED
and GREEN reports use separate directories.

The retained remote assertion and artifact inventory identify one deterministic
missing dependency, so a broader speculative hypothesis search is unnecessary.
Local replay qualifies the deployment contract; only a future authorized hosted
campaign can close the ARM64 runtime/package gates. Do not replay the approved
full QA run or CodeQL solely to refresh HISEW bookkeeping.

The existing consumer uses Microsoft's
[read-only manifest reader](https://learn.microsoft.com/en-us/windows/win32/api/appxpackaging/nf-appxpackaging-iappxfactory-createmanifestreader),
which validates XML outside an installed app package. The pinned
[artifact action's native path contract](https://github.com/actions/upload-artifact/blob/cf430e030ddbb5b0abf93d22962f4752f3646cd9/README.md#upload-using-multiple-paths-and-exclusions)
preserves the hierarchy below the common input ancestor. Supplying that declared
XML input is sufficient; no compiler, application activation or complete app
payload is needed in the ARM64 test lane for this case.

Working replay material belongs to HISEW inventory
`527058f99c614219a8c5d0219ceb8376` with a 64 MiB allowance. Retain selected
reports/source identities through native HISEW promotion and keep any directory
blocked by the existing cleanup defect intact with its recorded next actor.

## Focused results

The strengthened source contract fails against the original workflow with the
specific missing-manifest-fixture refusal, then passes after the transfer repair.
The existing native workflow validator and mutation controls pass, including the
new `arm64-without-manifest-fixture` refusal. The artifact's producer/consumer
share the revised identity; retention, compression, digest validation, native
architecture admission and the deployment-only runtime job remain unchanged.

The existing x64 presentation test host (SHA-256
`282d2907ef0d71e66ef353acfe8b6859f965ee9fcf032c64da31714265af5f6b`)
is copied without PDBs into two otherwise identical small deployment layouts.
The `[package-fixture]` selection fails with exit 42 at the same missing-file
assertion in the first layout. The second adds only the genuine generated
application manifest and passes the selected case, including the native Windows
manifest reader. Both invocations finish in under one second; executable hashes
match the original host. Child-only temporary-directory settings contain its
generated fixture under the declared working group.

This is a replay using an existing x64 test binary, not a rebuild of the current
candidate or native ARM64 acceptance. Original RED/GREEN JUnit reports remain
separate. No test is excluded and no failed hosted result is relabeled as passed.
