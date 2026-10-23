# Adobe static-library debug information qualification

Status: focused native packaging and application JPEG regression qualification
pass; final application matrix, independent-package, SBOM and release
qualification remain separate.

## Reproduction and authoritative contract

The final-candidate x64 Release build at signed checkpoint
`9a955dc598154ea1b0377030c0742c299e51c262` succeeds but reports LNK4099 for
`XMPCoreStatic.pdb`. Its subsequent headless run passes all 378 testcase
elements in five suites with zero failures/errors/skips. Neither result is a
warning-free release qualification or proof for the subsequent recipe revision.

The independent public-client regression first reaches an actual behavioral RED:
the installed revision-1 archive fails native LINK `/DEBUG /WX` with LNK4099 and
LNK1218. Missing-prefix and external-header fixture failures preceding that run
remain separate diagnostics, not the accepted RED. No image files are involved.

Microsoft's [LNK4099 guidance](https://learn.microsoft.com/en-us/cpp/error-messages/tool-errors/linker-tools-warning-lnk4099)
identifies the missing compiler PDB and permits embedded object debug information.
Its [debug-information contract](https://learn.microsoft.com/en-us/cpp/build/reference/z7-zi-zi-debug-information-format)
requires distributing the compiler PDB with a `/Zi` static library; `/Z7` instead
retains full symbolic information in the objects. Removing debug information,
disabling `/DEBUG`, or ignoring LNK4099 would not preserve that contract.

The existing [vcpkg PDB helper](https://learn.microsoft.com/en-us/vcpkg/maintainers/functions/vcpkg_copy_pdbs)
copies PDBs adjacent to DLLs. It does not repair this static-archive packaging
boundary. CMake's [CMP0141 policy](https://cmake.org/cmake/help/latest/policy/CMP0141.html)
and [native format property](https://cmake.org/cmake/help/latest/prop_tgt/MSVC_DEBUG_INFORMATION_FORMAT.html)
provide the supported selection mechanism; no replacement compiler-option
parser, binary rewriting or extra SDK facade is introduced.

## Root-cause change

The existing Adobe overlay retains its exact upstream commit/archive digest,
parser-preservation patches, public Exiv2 boundary, mitigations and sanitizer
inputs. Only the local packaging revision advances from 1 to 2.

- The top-level CMake minimum already establishes CMP0141 NEW before enabling
  the compiler. Set `XMPCoreStatic`'s native format property to `Embedded`.
- A narrowly scoped upstream **build-configuration** patch removes its hard-coded
  `/Zi`. Otherwise the SDK's raw flag would compete with CMake's typed setting.
  No parser code or runtime behavior changes.
- Native generated Release Ninja commands contain `-Z7` while retaining
  `/guard:cf /Qspectre`. Native vcpkg owns patch application, both configurations,
  post-build validation, package metadata and ABI caching.
- The maintained independent CMake consumer links only the installed public
  `AdobeXmp::Core` package with unconditional LINK `/DEBUG /WX` and exercises
  public initialization/termination where the host can execute the architecture.
  CMake owns package resolution, compilation, linking and CTest; no private
  MSBuild property or alternate argument grammar is required.

The probe restores CMake's normal imported-header classification for this
standalone client, which has no competing Exiv2-bundled header tree. First-party
compile warnings remain errors; the archive's linker diagnostics are never
excluded. Temporary-directory MSBuild MSB8029 diagnostics remain visible and
are not represented as application build failures or warning-free qualification.

## Focused native results

| Architecture | Native package Debug/Release | Public client Debug/Release       | Runtime boundary                           |
| ------------ | ---------------------------- | --------------------------------- | ------------------------------------------ |
| x64          | Both pass                    | Both link with `/DEBUG /WX`       | Native CTest initialization passes in both |
| x86          | Both pass                    | Both link with `/DEBUG /WX`       | WoW64 CTest initialization passes in both  |
| ARM64        | Both pass                    | Both cross-link with `/DEBUG /WX` | No ARM64 runtime claim on this x64 host    |

The x64 revision-2 native package ABI is
`1eb790542f1570de11214190b96d61aba6c81f5d0b25d6128aec33f7e0113655`.
Native installation changes the selected target set: returning to x64 for its
remaining Debug probe restores the qualified binary-cache input rather than
rebuilding or pretending a removed installation still exists.

Restoring the actual product's x64 dependency graph succeeds through native
vcpkg, including rebuilding Exiv2 against revision 2. The owning JPEG project's
native build log reaches the completed executable without warnings or errors.
Its terminal process exit was not retained across an interrupted tool-output
handoff; attempts to recover it through desktop MSBuild's replay API encountered
runtime compatibility failures. This is not recorded as a verified build exit.
The subsequent native JPEG suite **does** return exit 0: all 165 testcase elements
pass (838,575 assertions), with zero failures/errors/skips. The exercised test
executable has SHA-256
`cd778bf277a6c58870b9e2b32ad0319542726c0de3a32f4e01fe5ef6b9a54d75`.
Its original JUnit output and native logs are retained independently of the
working package tree. Final candidate build verification remains necessary.

The manual CI wiring contract also reaches meaningful RED on missing probe
selection, then GREEN after wiring it to every actual build-matrix configuration.
Native YAML and Actions validation pass. No hosted workflow is dispatched and
push/PR/schedule triggers remain absent. These are local source-wiring results,
not hosted execution evidence.

Original successful and failed command logs, raw JUnit results, the ordinary
checkpoint review and actual runtime output are retained in external evidence
group `b70f634d955345ee838ae8f64e56c9db`, execution
`1f8e9d2c-38ef-4f91-a9d3-d114332d547e`. Isolated working material is separately
registered as inventory `fc584966d35149bbbc0e0d99556367d6`; its native preservation
and disposal outcome must be recorded separately, not inferred from this text.

The first patch attempt failed native context matching and is retained as
counterevidence. The corrected contextual patch applies through native vcpkg.
No warning suppression, compatibility shim, security review or external review
was performed. A package-only public-client pass does not qualify the changed
application graph, corresponding-source delivery, signed update or Store release.
