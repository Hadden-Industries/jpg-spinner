# Native WinMD merger qualification — 2026-10-15

Result: **REJECTED for JPG Spinner metadata production**. The candidate produces
repeatable bytes but does not preserve the app's complete Windows Runtime
metadata. No application build integration, dependency-plan adoption, shipping
metadata removal, binary patching or release qualification follows this result.

## Authority and exact inputs

The user approved bounded qualification of Microsoft's `windows-metadata`
0.100.0 public merge API using isolated portable Rust 1.99.1. Adoption was
conditional on preservation and output equality; MIDL compilation, shipping
metadata and the strict complete-payload comparison must remain.

- [Published SDK 0.100.0](https://crates.io/crates/windows-metadata/0.100.0),
  crate SHA-256
  `1d3a758dc1da4ae61aa7f93afbb505d564c72dd6431e513ed581377857896a72`.
- Published crate source identity:
  [`a57cc321bfdbac6bb98c488ba49f399da8fcb98a`](https://github.com/microsoft/windows-rs/tree/a57cc321bfdbac6bb98c488ba49f399da8fcb98a/crates/libs/metadata).
  The crate has no third-party dependencies or build scripts. Its MIT and
  Apache-2.0 texts are retained with the unmodified qualification source; this
  is not adoption or redistribution clearance for an application dependency.
- [Official Rust standalone distributions](https://forge.rust-lang.org/infra/other-installation-methods.html):
  exact 1.99.1 `rustc`, standard-library and Cargo archives for
  `x86_64-pc-windows-msvc`, selected through the versioned official TOML manifest
  and checked against its published SHA-256 values. Observed compiler identity:
  `rustc 1.99.1 (58cf8f9c3 2026-10-13)`, LLVM 23.1.1.
- The existing installed Microsoft linker and SDK libraries link the
  qualification-only executable. No installer, system-wide toolchain, registry,
  PATH or Windows trust-store change is used. Cargo is acquired but not needed:
  `rustc` compiles the unmodified dependency directly and invokes its public API.

## Reproducer and independent oracle

The original two clean builds at signed source
`1cafa5b680f014d1c6808f2a309851f25b9e6201` remain held. The x64 input consists
of the three original unmerged WinMDs for presentation, the main window and the
XAML metadata provider. The existing SDK `mdmerge.exe` replay on identical
copied inputs already demonstrates different timestamps and module identifiers.
The candidate experiment does not require another application/dependency build.

The qualification executable only supplies explicit input/output paths to
`windows_metadata::merge().inputs(...).output(...).merge()`. The SDK owns all
parsing, signatures, merging and serialization. No vendor edits, compensating
transformations, private binary reader or output rewriting are permitted.

The independent oracle uses .NET `PEReader` and `MetadataReader` with
`MetadataReaderOptions.None`, so CLR WinRT projection does not alter the raw
table view. It compares the app's type identities and complete member/attribute
table counts against the existing Microsoft-produced WinMD. Equal counts alone
would not prove signature/attribute equivalence; this is a cheap necessary
preservation gate before deeper projection, semantic and runtime qualification.

The oracle first accepts the original WinMD against itself. The native SDK
then produces two output files in separate directories with the same hash:
`bfa6f0b921dfda3c4da4ed2934ce3ab2e482b0d387bc17f90ade163f3609fd6a`.
Both original input bytes and their hashes remain unchanged.

## Observed preservation failure

| Metadata table | Existing Microsoft output | Candidate SDK output |
| --- | ---: | ---: |
| MethodDef | 163 | 76 |
| Property | 138 | 0 |
| PropertyMap | 8 | 0 |
| Event | 3 | 0 |
| EventMap | 3 | 0 |
| MethodSemantics | 144 | 0 |
| MethodImpl | 85 | 0 |
| CustomAttribute | 41 | 38 |

The independent preservation gate fails. Published merger source additionally
shows that its ordinary type-copy path skips methods on WinRT runtime classes
and does not reconstruct the listed property/event association tables. The
writer's lower-level ability to emit such rows does not mean the public merger
preserves them. Do not add application-owned reconstruction code around it.

Qualification stops at this first substantive failure. No full application,
projection/runtime, all-architecture or package qualification is claimed; no
unsuccessful retry or vendor repair is attempted. Deterministic loss of API
surface is not a reproducibility fix.

## Retained evidence and next boundary

External evidence belongs to execution
`58b83a03-9fab-43f5-aaf1-8b148c076d4c`, qualification resource inventory
`183ddf51645a4632809cc1df2727a159`. It includes exact official manifests,
checksums, archives/licenses, native compiler/linker diagnostics, the minimal
invocation source/executable, both output files and the platform-reader oracle.
The original six clean builds and minimal SDK-mdmerge replay remain independent
retained inputs. Local evidence paths are not public-repository dependencies.

Keep Microsoft's existing build integration, every shipping WinMD and the
unchanged strict payload gate. Release remains blocked on native metadata
reproducibility. A supported complete deterministic producer or an explicitly
reviewed change to the release contract would require new evidence/authority;
neither follows from approval to qualify this rejected candidate. Continue
independent Store-SBOM and remaining source work without presenting it as an
unblocked release.

Subsequent decision on 2026-10-15: the owner explicitly approved bounded
qualification of the [closed metadata/block-map comparison contract](2026-10-15-package-payload-comparison-contract.md),
including compressed-size consequences on the same changed WinMD blocks. That
changes the later acceptance contract, not this rejected qualification result.
No shipping artifact or native merger changes; full byte equality is not claimed.
