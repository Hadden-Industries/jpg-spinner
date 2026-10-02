# Dependency qualification before Task 6 — 2026-10-03

## Decision and authorities

The user's standing instruction requires current packages. The existing live
freshness check returned RED for Exiv2 0.28.9, libjpeg-turbo's packaging revision
3.2.0#1, and Windows App SDK 2.5.1. Keep this configuration-only qualification
separate from the metadata behavior implementation; do not reopen Task 5.

- [Exiv2 0.28.9 release](https://github.com/Exiv2/exiv2/releases/tag/v0.28.9):
  parser fixes and Exif 3.1 tag support, published 2026-08-30.
- [Microsoft Windows App SDK release notes](https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/release-notes/windows-app-sdk-2-0):
  stable 2.5.1, published 2026-09-16; servicing fixes and additional APIs. This
  update enables no new optional composition engine or limited-access feature.
- [Official registry snapshot](https://github.com/microsoft/vcpkg/tree/c748cb44f2a435fcf015c35225c9d5545fe0021c):
  immutable resolution authority for the updated native graph, without overlays.
- [vcpkg manifest reference](https://learn.microsoft.com/en-us/vcpkg/reference/vcpkg-json#overrides):
  use the documented `#1` version suffix, not the deprecated separate
  `port-version` member. The exact closed manifest schemas remain unchanged.

## Execution evidence

- Before mutation: the live dependency-freshness check failed for the three
  updates above. This is the configuration acceptance RED, not a unit-test claim.
- Updated direct pins, policy expectations, current design/plan, and regenerated
  the app's NuGet lock graph through native MSBuild Restore with explicit
  `RestoreLockedMode=false` and `RestoreForceEvaluate=true`. Ordinary builds
  retain locked restore. No lock hashes were hand-edited.
- Live freshness and repository-policy reporting then passed.
- Debug x64 solution build passed. Native graph now resolves Expat 2.8.5,
  libspng 0.7.4#1 and zlib 1.3.2#2 in addition to the declared libraries;
  these are registry-owned transitive dependencies, not new direct authorities.
- Locked NuGet resolution passed for Win32, x64, and ARM64; the generated
  lockfile records App SDK InteractiveExperiences 2.1.9, avoiding the missing
  2.1.8 lower-bound package without a manual transitive override.
- Repository-policy negative controls and success-reporting tests passed.
- The full JPEG suite passed with AddressSanitizer: 837538 assertions in 44
  cases, seed 3046212093, using the updated instrumented dependency graph.
- An exploratory `HeadlessAll` run returned nonzero because the future-task
  suites still contain no tests. Its JPEG report had zero failures. Do not
  convert empty future suites to success; use the existing full profile for
  implemented Domain/JPEG behavior. No UI-runtime acceptance is implied.
- Final engine profile evidence is retained outside the repository and is
  checked before the signed milestone commit.

## Retention

Reuse the existing build/log directories and dependency caches; do not create
per-attempt checkout copies. Keep the currently useful instrumented dependency
graph for Task 6. The pre-existing untracked `tmp/` research data is excluded
from commits and has not been deleted.
