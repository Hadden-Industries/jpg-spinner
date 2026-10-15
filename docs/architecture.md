# Modern application architecture

This is the implemented module boundary and intended deployment contract, not
proof that all release environments have been qualified. The
[approved design](specs/2026-08-30-jpg-spinner-modernization-design.md) owns the
full policy; the [release register](release-qualification.md) tracks open proof.

## Modules

| Module | Responsibility | Does not own |
| --- | --- | --- |
| Domain | Immutable image plans, typed outcomes, limits and capability contracts | Storage, XAML, codec handles |
| JpegTransformation | Marker inventory, TurboJPEG transform, Exiv2 reconciliation, independent output validation | Replacement or folder authority |
| WindowsStorage | Granted-source capture, revision proofs, staging, backup, journals and recovery | JPEG interpretation or localized UI |
| BatchProcessing | Stable discovery and sequential reviewed execution, cancellation and result accounting | Dialogs or codec internals |
| Presentation | Bindable review/progress contracts and accessible shell state | Native storage mutation policy |
| App | WinUI composition, real picker grants, owned apartments and local diagnostics | Legacy engine compatibility |

Dependencies point from composition/orchestration to domain-owned capabilities.
The capability interfaces are the current seams, not adapters preserving the
legacy implementation. Internal codec, journal and fault-injection seams stay
inside their owning modules; tests do not justify a new public API by themselves.

## Transaction invariants

Only independently validated JPEG output reaches the transaction boundary.
The captured source revision is rechecked before commit. Identity-bound stages
and destinations are created exclusively; a name alone never proves ownership.
Replacement requires a retained, independently hashed backup and a published,
reread immutable journal generation. Corrected-copy mode does not replace the
source. Recovery uses actual granted Storage items, native identity and content
proofs rather than trusting paths serialized in a journal.

The closed journal graph includes `TransactionAbandonedBeforeCommit`: publish it
only after proving no commit occurred and the identity-bound stage was removed.
Terminal outcomes are not revisited as if later user edits were interrupted work.
Ambiguity remains `RecoveryConflict`; originals and verified backups are retained.

## Security boundary

The package manifest declares `packagedClassicApp` with `appContainer` trust and
no broad filesystem or network capabilities. Package identity alone is not that
boundary: runtime denial tests must establish effective access. User-mediated
picker grants authorize Storage items; native brokered handles supply file
identity and containment proof. Reparse paths and unqualified providers fail
closed. The app does not use a path-based full-trust compatibility fallback.

Untrusted encoded bytes cross bounded native scanner/codec/metadata boundaries.
Execution is sequential per reviewed batch. UI changes return to their owning
dispatcher; WinRT Storage work runs on explicitly owned apartments. Diagnostic
records are a separate typed privacy boundary, not native exception dumps.

## Guarantee limits

Flush/close requests and provider completion are not promises of power-loss
durability, disk-controller behavior, universal filesystem atomicity, or recovery
after external artifact manipulation. The supported provider must supply the
actual identity/canonical-path/stream semantics checked by preflight. FAT32 is
deliberately unsupported. Long-path operation requires both the executable
manifest and the documented Windows machine opt-in; the app changes neither.

Cancellation stops at defined transaction boundaries, not by undoing already
committed results. A diagnostic export may leave a partial destination on native
failure or cancellation. No backup-retention cleanup is implicit. Preserve
conflicting artifacts and obtain explicit recovery guidance rather than deleting
them. See [Microsoft's long-path contract](https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation)
and the declaration comments in the owning storage interfaces.
