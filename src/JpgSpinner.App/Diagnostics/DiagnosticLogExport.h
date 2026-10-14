#pragma once

#include "Diagnostics/LocalDiagnosticLog.h"
#include <memory>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>

namespace jpg_spinner::diagnostics
{
/// Exports the logger's bounded sanitized snapshot through an existing native
/// StorageFile grant. Does not pick a file, acquire a path-based grant, rotate
/// records or alter image transactions. The caller owns destination selection.
/// Completion requires native provider completion; failure/cancellation may
/// leave a partial destination, never a claimed durable or atomic export.
/// Owns both arguments across awaits, snapshots off the caller's UI thread,
/// and propagates native/schema errors for the caller's localized presentation.
/// Requires non-null arguments and a .jsonl destination (ordinal case-insensitive);
/// invalid arguments fail before snapshot/write. Cancellation propagates to
/// awaited native operations; it is not a physical rollback guarantee.
[[nodiscard]] winrt::Windows::Foundation::IAsyncAction exportDiagnosticLogAsync(
    std::shared_ptr<LocalDiagnosticLog> log, winrt::Windows::Storage::StorageFile destination);
} // namespace jpg_spinner::diagnostics
