# Task 4: bounded JPEG inventory and fuzzing

## Scope and ownership

Resume from `c88215c9a04d09ec9bf996e926271ce77ec6d593` and the existing Task 4
working changes. HISEW execution: `8dad17ee-b1cc-4221-9b48-eaaad0e011b1`.
Accepted-plan snapshot: `08e82b69-77f8-45ec-a3c3-04592a8a8fb5`.
This is the R2, test-first structural boundary for lossless processing, not a
complete JPEG decoder, metadata interpreter, or permission to transform a file.
No implementation subagents or additional reviews were run, per user instructions.

The inventory owns values and source-relative ranges, not borrowed storage.
TurboJPEG remains responsible for actual entropy/coefficient validity. Exiv2
remains responsible for TIFF/RDF/property semantics in Task 6. In particular,
EOI exposes every appended byte; Task 6 correlates Motion Photo/container XMP
and rejects unsupported appended assets before transformation. Zero bytes are
not silently treated as disposable padding. External C2PA XMP references also
belong at that existing metadata boundary, not in a custom XML parser.

## Authoritative implementation decisions

- [ITU-T T.81](https://www.w3.org/Graphics/JPEG/itu-t81.pdf), B.2.1 and B.2.2:
  EOI terminates the codestream; sampling factors are 1–4 and DCT quantization
  table selectors are 0–3. Hierarchical and reserved process extensions are not
  opaque metadata that a single-image transform may preserve blindly.
- [libjpeg-turbo 3.2 documentation](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/3.2.0/doc/libjpeg.txt):
  deferred height/DNL is unsupported. The scanner reports
  `UnsupportedJpegDeferredHeight`, not corruption, without a rewriting shim.
- [Motion Photo format 1.0](https://developer.android.com/media/platform/motion-photo-format):
  appended resources and primary-item padding require container metadata.
- [C2PA 2.4](https://spec.c2pa.org/specifications/specifications/2.4/specs/C2PA_Specification.html)
  and the [JPEG Systems reference implementation](https://gitlab.com/wg1/jpeg-systems/reference-software/jumbf-reference-implementation-2):
  exact UUID plus label identifies the manifest store. The reference writer in
  `dbench_jumbf/src/dbench_jumbf.cpp` repeats XLBox along with LBox/TBox in
  continuation packets. Only a bounded identification prefix is reconstructed;
  this is not a JUMBF content parser or credentials validator.
- [Microsoft ASan build reference](https://learn.microsoft.com/en-us/cpp/sanitizers/asan-building):
  `/fsanitize=fuzzer` supplies libFuzzer's main and is combined with
  `/fsanitize=address`. Dedicated x64 targets compile the production scanner
  with instrumentation. Native sanitizer-library paths are appended after
  Spectre paths; the selected native runtime directory is provided only to
  the fuzz child process. No toolset downgrade or sanitizer substitute is used.

## Observed development evidence

Raw RED/GREEN output is retained in this task's tool transcript. These are
development checks, distinct from the final exact-candidate HISEW receipt.

- EOI range API: observed compile-time RED for absent `trailingDataRange`, then
  448 assertions / 24 scanner cases passed after implementation.
- Sampling bounds: observed behavioral RED for an out-of-range factor, then
  473 assertions / 25 scanner cases passed.
- Deferred height: after adding the error vocabulary, observed behavioral RED
  (`MalformedJpegStructure` instead of `UnsupportedJpegDeferredHeight`), then
  475 assertions / 26 scanner cases passed.
- Extended JUMBF continuation: observed behavioral RED (metadata error instead
  of the C2PA-specific rejection), then GREEN; inconsistent repeated XLBox is
  independently rejected.
- Unsupported process markers: observed behavioral RED for accepted reserved
  extension, then 492 assertions / 28 scanner cases passed. A preceding test
  compilation warning was a fixture defect, not behavioral RED.
- Installed-toolchain negative control: a one-byte libFuzzer input triggered
  an ASan heap-buffer-overflow at the deliberate `data[size]` access, exit 1.
  The fault was removed; the retained smoke ran 100 inputs successfully.
- Initial scanner fuzz smoke: seed 20260930, 747,018 runs in 61 seconds,
  exit 0, no reported sanitizer/invariant failure; peak reported RSS 687 MiB.
  Later source edits require a fresh fuzz run. This smoke is not exhaustive
  validation and does not establish leak-sanitizer coverage on Windows.

Commands: `scripts/Invoke-Build.ps1 -Configuration Debug -Architecture x64`,
scanner Catch2 filter `[jpeg][scanner]`, and
`scripts/Invoke-ScannerFuzz.ps1 -DurationSeconds 60`.

## Resource disposition

Recycled 4,140,580,959 bytes of obsolete build products from
`artifacts/{obj,bin}/{ARM64,Win32}` and `artifacts/{obj,bin}/x64/Release`.
Containment, absence of links, lack of active compiler/linker consumers, and
post-removal absence were checked. Recovery is available through Recycle Bin.
Verification logs and maintained inputs were preserved.

Keep x64 Debug/Fuzz outputs and the small generated fuzz corpus for Task 5
regression work; reassess after its commit. Keep pinned vcpkg dependencies for
the immediate TurboJPEG/Exiv2 implementation. Keep `tmp/pdfs` and the prior
`tmp/c2pa-rs-reference` research copy until Task 6's metadata decisions are
recorded; neither is a build input or commit input. Their next cleanup owner is
the implementing agent at the Task 6 checkpoint. Other historical policy logs
remain evidence, not disposable generated binaries.
