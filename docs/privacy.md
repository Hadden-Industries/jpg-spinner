# Privacy in the modern application

This describes the maintained source, not the installed historical app or
Windows/Microsoft Store diagnostics. Release traffic and capability checks remain
explicit entries in the [qualification register](release-qualification.md).

## Local processing

Image processing is local. The replacement adds no custom network telemetry or
image-upload transport, and its manifest declares no network capability. Picking
files from a cloud-backed provider may itself cause that provider to retrieve or
upload bytes; this is distinct from an application telemetry endpoint. Windows,
Store health reporting and a user's sync provider have their own policies.

Selected source/output/backup folders and immutable recovery journals are
operational data, not diagnostic logs. Journals necessarily retain transaction
facts used for recovery. Do not share them as if they were sanitized diagnostics.

## Diagnostic records

[`ImageProcessingDiagnosticRecord`](../src/JpgSpinner.App/Diagnostics/LocalDiagnosticLog.h)
allows only `recordedAtUtc`, `applicationVersion`, `applicationArchitecture`,
`correlationIdentifier`, `stage`, `errorCode`, `elapsedMilliseconds` and
`resourceLimit`. Optional facts serialize as null. Correlation is an operation
UUID, not a device identifier. UTC time is a wall-clock observation; elapsed time
is measured separately.

Records exclude filenames, paths, image bytes, thumbnails, Exif/XMP/IPTC content,
GPS, arbitrary exception text, endpoints and stable device identifiers. Native
Windows JSON serialization owns escaping. Records are UTF-8 JSON Lines with one
final LF and no BOM, written in the app-local `LocalDiagnostics` folder.

## Retention

[`DiagnosticLogRetentionLimits`](../src/JpgSpinner.App/Diagnostics/LocalDiagnosticLog.h)
defaults to at most 256 KiB and 64 retained diagnostic-record files. A record is
at most 4096 bytes; the reserved pending slot is budgeted and never exported.
The empty writer lease is not a record. Rotation recognizes and validates owned
records only. Unknown, corrupt or unsupported objects are preserved and refuse
the operation; they are not silently exported or deleted. Best-effort logging
failure does not fail image processing.

These are diagnostic bounds, not automatic deletion of originals, corrected
copies, backups or recovery journals. The app does not silently expire backups.

## Export

Export requires explicit destination selection through the native file picker.
The app exports a bounded, revalidated sanitized `.jsonl` snapshot, not raw files
from a folder. The chosen destination may be synced by its provider. Review the
export before sharing it; operation times and outcomes can still be sensitive.
Native write failure/cancellation may leave a partial destination and does not
establish durable or atomic publication. No automatic submission is performed.
