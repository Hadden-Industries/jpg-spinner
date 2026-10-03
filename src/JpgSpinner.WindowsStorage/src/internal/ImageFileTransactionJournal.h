#pragma once

#include <jpg_spinner/domain/OutputDisposition.h>
#include <jpg_spinner/domain/SourceFileRevision.h>
#include <winrt/Windows.Storage.h>
#include <optional>
#include <string>
#include "FileTransactionOperations.h"
#include "StorageItemProof.h"

namespace jpg_spinner::storage::internal
{
/// Observed monotonic milestones, not promises of device-loss durability.
enum class ImageFileTransactionState
{
    TransactionInitialized,
    StagedOutputWritten,
    StagedOutputHashVerified,
    VerifiedBackupCreated,
    OutputCommitted,
    OwnedStagingArtifactsCleaned,
    /// No native commit occurred and the exact owned stage has been removed.
    /// This closes a refusal without claiming later user edits are corruption.
    TransactionAbandonedBeforeCommit
};

/// Immutable generation facts. File identities are native hexadecimal encodings
/// of FILE_ID_INFO; paths are StorageItem paths, not authority to open/delete them.
/// Recovery must independently establish identity, containment and byte hashes.
struct ImageFileTransactionJournalRecord final
{
    winrt::guid transactionIdentifier;
    std::uint32_t generation;
    ImageFileTransactionState state;
    domain::OutputDisposition outputDisposition;
    std::wstring selectedRootPath;
    std::wstring selectedRootIdentity;
    std::wstring sourcePath;
    std::wstring sourceIdentity;
    domain::SourceFileRevision sourceRevision;
    std::wstring stagePath;
    std::wstring stageIdentity;
    std::uint64_t outputLengthBytes;
    domain::Sha256Digest outputSha256;
    std::wstring backupPath;
    std::wstring backupIdentity;
    std::wstring destinationPath;

    bool operator==(const ImageFileTransactionJournalRecord &) const = default;
};

/// One transaction's immutable Windows.Data.Json generation chain. Call from an
/// explicit MTA, serialized by the transaction engine. The supplied folder is an
/// application-owned journal capability, never inferred from a selected source.
/// Unknown schemas, types, cardinality, transitions or changed immutable facts
/// throw native invalid-data/argument errors. Publish never overwrites a name;
/// only a successfully reread generation advances the in-memory chain. Pending
/// files are retained on failure and never used as published evidence.
class ImageFileTransactionJournal final
{
  public:
    ImageFileTransactionJournal(winrt::Windows::Storage::StorageFolder transactionFolder,
                                winrt::guid transactionIdentifier);
    void publish(const ImageFileTransactionJournalRecord &record,
                 const FileTransactionOperations &operations = FileTransactionOperations{});
    [[nodiscard]] std::optional<ImageFileTransactionJournalRecord> readLatest(
        const FileTransactionOperations &operations = FileTransactionOperations{}) const;

  private:
    const winrt::Windows::Storage::StorageFolder folder_;
    const winrt::guid transactionIdentifier_;
    const StorageItemProof folderProof_;
};
} // namespace jpg_spinner::storage::internal
