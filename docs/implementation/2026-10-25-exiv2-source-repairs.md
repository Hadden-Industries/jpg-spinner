# Exiv2 source repairs from the supplied CodeQL alerts

## Accepted scope and proof plan

The maintainer approved the fix-first recommendation on 2026-10-25. This
continues Tasks 6, 17 and 20 under the existing modernization execution; the
previous batch-dismissal proposal is set aside. Keep every GitHub alert open
until its actual disposition is separately authorized. Do not run another
security scan, external review or delegated implementation.

Exiv2 0.28.9 remains the latest stable upstream release. Use the existing native
Exiv2 overlay and its immutable archive, standard Git patches and vcpkg build
integration. No additional port, unreleased library, replacement converter,
parser, compatibility wrapper or system-wide configuration is introduced.
Any changed overlay revision must agree with the application manifest and its
closed policy/reporting controls. Historical SBOM/source fixtures retain their
original versions and bytes.

| Repair increment                           | Requirement / hypothesis                                                                                                                                                                                                                                               | Focused proof and limit                                                                                                                                                                                                                                                              |
| ------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Filesystem configure probe, alerts 9/10    | The actual Windows probe passes a wide-character path to `%s`. CMake consumes its exit status, not the printed path. Remove the unnecessary output and retain the native filesystem call.                                                                              | Compile/run the actual upstream CMake module with MSVC format diagnostics as errors: original source must fail specifically for the format mismatch, patched source must succeed. Preserve native configure logs. Cross-compilation is not runtime proof.                            |
| Windows character conversion, alerts 12–14 | Investigate the actual public conversion boundary against Microsoft's byte/code-unit contract. Prefer correctly typed native storage and exact capacity values if a repair is justified, preserving byte order, embedded NULs, error behavior and supported encodings. | First qualify actual upstream behavior through focused native tests; then verify any repair with Unicode, boundary and failure controls. Distinguish stronger API contracts from a claim that an exploit was reproduced. No speculative vendor change is adopted without this proof. |

The engineer owns integration. Use isolated qualification outputs and the
installed native CMake/MSVC/vcpkg tools, bounded concurrency and the existing
timeout categories. Reuse compatible dependency caches read-only where useful;
run only affected component checks while editing. Reserve the product's full
matrix and a separately authorized CodeQL run for the final candidate.

The actual source revision/archive/patches, commands, initial failures and
subsequent results must remain attributable. An upstream change in development
is useful design evidence but does not make an unreleased dependency eligible.
Native CMake, compiler and library behavior own the test oracle; do not parse
C++ or reimplement the Windows conversion APIs to manufacture a passing result.

Retain required reports and original inputs through HISEW, release consumers at
each checkpoint and use native cleanup. Preserve directories blocked by the
known cleanup admission defect until it is repaired; do not introduce another
deletion route. Reassess after two unsuccessful approaches to the same blocker
or if a public API/metadata-preservation change becomes necessary.

## Implemented boundary and focused evidence

Exiv2 `0.28.9#2` retains the same upstream version/archive, external Adobe XMP
backend and existing patches. Its two added patches remove the unused variadic
filesystem diagnostic and replace all three byte-to-wide pointer casts with
owned `std::wstring` storage. Native Win32 calls still perform conversion.
Their capacities use the exact returned code-unit/byte counts; checked narrowing
precedes allocation/API calls. Byte copies preserve the API's native-endian
representation, embedded NULs and existing conversion flags/error contract.
This improves the API/storage contract without claiming an exploit was reproduced.

The actual original CMake module fails with MSVC **C4477** under `/we4477`;
the patched module passes native configure/compile/link/run on x86 and x64.
Its ARM64 check passes cross-compilation/linking only. The production
port also supplies that narrow native warning-as-error to its configure checks.
The focused clang-tidy rule fails on exactly the three original conversion casts
and passes after the repair, without a custom C++ parser, source suppression or
CodeQL exclusion. This is a targeted type-contract check, not another security
scan or review.

Twenty independent byte cases pass through the actual public Exiv2 API both
before and after the repair. They cover every supported Windows conversion
direction, empty/embedded-NUL input, endian forms, surrogate pairs, replacement
policy, odd-length refusal and unchanged input on failed/unsupported conversion.
One shared catalog supplies the standalone native component client and the
ordinary product-linked Catch2 test; no converter implementation is copied.

Native vcpkg installs the exact revised x64 metadata closure successfully,
including the production Adobe/XMP feature graph and all four native patch
applications. Both Debug and Release installed-package clients pass all twenty
cases using the package's public CMake configuration. The source-only probe
isolates charset code through supported upstream options; it does not substitute
for that installed production-graph integration. Compiler/build-tool identities
and complete original reports remain in HISEW evidence inventory
`4ff2e26686144d8aa31e64d45d5ced87`; working material belongs to
`ec6f90a2d6d64a598d02f3ce832ec836`.

The initial harness path/header/preparation setup failures remain recorded and
are not behavioral RED evidence. Temporary-root MSB8029 diagnostics are retained;
this is not a warning-free final product qualification. No large-memory stress,
full application build matrix, native ARM64 run, new CodeQL analysis, review,
remote source publication, alert dismissal or source-delivery approval is claimed.
The other previously triaged alerts remain separate; GitHub must observe a
future deliberately authorized analysis before these five alerts can be reported
as fixed there. The old batch-dismissal recommendation remains superseded.

The affected repository-policy reporting check, native workflow syntax and
source-wiring checks pass. Negative controls reject omitted charset qualification
and diagnostics outside the retained artifact directory. Canonical qualification
budgets remain unchanged. Scoped PowerShell lint reports no diagnostics in the
five affected test scripts; the one-line repository-policy revision retains its
five unchanged baseline style diagnostics. Native clang-format checks pass for
all three added C++ files. The product-linked Catch2 adapter is wired into the
existing project but awaits compilation/execution in the final product campaign;
the shared cases have already run through the actual native package clients.

## Reusing the focused checks

`scripts/tests/Test-Exiv2FilesystemProbe.ps1` accepts the integrity-verified,
prepared upstream `-SourceDirectory`, a fresh `-OutputDirectory` and the selected
`-Architecture`. It configures the actual upstream module through native CMake.

`scripts/tests/Test-Exiv2WindowsCharset.ps1` accepts fresh `-WorkingDirectory`
and `-EvidenceDirectory` paths, plus architecture/configuration. Select either
`-SourceDirectory` for the actual source/type-contract qualification or
`-InstalledDependencyPrefix` for the actual production package. The latter is
also wired into every existing manual CI build lane. The script never restores
a substitute dependency graph; the caller must supply the package it qualified.
Native runtime execution occurs only where the host can execute the selected
architecture. Source and installed-package evidence remain distinct.

## Authoritative references

- [Exiv2 stable releases](https://github.com/Exiv2/exiv2/releases)
- [Pinned filesystem module](https://github.com/Exiv2/exiv2/blob/v0.28.9/cmake/FindFilesystem.cmake)
- [CMake source-run checks](https://cmake.org/cmake/help/latest/module/CheckCXXSourceRuns.html)
- [MultiByteToWideChar byte/code-unit contract](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar)
- [WideCharToMultiByte code-unit/byte contract](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte)
