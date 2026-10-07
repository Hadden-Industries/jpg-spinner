#pragma once

#include <windows.h>
#include <winrt/Windows.Storage.h>
#include <optional>
#include <string>
#include <chrono>

namespace jpg_spinner::storage::internal
{
/// One adapter-owned batch. Native file IDs are retained across execute calls;
/// a same-path replacement must not turn somebody else's folder into our batch.
/// Access is serialized by the adapter; construction performs no filesystem I/O.
struct ImageFileTransactionBatch final
{
    ImageFileTransactionBatch(winrt::Windows::Storage::StorageFolder selectedRoot,
                              winrt::Windows::Storage::StorageFolder applicationJournalStore);
    ImageFileTransactionBatch(winrt::Windows::Storage::StorageFolder selectedRoot,
                              winrt::Windows::Storage::StorageFolder applicationJournalStore,
                              winrt::guid batchIdentifier, std::chrono::sys_seconds batchCreationTimeUtc);

    const winrt::Windows::Storage::StorageFolder selectedSourceRoot;
    const winrt::Windows::Storage::StorageFolder journalStore;
    const winrt::guid identifier;
    const std::wstring directoryName;
    winrt::Windows::Storage::StorageFolder correctedCopyFolder{nullptr};
    winrt::Windows::Storage::StorageFolder backupFolder{nullptr};
    std::optional<FILE_ID_INFO> selectedRootIdentity;
    std::optional<FILE_ID_INFO> correctedCopyFolderIdentity;
    std::optional<FILE_ID_INFO> backupFolderIdentity;
};
} // namespace jpg_spinner::storage::internal
