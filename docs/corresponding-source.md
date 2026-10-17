# Corresponding-source preparation

## Scope and authority

Status: maintained preparation procedure, **not a completed source distribution,
legal opinion, source offer or release approval**. Task 18 still requires exact
source assembly, independent reconstruction and qualified rights review for the
distributed candidate. See the [release register](release-qualification.md).

The repository's [GPL version 3 text](../LICENSE), sections 1 and 6, defines
Corresponding Source and the conditions for conveying object code. It covers
more than an application checkout: include the preferred form for modification,
required interfaces and scripts controlling generation, installation and running,
subject to the license's actual exclusions. Do not infer a System Library
exclusion from a publisher name, package manager, DLL boundary or SDK label.
The qualified legal reviewer must settle the selected distribution route,
Exiv2 GPL-2.0-or-later compatibility, Microsoft terms, any applicable installation
information and the exact scope of permissible exclusions.

Use native source/package tools, not another dependency resolver or a custom
archive parser. A [vcpkg export](https://learn.microsoft.com/en-us/vcpkg/commands/export)
is a built developer SDK; it is not a corresponding-source export. Its existing
role in the [SBOM procedure](supply-chain-evidence.md) remains unchanged.
An SBOM, lock file, SPDX expression or upstream URL identifies inputs but does
not supply missing source bytes, establish availability or grant distribution
rights. Never publish internal evidence, account data, signing keys or credentials
as a substitute for source.

## Release binding

The release owner assembles one evidence record for the exact candidate, not a
floating `main` link. Record:

- the full signed application commit and tree, successful signature verification,
  clean checkout result and selected architecture/configuration;
- original unsigned submission container hashes and, separately, the eventual
  Store-signed or sideload container hashes, using the existing evidence contracts;
- original source archive filenames, byte lengths and SHA-256 hashes, plus
  upstream source digests required by the selected ports;
- original dependency locks, selected registry/baseline and overlay/patch bytes,
  native build provenance and the corresponding binary/component evidence;
- actual acquisition, reconstruction, test and rights-review reports, with the
  operator, date, command, tool identity and limitations of each result;
- the intended public source location, responsible maintainer, availability
  obligation and exact binary-to-source directions approved for distribution.

Keep the application source archive distinct from the complete corresponding-
source delivery set. The former is one input to the latter. A changed dependency,
patch, compiler input or application commit needs a newly assessed binding; do not
overwrite a historical archive or relabel an earlier binary's source as current.

## Source closure

Use the actual resolved graph and build evidence, rather than assuming every
installed package contributes code or that only direct dependencies matter.
The following are preparation boundaries, not automatic legal classifications.

| Input | Maintained authority | Material to retain and assess |
| --- | --- | --- |
| Application and generated interfaces | [modern solution](../JpgSpinner.sln), [application project](../src/JpgSpinner.App/JpgSpinner.App.vcxproj), source and tests | Preferred source, IDL/XAML/resources, original asset sources, generation/build/package/test scripts and applicable notices |
| Native dependency selection | [manifest](../vcpkg.json), [triplets](../vcpkg-triplets), actual native resolved exports | Exact resolved port versions/features for each target, registry source at the selected baseline and version-selected port trees, upstream archives and notices |
| Native modifications | [Exiv2 overlay](../vcpkg-ports/exiv2), [Adobe overlay](../vcpkg-ports/adobe-xmp-core) | Complete overlay build files, all named patches, original source archives and native patch-application/build results |
| NuGet inputs | [central versions](../Directory.Packages.props), [application lock](../src/JpgSpinner.App/packages.lock.json), [NuGet configuration](../NuGet.config) | Original resolved package identities/integrity evidence; distinguish generated/header source, redistributed/framework components and build-only tooling, with applicable source/terms decisions |
| Compiler and SDK | [toolchain lock](../eng/toolchain-lock.json), [component requirements](../.vsconfig), root build policy | Actual selected tool/SDK identities, public acquisition prerequisites, configuration and build instructions; no unreviewed redistribution of proprietary tools |
| Release tooling | [SBOM tool pins](../eng/sbom-tools.json), [Go module](../eng/artifact-sbom/go.mod), [Go checksums](../eng/artifact-sbom/go.sum), [SPDX validator lock](../eng/spdx-validation/package-lock.json) | Separate release-tool source/terms and exact tool inputs where required; do not mislabel these as application runtime components |

For the native graph, preserve the selected baseline's registry and version
metadata as well as the source archives. A baseline alone does not contain
upstream source. The Exiv2 overlay's two patches and Adobe overlay's two patches
are build inputs, not optional commentary; retain their original bytes and let
the existing vcpkg port execute them. Do not replace them with manually edited
build trees. Preserve source for actual transitive inputs, including Expat,
inih, libspng and zlib where the selected graph requires them. Catch2 is test-only;
its inclusion in reconstruction/test inputs does not make it a runtime component.

Microsoft documents [asset caching](https://learn.microsoft.com/en-us/vcpkg/concepts/asset-caching)
as integrity-checked preservation of downloaded assets, which can include source
archives, patches and tools. Classify the selected assets before distribution;
do not dump a mixed shared cache into the delivery. Preserve upstream SHA-512
checks and add archive identities to the release binding without rewriting them.
For [vcpkg install](https://learn.microsoft.com/en-us/vcpkg/commands/install),
`--only-downloads` is a preparation aid with documented limitations, not proof
that every port asset was acquired. Binary-cache restoration likewise does not
establish source acquisition. Final reconstruction must expose missing sources,
not silently rely on the engineer's existing installed tree or cache.

### Selected native-input preservation checkpoint (2026-10-17)

Bounded offline preservation at signed application commit
`5998e30173be0e4f8a42e39c42be83abc0a94c05` retains eight original upstream
archives (84,338,600 bytes), 12 complete selected port trees (66 ordinary files),
28 original installed SPDX documents and 11 baseline/version metadata files.
The observed target installations are x86, x64 and ARM64 static-md; the four
host-helper ports were inspected in the x64 installation. Historical sanitizer
results are not a freshly observed fourth installation.

| Upstream input | Original archive bytes | Retained SHA-256 |
| --- | ---: | --- |
| Adobe XMP Toolkit SDK 2025.03 | 23,171,605 | `7e9f4b0d62979e1a7d44e16febfe2c6ab337ceccdaf1dad092666ac14b2fdcd5` |
| Exiv2 0.28.9 | 47,189,810 | `700b76b97695b2fab4ef8c79619c68ae57d09e0c130724791cafbd39e0eb4aef` |
| Expat 2.8.5 | 8,474,242 | `fd022c541a189bd5bee042a22188351b31e39b9389c2525fa98c28bd05c9ef21` |
| inih r62 | 22,145 | `9c15fa751bb8093d042dae1b9f125eb45198c32c6704cd5481ccde460d4f8151` |
| libjpeg-turbo 3.2.0 | 2,537,858 | `980dd81f425082aa6d7c9e47fef27554ce7a9ffc8e2f6e863b97d263c5c50858` |
| libspng 0.7.4 | 159,230 | `47ec02be6c0a6323044600a9221b049f63e1953faf816903e7383d4dc4234487` |
| zlib 1.3.2 | 1,566,911 | `b99a0b86c0ba9360ec7e78c4f1e43b1cbdf1e6936c8fa0f6835c0cd694a495a1` |
| Catch2 3.16.0, test input only | 1,216,799 | `0957cae5821b17ce07f0833aaa52b5137643a8382203221f363a8303c109af34` |

These are frozen-input identities, not a new latest-version assessment. The
subsequent [Catch2 3.16.1 qualification](implementation/2026-10-17-catch2-native-overlay.md)
separately retains its new original archive, exact port/patch and Microsoft
recipe license; this table remains the historical 3.16.0 observation. Do not
reuse it as current test-distribution source closure. Native
Git 2.56.0.windows.2 and PowerShell 7.6.7 performed the qualification without a
build, download, installation, network-dependent restore or application change.
Each upstream archive matches the selected port's native SHA-512; independent
readback also checks its retained length and SHA-256. All three installed target
metadata sets agree on the observed source and complete port-file bytes. Adobe
and Exiv2 retain both overlay patches apiece, not manually patched build trees.
The additional host-port trees are `vcpkg-cmake` 2025-08-07,
`vcpkg-cmake-config` 2026-07-21, `vcpkg-cmake-get-vars` 2025-05-29 and
`vcpkg-tool-meson` 1.9.0#10. The Meson helper port is not the separately acquired
Meson/Python tool distribution; that acquisition closure remains separate.

The registry baseline is commit
`c748cb44f2a435fcf015c35225c9d5545fe0021c`. Version-selected port identities
are **Git trees**, not registry commits. Native `git archive` and the platform
ZIP reader preserve each selected tree; native membership, mode and Git-blob
checks reject missing/export-transformed bytes, and original SPDX port-file
SHA-256 checks agree. The retained baseline/version archive has exact native
blob readback and uniquely records each selected registry tree. No custom
archive parser, JSON grammar, dependency resolver or source-rewriting shim was
added. Microsoft's [vcpkg SPDX documentation](https://learn.microsoft.com/en-us/vcpkg/reference/software-bill-of-materials)
explicitly describes source-resource inference as heuristic: this observation
does not prove complete source/build-tool closure or binary contribution.

The actual preservation predicates refuse a missing upstream archive, changed
archive digest, omitted overlay patch and changed overlay patch. These are
bounded preservation controls, not product-code TDD or the full delivery-refusal
matrix below. Two earlier setup failures remain recorded: an absent sanitizer
installation and PowerShell single-item array unrolling. Their corrections
narrow the observed installation scope and fix orchestration, respectively;
neither suppresses an upstream integrity failure.

External evidence belongs to active modernization execution
`352c2077-8e60-441f-ac25-4b13a115a082`, resource inventory
`16a41a2cfe2f4aaf80ebe1ed0d80a46f`. Its raw
`native-source-preservation.json`, native transcripts and
`native-source-readback.json` preserve commands, per-port/tree identities,
original digests, outcomes and limitations. The readback report SHA-256 is
`5a4b493da32f9912f19f6d30b813e920a482c6b66b151c19aad77b8e6e0328e4`.
Adobe and Exiv2 source archives exceed the native evidence store's 16 MiB
per-payload promotion bound: retain their original operator-evidence copies for
the source-assembly/reconstruction consumer, with ownership and reassessment in
that inventory. A report or digest is not a replacement for those required
bytes. Do not split, recompress or delete them merely to fit transport limits.

This checkpoint does **not** qualify complete Corresponding Source, archive
extraction/buildability of all upstream sources, independent reconstruction,
NuGet/tool acquisition closure, final binary binding, recipient availability,
rights clearance or release. Application-source freeze still follows accepted
Task 19 cutover; all reconstruction and delivery steps below remain required.

## Reconstruction and delivery

1. Finish accepted cutover and freeze the release candidate. The legacy tree is
   still quarantined, not removed; do not publish its historical credential
   references in a new source deliverable or silently filter a source archive
   and call it an exact repository snapshot. Resolve that boundary in Task 19.
2. Verify the approved signed commit with the existing
   [source-signature trust contract](../scripts/tests/Test-SourceCommitSignatureTrust.ps1).
   Preserve its exact source identity. Use native
   [Git archive](https://git-scm.com/docs/git-archive) with that full commit to
   prepare an application-source archive; native Git owns archive and attribute
   semantics. Audit applicable `export-ignore` and `export-subst` attributes,
   including repository-local configuration, before accepting membership/bytes.
   Do not use `--worktree-attributes`, uncommitted add-files or a moving branch.
3. Extract with a maintained native archive reader in fresh owned working
   material. Compare source membership and bytes with the approved tree using
   native Git operations and the existing
   [payload-manifest producer](../scripts/New-PayloadManifest.ps1), accounting for
   declared Git modes and any qualified export transformation. Git metadata is
   absent from an archive: it cannot directly satisfy the current provenance
   scripts' clean-commit checks. Retain a verified native Git checkout as the
   reconstruction lane, and independently compare its source to the archive;
   do not synthesize a commit or fake a signature for the extracted tree.
4. Assemble the dependency source/patch/registry closure above, complete build
   instructions and original notices alongside that archive. Preserve each
   original archive and check its native expected digest before extraction or
   use. Record rights decisions and unresolved inputs explicitly. A bare list of
   download links, successfully extracted files or retained build trees is not
   the completed delivery set.
5. Reconstruct from the verified checkout in a clean, independently provisioned
   Windows environment with the declared prerequisites. Use only declared
   retained dependency inputs plus explicitly documented general tool/SDK
   acquisition. Constrain dependency restoration to those inputs when assessing
   source completeness; refuse missing material. Do not present a warm binary
   cache build as dependency-source reconstruction.
6. Run the maintained build/test/package entry points below for all required
   architectures and configurations, and the other Task 20 gates. Retain actual
   command streams, native locked restore and provenance. Compare rebuilt
   original packages under the
   [approved limited comparison contract](implementation/2026-10-15-package-payload-comparison-contract.md),
   preserving exact raw hashes and explicit variance. Do not promise complete
   byte-identical containers or weaken any unapproved difference.
7. Independently refuse missing application source, a missing transitive source
   archive, omitted/changed patches, altered locks, missing build instructions,
   mismatched candidate binding and an unavailable public source location.
   Structural documentation tests do not implement or satisfy these artifact
   controls. Complete them before claiming corresponding-source qualification.
8. Obtain the qualified rights review and deliberate release/publication
   authorization. Verify recipient access to the exact approved delivery bytes
   and clear binary-to-source directions without a special unpacking credential.
   Preserve evidence and the maintainer's ongoing availability obligation. Do not
   invent a written source offer or infer that a public repository alone
   satisfies the selected Store distribution contract.

### Maintained reconstruction entry points

From the verified repository checkout, install the declared prerequisites through
their supported distributors. Do not automatically update tools, retarget the
SDK, bootstrap Visual Studio's vcpkg installation, use user-wide integration or
remove toolchain/lock checks to accommodate a different machine. The native
[resolver](../scripts/Resolve-MSBuildToolchain.ps1) selects a complete stable
Visual Studio instance and checks the [toolchain lock](../eng/toolchain-lock.json).
The [component declaration](../.vsconfig) includes ARM64 UWP support, Spectre
libraries, SDK, NuGet and native package-reference support. PowerShell 7, Git and
the independently pinned release validators have their own prerequisites.

These commands illustrate one ordinary x64 qualification, not the complete
matrix or an offline-source guarantee. Choose an absent package output under a
fresh directory you own; no signing, installation or publication occurs:

```powershell
./scripts/Resolve-MSBuildToolchain.ps1
./scripts/Test-RepositoryPolicy.ps1
./scripts/Invoke-Build.ps1 -Configuration Release -Architecture x64 -MaximumBuildProcessCount 2
./scripts/Invoke-TestSuite.ps1 -Project HeadlessAll -Configuration Release -Architecture x64
./scripts/Invoke-Build.ps1 -Configuration Release -Architecture x64 -BuildEntryPoint PackagedApplication -UnsignedPackageOutputDirectory C:/OwnedFreshDrop/x64
```

Use the [independent-build entry point](../scripts/Invoke-IndependentPackageBuildComparison.ps1)
for its documented two-checkout package comparison, and the
[SBOM procedure](supply-chain-evidence.md) for exact artifact/component evidence.
Neither replaces corresponding-source closure or rights review. Keep normal
build concurrency explicit and bounded; this procedure does not change its
default. Recycle released reconstruction scratch only after source and actual
verification evidence are independently retained; record any remaining consumer
and its next reassessment checkpoint.
