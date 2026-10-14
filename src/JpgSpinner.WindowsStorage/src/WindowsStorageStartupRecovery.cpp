#include <jpg_spinner/storage/WindowsStorageStartupRecovery.h>
#include <jpg_spinner/storage/WindowsStorageImageFileTransactionRecoveryCatalog.h>
#include <jpg_spinner/storage/WindowsStorageImageFileTransactionEngine.h>
#include "internal/StorageItemProof.h"
#include <algorithm>
#include <utility>

namespace jpg_spinner::storage
{
WindowsStorageStartupRecovery::WindowsStorageStartupRecovery(winrt::Windows::Storage::StorageFolder journalStore)
    : journalStore_(std::move(journalStore))
{
    if (!journalStore_)
        throw winrt::hresult_invalid_argument();
}
domain::ImageProcessingResult<ImageFileTransactionRecoverySummary> WindowsStorageStartupRecovery::recover(
    std::span<const winrt::Windows::Storage::StorageFolder> restoredSourceFolders,
    std::stop_token cancellationToken) const
{
    using Result = domain::ImageProcessingResult<ImageFileTransactionRecoverySummary>;
    using domain::ImageProcessingErrorCode;
    using domain::ImageProcessingStage;
    try
    {
        const WindowsStorageImageFileTransactionRecoveryCatalog catalog{journalStore_};
        const auto requirements = catalog.inspectRecoveryRequirements(cancellationToken);
        if (const auto error = requirements.errorIfPresent())
            return Result::failure(*error);
        struct GrantedRoot final
        {
            winrt::Windows::Storage::StorageFolder folder;
            internal::StorageItemProof proof;
        };
        std::vector<GrantedRoot> requiredRoots;
        for (const auto &requirement : *requirements.valueIfPresent())
        {
            if (cancellationToken.stop_requested())
                return Result::failure(
                    {ImageProcessingErrorCode::Cancelled, ImageProcessingStage::TransactionRecovery});
            const auto matches = [&requirement](const auto &root) {
                return internal::isSameIdentity(root.proof.identity, requirement.selectedSourceRootIdentity);
            };
            if (std::ranges::any_of(requiredRoots, matches))
                continue;
            bool found{};
            for (const auto &folder : restoredSourceFolders)
            {
                if (!folder)
                    continue;
                try
                {
                    auto proof = internal::inspectItem(folder, true);
                    if (internal::isSameIdentity(proof.identity, requirement.selectedSourceRootIdentity))
                    {
                        // Pin the actual grant's identity/location through this
                        // pass. An unavailable unrelated cache entry is not a
                        // reason to reopen a persisted path or evict its token.
                        requiredRoots.push_back({folder, std::move(proof)});
                        found = true;
                        break;
                    }
                }
                catch (const winrt::hresult_error &)
                {
                    // Missing required access is reported below, before any
                    // recovery effects. Never infer authority from cache text.
                }
            }
            if (!found)
                return Result::failure(
                    {ImageProcessingErrorCode::RecoveryConflict, ImageProcessingStage::TransactionRecovery});
        }
        ImageFileTransactionRecoverySummary summary;
        for (const auto &root : requiredRoots)
        {
            const WindowsStorageImageFileTransactionEngine engine{root.folder, journalStore_};
            auto recovered = engine.recoverIncompleteTransactions(cancellationToken);
            if (const auto error = recovered.errorIfPresent())
                return Result::failure(*error);
            const auto &transactions = recovered.valueIfPresent()->recoveredTransactions;
            summary.recoveredTransactions.insert(summary.recoveredTransactions.end(), transactions.begin(),
                                                 transactions.end());
        }
        // A catalog is not a global snapshot. Reinspect after serial recovery
        // and refuse normal work if a record appeared or remains unresolved.
        const auto remaining = catalog.inspectRecoveryRequirements(cancellationToken);
        if (const auto error = remaining.errorIfPresent())
            return Result::failure(*error);
        if (!remaining.valueIfPresent()->empty())
            return Result::failure(
                {ImageProcessingErrorCode::RecoveryConflict, ImageProcessingStage::TransactionRecovery});
        return Result::success(std::move(summary));
    }
    catch (const winrt::hresult_error &error)
    {
        return Result::failure({ImageProcessingErrorCode::RecoveryConflict, ImageProcessingStage::TransactionRecovery,
                                domain::WindowsHResult{error.code().value}});
    }
    catch (const std::bad_alloc &)
    {
        return Result::failure(
            {ImageProcessingErrorCode::WorkingMemoryAllocationFailed, ImageProcessingStage::TransactionRecovery});
    }
}
} // namespace jpg_spinner::storage
