#pragma once

#include "ImageFileTransactionEngine.h"
#include <span>

namespace jpg_spinner::storage
{
/// Reconcile the application journal store before admitting new work. Call on
/// an explicitly owned MTA with actual restored folder grants, not journal paths.
/// Recovery is serial and delegates all artifact decisions to the transaction
/// engine. Missing grants or unresolved records fail closed; no cache entry is
/// evicted and no original/backup is deleted. The caller prevents concurrent
/// processing while this startup gate runs; per-root recovery acquires its lease.
class WindowsStorageStartupRecovery final
{
  public:
    explicit WindowsStorageStartupRecovery(winrt::Windows::Storage::StorageFolder journalStore);
    [[nodiscard]] domain::ImageProcessingResult<ImageFileTransactionRecoverySummary> recover(
        std::span<const winrt::Windows::Storage::StorageFolder> restoredSourceFolders,
        std::stop_token cancellationToken = {}) const;

  private:
    const winrt::Windows::Storage::StorageFolder journalStore_;
};
} // namespace jpg_spinner::storage
