#include "pch.h"
#include "Composition/SelectedFolderBatchComposition.h"
#include <jpg_spinner/storage/WindowsStorageImageSourceCollection.h>
#include <bit>
#include <filesystem>

namespace jpg_spinner::presentation
{
SelectedFolderBatchComposition composeSelectedFolderBatch(
    winrt::Windows::Storage::StorageFolder selectedSourceFolder,
    winrt::Windows::Storage::StorageFolder applicationJournalStore,
    std::vector<storage::IdentityBoundStorageFolder> applicationOwnedFolders)
{
    if (!selectedSourceFolder || !applicationJournalStore)
        throw winrt::hresult_invalid_argument();
    batch_processing::BatchProcessingRequest request;
    request.executionLimits = batch_processing::BatchExecutionLimits::recommended();
    const auto transactions = std::make_shared<storage::WindowsStorageImageFileTransactionEngine>(
        selectedSourceFolder, std::move(applicationJournalStore),
        std::bit_cast<winrt::guid>(request.identifier.fullIdentity()), request.identifier.creationTime());
    request.sourceCollection = std::make_shared<storage::WindowsStorageImageSourceCollection>(
        selectedSourceFolder, transactions, std::move(applicationOwnedFolders));
    const auto displayName = selectedSourceFolder.DisplayName();
    winrt::hstring backupDisplayPath;
    // This is the existing transaction layout's display label, not a folder
    // lookup or permission grant. A provider with no physical path cannot offer
    // a precise replacement destination; leave the replacement review gate shut.
    const auto selectedPath = selectedSourceFolder.Path();
    if (!selectedPath.empty())
        backupDisplayPath = (std::filesystem::path{selectedPath.c_str()} / L"JPG Spinner Backups" /
                             request.identifier.displayDirectoryName())
                                .native();
    return {std::move(request), transactions, displayName, backupDisplayPath};
}
} // namespace jpg_spinner::presentation
