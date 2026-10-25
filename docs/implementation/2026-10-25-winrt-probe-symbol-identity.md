# Stable symbol identity in the WinRT configuration probe

The [manual QA run at signed source `887bec0adf5b`](https://github.com/Hadden-Industries/jpg-spinner/actions/runs/40127045938)
finished with all five build lanes, policy, ASan, MSVC analysis and interactive
input publication passing. Its unsigned-package job built and natively inspected
all three architecture packages, produced the bundle, and passed the payload
comparison and false-block-hash controls. It then failed the existing
`Test-WinrtErrorSourceLocationPolicy.ps1` cross-root executable comparison.
Later Store-component and SBOM refusal controls in that step did not execute.
The overall run remains failed; its partial package output is not a release.

## Reproducer and isolation

The original public probe reproduced the same failure locally in six seconds
against the existing qualified C++/WinRT projection:

```powershell
./scripts/tests/Test-WinrtErrorSourceLocationPolicy.ps1 `
  -WinrtProjectionDirectory './src/JpgSpinner.App/Generated Files' `
  -OwnedOutputRoot '<native-owned-probe-workspace>'
```

All three executables returned zero: Release preserved `E_ACCESSDENIED` while
omitting optional error-origin file/line data, and Debug retained those origins.
Only the two Release executable hashes differed. Native `PEReader` observations
showed differing code/data and debug identity, so this was not merely a stored
PDB filename. Both images already embedded the same basename-only PDB reference.

Controlled native rebuilds varied one input at a time:

| Changed input                                              | Complete Release executable bytes            |
| ---------------------------------------------------------- | -------------------------------------------- |
| None: rebuild at the same source/output/intermediate paths | Identical                                    |
| Output directory only                                      | Identical                                    |
| Intermediate directory only                                | Identical                                    |
| Source directory only                                      | Different; matches the original second image |

The fixture placed its callback and observation state in an unnamed namespace.
Microsoft's native LINK maps exposed different generated namespace identifiers
in the callback's decorated name: `?A0xeb5633e6` versus `?A0xb0a1d6da`.
The compiler-generated identity therefore introduced an independent source-root
input into a probe intended to isolate WinRT error-origin configuration.

Microsoft documents [unnamed-namespace translation-unit scope](https://learn.microsoft.com/en-us/cpp/cpp/namespaces-cpp#anonymous-or-unnamed-namespaces)
and treats [decorated names as compiler implementation details](https://learn.microsoft.com/en-us/cpp/build/reference/decorated-names).
Those references explain the linkage/name boundary; the exact path-sensitive
identifiers and resulting image differences are observations of the selected
MSVC 14.51.36231 toolchain, not a promised general name-generation algorithm.
Microsoft's [`/PDBALTPATH` contract](https://learn.microsoft.com/en-us/cpp/build/reference/pdbaltpath-use-alternate-pdb-path)
controls the recorded PDB location, so it was not expanded into a general
source-path reproducibility guarantee.

## Repair and retained assertions

The single-translation-unit fixture now uses the explicit namespace
`jpg_spinner::test_support::winrt_error_origin_probe`. A function-local using
directive keeps the original observation expressions readable. Its state,
callback signature, HRESULT, control flow and configuration checks are unchanged.
The comment explains why this particular cross-root fixture needs stable
qualified symbol names; it introduces no application-wide namespace rule.

The unchanged public probe then passes against two distinct source directories:
complete Release executable hashes match, both native error-origin behavior
checks pass, and Debug retains nonempty source-file/line observations. No byte
comparison was removed or masked. No compiler/linker option, application code,
vendor header, metadata comparison exception or release requirement changes.
The existing failing test supplies the regression seam; no source-text test is
added merely to require a particular namespace spelling.

## Evidence and remaining qualification

The native repro workspace is inventory `f0ff7869726645be9dcfe6a30c6d47ac`.
Raw RED/GREEN output, native PE observations, same-root/output/intermediate/
source controls, original retained image/PDB and LINK maps are under the owning
external operator evidence group `a557edf27e6649e8b13d1c72004fb780`. Required
originals must be promoted before native disposal of spent probe intermediates.
Cleanup refusals preserve their sources and are not permission for a custom
removal route.

This focused repair does not turn the prior hosted attempt into a pass or prove
that its unexecuted packaging controls completed. A future remote dispatch needs
its own explicit authorization. Physical ARM64 execution, device/OS acceptance,
trusted upgrade, WACK, legal/source-delivery and release approval remain separate.
