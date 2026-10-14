#pragma once

#include <jpg_spinner/storage/WindowsStorageStartupRecovery.h>
#include <winrt/Windows.Storage.AccessCache.h>
#include <winrt/Windows.Foundation.Collections.h>

namespace jpg_spinner::presentation
{
/// Restore actual capabilities from the native app cache on an owned MTA, then
/// run serial recovery. Cache metadata is not a transaction ID or a pathname.
/// Unavailable entries are retained; only the recovery engine proves whether a
/// required root is present. No cache entry is evicted by startup inspection.
[[nodiscard]] inline domain::ImageProcessingResult<storage::ImageFileTransactionRecoverySummary>
recoverStartupTransactions(const winrt::Windows::Storage::StorageFolder &journalStore,
                           const winrt::Windows::Storage::AccessCache::IStorageItemAccessList &accessList,
                           std::stop_token cancellation = {})
{
    using Result = domain::ImageProcessingResult<storage::ImageFileTransactionRecoverySummary>;
    if (!accessList)
        throw winrt::hresult_invalid_argument();
    try
    {
        std::vector<winrt::Windows::Storage::StorageFolder> restoredFolders;
        // Copy the native bounded entry list before awaiting a restoration. A
        // successful OS restoration supplies the grant, never Metadata or Path.
        std::vector<winrt::hstring> tokens;
        const auto entries = accessList.Entries();
        if (entries.Size() > accessList.MaximumItemsAllowed())
            throw winrt::hresult_invalid_argument();
        for (const auto &entry : entries)
            tokens.push_back(entry.Token);
        for (const auto &token : tokens)
        {
            if (cancellation.stop_requested())
                return Result::failure(
                    {domain::ImageProcessingErrorCode::Cancelled, domain::ImageProcessingStage::TransactionRecovery});
            try
            {
                restoredFolders.push_back(accessList.GetFolderAsync(token).get());
            }
            catch (const winrt::hresult_error &)
            {
                // A disconnected or revoked entry stays available for retry.
                // Missing required roots fail closed at the identity boundary.
            }
        }
        return storage::WindowsStorageStartupRecovery{journalStore}.recover(restoredFolders, cancellation);
    }
    catch (const winrt::hresult_error &error)
    {
        return Result::failure({domain::ImageProcessingErrorCode::RecoveryConflict,
                                domain::ImageProcessingStage::TransactionRecovery,
                                domain::WindowsHResult{error.code().value}});
    }
    catch (const std::bad_alloc &)
    {
        return Result::failure({domain::ImageProcessingErrorCode::WorkingMemoryAllocationFailed,
                                domain::ImageProcessingStage::TransactionRecovery});
    }
}
} // namespace jpg_spinner::presentation
