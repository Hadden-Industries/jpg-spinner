# Native dependency refresh and solution mappings

Status: native integration and supported headless checks pass; complete
warning-free release qualification and desktop acceptance remain pending.
Source retirement is signed checkpoint `e1eb2361f432f459ffd5f99a47faddf8a0cd9523`.
This record distinguishes a successful process exit from a warning-free build.
Desktop acceptance and distribution remain separate.

## Observed defect

The retained ARM64 build completes with 0 errors and 80 warnings: MSB4121 for
missing unselected fuzz-project mappings and LNK4099 for `XMPCoreStatic.pdb`.
The actual product installation reports Adobe packaging revision 1 although
the maintained overlay is revision 2. Earlier isolated revision-2 package tests
did not establish that this product installation refreshed.

The existing public CMake consumer reproduces the precise RED in four seconds
against the application's actual installed ARM64 prefix: native LINK `/DEBUG
/WX` emits LNK4099 and fails with LNK1218. Separately, the real build driver
accepts an intentional native MSBuild Warning task with exit 0. The driver
regression uses a clean positive control and actual native targets; it does not
substitute a mock compiler, package producer or application.

The selected installed `vcpkg.targets` uses manifest/configuration files as its
installation-target inputs, without tracking overlay recipe files. Updating
only the Adobe overlay left an older installation behind the existing manifest
stamp. The original installed target is retained with the result evidence.

## Maintained repair

- Add the already-approved Adobe `2025.03#2` identity to the root exact overrides.
  It remains transitive through Exiv2; the direct set stays libjpeg-turbo, Exiv2
  and Catch2. Updating this packaging identity changes a native manifest input.
- Preserve the authoritative distinction: [overlay ports take precedence](https://learn.microsoft.com/vcpkg/concepts/overlay-ports)
  and [do not participate in version resolution](https://github.com/MicrosoftDocs/vcpkg-docs/blob/main/vcpkg/users/examples/versioning.getting-started.md#versions-and-custom-ports).
  The override alone does not enforce an overlay version. Repository policy
  closes the root pin set; freshness validation checks the local Adobe version
  and packaging revision against that pin before accepting the existing upstream
  release check.
- Provide `ActiveCfg = Fuzz|x64` for every ordinary solution lane, with no fuzz
  `Build.0` entries. Microsoft's [solution model](https://source.dot.net/Microsoft.Build/Construction/Solution/SolutionFile.cs.html)
  owns that distinction. Unsupported ARM64/Win32 fuzzers are not compiled.
- Keep the existing compiler warning policy and actual public-package LINK
  `/DEBUG /WX` probe. Manual CI already executes that probe in every selected
  architecture/configuration. No warning suppression or reduction of linker
  debug information is introduced. Triggers remain exclusively manual.
- Select policy fixture source through Git's NUL-delimited inventory and copy
  current working bytes. This includes new non-ignored source and excludes
  ignored generated WinRT headers, previously about 85 MiB per fixture copy.

## Focused results

The strengthened repository policy first refuses eight missing mappings and the
missing Adobe pin, then passes the corrected configuration. Five independent
public-entry-point negative controls remove/change the Adobe pin, attempt to
broaden the direct dependency set and remove two non-x64 mappings. Each reaches
its intended refusal. Official-authority dependency freshness also passes.

The actual ARM64 JPEG project build now runs native manifest installation,
restores Adobe revision 2 and returns exit 0. The installed Adobe ABI is
`e97bc1b9b782f47264640397525abf8856efabe68da7311a37eb2618c2aa049c`.
The same public consumer then passes LINK `/DEBUG /WX`. Its temporary probe
directory still produces the separate MSB8029 location warning; this is not a
warning-free application build or an ARM64 runtime result.

An additional native MSBuild [`-warnAsError`](https://learn.microsoft.com/visualstudio/msbuild/msbuild-command-line-reference)
qualification passes a clean/warning driver control but fails the actual ARM64
solution on CS1668 from Microsoft's Roslyn inline-task factory. The native
environment references nonexistent `NETFXSDK/4.8/lib/um/arm64`; the parent `LIB`
environment is empty, and neither 4.8 nor 4.8.1 ARM64 SDK directory exists here.
The target continues, as Microsoft documents, but the overall build returns 1.
This broader gate is **rejected**, not adopted or represented as successful
application qualification. Its control source, proposed patch, raw streams and
original native binlog remain. No SDK patch, empty-directory workaround,
environment shim, warning suppression or unapproved tooling installation is
introduced. The complete warning-free release requirement remains open.

## Evidence and cleanup

Raw originals are retained in external evidence group
`b70f634d955345ee838ae8f64e56c9db`:

- `native-dependency-adobe-red.log` and `native-dependency-adobe-green.log`;
- `native-dependency-policy-red.log`, `native-dependency-policy-green.log` and
  `native-dependency-focused-controls.log`;
- `native-dependency-arm64-focused-build.log` and its original binlog;
- `native-dependency-freshness.log`;
- `native-build-warning-red.log`, `native-build-warning-green.log`,
  `rejected-native-build-warning-control.ps1`,
  `rejected-native-warning-gate.patch` and the failed
  `native-dependency-final-build-arm64.log` and
  `rejected-native-warning-arm64.binlog`.

## Final supported headless checks

The established build driver returns exit 0 for Release x86, x64 and ARM64 after
the manifest/mapping repair. The original logs show no MSB4121 or LNK4099.
SDK inline-task CS1668 warnings remain in the x86/x64 restore paths, including
nonexistent `VC/Auxiliary/VS/lib` directories. The ARM64 incremental follow-up
is separate from its retained failed broader-gate attempt. None of these results
is a clean-clone, warning-free qualification or a waiver for those SDK warnings.

Both x86 and x64 Release headless commands return exit 0. Each has 378 testcase
elements in five original JUnit reports: Domain 38, JPEG Transformation 165,
Windows Storage 98, Batch Processing 15 and Presentation 62. No failure, error
or skipped element is present: 756 executions across two architectures, not
756 distinct tests. No ARM64 runtime execution is claimed.

The full policy-control component initially stops at an indentation-dependent
duplicate-member fixture. That setup failure is preserved. The fixture now uses
native `JsonDocument`/`GetRawText` boundaries before adding a deliberately repeated
member, so formatting changes cannot erase the intended input. Its component
follow-up passes all controls, including duplicate-member, exact-schema/type,
exclusive-configuration and native triplet refusals. The original successful
builds and suites are not rerun merely to attach another bookkeeping receipt.

The consolidated ordinary review and narrow fixture follow-up report no
findings. Final original reports are retained as `native-final-{x86,x64}-*.junit.xml`
and `native-final-{x86,x64,arm64}.binlog`. The native command streams are
`native-dependency-final-build-x86.log`, `native-dependency-final-build-x64.log`,
`native-dependency-arm64-application-followup.log`,
`native-dependency-final-headless-{x86,x64}.log` and
`native-dependency-final-policy-controls-followup.log`. Desktop checks remain
unrun under the owner's current instruction.

The source group's first two native promotion batches verify 128 original
streams at checkpoint `64c6f29d0dd74ea396f95749da596b78`. A further call with
11 remaining streams and eight native control logs/binlogs is refused with the
generic inventory-schema `oneOf` error. The last stored checkpoint is 123,449
bytes and has 128 mappings; the loaded schema separately caps a group's
`evidenceCopies` at 128. The exact rejected branch is not identified by that
diagnostic. The observation and original receipts are added to
[HISEW issue 187](https://github.com/Hadden-Industries/software-engineering-workflow/issues/187#issuecomment-6380983964).
Native held disposition `c3cda84f799a454387234dfda10bf3c7` preserves the source;
complete preservation or removal is not claimed. The owning coordinator
reassesses after the supported native cumulative-promotion/refinement route is
available, without repeating valid product tests for bookkeeping. Earlier
blocked groups remain intact too.

Working probes use preallocated native inventory
`c648b1a4408144a297109d9703907719`. Promote selected originals through HISEW and
dispose released working copies through its native lifecycle. Existing blocked
groups remain intact; their holds are not bypassed to claim cleanup completion.

No desktop use, security review, external review, hosted execution, app
installation, signing, trust change, Store submission or release is performed.
