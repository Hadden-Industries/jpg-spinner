#pragma once

#include <windows.h>
#include <winrt/Windows.Storage.h>

namespace jpg_spinner::storage
{
/// A granted physical folder paired with its previously observed native file
/// identity. This value does not prove application ownership by itself: the
/// trusted composition root supplies only folders actually created by its
/// transaction engines. Discovery revalidates the identity and pins its handle
/// for the traversal, so replacing a same-named directory cannot inherit an
/// exclusion. No display pathname or inferred naming convention grants access.
struct IdentityBoundStorageFolder final
{
    winrt::Windows::Storage::StorageFolder folder;
    FILE_ID_INFO identity;
};
} // namespace jpg_spinner::storage
