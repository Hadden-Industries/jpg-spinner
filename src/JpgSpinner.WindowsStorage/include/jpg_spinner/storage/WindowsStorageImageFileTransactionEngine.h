#pragma once

#include "ImageFileTransactionEngine.h"
#include <mutex>
#include <memory>
#include <chrono>

namespace jpg_spinner::storage
{
namespace internal
{
struct ImageFileTransactionBatch;
}
/// Native Windows Storage adapter for one selected-root batch. Its full UUID is
/// retained privately; the timestamp/UUID-prefix directory is created exclusively
/// on the first successful execution path and reused within this instance only.
/// Calls serialize per instance. Construct with a granted physical source folder
/// and an application-owned local journal store, which must outlive recovery.
/// Construction can throw native UUID/allocation errors, but performs no file I/O.
/// Unsupported identity/canonical-path providers fail closed. Using this class
/// does not put its process in AppContainer: deployment supplies that boundary.
/// Replacement requires a retained independently verified backup and a published,
/// reread journal gate. Recovery infers completed moves from identities and hashes;
/// uncertain artifacts remain untouched and produce RecoveryConflict.
class WindowsStorageImageFileTransactionEngine final : public ImageFileTransactionEngine
{
  public:
    WindowsStorageImageFileTransactionEngine(winrt::Windows::Storage::StorageFolder selectedSourceRoot,
                                             winrt::Windows::Storage::StorageFolder applicationJournalStore);
    /// Use the composition-owned full UUID and UTC time for a reviewed batch.
    /// This keeps journal identity and display labels aligned with its summary
    /// without introducing a Storage -> Batch dependency. A null UUID is invalid.
    WindowsStorageImageFileTransactionEngine(winrt::Windows::Storage::StorageFolder selectedSourceRoot,
                                             winrt::Windows::Storage::StorageFolder applicationJournalStore,
                                             winrt::guid batchIdentifier,
                                             std::chrono::sys_seconds batchCreationTimeUtc);
    ~WindowsStorageImageFileTransactionEngine() override;
    [[nodiscard]] domain::ImageProcessingResult<CommittedImageFile> execute(
        const ImageFileTransactionRequest &request, const domain::ValidatedJpegOutput &validatedOutput,
        std::stop_token cancellationToken = {}) const override;
    [[nodiscard]] domain::ImageProcessingResult<ImageFileTransactionRecoverySummary> recoverIncompleteTransactions(
        std::stop_token cancellationToken = {}) const override;

  private:
    const std::unique_ptr<internal::ImageFileTransactionBatch> batch_;
    mutable std::mutex executionMutex_;
};
} // namespace jpg_spinner::storage
