#pragma once

#include <jpg_spinner/storage/ImageFileTransactionEngine.h>
#include "FileTransactionOperations.h"
#include "ImageFileTransactionBatch.h"

namespace jpg_spinner::storage::internal
{
/// Per-call state owner. Application consumers cannot select phases or stage names.
/// Owns observable-state recovery too; journal transitions remain internal rather
/// than an application-controlled phase API. Uncertain artifacts are retained.
class RecoverableImageFileTransaction final
{
  public:
    [[nodiscard]] static domain::ImageProcessingResult<CommittedImageFile> execute(
        ImageFileTransactionBatch &batch, const ImageFileTransactionRequest &request,
        const domain::ValidatedJpegOutput &validatedOutput, std::stop_token cancellationToken,
        const FileTransactionOperations &operations = FileTransactionOperations{});
    [[nodiscard]] static domain::ImageProcessingResult<ImageFileTransactionRecoverySummary>
    recoverIncompleteTransactions(ImageFileTransactionBatch &batch, std::stop_token cancellationToken,
                                  const FileTransactionOperations &operations = FileTransactionOperations{});
};
} // namespace jpg_spinner::storage::internal
