# Task 7: independently validated JPEG output — 2026-10-03

Accepted modernization plan, Task 7, captured as HISEW snapshot
`4458a6bf-d57a-4d51-921b-baa97afb71b5`; continues the existing R2 route.
Baseline: `ca005559990e2f6414d260abfc12ec9f5cbf6e8a`.
User approved implementation, TDD, configuration changes, milestone commits and
bounded consolidated review. No publication is authorized by this milestone.

## Slice and purpose

An application receives one move-only, owned output only after structural,
metadata-preservation and full-decode checks succeed. This supplies Task 9's
stage/hash/commit boundary; it does not establish transaction or Store readiness.
The public operation owns source bytes and the approved analysis across its
synchronous work; internal scanner, coefficient and metadata operations stay
inside the JPEG module. Cancellation publishes no output.

Test-first route: first exercise a positive native fixture and a truncated output
through the internal validator, observe behavioral RED, then add one-property
negative controls and the public engine contract. Independent expectations use
literal dimensions/orientations, public Exiv2 observations, native libjpeg decode,
known cryptographic vectors, and retained marker payloads. Final scope is all JPEG
tests under AddressSanitizer and the configured governed full profile.

## Reuse research and residual application contract

Checked 2026-10-03: [libjpeg-turbo releases](https://github.com/libjpeg-turbo/libjpeg-turbo/releases)
still designate the adopted **3.2.0** as latest stable. Retain its pinned native
integration and existing license/notice decision; no new package is introduced.
The [3.2.0 consumer manual](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/3.2.0/doc/libjpeg.txt)
specifies scanline decompression, `jpeg12_read_scanlines` with `J12SAMPLE`, warning
callbacks, and matching input/output color spaces to bypass color conversion.
Use those native capabilities instead of an entropy decoder or full-image buffer.
The native memory manager's `max_memory_to_use` bounds virtual coefficient buffers
and fails when backing storage would be needed; it is not a total process-memory
limit. Keep scanner input/pixel/scan bounds and checked application row storage.

Reuse the repository's bounded scanner and isolated public Exiv2 payload APIs.
Exiv2 already owns TIFF/RDF parsing and semantic metadata values. WIC does not
provide this adopted 12-bit/arithmetic coefficient and preservation contract;
TurboJPEG's whole-image decompression requires an unnecessary uncompressed image
buffer. Neither replaces the application's comparison against an approved plan.
The only custom validation is that comparison and ownership composition, not a
second JPEG, TIFF, XML or cryptographic implementation.

Use platform [BCryptHash](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcrypthash)
with the existing SHA-256 provider for immutable encoded bytes and preserved-marker
evidence. It is available above the application's Windows minimum. The existing
512 MiB encoded bound fits its ULONG input length; check this before conversion.
Storage will independently hash the closed staged file in Task 8/9.

Names distinguish a requested scan organization from the observed coding process,
and validated output from intermediate encoded bytes. Typed symbolic validation
rules carry diagnostics without localized/native text. Native errors and warnings
never reach presentation as strings. Declarations document ownership and failure.

## Workflow and resource observations

The reported engine-update failure was already superseded: packaged setup inspect
reports ready, matching dev.16 release `a420d0c8…`, usable launchers, and update
attempt `a420d0c8-01` complete. Its native result records process exit 0 and complete
diagnostics. Session hook observations match this session/release, although their
receipt match is false; these are retained observations, not fresh activation proof.
No installation retry or workflow-control changes were required.

Task 6 research working copies remain inputs until this consumer qualification
settles. Owner: modernization implementation; reassess after Task 7's commit.
Raw failed/successful verification and reviews remain evidence. The user separately
authorized recycling the obsolete `tmp/` clone/PDF; both are now absent.

## Development evidence

Execution `5395eb4e-215e-4066-a61e-10bee37b24b7`, generation 1.
The engine's `--baseline` argument names repository-policy authority, not a Git
commit; the initial start rejected that argument without mutation. The personal
route then started against the accepted snapshot; its recorded start HEAD is the
baseline above. No authority was switched or waived.

Focused command: build the JPEG test vcxproj with the locked MSBuild executable,
`-noAutoResponse -nodeReuse:false -p:Configuration=Debug -p:Platform=x64`, then
run the selected Catch2 tags with seed **1220703**.

- Validator behavioral RED: real complete JPEG rejected (1 failing assertion).
- Metadata RED: stale Exif orientation, standard-XMP orientation, derived width,
  changed ICC and changed comment all incorrectly accepted (5 failures).
- Public engine RED: all eight approved orientation cases rejected (8 failures).
- Residual MPF diagnostic RED: incorrect symbolic rule (1 failure), then fixed.
- Deterministic adapter RED: absent valid controlled completion; then implemented.
- Initial focused GREEN: **404 assertions in 12 cases** before final formatting.
  Matrix includes 30 native supported precision/color/process combinations.

Initial missing `<algorithm>` and Windows `max` macro errors were compile/setup
mistakes, not behavioral RED. A draft test compared TurboJPEG color enum integers
with libjpeg enums; replaced with explicit documented color meanings and native
source preconditions. This was a test correction, not a product defect.

Two draft Exif mutation attempts stalled and were terminated within a bounded
investigation. The statement that the fresh encode overload caused the stall was
an unconfirmed hypothesis: the existing-buffer attempt also stalled. Inspection
also found a draft iterator range made from separately occurring string literals;
those can denote distinct arrays and are not a valid iterator pair. The maintained
fixture retains one identifier span. Its corruption controls now mutate one
literal TIFF entry with native observations before/after, avoiding serializer
changes to unrelated fields. No Exiv2 production defect is claimed from the stall.

Cancellation before production work is exercised. Controlled completion after a
real native pipeline is exercised through the batch adapter; this establishes its
public completion contract, not the timing of cancellation inside a production
TurboJPEG call. Production checks immediately before/after that noninterruptible
call, between decode rows, in native progress callbacks and before approval are
subject to the consolidated review. No production phase-control test hook is added.

Integration RED also exposed an identity pixel transform with authoritative Exif
orientation 1 and conflicting standard XMP orientation 6. Canonicalization fixes
that reviewed conflict without dropping a valid thumbnail. A separate identity
fixture then failed because Exif width remained 32 instead of the real 48 pixels
(1 failing assertion out of 9). The writer now updates every present derived
dimension even for identity, preserves legal Exif types, and avoids rewriting
already-correct packets. Independent validation enforces that same contract
without invoking either serializer as its oracle. Direct Exif/XMP dimension
corruptions are rejected even when the pixel plan is identity.

Current affected development GREEN: **882 assertions in 46 cases**, seed 1220703,
for `[jpeg][metadata],[jpeg][validator],[jpeg][engine]`. The added independent
identity controls are characterization/sensitivity checks after the causal RED,
not additional claimed pre-fix failures.

## Approved resume and consolidated-review disposition

The naming pause (generation 2) ended on the user's "Follow your recommendation"
approval. Generation 3 resumed the same execution, rather than creating a second
change. The shared value is now `JpegFrameProperties`, with its matching header,
all source/output consumers and project items updated, without an alias. Source
analysis still correctly calls its field `sourceProperties`. The marker-evidence
comment now specifies physical **output** order and output framing.

One consolidated read-only Claude Code assessment covered the frozen 25-path
candidate `0eef13025b02f01b4a5123041baad2915bc132a5`. The retained native result
reports Claude Code 2.1.285, `claude-opus-5-5`, exit 0, and no spawned agents.
It did not run the Windows checks. Reports, original patch and assignment remain
under `C:/Users/maksy/.hi/w/e/task-artifacts/jpg-task7`.

- D1: The example staging command was incomplete for a fresh index, but the
  original candidate already staged TestSupport and this log. `git add` of a
  subset does not unstage the other entries, so the asserted broken candidate
  does not follow. Clarify the plan's example to include the complete task scope.
- D2: Replace the Photoshop writer-as-oracle call with independent observations
  of both payloads through [Exiv2's public IRB locator](https://github.com/Exiv2/exiv2/blob/v0.28.9/include/exiv2/photoshop.hpp).
  Require no output resource 1033/1036; compare every other complete native-bounded
  record in source order. Reuse native format semantics, not a custom IRB parser
  or expected serializer. The retained-name overflow guard remains necessary for
  the [pinned locator implementation](https://github.com/Exiv2/exiv2/blob/v0.28.9/src/photoshop.cpp).
  The one-property negative control first failed on the wrong symbolic rule:
  changed unrelated data was classified as thumbnail removal. After repair,
  restored preview and changed retained resource receive their distinct rules.
- D3: Apply the approved frame-properties name and correct output digest comment.
- G1/G2: Native TurboJPEG independently observes `TJERR_WARNING` for shortened
  entropy and `TJERR_FATAL` for an undefined Huffman table. The validator rejects
  each with `FullDecode`; the actual production engine propagates a structured
  coefficient-transformation failure for each analyzed source. A separate public
  interface test uses the deterministic adapter to propagate a **real validator**
  ICC rejection from controlled completion. It is not a claim that the production
  writer was made faulty or that an internal production phase was injected.
- G3: Retain the cancellation timing limitation above. Check placement is verified
  by inspection; deterministic completion cancellation belongs to the public test
  adapter. Task 9/11 must independently check cancellation at storage boundaries.
- G4: Quantization and restart preservation are part of lossless fidelity, not
  merely successful decoding. Four behavioral RED assertions showed that valid
  eight-/twelve-bit streams with changed quantizers or restart intervals were
  approved. Read the source coefficients and fully decode completed output in
  separate native lifetimes. Use the documented
  [critical-parameter copy API](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/3.2.0/doc/libjpeg.txt)
  while the complete reader awaits `jpeg_finish_decompress`; compare public
  compressor table values per component. The initial restart check observed a
  native current MCU interval; the follow-up below replaces that insufficient
  single-point observation with the full scanner-bounded interval policy.
  Axis exchange transposes the natural-order frequency table, as in the
  [native transform implementation](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/3.2.0/src/transupp.c).
  No DQT parser or access to private saved-table fields is introduced. Typed rules
  distinguish quantization from restart changes. The first final-reset control
  exercised single-scan output only; it did not prove progressive scan history.
- G5: A real source without APPn/COM succeeds through the production engine;
  its zero-marker SHA-256 matches the literal empty-message known answer.
- G6: The [native output-size contract](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/3.2.0/src/turbojpeg.h)
  supports the conservative 4:4:4 bound for the adopted 8/12-bit and four-component
  cases. Release the writer's metadata and worst-case encoded buffer before
  independent validation. At the configured extreme, transform-phase allocations
  can approach 512 MiB source + two 512 MiB encoded buffers + 512 MiB native virtual
  coefficient budget + 32 MiB reconciled metadata. That is approximately 2 GiB
  **before** inventories, other native allocations and allocator overhead: it is
  neither a measured peak nor a process-wide cap. Composition then overlaps source,
  codec buffer and completed output; validation no longer retains the codec buffer.
  Allocation failure remains a redacted structured failure, never output approval.
  The planned one-at-a-time Batch processing and final memory qualification remain
  necessary; this does not claim a maximum-size x86 allocation succeeds.
- G7: The pinned Exiv2 source and public
  [XMP parser contract](https://exiv2.org/doc/classExiv2_1_1XmpParser.html)
  use Exiv2 errors or nonzero return status; XMP toolkit errors are caught by the
  native wrapper. Source search found no explicit `throw std::...` in the adopted
  library. `Exifdatum::write` catches its documented `std::out_of_range` internally;
  the production semantic snapshot does not use that print path. Retain explicit
  native-error handling and the engine's `std::bad_alloc` handling; do not add a
  blanket exception catch that conceals programmer faults. This source inspection
  is not proof that allocation can never fail inside a dependency.
- G8: Behavioral RED is stronger evidence than absent compilation and is recorded
  accurately. Malformed Extended XMP offsets fail the fresh structural scan as
  `CompleteJpegStructure`. The test adapter's supplied factory is explicitly required
  to be deterministic and concurrency-safe. Final ASan/full results remain pending
  until their native captures exist; no checklist or report substitutes for them.

After these repairs and formatting, the affected development suite passes
**955 assertions in 51 cases**, seed 1220703. The quantizer test's initial byte
arithmetic error and new fixture include/getter corrections were setup mistakes,
not behavioral RED. All active source/test/doc references use the approved frame
name. One follow-up assessment is limited to these repairs and their actual delta;
it must not repeat the unrelated consolidated review.

The narrow follow-up of tree `16714355…` found that progressive native preload
can expose only the final DRI at the chosen observation point. This inference
was checked against [pinned `jpeg_start_decompress`](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/3.2.0/src/jdapistd.c)
and reproduced: final-reset progressive controls failed at both 8 and 12 bits
(two behavioral RED assertions). The repair removes the single-point restart
snapshot. Independently inspect only scanner-proven two-byte DRI payloads and SOS
ordering, as specified by [T.81 B.2.4.4](https://www.w3.org/Graphics/JPEG/itu-t81.pdf).
Pre-first-scan definitions replace prior values; once a scan begins, any changed
interval is rejected for either source or output, including a late reset. Compare
the two effective fixed MCU intervals. This is the adapter's existing restricted
restart contract, not a claim that all legal scan-dependent JPEG intervals are
supported. The native library still owns entropy decoding and restart-marker
validation; no new marker walk or entropy parser is added. The critical-table
comment now distinguishes complete coefficients from complete output scanlines:
single-scan finish may still consume terminal markers.

Negative controls now cover sequential/progressive output at both precisions and
reject a source with masked interval changes directly at the validator seam.
Positive public-engine controls approve restart interval 3 through 90-degree axis
exchange and sequential-to-progressive conversion at both precisions. The first
positive-control draft's signed/unsigned warning was a setup correction, not RED.
The follow-up found no actionable issue in the other repairs. Its path/identifier
citations were checked against its frozen clone; pinned headers were unavailable
there, and references to legacy IJG behavior were explicitly inference until the
live pinned source check above. It ran no native commands or subagents.

This semantic restart repair invalidates the earlier all-JPEG ASan observation
and full run `21dfd709-93d4-4ef4-b488-bb4cae62d04e` for the new candidate. Keep
those successful historical results bound to `16714355…`; rerun the affected
ASan suite and exact-snapshot governed full profile after final staging. One
final bounded follow-up may assess only this restart repair and its controls;
do not reopen settled Task 7 questions.

## Final qualification boundary

The registered full profile contains policy checks, the Debug x64 solution build,
Domain/JPEG tests and a 60-second scanner fuzz run. Its covered-path declarations
are absent: successful receipts establish those commands, not measured path
coverage. Their build/test composition exercises all added production/test code;
the document and native contracts require read-only assessment. Supplement with
the accepted all-JPEG AddressSanitizer qualification. No profile reconfiguration,
Store packaging, ARM64 run or production transaction qualification is included.

Final independent assessment uses the already accepted cross-vendor read-only
CLI route on one frozen plain clone; it is static assessment, not independent
execution of the Windows suite. The installed Review Agent instructions were
read after native discovery, but this session exposes no review-dispatch tool.
Do not relabel the alternate assessment as an OpenAI Review Agent run. Retain
the frozen patch, actual report, fabrication check, final command captures,
resource disposition and signed-commit readback in the external Task 7 handoff;
do not append later results here and invalidate the final exact-snapshot receipts.
