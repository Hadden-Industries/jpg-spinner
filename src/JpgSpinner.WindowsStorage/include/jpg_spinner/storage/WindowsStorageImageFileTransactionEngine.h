#pragma once

#include "ImageFileTransactionEngine.h"
#include <mutex>
#include <memory>

namespace jpg_spinner::storage
{
namespace internal
{
struct CorrectedCopyBatch;
}
/// Native Windows Storage adapter for one selected-root batch. Its full UUID is
/// retained privately; the timestamp/UUID-prefix directory is created exclusively
/// on the first successful execution path and reused within this instance only.
/// Calls serialize per instance. Construct with a granted physical StorageFolder;
/// construction can throw native UUID/allocation errors, but performs no file I/O.
/// Unsupported identity/canonical-path providers fail closed. Using this class
/// does not put its process in AppContainer: deployment supplies that boundary.
/// Replacement currently returns OriginalReplacementFailed/E_NOTIMPL without I/O;
/// its mandatory backup/journal protocol is the next accepted Task 10 slice.
class WindowsStorageImageFileTransactionEngine final : public ImageFileTransactionEngine
{
  public:
    explicit WindowsStorageImageFileTransactionEngine(winrt::Windows::Storage::StorageFolder selectedSourceRoot);
    ~WindowsStorageImageFileTransactionEngine() override;
    [[nodiscard]] domain::ImageProcessingResult<CommittedImageFile> execute(
        const ImageFileTransactionRequest &request, const domain::ValidatedJpegOutput &validatedOutput,
        std::stop_token cancellationToken = {}) const override;

  private:
    const std::unique_ptr<internal::CorrectedCopyBatch> batch_;
    mutable std::mutex executionMutex_;
};
} // namespace jpg_spinner::storage
