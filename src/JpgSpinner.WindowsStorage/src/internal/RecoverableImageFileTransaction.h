#pragma once

#include <jpg_spinner/storage/ImageFileTransactionEngine.h>
#include "FileTransactionOperations.h"
#include "CorrectedCopyBatch.h"

namespace jpg_spinner::storage::internal
{
/// Per-call state owner. Application consumers cannot select phases or stage names.
/// Task 10 extends this boundary with observable-state recovery, not a public journal API.
class RecoverableImageFileTransaction final
{
  public:
    [[nodiscard]] static domain::ImageProcessingResult<CommittedImageFile> executeCorrectedCopy(
        CorrectedCopyBatch &batch, const ImageFileTransactionRequest &request,
        const domain::ValidatedJpegOutput &validatedOutput, std::stop_token cancellationToken,
        const FileTransactionOperations &operations = FileTransactionOperations{});
};
} // namespace jpg_spinner::storage::internal
