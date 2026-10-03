#pragma once

#include <jpg_spinner/domain/ImageProcessingResult.h>
#include <jpg_spinner/domain/OutputDisposition.h>
#include <jpg_spinner/domain/SourceFileRevision.h>
#include <jpg_spinner/domain/ValidatedJpegOutput.h>
#include <winrt/Windows.Storage.h>
#include <stop_token>
#include <vector>

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

/// Recovery reports observed bytes, never a blind retry of replacement or rollback.
enum class ImageFileTransactionRecoveryOutcome
{
    OriginalPreserved,
    OutputCommitted
};

/// Identifies one deterministically reconciled transaction without exposing journal
/// phases or granting deletion authority. Verified backups remain retained.
struct RecoveredImageFileTransaction final
{
    winrt::guid transactionIdentifier;
    ImageFileTransactionRecoveryOutcome outcome;
};

/// Newly reconciled incomplete transactions for the granted selected root.
/// Verified terminal records and other roots are not reopened. Inspection is
/// serial and isolates transaction conflicts: other independently explained
/// transactions can still be recovered, but any unresolved conflict makes the
/// aggregate operation fail with RecoveryConflict. JournalStoreBusy means a live
/// owner holds the lease and no recovery began; it is not contradictory evidence.
struct ImageFileTransactionRecoverySummary final
{
    std::vector<RecoveredImageFileTransaction> recoveredTransactions;
};

/// Batch-scoped capability hiding staging, verification and owned cleanup. Call on
/// an explicitly initialized MTA worker, never the UI thread. The validated output
/// stays alive and immutable for the whole synchronous call. Cancellation is checked
/// between awaited native operations; once the commit starts its result is reported
/// truthfully rather than pretending cancellation undid a successful move.
/// Failure never authorizes deleting arbitrary paths, replacing a backup, or
/// overwriting the original without the verified-backup and persisted-journal gate.
class ImageFileTransactionEngine
{
  public:
    virtual ~ImageFileTransactionEngine() = default;
    [[nodiscard]] virtual domain::ImageProcessingResult<CommittedImageFile> execute(
        const ImageFileTransactionRequest &request, const domain::ValidatedJpegOutput &validatedOutput,
        std::stop_token cancellationToken = {}) const = 0;
    [[nodiscard]] virtual domain::ImageProcessingResult<ImageFileTransactionRecoverySummary>
    recoverIncompleteTransactions(std::stop_token cancellationToken = {}) const = 0;
};
} // namespace jpg_spinner::storage
