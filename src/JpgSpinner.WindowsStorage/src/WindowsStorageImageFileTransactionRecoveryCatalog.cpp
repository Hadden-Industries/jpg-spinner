#define NOMINMAX
#include <windows.h>
#include <jpg_spinner/storage/WindowsStorageImageFileTransactionRecoveryCatalog.h>
#include "internal/ImageFileTransactionJournal.h"
#include "internal/StorageItemProof.h"
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Search.h>
#include <limits>
#include <stdexcept>
#include <utility>

namespace jpg_spinner::storage
{
WindowsStorageImageFileTransactionRecoveryCatalog::WindowsStorageImageFileTransactionRecoveryCatalog(
    winrt::Windows::Storage::StorageFolder journalStore)
    : journalStore_(std::move(journalStore))
{
    if (!journalStore_)
        throw winrt::hresult_invalid_argument();
}

domain::ImageProcessingResult<std::vector<ImageFileTransactionRecoveryRequirement>>
WindowsStorageImageFileTransactionRecoveryCatalog::inspectRecoveryRequirements(const std::stop_token cancellation) const
{
    using Result = domain::ImageProcessingResult<std::vector<ImageFileTransactionRecoveryRequirement>>;
    using domain::ImageProcessingErrorCode;
    using domain::ImageProcessingStage;
    try
    {
        APTTYPE type;
        APTTYPEQUALIFIER qualifier;
        winrt::check_hresult(CoGetApartmentType(&type, &qualifier));
        if (type != APTTYPE_MTA || qualifier == APTTYPEQUALIFIER_IMPLICIT_MTA)
            throw winrt::hresult_wrong_thread();
        const auto storeProof = internal::inspectItem(journalStore_, true);
        std::vector<ImageFileTransactionRecoveryRequirement> requirements;
        constexpr std::uint32_t maximumFoldersPerPage = 500;
        for (std::uint32_t offset = 0;;)
        {
            if (cancellation.stop_requested())
                return Result::failure(
                    {ImageProcessingErrorCode::Cancelled, ImageProcessingStage::TransactionRecovery});
            const auto folders = journalStore_
                                     .GetFoldersAsync(winrt::Windows::Storage::Search::CommonFolderQuery::DefaultQuery,
                                                      offset, maximumFoldersPerPage)
                                     .get();
            if (folders.Size() == 0)
                return Result::success(std::move(requirements));
            if (folders.Size() > maximumFoldersPerPage ||
                folders.Size() > std::numeric_limits<std::uint32_t>::max() - offset)
                throw winrt::hresult_invalid_argument();
            offset += folders.Size();
            for (const auto &folder : folders)
            {
                if (cancellation.stop_requested())
                    return Result::failure(
                        {ImageProcessingErrorCode::Cancelled, ImageProcessingStage::TransactionRecovery});
                const auto proof = internal::inspectItem(folder, true);
                if (proof.canonicalPath.parent_path() != storeProof.canonicalPath)
                    throw winrt::hresult_invalid_argument();
                const winrt::guid identifier{std::wstring_view{folder.Name()}};
                // The owning validator checks the entire published generation
                // chain, bounded bytes, schema/types and immutable facts. Never
                // parse the latest filename or copy a JSON grammar into startup.
                const internal::ImageFileTransactionJournal journal{folder, identifier};
                const auto latest = journal.readLatest();
                if (!latest)
                    throw winrt::hresult_invalid_argument();
                if (!internal::isTerminalImageFileTransactionState(latest->state))
                    requirements.push_back(
                        {identifier, internal::decodeStorageItemIdentity(latest->selectedRootIdentity)});
            }
        }
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
    catch (const std::invalid_argument &)
    {
        // C++/WinRT's native GUID constructor reports invalid text through the
        // standard exception, not hresult_error. Preserve structured refusal;
        // never replace the consumer parser or expose its diagnostic message.
        return Result::failure({ImageProcessingErrorCode::RecoveryConflict, ImageProcessingStage::TransactionRecovery,
                                domain::WindowsHResult{E_INVALIDARG}});
    }
}
} // namespace jpg_spinner::storage
