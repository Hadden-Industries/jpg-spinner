#pragma once

#include <filesystem>
#include <windows.h>
#include <winrt/Windows.Storage.h>

namespace jpg_spinner::storage
{
/// A granted physical folder paired with its previously observed native file
/// identity and handle-derived canonical location. This value does not prove
/// application ownership by itself: the
/// trusted composition root supplies only folders actually created by its
/// transaction engines. The recorded location scopes exclusions before opening
/// their grants, so unavailable outputs outside the selected tree do not block
/// discovery. Relevant folders must retain both identity and location; their
/// handles remain pinned for traversal. The NT path is comparison metadata only,
/// never an I/O pathname, display pathname or source of authority.
struct IdentityBoundStorageFolder final
{
    winrt::Windows::Storage::StorageFolder folder;
    FILE_ID_INFO identity;
    std::filesystem::path canonicalPath;
};
} // namespace jpg_spinner::storage
