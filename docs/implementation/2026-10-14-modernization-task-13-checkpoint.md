# Task 13 source checkpoint — 2026-10-14

This records an intermediate implementation checkpoint, not completed Task 13
assurance, application release or completion of the modernization plan.
Baseline: `a30130b0232ac3e50779b9176218f3ac971ba71a`.

## Delivered behavior

The accessible WinUI shell composes the real selected-folder adapter and batch
coordinator. Authored semantic regions and AutomationIds support native UI
Automation, bounded adaptive layout and minimum-height scrolling. Analysis
displays exact eligibility and discovery issues. Review distinguishes corrected
copies from replacement with verified backups, exact trim loss and explicit
consent. Picker cancellation preserves the existing review. No shipping test
hook or broad filesystem capability was added.

Real AppContainer acceptance initially exposed a production access failure:
Windows Storage could enumerate a broker-picked external folder, but path-based
FromApp metadata opens returned access denied. The shared proof module now
consumes actual Storage items through native brokered handles, typed native
attributes, file identity and canonical containment. Discovery, transactions
and journals use the same owning contract. Reparse targets are not traversed;
unavailable proofs fail closed.

## Approved Windows long-path prerequisite

On 2026-10-14 the owner explicitly approved replacing unconditional/no-machine-
opt-in long-path support with the app's `longPathAware=true` process manifest
plus Windows `LongPathsEnabled=1`. Both native test hosts now declare the app's
existing process policy. No registry setting, filesystem capability or OS
security policy was changed.

Transaction and recovery preflight read the documented typed prerequisite and
refuse an unqualified host before output, backup, stage, journal or lease
creation. This applies to short sources too because generated artifacts may
cross ordinary path limits. `WindowsLongPathSupportUnavailable` supplies specific
guidance in en-US, en-GB and ru; it is not misreported as provider corruption.
Native policy-disabled VM qualification remains a release obligation. Fault-
injected refusal controls do not claim that environment was exercised here.

The decision follows [Microsoft's long-path contract](https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation),
[Storage-item handle interoperability](https://learn.microsoft.com/en-us/windows/win32/api/windowsstoragecom/nf-windowsstoragecom-istorageitemhandleaccess-create)
and [typed RegGetValueW retrieval](https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-reggetvaluew).
Metadata/read/parent-relative/extended-path alternatives were actually tested
and failed; they were not retained as fallbacks. A copied manifest-aligned native
test executable isolated the process-policy mismatch before maintained changes.

## Observed development evidence

All raw failures and successes are retained in the external HISEW evidence group
`dcdecf64a1514c35be685b5027ff2377`, not committed as generated artifacts.

- Actual picker review/cancel: 55 assertions; trim/backup/consent: 66. Start was
  never invoked, and synthetic source bytes remained unchanged.
- Actual external AppContainer grant: 22 worker assertions plus three controller
  assertions. Exact abc capture, escaping-link rejection, and denial of existing
  ungranted parents/siblings after the child grant all pass.
- Five-case automated UI selection: four passed; the provider test exposed a
  numeric AutomationId collision. Its native ID/type condition repair passed
  the focused 19-assertion rerun. No redundant full desktop rerun was performed.
  This follows [UI Automation's composed-condition guidance](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-obtainingelements).
- Final windowless packaged worker: 48 assertions in three cases, including
  actual AppContainer token, real adapter analysis, source preservation and
  retained-grant lifetime. Controller passed three assertions.
- New transaction/recovery refusal controls first failed behaviorally: execution
  committed and recovery created a lease despite the disabled-policy observation.
  The new gate fixes both. The specific localization test also failed first
  because native resources were absent; diagnostic-name coverage failed first.
- x64 and x86 focused long-path/refusal/reparse regression: 84 assertions in five
  cases per architecture. Both original long-path behavior cases pass with the
  approved host contract, not weakened expected output bytes.
- x64 and x86 headless presentation: 2,741 assertions in 37 cases per architecture.
  UIA, packaged-only workers and assistive-technology cases are explicitly excluded.
- Domain diagnostic-name/result selection: 66 assertions in four cases.
- Broader owning x64 storage regression, excluding the separately retained
  discovery benchmark: 3,025 assertions in 77 cases. Package policy, native test
  selection, interactive refusal and visual-state declaration controls pass.
- Both native Debug architecture builds succeed with bounded two-process
  concurrency and node reuse disabled. Existing dependency PDB warnings remain
  visible; x86 solution warnings identify intentionally x64-only fuzz targets.

Screen-saver suppression was bounded and restored the captured active=1,
secure=0, timeout=600 after each applied session. The desktop was released, with
no owned app or presentation worker left running.

## Remaining before Task 13 completion

Canonical R2 full verification, final frozen-candidate ordinary/security assurance
and resource disposition remain separate obligations. Existing WIL/MSVC analysis
warnings have a retained minimized compiler reproducer and negative control;
no warning suppression, vendor patch or clean-analysis claim is made.
Assistive-technology qualification and policy-disabled native VM coverage remain
explicit plan/release work. No deployment or Store submission is authorized by
this source checkpoint.

The installed commit helper answered its version probe, but the current native
HISEW resolver reported `execution-route-unavailable` for it. The authorized
ordinary Git route is used for this intermediate checkpoint; helper-specific
message-format, signature, result and recovery checks do not apply. Git's own
commit identity and signature status are the checkpoint's inspectable evidence.
No helper-witnessed check evidence is attached to this transaction.
