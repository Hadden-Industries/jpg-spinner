#pragma once

#include <jpg_spinner/domain/ImageProcessingResult.h>
#include <jpg_spinner/domain/OutputDisposition.h>
#include <jpg_spinner/domain/SourceFileRevision.h>
#include <jpg_spinner/domain/ValidatedJpegOutput.h>
#include <winrt/Windows.Storage.h>
#include <stop_token>

namespace jpg_spinner::storage
{
/// The exact analyzed source and its captured revision; the engine owns destination policy.
struct ImageFileTransactionRequest final
{
    winrt::Windows::Storage::StorageFile sourceFile;
    domain::SourceFileRevision expectedSourceRevision;
    domain::OutputDisposition outputDisposition{domain::OutputDisposition::CreateCorrectedCopy};
};

/// A completed native move, not a claim of power-fail atomicity or durable-media persistence.
struct CommittedImageFile final
{
    winrt::Windows::Storage::StorageFile committedFile;
};

/// Batch-scoped capability hiding staging, verification and owned cleanup. Call on
/// an explicitly initialized MTA worker, never the UI thread. The validated output
/// stays alive and immutable for the whole synchronous call. Cancellation is checked
/// between awaited native operations; once the commit starts its result is reported
/// truthfully rather than pretending cancellation undid a successful move.
/// Failure never authorizes deleting arbitrary paths or overwriting a destination.
class ImageFileTransactionEngine
{
  public:
    virtual ~ImageFileTransactionEngine() = default;
    [[nodiscard]] virtual domain::ImageProcessingResult<CommittedImageFile> execute(
        const ImageFileTransactionRequest &request, const domain::ValidatedJpegOutput &validatedOutput,
        std::stop_token cancellationToken = {}) const = 0;
};
} // namespace jpg_spinner::storage
