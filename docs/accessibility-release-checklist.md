# Accessibility and localization release qualification

This is an acceptance protocol, **not a record of passing results**. Source,
compiled resources, headless model tests, UI Automation, and human assistive-
technology observations prove different contracts. A missing lane or tool is
unavailable evidence, never a passing result. Do not publish a release until
the applicable plan gates have actual results tied to its frozen candidate.

## Authority and test boundaries

Follow Microsoft's [keyboard accessibility](https://learn.microsoft.com/en-us/windows/apps/design/accessibility/keyboard-accessibility),
[access-key guidance](https://learn.microsoft.com/en-us/windows/apps/develop/input/access-keys),
[accessible text requirements](https://learn.microsoft.com/en-us/windows/apps/design/accessibility/accessible-text-requirements),
and [global-ready formatting](https://learn.microsoft.com/en-us/windows/apps/design/globalizing/use-global-ready-formats).
[WCAG 2.2](https://www.w3.org/TR/WCAG22/) Level AA supplies useful cross-platform
heuristics; Windows UI Automation and native Microsoft guidance own this app's
platform contracts. Web-only automated checks do not qualify a native window.

Headless `[presentation][localization]` checks exercise exact integer formatting,
equal resource identities, nonempty values, native PRI language provenance,
entry-specific translator comments, and localized errors/outcomes. The current
UI deliberately separates numeric values from labels: there are no interpolated
format placeholders or grammatical plural templates. Before adding one, use a
native formatting capability and test every locale's placeholder/plural contract;
do not treat this current absence as permanent exemption.

Headless `[presentation][accessibility]` and `[metadata][shell]` check authored
declarations and native generated metadata. Compiled XAML verifies property and
binding types. These cannot establish actual focus traversal, peer events,
contrast, high-contrast rendering, or speech. Native `[ui-automation]` checks
exercise actual peers in the approved interactive lane. Record independent
human checks below; do not infer them from the presence of automation properties.

## Preparation and evidence record

- Record full commit/tree, signed package hash, architecture, app version,
  Windows build, framework version, locale and region, display scale, text size,
  viewport, Narrator/Accessibility Insights versions and test operator.
- Use a dedicated interactive Windows VM/session where possible. Deploy the
  built package and tests; no compiler or source build is required there.
  Hosted service sessions are headless lanes, not equivalent desktop lanes.
- Use only uniquely owned synthetic JPEGs: eligible rotation, already-normalized,
  unsupported/malformed, trim-required, failed access, and recovery conflict.
  Keep before/after hashes and expected outcomes outside the deliverable.
- Capture/restore any temporary screen-saver or accessibility-setting changes.
  Never disable secure-desktop/lock safeguards or change the user's preferences
  permanently. Restore settings even after failure/cancellation.
- Record each check as pass, fail, unavailable or not applicable with reason,
  exact scenario, expected/actual observation, evidence reference and owner.
  A fixed failure requires a narrowly scoped retest against the repaired tree.

## Keyboard-only workflow

- Start without a pointer. Tab and Shift+Tab follow the visible workflow: source
  access, row-major choices, review/acknowledgment, then available actions.
  Disabled/hidden commands must not trap focus or remain actionable.
- Show native keytips with Alt. All top-level keys must be discoverable,
  locale-specific and collision-free; verify invoking each enabled command.
  Native combo boxes retain arrow-key selection and button Enter/Space behavior.
- Open each real Windows folder/save picker; cancel and accept owned fixtures.
  Focus returns to its initiating enabled control or the appropriate enabled
  next step. Cancellation must neither alter the source nor lose keyboard access.
- Traverse all review rows, long filenames, trim consent, discovery observations,
  result counts, errors and remedies. Confirm scrolling brings focused commands
  into view and does not require horizontal scrolling at the qualified viewport.
- During analysis/processing, cancellation is reachable. A close request waits
  for actual native work to settle; it must not announce an uncommitted success.
  A recovery conflict retains clear artifact-preservation instructions.

## Narrator and actual automation peers

- In every experience state, verify name, role/control type, enabled state,
  focusability, help text and applicable native value/range/selection patterns.
  Display-only counts/paths are text, not editable controls or extra tab stops.
- Each policy choice announces its header, selected choice and help; review
  rows announce filename, textual outcome, error explanation/remedy and exact
  discarded source-edge pixels. Status is never communicated by color alone.
- Analysis/progress/results use restrained polite announcements. Recovery errors
  may be assertive. The entire mutable row table must not be a live region that
  repeatedly speaks every row or steals focus during progress updates.
- Listen to state changes and final counts; check that announcements occur once
  at meaningful boundaries and that focus remains usable. A declaration of
  `LiveSetting` is not proof that the expected event or speech occurred.
- Test source/save picker cancellation, native provider failure, and warnings.
  No HRESULT, codec symbol, native exception prose or unrequested full path may
  leak into localized error speech. An authorized path display is not log prose.

## Accessibility Insights for Windows

Run [FastPass](https://accessibilityinsights.io/docs/windows/getstarted/fastpass/)
against each representative state, with both automated checks and its keyboard
tab-stop inspection. Save the report tied to the package identity. Investigate
every confirmed failure; document false positives with native peer evidence,
not blanket suppression. Re-run the affected state after repair. FastPass does
not replace the broader speech/reflow/localization checks in this protocol.

## Reflow, contrast, locales and direction

Qualify en-US, en-GB and ru individually. Set the app's supported test-lane
language/region preferences; capture and restore them. Verify native PRI
candidate provenance, translated keytips, labels, errors and integer grouping.
Check zero, one, large counts, longest messages and long mixed-script filenames.
There is no visible date/byte-size value currently; when introduced, qualify
native localized units and dates instead of adding invariant concatenation.

Repeat representative states in light, dark and Windows contrast themes, at
200% text size, 400% effective zoom, and a 1024×768-equivalent viewport. Record
actual supported OS/display values rather than calling a window resize text
scaling. Text, content templates, focus indicators, errors and actions must
remain readable/reachable; no clipping, overlap, lost controls or color-only
meaning. Use the contrast tool for actual colors, not visual guesswork.

Run a separate right-to-left pseudo-localization candidate in the disposable
lane, with expanded text and mixed-direction path/number samples. Verify flow,
reading order, focus order, punctuation and source-space edge meanings; do not
relabel an en-US build as pseudo-localized or add an unsupported shipping locale
merely to test it. If that candidate/lane is unavailable, keep the release gap.

## Handoff

Retain reports, bounded observations, source hashes and failed counterevidence
under the selected external evidence destination. Remove only spent task-owned
test packages/fixtures after all consumers finish. Publish a concise coverage
matrix and unresolved gates; do not mark this checklist complete by editing its
instructions or by passing only the headless suite.
