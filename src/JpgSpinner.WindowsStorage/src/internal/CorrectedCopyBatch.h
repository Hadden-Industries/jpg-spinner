#pragma once

#include <windows.h>
#include <winrt/Windows.Storage.h>
#include <optional>
#include <string>

namespace jpg_spinner::storage::internal
{
/// One adapter-owned batch. Native file IDs are retained across execute calls;
/// a same-path replacement must not turn somebody else's folder into our batch.
/// Access is serialized by the adapter; construction performs no filesystem I/O.
struct CorrectedCopyBatch final
{
    explicit CorrectedCopyBatch(winrt::Windows::Storage::StorageFolder selectedRoot);

    const winrt::Windows::Storage::StorageFolder selectedSourceRoot;
    const winrt::guid identifier;
    const std::wstring directoryName;
    winrt::Windows::Storage::StorageFolder folder{nullptr};
    std::optional<FILE_ID_INFO> selectedRootIdentity;
    std::optional<FILE_ID_INFO> folderIdentity;
};
} // namespace jpg_spinner::storage::internal
