#define NOMINMAX
#include <windows.h>
#include <fileapifromapp.h>
#include <pathcch.h>
#include "StorageItemProof.h"
#include <cstring>
#include <array>
#include <string>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.Storage.Streams.h>

#pragma comment(lib, "windowsapp.lib")
#pragma comment(lib, "pathcch.lib")

namespace jpg_spinner::storage::internal
{
namespace
{
winrt::handle openStorageItemMetadataHandle(const winrt::hstring &path, const bool directory, const DWORD access,
                                            const DWORD sharing = FILE_SHARE_READ | FILE_SHARE_WRITE,
                                            const DWORD creation = OPEN_EXISTING)
{
    if (path.empty())
        throw winrt::hresult_invalid_argument();
    // Windows Storage can create a stage beyond MAX_PATH even when this process
    // has no longPathAware manifest/registry opt-in. Let the supported App-family
    // PathCch consumer produce extended-length drive/UNC syntax; never hand-parse
    // prefixes or change machine policy. This is only an open-path representation,
    // not identity or containment proof. ENSURE implies preserving trailing dots
    // and spaces and must not be combined with ALLOW_LONG_PATHS (SDK contract).
    std::wstring extendedLengthOpenPath(PATHCCH_MAX_CCH, L'\0');
    winrt::check_hresult(PathCchCanonicalizeEx(extendedLengthOpenPath.data(), extendedLengthOpenPath.size(),
                                               path.c_str(), PATHCCH_ENSURE_IS_EXTENDED_LENGTH_PATH));
    // FromApp honors native security; never retry denied metadata access through
    // an unrestricted desktop open. Open the reparse point itself to reject it.
    const auto rawHandle =
        CreateFileFromAppW(extendedLengthOpenPath.c_str(), access, sharing, nullptr, creation,
                           FILE_FLAG_OPEN_REPARSE_POINT | (directory ? FILE_FLAG_BACKUP_SEMANTICS : 0), nullptr);
    if (rawHandle == INVALID_HANDLE_VALUE)
        winrt::throw_last_error();
    return winrt::handle{rawHandle};
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

bool hasQualifiedRecoveryFileSystem(const winrt::hstring &folderPath)
{
    const auto handle = openStorageItemMetadataHandle(folderPath, true, FILE_READ_ATTRIBUTES);
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
    return true;
}

winrt::handle acquireJournalStoreLease(const winrt::hstring &folderPath, const StorageItemProof &folderProof)
{
    const auto leasePath = (std::filesystem::path{folderPath.c_str()} / L".jpg-spinner-journal-store-lease").wstring();
    // Attribute-only access is explicitly exempt from share-mode exclusion.
    // Request read/write access with share mode zero to establish a real native
    // lease; OPEN_ALWAYS never truncates an earlier file. No delete-on-close race.
    auto lease =
        openStorageItemMetadataHandle(winrt::hstring{leasePath}, false, GENERIC_READ | GENERIC_WRITE, 0, OPEN_ALWAYS);
    requirePhysicalItemKind(lease, false);
    LARGE_INTEGER size{};
    winrt::check_bool(GetFileSizeEx(lease.get(), &size));
    if (size.QuadPart != 0)
        throw winrt::hresult_invalid_argument();
    const auto proof = inspectItem(winrt::hstring{leasePath}, false);
    if (proof.canonicalPath != folderProof.canonicalPath / L".jpg-spinner-journal-store-lease")
        throw winrt::hresult_invalid_argument();
    return lease;
}

StorageItemProof inspectItem(const winrt::hstring &path, const bool directory, const DWORD access)
{
    auto handle = openStorageItemMetadataHandle(path, directory, access);
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

DWORD storageItemAttributes(const winrt::hstring &path, const bool directory)
{
    const auto handle = openStorageItemMetadataHandle(path, directory, FILE_READ_ATTRIBUTES);
    FILE_ATTRIBUTE_TAG_INFO attributes{};
    winrt::check_bool(
        GetFileInformationByHandleEx(handle.get(), FileAttributeTagInfo, &attributes, sizeof(attributes)));
    return attributes.FileAttributes;
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
