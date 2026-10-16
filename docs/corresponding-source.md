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
