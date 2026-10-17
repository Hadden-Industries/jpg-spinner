# Native picker focus continuity and startup readiness

Status: bounded development qualification, not release acceptance. The real
Microsoft UI Automation cancellation test reproduced loss of keyboard focus;
the repaired application passes that same native picker boundary. No test-side
focus assignment replaces the postcondition.

## Fault Locate

Scope: `MainWindow::ExportDiagnostics_Click` and the cancellation test in
`WindowStateAutomationTests.cpp`. Fault Confidence: High.

### Primary fault: [L6] Re-enabling a control does not restore its focus

Premises: the test focuses the actual export button, invokes its HWND-owned
save picker, cancels through the native provider and expects keyboard focus
back on the initiating button. Cancellation must leave every diagnostic byte
unchanged.

Trace: the handler sets `pickerActive_`, publishes `CanExportDiagnostics=false`
and consequently disables the focused button. After `PickSaveFileAsync` returns,
the handler resumes on the UI dispatcher, clears the busy flag and publishes
the enabled state. It previously never requested focus restoration. Native UIA
observes an enabled button without keyboard focus, both immediately and after a
five-second bounded wait; the unchanged-byte assertions still pass.

Divergence: [L6] the handler relied on re-enabling the control/native picker
completion to supply a focus-restoration guarantee that this interaction does
not provide. The five-second RED distinguishes that omission from merely
asynchronous focus notification.

Trigger: the retained `[ui-automation][diagnostic-export]` case runs against a
fresh UUID-owned application registration, focuses ExportDiagnostics, invokes
the real save picker and cancels. Native exit 42 occurs before the repair.

Remedy: capture the initiating WinUI Control and its FocusState before disabling
it. After resuming on the UI dispatcher and publishing the enabled state, call
`exportInitiator.Focus(initiatingFocusState)` only if the window is not closing,
the control remains loaded/enabled and the captured state is not Unfocused.
Do not force global foreground activation. The regression awaits the actual
UIA keyboard-focus postcondition for at most five seconds, without refocusing
the control from the test.

[Microsoft's Focus contract](https://learn.microsoft.com/en-us/windows/windows-app-sdk/api/winrt/microsoft.ui.xaml.uielement.focus)
distinguishes logical from keyboard focus; the
[FocusState contract](https://learn.microsoft.com/en-us/windows/windows-app-sdk/api/winrt/microsoft.ui.xaml.focusstate)
defines the input modality and prohibits passing Unfocused. Preserve that
modality rather than inventing a synthetic keyboard action.

## Startup boundary

The isolated fixture proves the native window belongs to the verified package
and process, not that XAML loading and asynchronous startup recovery have
finished. The initial heading query reproduced an absent peer. The picker
invocation failed with `0x80040200`, Microsoft's documented
[UIA_E_ELEMENTNOTENABLED](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-error-codes),
not `UIA_E_ELEMENTNOTAVAILABLE`.

Both call sites now reuse the existing bounded `waitForEnabledControl` predicate
before asserting initialized content or invoking the picker. Native IsEnabled
and owner-window state remain the readiness authority. Every original heading,
visibility, disabled-action, default-selection and popup-ownership assertion
remains; there is no fixed startup sleep, swallowed UIA exception, production
test hook or automatic retry until success.

The readiness-only correction remained RED: source selection never became
enabled within five seconds. An empty-store regression through the existing
windowless AppContainer worker passes, including the shell's actual background
transition and explicit MTA lifetime. That distinguishes background recovery
itself from the UI-to-MTA acquisition boundary.

The shell previously acquired FutureAccessList on the XAML UI thread and used
that same instance after switching apartments. It now acquires the object inside
the explicit MTA that enumerates/restores the capabilities. The unchanged native
startup-state contract then passes all 69 assertions in one diagnostic case.
[Microsoft's C++/WinRT agility guidance](https://learn.microsoft.com/en-us/windows/apps/develop/cpp-winrt/agile-objects)
requires respecting apartment boundaries for non-agile objects. Acquiring the
cache where it is consumed avoids needing a UI-origin instance to be agile or
adding an unnecessary marshaling adapter. No token, grant or journal policy is
relaxed. This qualification does not independently identify the failing native
HRESULT or assert a universal agility property of every access-list instance.

### Startup trace: [L7] UI-origin capability cache crosses apartments

Mode: Fault Locate. Scope: `recoverStartupAsync` through
`recoverStartupTransactions`; Fault Confidence: Medium for the native
cross-apartment mechanism, with execution-verified state recovery after the
single-variable acquisition change.

Premises: startup must inspect the owned store before enabling normal work;
cache restoration is a synchronous operation on an explicitly initialized MTA.

Trace: the UI thread acquired the access-list instance; `resume_background`
changed context; the MTA called `Entries`/restoration through that instance;
recovery refusal kept source selection unavailable. The existing windowless MTA
path accepted the fresh store, whereas the shell's startup-state test remained
RED after readiness synchronization alone. Acquiring the list in the consuming
MTA makes that same shell state test GREEN.

Divergence: [L7] ownership/acquisition and capability use straddled the apartment
transition without an established agility/marshaling guarantee. The exact
native failing call/HRESULT remains unobserved; do not overstate that detail.

Trigger: first launch of the UUID-owned shell, before any source grant, followed
by the existing SourceSelectionRegion/SourceFolderPicker assertions.

Remedy: place `StorageApplicationPermissions::FutureAccessList()` immediately
after `wil::RoInitialize(RO_INIT_MULTITHREADED)` inside the background scope;
retain all recovery refusals and the subsequent dispatcher-affine publication.

## Evidence and limits

External inventory `9dc42d2aa078430f8a75a8ff29400e11`, execution
`352c2077-8e60-441f-ac25-4b13a115a082`, retains original RED JUnit, focused RED
with the bounded wait, native compiler streams and subsequent GREEN results.
The baseline is signed source `d7c8c564e42698288a92e4206b0e60379111df64` plus the
two explicitly recorded application/test changes.

The focused cancellation GREEN has native exit 0, one case and 22 assertions,
including unchanged diagnostic bytes. Several intervening attempts stopped at
the independent foreground-qualification gate with exit 78 and ran no product
tests; they are retained as environment refusals, not relabelled GREEN. The
successful run qualified the native foreground naturally; no Computer Use
input reached a product or probe window. Screen-saver, Windows trust, Store
baseline and system settings were unchanged.

The subsequent startup-state observation invokes only the existing read-only
native console case. Its fixture retains actual desktop/lane and package/
process checks, but this diagnostic does not run the acceptance driver's
foreground probe and is not final keyboard/UI acceptance. Later Computer Use
attempts to activate only the returned qualification probe failed, including
`foreground window did not report a process id`; those outcomes do not supply
foreground evidence. No product-window input was sent by Computer Use.

After removing all temporary diagnostics and rebuilding, the existing native
console entry point passes all six automated UI cases (178 assertions, zero
failures/errors), including the previously failing startup, source-picker and
diagnostic-focus contracts. The five-project x64 Release headless suite also
passes. These are development regression results: the console run does not
execute the acceptance driver's separate foreground probe, includes no
operator-assisted cases and is not promoted to final UI acceptance.

The cleaned development application SHA-256 is
`c5854735a95a77ab1b2c58cfd56a7aa8bb5f233b5416ef2d671a559efefebc38`;
the native test host SHA-256 is
`5af058fae148eb5e839460b1ce059c98494e5ec4b0e38676300dfb49d4f2b75b`.
Raw `ui-checkpoint-development.junit.xml`, result metadata, both streams and
the exact bounded invocation remain in the inventory. Native test packages are
UUID-isolated and removed through the existing owning fixture, not installed
over the user's Store baseline.

The later native analyzer caught C26811 in the new test coroutine's reference
to caller-owned result storage. The helper now owns that storage through a
shared pointer carried by value across suspension; the caller still awaits
completion before observing it. No suppression or broader lifetime assumption
was added. The subsequent native `ModernSolution` analyzer `Build` passes with
warnings treated as errors; this is incremental delta analysis, not another
fresh whole-tree analyzer rebuild. The earlier whole-tree baseline remains
separate evidence.

Both affected native runs pass again after that ownership correction: the
windowless packaged worker and all six automated UI development cases. Their
application SHA-256 is
`8ebd5a6ad49628bfdc45bf7411246239a73895e7a89dfe4144b9f0ebf2b6acef`;
test-host SHA-256 is
`9b75f9413c798406d9cbe4e74aa1b91e93621edf7da46cb158d258e083d4e42a`.
The `ui-checkpoint-owned-result` and `startup-windowless-owned-result` raw
reports preserve that later producer identity. Post-run native enumeration
finds no remaining `JpgSpinner.UI.*` test registration. No independent final
Task 20 review, hosted run or final release claim follows from this checkpoint.

This does not establish the full accessibility/localization matrix, isolated
machine acceptance, a trusted Store upgrade, final package identity, hosted
execution, certification or release approval. Later application changes require
their own affected native qualification and final artifact evidence.
