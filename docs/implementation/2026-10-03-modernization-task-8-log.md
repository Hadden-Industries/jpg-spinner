# Task 8: authoritative source-file revisions — 2026-10-03

Continues the accepted modernization plan and R2 route from signed Task 7
baseline `b5a63602280d23185a373035103c473361527a8f`. Implementation, configuration
changes and milestone commits are preauthorized; publication is not.

## Purpose, contract and research

Task 9 needs an independently captured revision before committing corrected
output. SHA-256 distinguishes same-length edits even when modification time is
restored. Length and UTC modification time provide change detection/diagnostics,
not a substitute for the digest or a promise of filesystem transaction atomicity.

Checked 2026-10-03 against official Microsoft documentation and the installed,
locked MSVC 14.51.36231, Windows SDK 10.0.28000.0 and C++/WinRT 3.0.260818.1.
The official NuGet version index still ends with C++/WinRT 3.0.260818.1.
Retain the existing Windows/Visual Studio redistribution and package-notice
decisions; this slice adds no downloaded cryptography or coroutine package.

- [StorageFile.OpenAsync](https://learn.microsoft.com/en-us/uwp/api/windows.storage.storagefile.openasync)
  and [StorageOpenOptions](https://learn.microsoft.com/en-us/uwp/api/windows.storage.storageopenoptions)
  provide the broker-compatible read stream and reader-only sharing choice.
  A `StorageFile` is a representation, not an already locked handle:
  [Microsoft file-I/O guidance](https://learn.microsoft.com/en-us/windows/apps/develop/files/best-practices-writing-files).
  Treat incompatible concurrent activity as a structured error, not a retry loop.
- [CryptographicHash](https://learn.microsoft.com/en-us/uwp/api/windows.security.cryptography.core.cryptographichash)
  provides native incremental SHA-256 under UniversalApiContract v1. Use a fixed
  64-KiB input buffer, append the actual buffer returned by
  [ReadAsync](https://learn.microsoft.com/en-us/uwp/api/windows.storage.streams.iinputstream.readasync),
  and check length accumulation and cancellation between reads. The API explicitly
  permits a returned buffer different from the supplied one.
- [C++/WinRT concurrency guidance](https://learn.microsoft.com/en-us/windows/apps/develop/cpp-winrt/concurrency)
  recommends native `concurrency::task<T>` for non-WinRT result types, while also
  supporting blocking `.get()` on background workers. Bounded integration found
  that MSVC's `ppltasks.h:651-667` requires a default-constructible, assignable
  result holder. Our accepted never-empty `ImageProcessingResult` deliberately
  has neither. Preserve that invariant, do not add an allocation wrapper/shim or
  a custom task type, and use a synchronous initialized-MTA worker contract like
  the JPEG engine. The Task 11 batch layer owns background scheduling. The adapter
  rejects STA/uninitialized callers before I/O, including Release builds.
- [DateModified](https://learn.microsoft.com/en-us/uwp/api/windows.storage.fileproperties.basicproperties.datemodified)
  supplies the modification instant; reject an unavailable zero date. Use the
  pinned [C++/WinRT clock conversion](https://github.com/microsoft/cppwinrt/blob/3.0.260818.1/strings/base_chrono.h)
  to normalize into `std::chrono::sys_time` at 100-ns precision, not custom epoch math.
- CNG is also supported, but WinRT's native incremental object avoids manual
  handle/buffer ownership here. `IStorageItemHandleAccess::Create` is documented
  [desktop-only](https://learn.microsoft.com/en-us/windows/win32/api/windowsstoragecom/nf-windowsstoragecom-istorageitemhandleaccess-create);
  it is not this WinRT adapter's production boundary. OpenSSL adds an
  unnecessary package; whole-file `FileIO.ReadBufferAsync` defeats bounded memory.
  Microsoft's separate `cpp-async` project would add an unnecessary scheduling
  dependency: this layer has a synchronous MTA-worker contract.

Residual application code composes native stream/hash/metadata APIs into one
structured result, checks before/after metadata, and maps native HRESULTs. It
does not implement hashing, stream parsing, or an alternative file-access broker.
`SourceFileRevision` retains the accepted name and means observed file state,
not stable file identity or a permission capability. Move the existing neutral
`Sha256Digest` declaration into its own Domain header for both source and output.

An optional byte-progress observer is a production observability boundary:
worker-thread notification after each hashed block (including the opened, zero-byte
state). It must return promptly and must not throw; UI consumers marshal/throttle
notifications. Tests use it to coordinate a real concurrent replacement attempt
and cancellation without mocking streams or exposing a test-only bypass.

## Test-first and verification scope

Start with a real `abc` file, literal SHA-256 and independently set UTC timestamp;
record the planned missing-declaration compile-time RED truthfully. Then exercise
empty/multi-block files, same-length edits with restored timestamp, cancellation,
sharing conflict, deleted-file I/O and barrier-coordinated concurrent replacement.
Check retained file contents and handle release as well as result variants.

Focused: WindowsStorage test project, `[storage][revision]`, Debug x64 and Win32.
Final: MSVC static analysis of this adapter, affected Domain/JPEG regressions and
the governed full profile, extended with the new storage suite. Consolidate before
one independent review; only material repaired questions receive narrow follow-up.
Packaged broker authorization and final commit races remain Tasks 9/15, not claims
established by these unpackaged native file tests.

## Evidence

HISEW execution `02dd2446-6c4e-47f4-a78d-0587aadf67e0`, generation 1, snapshot
`0d8a6031-88cc-4fa3-8ebf-f234aaa9e39c`. An initial attempt to pass `--baseline`
was rejected because that flag requires repository authority, not personal-mode
authority. The corrected personal invocation captured the same signed starting
HEAD natively; no authority or risk policy was weakened.

The first test-project build reached the new test and failed C1083 because the
calculator declaration did not exist: the planned compile-time contract RED,
not behavioral RED. Next builds exposed missing native error declarations,
a checked-index-width issue, and the actual PPL assignment incompatibility above.
These are integration/draft failures, not claimed behavioral red. The initial
`abc` real-file case passed 40 assertions after integration.

Additional real-file cases passed 205 assertions across 9 cases on Debug x64,
seed 1224803. The first draft incorrectly assumed the broker would report
ERROR_SHARING_VIOLATION for an existing writer; it returned E_ACCESSDENIED.
Both are structured access failures; keep the native HRESULT and do not infer
sharing from an ambiguous access-denied result. A native MoveFileEx replacement
attempt between hashed blocks was rejected with ERROR_ACCESS_DENIED. Preserve
that stronger non-mutation precondition and require the original bytes/revision
to remain unchanged; if a provider permits replacement, capture must fail without
a digest. A metadata-only timestamp change is allowed by the platform and the
calculator detects it, discarding the digest. No mock stream or production test
bypass was added. Remaining evidence is recorded below when available.

Debug Win32 also passed 205 assertions / 9 cases, seed 1224803. MSVC static
analysis (`RunCodeAnalysis=true`, rebuilt WindowsStorage project and Domain
dependency, Debug x64) exited zero with no warnings. The actual CL task's
`CL.command.1.tlog` contains `/analyze`, the NativeRecommendedRules ruleset and
`EspXEngine.dll` for `SourceFileRevisionCalculator.cpp`; this is an observed
analysis run, not merely a requested property.

The preauthorized personal-profile update retains policy/Domain/JPEG/fuzz checks
and adds storage x64, an x86 solution build and storage x86 (8 full commands).
Native proposal `f5a8b478-9ed4-4171-a0bc-40d7ed4fbe48` was inspected and applied;
the native reroute requires a transient paused owner and increments generation.
Current execution is `cde532b5-4ecc-4d2e-b4c6-4d1bbac99fcc`, generation 3, linked
to the original execution. Risk, session, accepted snapshot and starting HEAD
are unchanged. Profile declarations still do not establish measured path coverage.

Final evidence is retained outside Git at
`C:/Users/maksy/.hi/w/e/task-artifacts/jpg-task8/`: frozen candidate patch,
independent review output, targeted negative-control output, full verification
and handoff receipts. Results pending are not relabeled as passes in this log.
Current vcpkg/build outputs remain needed by Tasks 9 onward; owned test children
remove themselves and explicitly assert cleanup success. Disposable assessment
copies are eligible for recycling after their consumers finish.

## Consolidated review corrections

Claude Code 2.1.285 completed the read-only independent assessment of frozen
tree `8e5810c6fd9daa3f9ee146dc76d0107edae19ae2`, session
`c664f6fc-7dd8-4893-9292-aeadf2bbdae9`. Its cited identifiers, locations and
acceptance clauses were checked against that clone. Four actionable findings:
the effective-build-policy gate still selected the deleted placeholder;
the NuGet whitelist omitted the test project's direct C++/WinRT reference;
the caller check admitted implicit MTA; and the permitted-replacement branch
did not constrain its failure code. No independent commands ran. The first
assignment exposed the staged index but not a readable exact patch to its
Read/Grep/Glob tools; the narrow follow-up receives the actual patch inside
the clone and is limited to these repairs and their affected consumers.

Native policy RED reproduced both wiring failures before correction. The MTA
regression independently proved `S_OK`, `APTTYPE_MTA` and
`APTTYPEQUALIFIER_IMPLICIT_MTA` on a fresh uninitialized worker while another
thread held MTA; the pre-fix calculator incorrectly returned a revision.
[Microsoft's qualifier contract](https://learn.microsoft.com/en-us/windows/win32/api/objidlbase/ne-objidlbase-apttypequalifier)
distinguishes implicit inheritance from explicit per-thread initialization.
Reject it before I/O with `CO_E_NOTINITIALIZED`, not by initializing the caller
on its behalf. The repaired real-file suite passed 219 assertions / 10 cases,
Debug x64, seed 1224803. NuGet clean-cache locked resolution then passed Win32,
x64 and ARM64 without changing committed lock bytes.

The replacement test now requires change/access/sharing errors without a digest;
the locally blocked replacement branch remains the observed one. Cancellation
allows a positive partial first read up to 64 KiB rather than assuming a full
buffer. New error diagnostic names received test-first coverage: pre-fix RED
returned `InvalidImageProcessingErrorCode`; repaired names passed 38 assertions.

Codex Security scan `564230ca-8ed9-46d8-8aeb-cc810d515bdd` sealed the original
frozen patch with no security findings. All seven source entries (including the
baseline deletion), seven other diff artifacts and relevant authority consumers
were assessed sequentially, not by an independent security worker. The sealed
report is in the native Codex Security scan store; its snapshot is not silently
rebound to these later corrections. These strengthen the thread guard and test/
policy wiring; focused follow-up evidence covers the changed bytes. No packaged
isolation, future transaction safety or remote-provider qualification is claimed.
Native usage recorded 2,556,354 total tokens, including 2,455,424 cached input
tokens; this is the host's scan-window accounting, not isolated review cost.

The narrow Claude follow-up completed on tree
`362f01743bc24fb2a75dc28bdbd8fd2d14923eaf`, with the exact patch readable inside
the clone. F1/F2/F3 and the diagnostic/partial-read corrections were accepted.
It found one remaining fixture weakness: replacement timestamps could coincide
on a coarse-resolution provider. Give the replacement an independently distinct
write time (+1 hour) and assert that precondition. Equal-length/equal-time
replacement is not detectable from capture-time metadata alone; the independently
recomputed digest immediately before commit is the Task 9 safety boundary.
No stronger identity/transaction guarantee is inferred from `StorageFile`.
The locally blocked replacement branch remains observed; allowed-provider behavior
is still a qualification limit, not a claimed live pass.

Also keep all Catch2 assertions on the test thread after joining the native
worker, following [Catch2's threading limitation](https://catch2-temp.readthedocs.io/en/latest/limitations.html#thread-safe-assertions).
The final correction changes test oracles only, not production capture semantics;
the main agent inspected that bounded delta without another broad review.

The disposable original review clone was repurposed after its reviewer exited
for a targeted negative control: remove only the post-read modification-time
comparison. The real timestamp-mutation case reached the calculator and failed
because a revision was incorrectly published (5 assertions, 1 failure). Main
source was never mutated by this control. Raw build/test output is retained.
The corrected effective-policy positive check passed Debug x64, and the existing
real-project rightmost `/sdl-` negative control passed and restored exact Domain
project bytes. Final MSVC `/analyze` on the corrected production calculator exited
zero without warnings. Final governed full checks and signed checkpoint are
recorded in the external handoff after the tracked candidate is frozen.

The first governed full run `f1376f60-03e0-4c79-b945-b2b585b554d1` passed its
x64 checks, then failed the newly included x86 solution build: the earlier
`MarkerReconciliationTests` passed a uint64 range length to Exiv2's native
`size_t` API, warning C4244 promoted by `/WX`. This is an existing portability
defect discovered by the wider profile, not a failure of revision capture.
Keep `/WX`; prove the range fits its actual output vector before an explicit
`size_t` conversion. The remedy changes only this test's native API call and
adds containment preconditions. The failed run remains retained and is not
relabelled; a fresh governed full run qualifies the final staged bytes.
