#define NOMINMAX
#include <windows.h>
#include <jpg_spinner/storage/WindowsStorageImageFileTransactionEngine.h>
#include "internal/RecoverableImageFileTransaction.h"
#include "internal/ImageFileTransactionBatch.h"
#include <chrono>
#include <format>
#include <new>
#include <utility>

#pragma comment(lib, "ole32.lib")

namespace jpg_spinner::storage
{
namespace
{
winrt::guid createIdentifier()
{
    winrt::guid identifier;
    winrt::check_hresult(CoCreateGuid(reinterpret_cast<GUID *>(&identifier)));
    return identifier;
}
} // namespace

internal::ImageFileTransactionBatch::ImageFileTransactionBatch(
    winrt::Windows::Storage::StorageFolder selectedRoot, winrt::Windows::Storage::StorageFolder applicationJournalStore)
    : selectedSourceRoot(std::move(selectedRoot)), journalStore(std::move(applicationJournalStore)),
      identifier(createIdentifier()),
      directoryName(std::format(L"{:%Y%m%dT%H%M%SZ}-{:08x}",
                                std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()),
                                identifier.Data1))
{
}

WindowsStorageImageFileTransactionEngine::WindowsStorageImageFileTransactionEngine(
    winrt::Windows::Storage::StorageFolder selectedSourceRoot,
    winrt::Windows::Storage::StorageFolder applicationJournalStore)
    : batch_(std::make_unique<internal::ImageFileTransactionBatch>(std::move(selectedSourceRoot),
                                                                   std::move(applicationJournalStore)))
{
}

WindowsStorageImageFileTransactionEngine::~WindowsStorageImageFileTransactionEngine() = default;

domain::ImageProcessingResult<CommittedImageFile> WindowsStorageImageFileTransactionEngine::execute(
    const ImageFileTransactionRequest &request, const domain::ValidatedJpegOutput &validatedOutput,
    const std::stop_token cancellationToken) const
{
    // Serialize the lazy batch-folder creation as well as transactions. Batch
    // orchestration may cancel while waiting, so check the token again inside.
    const std::lock_guard lock{executionMutex_};
    return internal::RecoverableImageFileTransaction::execute(*batch_, request, validatedOutput, cancellationToken);
}

domain::ImageProcessingResult<ImageFileTransactionRecoverySummary> WindowsStorageImageFileTransactionEngine::
    recoverIncompleteTransactions(std::stop_token cancellationToken) const
{
    const std::lock_guard lock{executionMutex_};
    return internal::RecoverableImageFileTransaction::recoverIncompleteTransactions(*batch_, cancellationToken);
}
} // namespace jpg_spinner::storage
