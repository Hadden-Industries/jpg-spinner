# Task 10 — verified backups and observable-state recovery

## Accepted scope

Continue approved modernization Task 10 from signed `8d583d00253cf325498d17e75844d9173f225356`.
Reuse the accepted R2 route; original replacement now activates difficult-reversal
and persistent-data concerns. Implementation remains single-writer, test-first,
with no implementation subagents, shims or publication. The purpose is to preserve
the user's original bytes while enabling validated replacement and restart recovery.
Replacement must remain disabled until verified backup publication and journal
reread establish the accepted commit gate. Earlier corrected-copy behavior remains
covered by its real integration tests.

## Native capability selection — checked 2026-10-03

Reuse Windows SDK 10.0.28000.0 and pinned C++/WinRT 3.0.260818.1; the
[official NuGet inventory](https://www.nuget.org/packages/Microsoft.Windows.CppWinRT/)
still identifies that stable release. Existing Microsoft SDK and MIT projection
adoption applies; there is no new third-party redistribution or legal clearance claim.

- [CopyAsync](https://learn.microsoft.com/en-us/uwp/api/windows.storage.storagefile.copyasync)
  supplies native copy semantics; explicitly request `FailIfExists`, never replace
  a backup. The returned StorageFile is not a locked handle or a proof of copied bytes.
- [FlushAsync](https://learn.microsoft.com/en-us/uwp/api/windows.storage.streams.ioutputstream.flushasync)
  improves persistence but does not guarantee coherent/durable media. Request it,
  close, reopen and reuse the existing incremental SHA-256 revision calculator.
  Compare backup length/digest, not last-write time: copy metadata is not the oracle.
  Rehash the original immediately afterward using the full captured source revision.
- [MoveAndReplaceAsync](https://learn.microsoft.com/en-us/uwp/api/windows.storage.storagefile.moveandreplaceasync)
  observes replacement completion, not power-failure atomicity. Do not infer a
  successful or unsuccessful commit from an HRESULT alone after uncertain completion.
- [Microsoft file-writing guidance](https://learn.microsoft.com/en-us/windows/apps/develop/files/best-practices-writing-files)
  explains the single-file transactional convenience layer and the loss of granular
  failure control. It does not supply a backup plus immutable multi-file recovery
  protocol. Reuse granular native streams for the accepted failure boundaries.
- [Transactional NTFS guidance](https://learn.microsoft.com/en-us/windows/win32/fileio/transactional-ntfs-portal)
  recommends alternatives rather than new TxF adoption. A database transaction
  would not include brokered external image replacement; neither it nor TxF removes
  the residual application-owned observable-state reconciliation requirement.
- [JsonObject](https://learn.microsoft.com/en-us/uwp/api/windows.data.json.jsonobject)
  explicitly retains the last duplicate name. It cannot prove unique raw members.
  [JsonArray](https://learn.microsoft.com/en-us/uwp/api/windows.data.json.jsonarray)
  provides native ordered typed values without dictionary conversion. Evaluate a
  versioned, exact-cardinality scalar tuple for the internal journal schema: no JSON
  objects accepted anywhere, hence no lossy duplicate-member acceptance. Version,
  field order/types, bounds and monotonic transitions are application semantics,
  not a second JSON grammar. Encode large integers as canonical decimal strings
  using standard-library conversion, not lossy JSON doubles or a custom parser.

The residual custom work is transaction ordering, closed application journal schema,
artifact identity/hash reconciliation and owned-stage cleanup. Retain native path,
UUID, JSON, I/O and cryptographic consumers. No generic serialization, filesystem,
database or compatibility layer is needed. Provider and AppContainer qualification
remain separate from ordinary desktop integration evidence.

## Verification and retention

Start with real `[storage][transaction][replace]` backup/unchanged-original tests;
record behavioral RED before implementation. Add recovery and immutable-generation
fault controls as their consumer interface becomes executable. Run focused native
build/tests during development, then the canonical configured full profile once
on the consolidated staged candidate. Follow Task 9's retained tree-handoff-before-
signed-commit capture order; no repeated full run solely for commit-helper receipts.
Retained evidence and scratch ownership: `C:/Users/maksy/.hi/w/e/task-artifacts/jpg-task10`.
Dispose of proven spent scratch recoverably after consumers finish; preserve failed
results and all recovery artifacts. Old HISEW retirement remains held by actual
other-app selections and the loaded old Codex plugin, not by this product milestone.

## Implemented contract and retained qualification

The deep transaction module now owns both dispositions, immutable generation
publication and restart reconciliation behind the existing execute operation and
the new recovery operation. The internal batch was renamed
`ImageFileTransactionBatch` because it now owns backup/journal facts as well as
corrected-copy destinations; no legacy alias or compatibility shim remains.

Replacement requests native exclusive CopyAsync backup creation, flush/close,
independent length/SHA verification and two source rechecks. Only a published,
independently reread `VerifiedBackupCreated` record admits native replacement.
Completion rechecks output bytes and the transferred stage file ID. Uncertain
native completion or postcommit allocation/journal failure is RecoveryConflict,
never an assertion that the original remained untouched. Retained backups are
never rollback deletion targets. Interrupted precommit/recovery hashes retain
their precise Cancelled classification and artifacts.

The implemented version-1 JSON format is an exact 20-scalar Windows.Data.Json
array tuple, with native parsing, closed types/cardinality/state graph, canonical
standard-library decimal integer strings and native hexadecimal encoding. No
JSON objects are accepted, avoiding JsonObject duplicate-member collapse.
Each pending generation is bounded, flushed, closed, independently parsed and
hash-bound to its predecessor before exclusive publication and reread. Recovery
uses the highest complete valid prefix, but refuses unexplained, missing,
substituted or ambiguous artifacts. Only the full-UUID named, identity-bound
stage under the exact selected output subtree may be deleted. Originals and
backups are never deleted by recovery.

Physical NTFS is the currently qualified filesystem, not a generic provider
promise. [Microsoft's file-ID contract](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/ns-fileapi-by_handle_file_information)
documents FAT ID changes on rename and NTFS replacement-ID transfer. Native
[GetVolumeInformationByHandleW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getvolumeinformationbyhandlew)
selects the filesystem before NTFS-specific metadata queries. Unsupported
filesystems are refused before image/output/backup/journal effects. Desktop SDK
availability does not establish AppContainer, picker, cloud or physical-device
loss acceptance. No ReFS or cloud-backed qualification environment was available;
these capabilities remain unqualified, not silently reported as passing.

Per-instance serialization alone was insufficient: a real negative control
showed another engine recovering a live closed stage. An application-journal-store
lease now reuses [CreateFileFromAppW native sharing](https://learn.microsoft.com/en-us/windows/win32/api/fileapifromapp/nf-fileapifromapp-createfilefromappw),
with read/write access and share mode zero. Metadata-only access is not an
exclusive lease under that specification. The empty reserved file is retained;
ownership is the live handle, released automatically on process death.

Retained behavioral RED/GREEN pairs cover backup preservation, contradictory
pending predecessors, restart recovery, replacement admission, backup cancellation,
postcommit allocation classification, native identity substitution, live-store
ownership and recovery cancellation. Publication faults span both complete state
graphs. Initial parent-process qualification passed 88 native boundaries with
fresh-process, repeated recovery; it was already satisfied on first execution,
not a claimed behavioral RED. The harness delegates command escaping to
[ProcessStartInfo.ArgumentList](https://learn.microsoft.com/en-us/dotnet/api/system.diagnostics.processstartinfo.argumentlist)
and kills only its retained child process handle, proving exit before restart as
required by [Process.Kill](https://learn.microsoft.com/en-us/dotnet/api/system.diagnostics.process.kill).
Successful generated-data runs remove only their exclusively owned UUID fixture
child after report retention; failures retain exact fixtures. This implements the
accepted ongoing scratch-cleanup requirement without deleting user data.

The owner executed the FAT32 qualification because the command guard rejected
the environment-assignment invocation. Native retained JUnit established the
initial incorrect generic error, a still-failing gate-order attempt, and the
corrected precise preflight refusal with unchanged source bytes (17 assertions,
zero failures). This is supplemental operator-invoked producer evidence, not a
HISEW-governed receipt. NTFS native replacement and recovery are exercised by the
ordinary Storage suite. Final frozen-tree repetition, process-death rerun,
independent review/security and governed full checks remain required before commit.

## Approved recovery lifecycle correction — 2026-10-04

Independent static review exposed a genuine mismatch between incomplete recovery
and indefinite integrity checking of completed user files. A closed transaction
must not reopen because a user later edits/deletes its image or starts another
batch. The owner explicitly approved amending the six-state graph with terminal
`TransactionAbandonedBeforeCommit`. This branch is not rollback: only positive
non-commit proof plus identity-bound stage cleanup admits its immutable record.
Ambiguous native completion, untrustworthy first-generation publication and
unexplained artifacts remain conflicts, with originals/backups retained.

The design follows first principles: distinguish a transaction's closed outcome
from the current contents of a user-editable file. Established journal practice
also distinguishes active recovery evidence from completed transactions; see
[SQLite's hot-journal discussion](https://www.sqlite.org/atomiccommit.html#hot_rollback_journals).
This is a lifecycle analogy, not adoption of SQLite or its durability guarantees.
[Microsoft's native file-writing guidance](https://learn.microsoft.com/en-us/windows/apps/develop/files/best-practices-writing-files)
does not supply this cross-file backup/recovery graph. The residual application
ordering remains explicit; native JSON, GUID, paths, streams and hashes are reused.

Recovery skips valid terminal records, scopes incomplete work to the granted root
without opening another root, and continues independently explained transactions
before returning an aggregate conflict. An empty/unpublished journal remains
unresolved: no missing evidence authorizes guessing ownership or deleting it.
The summary describes newly reconciled incomplete transactions, so repeated
recovery converges to the same physical/terminal state and reports no new work.

Native lease sharing violation is retryable `JournalStoreBusy` at
`JournalStoreLeaseAcquisition`, not artifact `RecoveryConflict`. The lease outlives
the execute try block, including exception cleanup and abandonment publication.
[CreateFileFromAppW](https://learn.microsoft.com/en-us/windows/win32/api/fileapifromapp/nf-fileapifromapp-createfilefromappw)
retains the platform sharing contract; no lock-file content parser or stale-owner
heuristic was introduced. [Microsoft's GUID implementation](https://github.com/microsoft/cppwinrt/blob/master/strings/base_types.h)
throws `std::invalid_argument` for malformed text. Catch that documented consumer
exception at the journal/result boundary, rather than inventing another GUID parser.

Maintained regressions exercised real native artifacts and retained behavioural
RED/GREEN for malformed GUID prefix/folder handling, later edits to a completed
transaction, store contention, refused-transaction closure, selected-root scoping
and conflict isolation. Closed-disposition negative controls were already satisfied
on first run, not claimed as RED. The publication matrix deliberately injects
one selected boundary failure; abandonment's subsequent native reads/closes must
not accidentally turn that fixture into repeated independent failures. Repeated
physical artifact assertions remain, including source/backup preservation.

The initial independent review covered tree
`81798c7d0018110e55ddcaa1fb48e724376d7ebd`. The native security assessment was
finalized for that original snapshot only, not the later repair delta. Its first
draft was rejected for incorrect coverage field names/types and completion was
prematurely attempted; continuation corrected the named fields and finalized the
existing scan without bypassing native artifact writers. The original results and
test receipts remain bound to their original bytes. Final assurance must cover the
repair delta and the consolidated current candidate; no stale pass is substituted.

The single focused follow-up confirmed the graph, lease lifetime, GUID boundary
and closed-disposition repairs, and identified two residual no-commit proof gaps.
Native replacement failure is not a specification of the original's final state;
the [MoveAndReplaceAsync contract](https://learn.microsoft.com/en-us/uwp/api/windows.storage.storagefile.moveandreplaceasync)
does not provide that error-outcome guarantee. Before failed-replacement cleanup,
reopen the recorded original path and require its full captured revision and file
identity, retaining the metadata handle through terminal publication. An unknown,
renamed, changed or same-bytes/different-identity original leaves the stage and
verified-backup generation intact as conflict. This uses native observations,
not assumptions about WinRT calling a particular Win32 replacement implementation.

A copy does not change its source: after commit admission, an unchanged source
with both stage and output absent cannot distinguish refusal from a completed
move followed by a user rename/deletion. That state remains conflict, not terminal
abandonment. Real-file regressions first failed with 11 assertions and then drive
these two narrowly scoped corrections. The earlier eight-check governed pass,
88-case process-death pass and native security follow-up apply to tree
`127c392ad1eedeba3024601b5ec212f604f3f734` only. Its repetition producer was
explicitly stopped at the owned child handle when the candidate needed repair;
the partial receipt is retained, not counted as a successful qualification.
Final supplemental repetitions may run as independent bounded shards whose
receipt counts sum to 100; each shard binds the exact executable and owns its
UUID fixtures. No further ordinary broad or follow-up review is scheduled:
complete the finding-driven proof tests and final native assurance on frozen bytes.
