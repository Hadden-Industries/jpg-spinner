# Native ARM64 qualification without a physical-device release gate

## Accepted requirement and implementation plan

The maintainer's 2026-10-25 amendment makes physical-device acceptance optional
and nonblocking. Task 20 still requires native ARM64 execution; cross-compilation
does not satisfy it. A Windows ARM64 VM can supply that evidence. General
interactive accessibility, supported OS, performance, package, legal and trusted
Store-upgrade requirements remain open until their own evidence exists; none
requires a physical device solely because of its form factor.

GitHub provides the generally available
[`windows-11-vs2026-arm` image](https://github.blog/changelog/2026-08-20-windows-11-arm64-vs2026-image-generally-available/).
Use it for a deployment-only job within the existing manual CI run. The existing
x64-hosted build job remains the compiler. GitHub's native
[same-run artifact transfer](https://docs.github.com/en/actions/tutorials/store-and-share-data#passing-data-between-jobs-in-a-workflow)
and fail-on-digest-mismatch download supply the transfer boundary. Do not create
a second archive format, parser, cross-workflow authentication protocol or
"interactive" payload for these headless tests.

This is a bounded continuation of Task 20 and SLICE-003 in the
[release completion replan](../plans/2026-10-25-release-completion-replan.md):

| Increment               | Existing requirement                                      | Falsifiable proof and evidence boundary                                                                                                                                                                                                                                                                                              |
| ----------------------- | --------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Native execution lane   | Task 20.2, transform/transaction/recovery/batch execution | Transfer prebuilt Release ARM64 tests from the same successful producer; reject a nonnative OS or wrong-machine executable using .NET's native architecture and PE APIs. Run the maintained headless selector and retain native JUnit, machine identities and runner image information. No compilation or desktop input in this job. |
| Deployment prerequisite | Existing unpackaged presentation-host contract            | Extend the existing Microsoft runtime installer integration to the official ARM64 binary. Independently verify its exact digest and Microsoft signature without installing locally. Retain the existing SDK-version association and hosted-only deployment controls.                                                                 |
| Release gate update     | Task 20.2/20.5 and the explicit owner amendment           | Update current gate registers consistently. Physical customer-device observations are optional; native packaged/app and general interactive evidence remain distinct from headless results. A VM is eligible for their appropriate qualified lane.                                                                                   |

The engineer integrates these changes as one checkpoint. Write focused failing
controls first, then implement and run only the affected contracts. Reuse the
central qualification budgets, artifact retention/compression policy, pinned
actions and test selector; do not duplicate their category values. Inspect
actual executable architecture through `System.Reflection.PortableExecutable`,
not a handwritten PE parser or an inference from the runner label.

The principal external uncertainty is execution on the actual hosted image.
Source contracts and local identity/refusal controls cannot prove it. Freeze and
publish one candidate under separate scoped authority, then include this lane in
the single final manual campaign. A real loader/runtime failure gets a bounded
repair and impact assessment, not an automatic rerun or a physical-device waiver
of the native execution requirement. No cloud provisioning, remote dispatch,
Store action or installation on the developer's computer is authorized here.

Retain original focused results and release consumers through HISEW. The hosted
transfer contains only reusable test binaries and dependencies, expires after one
day, and excludes reconstructible PDBs. Native cleanup admission blockers retain
their working directories and recorded next actor; introduce no alternate
deletion route. Existing historical compile-only results retain their original
meaning.

## Focused implementation evidence

The implemented `arm64-runtime` job consumes only the same CI run's
`arm64-headless-test-inputs` artifact, with native digest mismatch treated as an
error. The transfer includes the generated application manifest consumed by the
non-interactive package-fixture test, beside the test hosts. See the retained
[first-campaign omission and bounded repair](2026-10-25-arm64-manifest-fixture.md).
It installs the pinned ARM64 Windows App Runtime and executes the existing
`HeadlessAll` selector with native OS/PE admission before any test process starts.
Both unsigned packaging and interactive-input publication depend on this job.
The existing policy generates the timeouts and minimum artifact retention;
the test runner's project selection remains its single authority.

The native runtime selection now evaluates `Directory.Packages.props` through
the installed .NET SDK's MSBuild without targets or C++ project imports. This
retains unique central-version admission while removing an inappropriate C++
toolchain prerequisite from deployment. No runtime is installed locally.

Microsoft's official ARM64 2.5.1 download resolves to the pinned binary with
SHA-256 `d5e4d34547eb4e31c64bf1532415b3019c0d92b750e72eb18d3d95bd00feacbb`,
PE machine `Arm64`, FileVersion `2.5` and a valid Microsoft Corporation signature.
Read-only WhatIf qualification and the actual wrong/ambiguous-SDK, non-hosted,
missing acquisition input and wrong-binary refusals pass.

The source contract initially fails for the absent ARM64 job; the executable
test initially fails for the absent native inspector. The subsequent focused
controls pass for the actual native/ARM64 PE identities, hash binding,
invalid/missing/wrong-machine/DLL refusals and nonnative OS admission through
both the inspector and public test-suite entry point. These fixtures do not run
the Microsoft installer or pretend to be product test execution.

Native YamlDotNet/actionlint source validation and workflow mutation controls
pass, including an x64 host, wrong producer, weakened digest behavior, omitted
native admission, compilation on the test runner and bypass of the package
dependency. Actionlint 1.7.12 predates the GA image label, so its supported native
label configuration declares that exact published label. Explicit `-config-file`
selection applies the same repository configuration to external refusal fixtures;
there is no parser patch or skipped diagnostic.

Original focused logs are retained in HISEW operator group
`f98310a13af74542b3d9d5f99884cb5d`; working inventory
`dd7435ef951444e6b13fe202f00a4aa7` owns the bounded installer/tool/fixture inputs.
At that focused checkpoint, the actual hosted ARM64 job, native packaged-app
smoke, final campaign and release remained unexecuted. No source publication,
cloud run, desktop input, external review or security review was performed by
that checkpoint. The later
[hosted execution record](2026-10-25-exiv2-hosted-qualification.md#native-arm64-execution)
records successful native headless execution on the exact signed candidate,
including the repaired manifest fixture. Packaged-app smoke and the other
release requirements remain separate.
