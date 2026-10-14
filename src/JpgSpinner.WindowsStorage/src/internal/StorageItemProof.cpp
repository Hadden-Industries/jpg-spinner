#define NOMINMAX
#include <windows.h>
#include "StorageItemProof.h"
#include <cstring>
#include <array>
#include <string>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Storage.FileProperties.h>
#include <winrt/Windows.Foundation.Collections.h>

#pragma comment(lib, "windowsapp.lib")

namespace jpg_spinner::storage::internal
{
namespace
{
winrt::handle openStorageItemHandle(const winrt::Windows::Storage::IStorageItem &item,
                                    const HANDLE_ACCESS_OPTIONS access,
                                    const HANDLE_SHARING_OPTIONS sharing = HSO_SHARE_READ | HSO_SHARE_WRITE)
{
    if (!item)
        throw winrt::hresult_invalid_argument();
    // Preserve broker authority across the Storage/native boundary. A picked
    // object's path is metadata, not a transferable grant to a path-based open.
    // This interface also avoids reconstructing extended-length open syntax.
    // https://learn.microsoft.com/windows/win32/api/windowsstoragecom/nf-windowsstoragecom-istorageitemhandleaccess-create
    winrt::handle handle;
    winrt::check_hresult(item.as<IStorageItemHandleAccess>()->Create(access, sharing, HO_NONE, nullptr, handle.put()));
    if (!handle || handle.get() == INVALID_HANDLE_VALUE)
        throw winrt::hresult_invalid_argument();
    return handle;
}

void requirePhysicalItemKind(const winrt::handle &handle, const bool directory)
{
    FILE_ATTRIBUTE_TAG_INFO attributes{};
    winrt::check_bool(
        GetFileInformationByHandleEx(handle.get(), FileAttributeTagInfo, &attributes, sizeof(attributes)));
    if ((attributes.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ||
        ((attributes.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) != directory)
        throw winrt::hresult_invalid_argument();
}
} // namespace

bool hasQualifiedRecoveryFileSystem(const winrt::Windows::Storage::StorageFolder &folder)
{
    const auto handle = openStorageItemHandle(folder, HAO_READ_ATTRIBUTES);
    std::array<wchar_t, MAX_PATH + 1> fileSystemName{};
    winrt::check_bool(GetVolumeInformationByHandleW(handle.get(), nullptr, 0, nullptr, nullptr, nullptr,
                                                    fileSystemName.data(), static_cast<DWORD>(fileSystemName.size())));
    // Native volume metadata, not a guessed path prefix, selects the qualified
    // contract. FAT IDs can change on rename; ReFS/cloud qualification is separate.
    if (std::wstring_view{fileSystemName.data()} != L"NTFS")
        return false;
    // Unsupported filesystems may not implement FileAttributeTagInfo at all.
    // Select the volume contract first, then validate the qualified physical
    // folder kind. Reparse rejection on NTFS remains mandatory, never bypassed.
    requirePhysicalItemKind(handle, true);
    if ((storageItemAttributes(folder) & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
        throw winrt::hresult_invalid_argument();
    return true;
}

winrt::handle acquireJournalStoreLease(const winrt::Windows::Storage::StorageFolder &folder,
                                       const StorageItemProof &folderProof)
{
    // Attribute-only access is explicitly exempt from share-mode exclusion.
    // Request read/write access with share mode zero to establish a real native
    // lease; OpenIfExists never truncates an earlier file. No delete-on-close race.
    const auto file = folder
                          .CreateFileAsync(L".jpg-spinner-journal-store-lease",
                                           winrt::Windows::Storage::CreationCollisionOption::OpenIfExists)
                          .get();
    auto lease = openStorageItemHandle(file, HAO_READ | HAO_WRITE, HSO_SHARE_NONE);
    requirePhysicalItemKind(lease, false);
    LARGE_INTEGER size{};
    winrt::check_bool(GetFileSizeEx(lease.get(), &size));
    if (size.QuadPart != 0)
        throw winrt::hresult_invalid_argument();
    const auto proof = inspectItem(file, false);
    if (proof.canonicalPath != folderProof.canonicalPath / L".jpg-spinner-journal-store-lease")
        throw winrt::hresult_invalid_argument();
    return lease;
}

StorageItemProof inspectItem(const winrt::Windows::Storage::IStorageItem &item, const bool directory,
                             const HANDLE_ACCESS_OPTIONS access)
{
    // The supported broker interface has no OPEN_REPARSE_POINT option. Query
    // the original item's native attributes before taking its physical handle,
    // then retain handle-derived kind/identity and canonical containment proof.
    // Never manufacture an undocumented HANDLE_OPTIONS value or bypass denial.
    if ((storageItemAttributes(item) & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
        throw winrt::hresult_invalid_argument();
    auto handle = openStorageItemHandle(item, access);
    requirePhysicalItemKind(handle, directory);
    FILE_ID_INFO identity{};
    winrt::check_bool(GetFileInformationByHandleEx(handle.get(), FileIdInfo, &identity, sizeof(identity)));
    // NT volume names avoid translating through the Mount Manager. The native
    // result is only compared; it is never passed back as an I/O path.
    std::wstring finalPath(32768, L'\0');
    const auto length =
        GetFinalPathNameByHandleW(handle.get(), finalPath.data(), static_cast<DWORD>(finalPath.size()), VOLUME_NAME_NT);
    if (length == 0)
        winrt::throw_last_error();
    if (length >= finalPath.size())
        throw winrt::hresult_invalid_argument();
    finalPath.resize(length);
    return {std::move(handle), std::filesystem::path{finalPath}, identity};
}

DWORD storageItemAttributes(const winrt::Windows::Storage::IStorageItem &item)
{
    if (!item)
        throw winrt::hresult_invalid_argument();
    // System.FileAttributes is specified as UInt32 with WIN32_FIND_DATA bit
    // semantics. Native projection/type checking owns its representation.
    // https://learn.microsoft.com/windows/win32/properties/props-system-fileattributes
    const auto properties =
        item.GetBasicPropertiesAsync().get().RetrievePropertiesAsync({L"System.FileAttributes"}).get();
    return winrt::unbox_value<std::uint32_t>(properties.Lookup(L"System.FileAttributes"));
}

bool isSameIdentity(const FILE_ID_INFO &left, const FILE_ID_INFO &right) noexcept
{
    return left.VolumeSerialNumber == right.VolumeSerialNumber &&
           std::memcmp(left.FileId.Identifier, right.FileId.Identifier, sizeof(left.FileId.Identifier)) == 0;
}

std::wstring storageItemIdentityText(const FILE_ID_INFO &identity)
{
    static_assert(sizeof(FILE_ID_INFO) == 24, "Journal version 1 stores the Windows 24-byte volume/file identity.");
    using winrt::Windows::Security::Cryptography::CryptographicBuffer;
    const auto begin = reinterpret_cast<const std::uint8_t *>(&identity);
    return std::wstring{CryptographicBuffer::EncodeToHexString(CryptographicBuffer::CreateFromByteArray(
        winrt::array_view<const std::uint8_t>{begin, begin + sizeof(identity)}))};
}
} // namespace jpg_spinner::storage::internal
