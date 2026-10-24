# Qualification timing policy

On 2026-10-24 the owner requested generous deadlines for infrequent
qualification/releases, followed by a single definition per category under the
DRY principle. This changes engineering waiting budgets, not test expectations,
required evidence, resource limits, failure admission or release authority.
No automatic retry is introduced.

The immediate reproducer is x86 Release in
[manual attempt 40115479000/1](https://github.com/Hadden-Industries/jpg-spinner/actions/runs/40115479000)
at signed source `5e76944e2989fc47cf7f21841e04577c67e56777`. SDK installation
succeeded; its native environment probe then exceeded the former 30-second
process ceiling. Empty text streams and Microsoft's incomplete native binary-log
replay establish no successful completion or definitive root cause. That failed
attempt remains unchanged. The other executed jobs subsequently passed.

## One authority

[eng/qualification-policy.json](../../eng/qualification-policy.json) is the
authoritative definition of category durations, UI polling intervals, artifact
retention/compression and workflow-to-category bindings. Different operations
may intentionally have equal durations; equality alone does not make them the
same concept. Repeated consumer literals are not another policy authority.

[QualificationPolicy.psm1](../../scripts/QualificationPolicy.psm1) validates raw
JSON uniqueness and schema/types before exposing an immutable native JSON
snapshot. Its category getter owns seconds/milliseconds/whole-minute conversion
using `TimeSpan`. A consumer cannot silently round a fractional minute or use a
caller-supplied duplicate-member JSON element to bypass validation. One process
uses a coherent policy snapshot even if an author edits the definition.

PowerShell consumers resolve named categories directly. Native project/task
evaluation inherits `nativeToolProcess`; it no longer maintains short local
overrides. Nested prerequisite hosts, native compiler probes, package builds,
SBOM production, validation, transport, recovery and interactive supervision
select their actual categories instead of copying numeric values. The existing
shared native process boundary still owns execution, termination and streams.

## Derived consumers

The repository requires literal GitHub workflow ceilings, and C++ test constants
must be available without runtime configuration/deployment dependencies.
[Sync-QualificationPolicy.ps1](../../scripts/Sync-QualificationPolicy.ps1)
materializes those consumers from the same definition:

- Existing workflow job/step timeout and artifact-policy scalars. YamlDotNet
  supplies actual node boundaries; only those scalar spans change, preserving
  comments, role boundaries and unrelated bytes. No new YAML parser, workflow
  wrapper, runtime repository variable or reusable-workflow entry point is added.
- The generated native
  [QualificationTiming.h](../../tests/JpgSpinner.Presentation.Tests/QualificationTiming.h),
  containing typed `chrono` durations for UI deadlines and polling intervals.
  Production application behavior is unaffected; no extra runtime JSON asset is
  needed by the test drop.

Default `Check` mode is read-only and refuses stale outputs. After editing the
policy, run `Sync-QualificationPolicy.ps1 -Mode Write -ToolsDirectory <qualified-tools>`
and the focused policy controls. Existing native YAML/actionlint validation
remains required. CI checks synchronization rather than accepting hand-edited
duplicates or generating changes during qualification.

The same rule removes other duplicated settings in this area: upload retention
and compression are read from `artifactPolicy`, and UI polling periods are
emitted from `cppPollingMilliseconds`. Independent expected values in regression
fixtures remain independently authored; deriving them from the implementation
would remove their ability to catch a wrong policy.

## Operational boundaries

Cold tool initialization and operator-mediated input are not product latency
measurements. Configured headroom remains finite, requires actual completion and
does not excuse failure, a missing grant, unresolved termination or undrained
streams. The existing picker-cancellation behavioral assertion, deliberate
short timeout-refusal cases, fixed seeds, corpus assertions, input limits and
production resource limits remain unchanged.

UI Automation's former three-second transaction override was below Microsoft's
[documented default](https://learn.microsoft.com/en-us/windows/win32/api/uiautomationclient/nf-uiautomationclient-iuiautomation2-put_transactiontimeout).
Its native provider limits and the surrounding polling deadlines remain
distinct. UI qualification now uses explicit category constants for both.

Workflow roles map to their categories in the policy. Serial package
qualification can use the [GitHub-hosted execution ceiling](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax#jobsjob_idtimeout-minutes).
The job remains an aggregate cap, not a promise that every nested operation can
consume its individual maximum. Manual-only triggers, superseded-run
cancellation and default-off dependency caching remain in force. Editing the
existing CodeQL job's budget does not authorize or start a scan.

## Verification and limits

The shared process contract first rejects a literal default instead of a
canonical category query. Native process controls still prove success, nonzero
exit and timeout refusal preserve streams and leave the owned child quiescent.
Policy controls exercise independent platform floors, nested-process envelopes,
strict JSON types/uniqueness, unit conversion and stale YAML/C++ materialization
refusals. A passing source check is not a completed remote or desktop run.

Raw observations and native results belong to external inventory
`58569b4ab5254cca95dd8d02c184460a`. These changes do not retroactively qualify
the published candidate, authorize another remote attempt or discharge
release/device/legal gates.
