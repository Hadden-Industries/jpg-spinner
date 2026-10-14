#pragma once

#include <jpg_spinner/batch/BatchProcessingRequest.h>
#include <jpg_spinner/storage/WindowsStorageImageFileTransactionEngine.h>
#include <winrt/Windows.Storage.h>
#include <memory>
#include <vector>

namespace jpg_spinner::presentation
{
/// One selected-folder batch's native dependencies and display context. The
/// request and transaction engine share the complete UUID/time, not an inferred
/// directory-name identity. Display labels never grant additional access.
struct SelectedFolderBatchComposition final
{
    batch_processing::BatchProcessingRequest request;
    std::shared_ptr<const storage::WindowsStorageImageFileTransactionEngine> transactionEngine;
    winrt::hstring sourceFolderDisplayName;
    winrt::hstring backupRootDisplayPath;
};

/// Compose from granted capabilities only. No picker, parent/sibling lookup,
/// discovery, transformation, directory creation or transaction is performed.
/// A missing grant is invalid; caller supplies its app-local journal store and
/// actual owned-folder capabilities rather than guessing ownership from names.
[[nodiscard]] SelectedFolderBatchComposition composeSelectedFolderBatch(
    winrt::Windows::Storage::StorageFolder selectedSourceFolder,
    winrt::Windows::Storage::StorageFolder applicationJournalStore,
    std::vector<storage::IdentityBoundStorageFolder> applicationOwnedFolders = {});
} // namespace jpg_spinner::presentation
