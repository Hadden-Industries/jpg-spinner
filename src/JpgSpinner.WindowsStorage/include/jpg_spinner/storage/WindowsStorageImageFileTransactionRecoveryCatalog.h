#pragma once

#include <windows.h>
#include <jpg_spinner/domain/ImageProcessingResult.h>
#include <winrt/Windows.Storage.h>
#include <stop_token>
#include <vector>

namespace jpg_spinner::storage
{
/// A validated incomplete transaction and its required source-root identity.
/// Neither identifier grants access. Startup must retrieve an actual retained
/// StorageFolder and independently match its native identity before recovery.
/// No persisted source pathname is exposed to presentation or diagnostics.
struct ImageFileTransactionRecoveryRequirement final
{
    winrt::guid transactionIdentifier;
    FILE_ID_INFO selectedSourceRootIdentity;
};

/// Read-only inventory of an application-owned journal store, using the same
/// immutable-chain validator as transaction recovery. Call from an explicitly
/// initialized MTA. Corrupt/unpublished chains fail closed with RecoveryConflict;
/// cancellation reports Cancelled. Inspection creates no lease or artifacts.
/// This inventory is not an atomic global snapshot or admission for later writes:
/// startup must stay serialized with normal processing, and recovery still owns
/// its lease and revalidation. Inspection alone never authorizes cache eviction.
class WindowsStorageImageFileTransactionRecoveryCatalog final
{
  public:
    explicit WindowsStorageImageFileTransactionRecoveryCatalog(winrt::Windows::Storage::StorageFolder journalStore);
    [[nodiscard]] domain::ImageProcessingResult<std::vector<ImageFileTransactionRecoveryRequirement>>
    inspectRecoveryRequirements(std::stop_token cancellation = {}) const;

  private:
    const winrt::Windows::Storage::StorageFolder journalStore_;
};
} // namespace jpg_spinner::storage
