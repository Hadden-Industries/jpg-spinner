#define NOMINMAX
#include <windows.h>
#include <jpg_spinner/storage/WindowsStorageImageFileTransactionEngine.h>
#include "internal/RecoverableImageFileTransaction.h"
#include "internal/CorrectedCopyBatch.h"
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

internal::CorrectedCopyBatch::CorrectedCopyBatch(winrt::Windows::Storage::StorageFolder selectedRoot)
    : selectedSourceRoot(std::move(selectedRoot)), identifier(createIdentifier()),
      directoryName(std::format(L"{:%Y%m%dT%H%M%SZ}-{:08x}",
                                std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()),
                                identifier.Data1))
{
}

WindowsStorageImageFileTransactionEngine::WindowsStorageImageFileTransactionEngine(
    winrt::Windows::Storage::StorageFolder selectedSourceRoot)
    : batch_(std::make_unique<internal::CorrectedCopyBatch>(std::move(selectedSourceRoot)))
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
    return internal::RecoverableImageFileTransaction::executeCorrectedCopy(*batch_, request, validatedOutput,
                                                                           cancellationToken);
}
} // namespace jpg_spinner::storage
