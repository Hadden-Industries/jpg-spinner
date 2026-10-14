#pragma once

#include <windows.h>
#include <winrt/base.h>
#include <winrt/Windows.Storage.h>
#include <windowsstoragecom.h>
#include <filesystem>

namespace jpg_spinner::storage::internal
{
/// Handle-derived identity and canonical location. The retained handle prevents
/// delete/rename substitution; it does not exclude external writes. Paths are
/// metadata comparisons, never weaker authority for arbitrary I/O or deletion.
struct StorageItemProof final
{
    winrt::handle handle;
    std::filesystem::path canonicalPath;
    FILE_ID_INFO identity;
};

/// Inspect the actual granted item through its native brokered handle and
/// path/file-ID consumers. Reparse points and unavailable proofs fail closed.
/// The caller owns the returned handle and must close it before authorized moves.
[[nodiscard]] StorageItemProof inspectItem(const winrt::Windows::Storage::IStorageItem &item, bool directory,
                                           HANDLE_ACCESS_OPTIONS access = HAO_READ_ATTRIBUTES);
/// Observe the granted item's System.FileAttributes through the native property
/// provider. Missing or wrongly typed metadata fails closed; no path is reopened.
[[nodiscard]] DWORD storageItemAttributes(const winrt::Windows::Storage::IStorageItem &item);
/// The journal depends on identity surviving native rename/replacement. Only
/// physical NTFS has passed this contract's qualification; a file-ID-shaped value
/// from another provider is insufficient. This is not a durability guarantee.
[[nodiscard]] bool hasQualifiedRecoveryFileSystem(const winrt::Windows::Storage::StorageFolder &folder);
/// Serialize execution/recovery across adapter instances and processes sharing
/// this application-owned store. The native exclusive handle releases on process
/// death; the empty reserved file is retained, not a stale ownership marker.
[[nodiscard]] winrt::handle acquireJournalStoreLease(const winrt::Windows::Storage::StorageFolder &folder,
                                                     const StorageItemProof &folderProof);
[[nodiscard]] bool isSameIdentity(const FILE_ID_INFO &left, const FILE_ID_INFO &right) noexcept;
/// Version-1 journal representation of the native volume/file identity. Encoding
/// is delegated to CryptographicBuffer; this text is never a pathname or grant.
[[nodiscard]] std::wstring storageItemIdentityText(const FILE_ID_INFO &identity);
} // namespace jpg_spinner::storage::internal
