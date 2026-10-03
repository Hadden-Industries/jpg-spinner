# Task 9 — non-destructive corrected-copy transactions

## Accepted slice and authority

Continue accepted modernization Task 9 at signed `b88621d9`, risk R2. The user's
"Proceed with your recommendation" approves `WindowsStorageImageFileTransactionEngine`
throughout the plan/design; AppContainer deployment remains required. Implementation
is inline, test-first, with no implementation subagents, shims, or publication.
The beneficiary is a user who receives a corrected, independently validated JPEG
without changing the original. Replacement, backups and recovery remain Task 10.

## Native capability selection (checked 2026-10-03)

Reuse the approved Windows SDK 10.0.28000.0 and pinned C++/WinRT 3.0.260818.1
([current NuGet release](https://www.nuget.org/packages/Microsoft.Windows.CppWinRT/)).
No dependency or redistribution terms change: the existing Microsoft SDK and MIT
projection adoption remain the applicable clearance, not a new legal judgment.

- [Microsoft file-writing guidance](https://learn.microsoft.com/en-us/windows/apps/develop/files/best-practices-writing-files)
  distinguishes a StorageFile representation from a locked handle and explains the
  opaque FileIO transactional convenience methods. Use granular native stream I/O
  for the accepted phase/error boundaries; no parallel filesystem or buffer library.
- [WriteAsync](https://learn.microsoft.com/en-us/uwp/api/windows.storage.streams.ioutputstream.writeasync)
  returns the actual written count; use supported CryptographicBuffer buffers in
  bounded chunks, not a custom IBuffer. Check short writes.
- [FlushAsync](https://learn.microsoft.com/en-us/uwp/api/windows.storage.streams.ioutputstream.flushasync)
  explicitly does not guarantee durable/coherent storage. Request it, check its
  result, close, and reuse the Task 8 incremental native SHA-256 calculator on a
  newly opened stage. Do not claim physical-media durability or power-fail atomicity.
- [MoveAsync](https://learn.microsoft.com/en-us/uwp/api/windows.storage.storagefile.moveasync)
  must use the explicit three-argument `FailIfExists` overload. The two-argument
  overload generates a new name, contrary to the collision contract.
- [CoCreateGuid](https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-cocreateguid)
  provides native UUIDs; retain a full batch UUID internally and use the accepted
  UTC timestamp/eight-hex-digit display name. Fail exclusive batch-root creation
  on collision rather than suffixing filenames or retrying indefinitely.
- [IsEqual](https://learn.microsoft.com/en-us/uwp/api/windows.storage.storagefolder.isequal)
  may compare Path values. It is not, alone, a canonical containment or stable
  file identity proof. Native final-path and file-ID queries must supply those
  checks; no custom path grammar or normalized string-prefix authorization.
- [CreateFileFromAppW](https://learn.microsoft.com/en-us/windows/win32/api/fileapifromapp/nf-fileapifromapp-createfilefromappw)
  follows the UWP security model. Native metadata handles may fail closed where
  the provider or capability cannot supply the required proof. Do not replace
  a denied request with an unrestricted desktop open or widen capabilities.
- [GetFinalPathNameByHandleW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfinalpathnamebyhandlew)
  resolves links; compare complete path components, not string prefixes. Use the
  documented NT-volume representation for metadata comparisons only, never as an
  I/O path or an ad-hoc drive translation. Provider/AppContainer qualification is
  distinct from desktop native test acceptance and remains explicitly required.

The residual custom responsibility is orchestration of the accepted stage/hash/
source-recheck/no-overwrite protocol and its structured errors. FileIO transacted
writes cannot replace that protocol; desktop-only StorageItem handle interop does
not establish the required AppContainer contract. No JPEG or cryptographic parsing
is introduced. Each concrete adapter instance represents one batch selected root;
its private unique batch folder is shared by its sequential execute calls. The
abstract interface exposes terminal execute results, not internal phases.

## Evidence plan

Independent oracle: the actual filesystem bytes equal the immutable validator
capability, source bytes remain equal to the original fixture, and native directory
enumeration observes exact destinations and absence of owned stages. Capabilities
come from the real analyzer/native JPEG engine, never a test approval constructor.
Use real temporary files; inject only failures Windows cannot safely/deterministically
produce, at a narrow internal effect boundary. Record each RED/GREEN truthfully.

Focused: Storage `[storage][transaction][copy]`; regressions: Domain, JPEG and all
Storage on x64/x86. The configured full profile includes policy, both builds, those
tests and the retained scanner fuzz check. Repeat Storage 100 times after consolidation.
No claim of AppContainer/provider acceptance or crash recovery from desktop tests.

## First RED

The x64 Storage project build discovered `ImageFileTransactionTests.cpp` and failed
with C1083 on the intentionally absent approved adapter declaration. This is the
plan's deliberate compile-time contract RED, not runtime behavior evidence. The
real fixture/analyzer/capability preconditions still require a running test. Raw
output: external `jpg-task9/copy-contract-red-build.log`. No transaction production
code existed at this point. The initial sensitive-scope execution start was rejected
because this installed engine requires a complete route record; no execution was
created by that rejected call. Retain the actual rejection, supply the reported
R2 trigger/lens rationale, and do not silently omit the sensitive scope.

## Consolidated implementation and observed tests

The native adapter serializes one selected-root batch. A private batch object
retains native file IDs for the selected root and exclusively created batch folder;
same-path replacements cannot inherit ownership. Each stage retains its native ID,
canonical parent and exact canonical stage path. Cleanup opens the inspected file
with DELETE access and uses FileDispositionInfo on that handle; there is no
recursive deletion or pathname-only fallback. A move that succeeds but loses its
completion notification retains the corrected copy and reports RecoveryConflict,
rather than deleting the now-final file. Task 10 supplies recovery, not this slice.

Canonical containment compares complete native NT-volume path components. Source
ancestors and destination components are inspected for reparse points. Metadata
handles deny delete sharing for the retained root, source and final destination
during execution. This is not immunity to arbitrary external content writers or
every hostile ancestor race. Unsupported native identity/path providers fail
closed; desktop acceptance does not qualify AppContainer or remote providers.

Only validated JPEG capabilities enter the public execute boundary. The native
stage is exclusively created, written in 64 KiB chunks with actual counts checked,
flushed, explicitly closed, reopened and SHA-256 hashed by the existing revision
calculator. Its length/digest must match the capability. The source revision is
captured after stage verification immediately before the no-overwrite move. A
cancelled call removes only its owned stage; cancellation after native commit does
not relabel a completed write. Explicit MTA initialization is required, including
rejection of implicit MTA threads with CO_E_NOTINITIALIZED. Replacement is a valid
domain choice but remains E_NOTIMPL/OriginalReplacementFailed without I/O until
Task 10. Moved-from empty capabilities fail OutputValidation before filesystem I/O.

An internal native-effect seam permits unsafe/nondeterministic fault injection;
it is not an application phase API or a generic filesystem replacement. Reopen and
hash remain together in the actual revision calculator. The deterministic batch
adapter consumes real capabilities, preserves completed I/O despite subsequent
cancellation and counts calls/concurrency. Its shared source is compiled only by
storage-capability test consumers, keeping neutral TestSupport/Domain free of
WinRT dependencies. No NuGet versions change; the final native project-reference
lock update is recorded below. psapi.lib is linked
like the existing JPEG test consumer because the real Exiv2 pipeline requires it.

Retained raw outputs live outside Git at
`C:/Users/maksy/.hi/w/e/task-artifacts/jpg-task9/`:

- Initial fixture/compile/link corrections are setup, not behavior RED: the real
  non-MCU-aligned fixture explicitly selects the accepted trim policy; native test
  paths use standard make_preferred; native Exiv2 linking needs psapi.lib. Vendor
  PDB LNK4099 warnings remain visible, not suppressed.
- `short-write-contract-red.log` is the absent private effect declaration RED.
  Native partial write returns a short count; actual files verify no commit and
  owned-stage cleanup.
- `close-behavior-red-test.log` proves close incorrectly reported flush failure;
  `close-diagnostic-red-test.log` proves the diagnostic table lacked the new name.
  Separate StagingClose/ StagingCloseFailed handling and table coverage fix both.
- `reopen-behavior-red-test-2.log` proves inaccessible stage reopening was wrongly
  called hash mismatch with invented E_FAIL. The real incompatible writer produces
  native access/sharing denial; StagedOutputVerificationFailed preserves that
  projection. `reopen-diagnostic-red-test.log` checks the diagnostic name.
- `batch-identity-red-test.log` proves a replaced same-path batch was accepted;
  retained native file IDs fix it. `move-outcome-red-test.log` proves cleanup deleted
  a successfully moved final copy; exact original stage-path ownership fixes it.
- Destination-creation RED separates its native error/stage from stage creation;
  diagnostic table RED precedes its corresponding StringMaker update.
- Real equal-length stage corruption is detected by digest, not merely length.
  Cancellation after hashing preserves an unknown stage sentinel while removing
  only the owned stage; native IIDFromString validates generated nonzero UUIDs.
- The eight-case fault matrix retains real files/revision checks and injects stage
  creation/write/flush/hash/source-hash/move errors, disk-full and destination denial.
  These passing controls do not pretend to be physical full-disk qualification or
  behavioral RED when the implementation already handled them correctly.
- Real sibling-prefix and in-root symbolic-link tests reject unsafe containment.
  The symbolic-link fixture requires Windows Developer Mode or native link privilege;
  the host already has Developer Mode, and the test fails rather than silently skips
  a missing prerequisite. No host setting was changed. This is a desktop fixture,
  not AppContainer/provider qualification.
- `moved-capability-red-test.log`, `worker-recheck-red-test.log` and
  `replacement-unavailable-red-test.log` precede the empty-capability, implicit-MTA
  native-error and replacement-not-implemented diagnostic fixes respectively.
- A real source edit after stage hashing, with original timestamp restored and
  equal length, blocks commit on digest mismatch and preserves the external edit.
- `storage-green-preformat.log`: all Storage tests passed, **732 assertions in 31
  cases** (10 revision and 21 transaction cases), before consolidation formatting.

Final cross-architecture/profile checks, 100 repeated Storage runs, independent
review, scoped filesystem-security assessment and signed milestone commit are
still pending at this checkpoint. This log does not claim their future outcomes.

## One consolidated review and bounded repair

Frozen staged tree `b588be77111b48f68846a192ff1cf9774b95e735` was inspected by a
read-only cross-vendor Claude review in an exact plain clone. Default installed
review-agent discovery required a host entry point unavailable on the current tool
surface; the previously approved cross-vendor route was used, not a claimed /review
execution. The reviewer read all changed files and supporting revision code, but
executed no tests. Raw assignment/stdout/stderr are retained under external
`jpg-task9/`; implementer checks below establish the actual reproductions.

Three actionable correctness findings were confirmed and repaired inline:

1. Native NT volume-root paths retain a trailing separator. Comparing a child's
   parent_path with that root falsely rejects it. The read-only native volume-root
   regression failed (`volume-root-behavior-red-test.log`) and passed after comparing
   the exact composed `root / "JPG Spinner Output"` path. The small internal policy
   names that single authorized child, not a generic path parser. Its negative
   controls reject other direct children and nested output paths. No drive-root
   output was created to test this behavior.
2. A completed move can leave a stale StorageFile projection. A separately obtained
   real native projection moves the stage, proves the original Path stayed stale,
   and then simulates loss of completion. `stale-move-red-test.log` proves a definite
   commit failure was wrongly reported. Cleanup now rejects a missing stage after
   a failed commit as an uncertain RecoveryConflict; before commit, absence remains
   idempotent cleanup. Both projection variants assert RecoveryConflict/TransactionRecovery
   and actual retained validated final bytes. A before-move fault still reports the
   definite commit failure and cleans its known stage (185 assertions/two cases GREEN).
3. Source/root inspection and containment now run under SourceRevisionRevalidation.
   Missing/path-missing maps to SourceChangedAfterAnalysis, native access/sharing
   failures to their Source* codes, and unprovable containment to
   SourceRevisionCaptureFailed with the native cause. `source-phase-red-test.log`
   precedes the fix; real missing-source, denied content reader, sibling-prefix and
   in-root-link checks passed (60 assertions/three cases). The initial denied-reader
   fixture incorrectly expected metadata inspection to enforce content sharing and
   exact sharing HRESULT; that assumption was corrected using an independently
   observed real calculator result. Native metadata access is not content access
   ([Microsoft filesystem behavior](https://download.microsoft.com/download/4/3/8/43889780-8d45-4b2e-9d3a-c696a890309f/file%20system%20behavior%20overview.pdf)).
   No redundant pre-staging whole-source hash was introduced; the decisive recheck
   still occurs immediately before commit.

The replaced-batch and containment tests now assert their exact error codes/stages,
not merely any error. After formatting, all Storage tests passed **803 assertions
in 33 cases** (`followup-storage-green.log`). The original candidate passed 100
complete Storage iterations with seeds 1–100; the repaired candidate requires its
own repeat evidence and final canonical full profile.

The scoped parent Codex Security assessment `77b0fe98-c6dd-4e19-a247-4cdce1bad17d`
completed with zero security findings against the original frozen tree, accounting
for all ten production inventory paths and changed tests/projects/docs as context.
It is not independent security verification or evidence for later repair bytes.
The three correctness findings do not establish a security privilege gain: copy
mode never writes the source, final moves never overwrite, and cleanup remains
native-handle/ID/path guarded. The security effect of the repair delta must be
inspected separately and retained without reopening or modifying the sealed scan.
One narrow follow-up review is planned for these repairs; no second broad review.

The narrow follow-up completed: all three defects resolved, no introduced delta
finding. Some reviewer line references are patch offsets rather than source lines;
the implementer checked actual consumers (`CorrectedCopyPathPolicy.h:9`, native
cleanup `:132`, source phase `:204`, volume test `:744`, denied-source test `:795`)
instead of treating those references as proof. Read/Grep/Glob were the only reviewer
tool operations; no implementation, shell, tests or workers were delegated.
The repaired candidate also passed 100 complete Storage runs with seeds 1–100,
each **803 assertions/33 cases**. Test-owned storage directories were empty afterward.

Canonical full attempt `5c8088cf-03b0-45b8-b9b9-287d4c589173` failed its first
policy command: the closed graph still prohibited the Storage test project's real
JPEG validator dependency. No later commands in that attempt count as acceptance.
The policy now admits exactly that test-only edge; production Storage remains
Domain-only. Existing negative-control infrastructure adds two actual graph
mutations: reject the same edge in production and require the edge exactly once
in the test consumer. The targeted selector preserves the full default suite and
its cleanup. `test-edge-policy-red.log` reproduces the valid-graph rejection before
the policy fix; `test-edge-policy-green.log` passes the valid baseline and both
negative controls. This is an integration correction, not broadening the runtime
dependency graph or package authority. A fresh canonical full run is required on
the resulting staged tree; earlier receipts retain their original identities.

The sealed security result remains unchanged. A separate target-bound, source-based
repair disposition is retained by Codex Security under
`artifacts/02_discovery/task9_repair_security_delta.md`; it finds no introduced
security defect and explicitly limits its claim to implementer delta inspection,
not another sealed scan or independent runtime qualification. The policy correction
also changes no runtime storage path, authority or dependency selection.

Canonical full attempt `f074425b-4854-456f-8ad1-59e65f3e6591` passed policy but
stopped at locked restore (NU1004): the Storage test lockfile lacked its new JPEG
project-reference edge. Earlier incremental builds did not validate that restore
closure. Native MSBuild Restore with one-shot RestoreLockedMode=false and
RestoreForceEvaluate=true regenerated the affected lockfile; its only semantic
change records the project edge and Domain dependency. Package versions and hashes
remain unchanged. Normal builds retain locked mode. This follows
[Microsoft NU1004 guidance](https://learn.microsoft.com/en-us/nuget/reference/errors-and-warnings/nu1004),
not a hand-authored lockfile or relaxed ongoing restore policy. The raw failed
receipt and successful native generation log are retained externally. A targeted
locked restore passed (`test-edge-locked-restore.log`); a fresh final full profile
remains required at this checkpoint.

## Configured-runner path-length correction

After activating the already-qualified issue 116 engine, full run
`a5299c96-33a0-4076-a5c3-b53a176fa8de` captured policy, locked x64 build, Domain
and JPEG tests successfully with quiescent producers. Storage failed from its
configured executable-directory working directory. The first public copy test
reproduced RED there and passed from the repository root; native context showed
RecoveryConflict/TransactionRecovery with ERROR_PATH_NOT_FOUND. Windows Storage
created a stage whose generated path exceeded MAX_PATH, while the ownership open
used an ordinary DOS path. The source path itself was only 180 characters.

The ownership open now reuses native PathCchCanonicalizeEx with
PATHCCH_ENSURE_IS_EXTENDED_LENGTH_PATH and feeds its result to CreateFileFromAppW.
It does not implement a prefix/UNC parser, enable machine policy, substitute a
desktop open, or use lexical canonicalization as identity/containment proof.
The same native handles, FileIdInfo, reparse rejection, NT final-path comparisons,
no-overwrite move and handle-only cleanup remain decisive. ENSURE preserves
trailing dots/spaces and must not be combined with ALLOW_LONG_PATHS according to
the selected SDK header. The API is in the App/System family and documented for
UWP as well as desktop consumers:
[PathCchCanonicalizeEx](https://learn.microsoft.com/en-us/windows/win32/api/pathcch/nf-pathcch-pathcchcanonicalizeex),
[CreateFileFromAppW](https://learn.microsoft.com/en-us/windows/win32/api/fileapifromapp/nf-fileapifromapp-createfilefromappw).

A first draft long-source fixture failed in its standard-library directory setup,
not production; that is not behavioural RED. The corrected regression independently
observes a real Unicode stage beyond MAX_PATH while keeping source/final fixture
I/O below that limit. It reached the transaction and failed before the fix (eight
passing precondition assertions), then passed 24 assertions afterward. Its native
test-root cleanup prevents intentionally failed long stages from leaking scratch.

The initial all-Storage rerun exposed three fault-fixture assumptions: ordinary
Win32 writer access, standard-library sentinel reads, and a reprojected native move
failed before their intended fault when rooted beneath the deep runner directory.
Fault fixtures now use the OS temporary directory plus the existing UUID-owned
TemporaryDirectory boundary; no fault expectation was weakened. The public copy
integration still uses the configured working directory and the dedicated regression
still requires its observed stage length to exceed MAX_PATH. The configured x64
runner then passed 827 assertions/34 cases. No AppContainer/provider qualification
is inferred. Earlier full receipts/repeat results retain their prior candidates;
this semantic correction requires final repeat/profile evidence and one narrow
path-handling follow-up, not another broad review.

## Final narrow path review and generated-fixture cleanup

Read-only path-delta review `8eabd421-d18c-4151-8c25-e350bdd657f0`
found no production-path defect and one test cleanup policy ambiguity. Microsoft's
[StorageDeleteOption contract](https://learn.microsoft.com/en-us/uwp/api/windows.storage.storagedeleteoption?view=winrt-28000)
does not promise that Default always recycles: outside application storage it
uses Explorer's location-dependent policy, which may recycle or permanently delete.
The review's prediction of 100 recycled fixtures is therefore not an observed fact.
A parent read-only Shell inventory found zero matching OS-temp fixture entries.

Generated UUID-owned fixture cleanup now explicitly passes PermanentDelete; it
does not change product cleanup or remove user data. A test first required that
the actual deletion option be PermanentDelete while leaving it at Default: RED,
one failed policy assertion with the other 24 long-stage assertions passing.
The corrected option passes that same test. This is a deletion-option contract
test, not a claim that a particular Explorer configuration recycled the fixture.

Full run `5caeded6-0703-419a-add2-b442f165b369` passed all eight commands on
tree `5e7f1d8f386a847bf0fa235379959fdeb158a857`; every producer was quiescent
without forced descendant termination. It used fixed engine release dc31e281...
and captured stable before/after identities. Its success is retained, but the
subsequent test-only cleanup correction requires a fresh final profile. No further
broad review, replacement implementation or provider acceptance is implied.
