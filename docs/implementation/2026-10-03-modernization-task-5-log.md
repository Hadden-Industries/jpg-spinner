# Task 5 implementation evidence

Scope: approved Task 5 coefficient-transform adapter, R2, continuing execution
`5b36a12b-692d-483d-a6b9-c446e7a6eca2` from signed Task 4 commit
`a6d0cd5e9615dedf1e7ba902a00014919dadeacc`. No UI, storage or metadata-rewrite
implementation is included in this slice. User authorization on 2026-10-03
permits research-led resolution of open decisions and continued implementation.

## Decisions and source refresh

- First principles: preserve quantized coefficients and their frequency-dependent
  quantization, permit only explicitly planned edge loss, and publish no partial
  output on failure. Transposed transforms also transpose the quantization table;
  preserving its byte order would change the reconstructed image.
- Native reuse: keep entropy parsing/transformation in public TurboJPEG 3 APIs,
  with standard-library RAII and `std::stop_token`. A bounded caller-owned span
  is the output sink; a temporary `tj3Alloc` buffer with `NOREALLOC` avoids
  publishing partial bytes. The application does not implement a DCT codec.
- The [maintainer release API](https://api.github.com/repos/libjpeg-turbo/libjpeg-turbo/releases/latest)
  returned 3.2.0 on 2026-10-03. The prior selected dependency/license decision
  remains applicable; no different codec or new external library is introduced.
- The [3.2.0 implementation](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/3.2.0/src/turbojpeg.c)
  multiplies `MAXMEMORY` by 1,048,576. Correct the approved plan's mistaken
  decimal-MB assertion and use `maximumIntermediateBufferMemoryMebibytes=512`.
  The encoded-file bound remains independent. The working-memory parameter is
  not a total-process-memory guarantee.
- [TurboJPEG's interface](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/3.2.0/src/turbojpeg.h)
  defines `COPYNONE`, `NOREALLOC`, per-operation scan options and restart-block
  output configuration. Clear the context's header-derived progressive/arithmetic
  flags before applying explicit operation flags, including sequential output.
- [T.81](https://www.w3.org/Graphics/JPEG/itu-t81.pdf), B.2.4.4, defines the
  two-byte DRI MCU interval. Read only scanner-bounded DRI payloads. A constant
  interval is preserved; scan-dependent interval changes are explicitly
  unsupported because one native output setting cannot preserve them.
- [Microsoft's container-annotation guidance](https://learn.microsoft.com/en-us/cpp/sanitizers/error-container-overflow)
  requires consistent annotated STL objects across linked libraries. The first
  ASan attempt lacked the runtime library search directory; the corrected attempt
  exposed annotation ABI mismatches with ordinary Catch2/Exiv2 dependencies.
  Neither result is an ASan pass. Stop retrying that incompatible graph.
- Root correction: use [native vcpkg triplet flags](https://learn.microsoft.com/en-us/vcpkg/users/triplets)
  in a separate `x64-windows-static-md-asan` release-only dependency graph,
  instrumenting dependencies without suppressing container annotations. Keep
  normal triplets unchanged. Extend the closed executed-CMake policy to this
  exact fourth triplet and add a negative control for removed instrumentation.

## TDD and focused evidence

Commands use the locked toolchain via `scripts/Invoke-Build.ps1 -Configuration
Debug -Architecture x64`; focused Catch2 selection is `[jpeg][transform]`.

- Memory-limit and adapter declaration tests first failed to compile because
  their required new headers were absent: compile-time contract RED, not a
  behavioral failure. One incorrect scanner accessor and one missing standard
  header were fixture-authoring errors and were corrected, not counted as RED.
- Independent public-libjpeg coefficient extraction checks every block,
  frequency parity, component, precision and geometrically mapped quantization
  entry. The new spatial matrix failed at horizontal flip before implementing
  the explicit domain-to-TurboJPEG operation mapping, then passed.
- Explicit scan-organization tests observed four incorrect outputs before
  the context-flag correction; the fixed implementation passed sequential/
  progressive crossed with Huffman/arithmetic and all three requested modes.
- Partial-edge test first observed a generic failure instead of the perfect-
  transform error, and failed explicit trim. Both passed after edge-policy
  implementation. The wider domain-plan geometry matrix passed as characterization.
- Native restart fixture proved an input interval of two MCUs, observed output
  zero (RED), then two after preserving DRI through `TJPARAM_RESTARTBLOCKS`.
- A 512x512 real fixture succeeded incorrectly under a deliberately ignored
  one-MiB budget. Applying the budget made it fail without publishing bytes;
  the same fixture succeeds with the production budget (positive control).
- Failure-path characterization verifies tiny destinations and truncated entropy
  leave sentinels intact. Cancellation and oversized malformed input also leave
  the destination unchanged. No sleeps or retry-until-green assertions are used.
- Latest normal focused run at this checkpoint: 836,951 assertions in 12 cases.
  This is development evidence, not final candidate or full-plan completion.

## Verification and retry discipline

Consolidate the slice before broad assurance. Follow-up checks cover changed
contracts and affected callers. Do not repeat an unchanged failed command: after
two unsuccessful integration attempts, inspect the root cause and change the
supported integration strategy before further execution. No review has been
dispatched during this implementation slice.

The separate ASan dependency graph compiled successfully. The maintained command
`pwsh -NoProfile -File scripts/Invoke-JpegSanitizerTests.ps1` passed 836,956
assertions in 14 cases after formatting and the last behavioral changes. These
include observed RED/GREEN for inconsistent output dimensions and a zero codec
budget. The policy negative controls passed, including removed ASan instrumentation.
The targeted clang-tidy run reported no user-code diagnostics. Final normal/profile
verification and Task 5 commit follow this consolidated candidate; their raw
outcomes belong to the task transcript and retained HISEW receipts.

Generated binaries/dependencies stay under ignored `artifacts/` and
`vcpkg_installed/`; retain the sanitizer graph for Tasks 6/7/17. The pre-existing
`tmp/` research is not part of the commit and remains input to Task 6.
