#define NOMINMAX
#include <windows.h>
#include <jpg_spinner/storage/WindowsStorageImageSourceCollection.h>
#include "internal/StorageItemProof.h"
#include "internal/SourceFileCapture.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Search.h>
#include <array>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace jpg_spinner::storage
{
namespace
{
using namespace winrt::Windows::Storage;
using DiscoveryResult = domain::ImageProcessingResult<domain::ImageSourceDiscovery>;

/// Shared ancestor handles pin the actual chain acquired through the selected
/// root while candidates are reviewed; strings alone cannot prevent substitution.
struct SourceFolderAncestry final
{
    internal::StorageItemProof proof;
    StorageFolder folder;
    std::shared_ptr<const SourceFolderAncestry> parent;
};

void requireDiscoveryWorker();

/// Storage-backed source capability; no filename can manufacture this object.
class WindowsStorageImageSource final : public domain::ImageSource
{
  public:
    WindowsStorageImageSource(std::wstring leafName, FILE_ID_INFO identity,
                              std::shared_ptr<const SourceFolderAncestry> ancestry,
                              std::shared_ptr<const ImageFileTransactionEngine> transactions)
        : leafName_(std::move(leafName)), identity_(identity), ancestry_(std::move(ancestry)),
          transactions_(std::move(transactions))
    {
    }

    domain::ImageProcessingResult<domain::CapturedImageSource> capture(
        const std::uint64_t maximumEncodedLengthBytes, const std::stop_token cancellation) const override
    {
        using Result = domain::ImageProcessingResult<domain::CapturedImageSource>;
        if (cancellation.stop_requested())
            return Result::failure(
                {domain::ImageProcessingErrorCode::Cancelled, domain::ImageProcessingStage::SourceRevisionCapture});
        try
        {
            requireDiscoveryWorker();
            const auto file = ancestry_->folder.GetFileAsync(leafName_).get();
            const auto proof = internal::inspectItem(file.Path(), false);
            if (!internal::isSameIdentity(proof.identity, identity_) ||
                proof.canonicalPath != ancestry_->proof.canonicalPath / leafName_)
                return Result::failure({domain::ImageProcessingErrorCode::SourceChangedAfterAnalysis,
                                        domain::ImageProcessingStage::SourceRevisionCapture});
            auto captured = internal::captureSourceFile(file, cancellation, {}, maximumEncodedLengthBytes);
            if (const auto error = captured.errorIfPresent())
                return Result::failure(*error);
            auto &value = *captured.valueIfPresent();
            return Result::success({std::move(*value.encodedBytes), std::move(value.revision)});
        }
        catch (const winrt::hresult_error &error)
        {
            return Result::failure({domain::ImageProcessingErrorCode::SourceRevisionCaptureFailed,
                                    domain::ImageProcessingStage::SourceRevisionCapture,
                                    domain::WindowsHResult{error.code().value}});
        }
        catch (const std::bad_alloc &)
        {
            return Result::failure({domain::ImageProcessingErrorCode::WorkingMemoryAllocationFailed,
                                    domain::ImageProcessingStage::SourceRevisionCapture});
        }
    }
    domain::ImageProcessingResult<domain::CommittedImageSource> commit(
        const domain::SourceFileRevision &expectedRevision, const domain::ValidatedJpegOutput &output,
        const domain::OutputDisposition disposition, const std::stop_token cancellation) const override
    {
        using Result = domain::ImageProcessingResult<domain::CommittedImageSource>;
        if (cancellation.stop_requested())
            return Result::failure({domain::ImageProcessingErrorCode::Cancelled,
                                    domain::ImageProcessingStage::SourceRevisionRevalidation});
        try
        {
            requireDiscoveryWorker();
            const auto file = ancestry_->folder.GetFileAsync(leafName_).get();
            // Allocate observation text before admitting I/O. A post-commit name
            // query/allocation must not relabel a real commit as a failed operation.
            std::wstring outputDisplayName{leafName_};
            // The proof denies rename, so release it before the owning engine's
            // authorized move. Ancestor capabilities remain pinned throughout.
            {
                const auto proof = internal::inspectItem(file.Path(), false);
                if (!internal::isSameIdentity(proof.identity, identity_) ||
                    proof.canonicalPath != ancestry_->proof.canonicalPath / leafName_)
                    return Result::failure({domain::ImageProcessingErrorCode::SourceChangedAfterAnalysis,
                                            domain::ImageProcessingStage::SourceRevisionRevalidation});
            }
            // Transfer the observed identity across the unavoidable release/open
            // boundary. The engine compares it under its own retained handle,
            // before staging, rather than trusting matching content or a pathname.
            const auto committed =
                transactions_->execute({file, expectedRevision, disposition, identity_}, output, cancellation);
            if (const auto error = committed.errorIfPresent())
                return Result::failure(*error);
            return Result::success({std::move(outputDisplayName)});
        }
        catch (const winrt::hresult_error &error)
        {
            return Result::failure({domain::ImageProcessingErrorCode::SourceRevisionCaptureFailed,
                                    domain::ImageProcessingStage::SourceRevisionRevalidation,
                                    domain::WindowsHResult{error.code().value}});
        }
        catch (const std::bad_alloc &)
        {
            return Result::failure({domain::ImageProcessingErrorCode::WorkingMemoryAllocationFailed,
                                    domain::ImageProcessingStage::SourceRevisionRevalidation});
        }
    }

  private:
    // The 10,000-item baseline exposed costly retained StorageFile projections.
    // Keep the granted parent and an identity-bound leaf; resolve through that
    // folder only for the active operation, never GetFileFromPathAsync.
    const std::wstring leafName_;
    FILE_ID_INFO identity_;
    std::shared_ptr<const SourceFolderAncestry> ancestry_;
    std::shared_ptr<const ImageFileTransactionEngine> transactions_;
};

bool hasCandidateExtension(const winrt::hstring &name)
{
    const auto extension = std::filesystem::path{name.c_str()}.extension().wstring();
    constexpr std::array<std::wstring_view, 4> extensions{L".jpg", L".jpeg", L".jpe", L".jfif"};
    return std::any_of(extensions.begin(), extensions.end(), [&](const auto accepted) {
        return CompareStringOrdinal(extension.c_str(), static_cast<int>(extension.size()), accepted.data(),
                                    static_cast<int>(accepted.size()), TRUE) == CSTR_EQUAL;
    });
}

domain::ImageProcessingError discoveryError(const winrt::hresult code)
{
    const auto symbolic = code == E_ACCESSDENIED ? domain::ImageProcessingErrorCode::SourceAccessDenied
                                                 : domain::ImageProcessingErrorCode::SourceDiscoveryFailed;
    return {symbolic, domain::ImageProcessingStage::CandidateDiscovery, domain::WindowsHResult{code.value}};
}

void requireDiscoveryWorker()
{
    APTTYPE type;
    APTTYPEQUALIFIER qualifier;
    winrt::check_hresult(CoGetApartmentType(&type, &qualifier));
    if (type != APTTYPE_MTA || qualifier == APTTYPEQUALIFIER_IMPLICIT_MTA)
        winrt::check_hresult(RPC_E_WRONG_THREAD);
}
} // namespace

WindowsStorageImageSourceCollection::WindowsStorageImageSourceCollection(
    winrt::Windows::Storage::StorageFolder selectedRoot,
    std::shared_ptr<const ImageFileTransactionEngine> transactionEngine,
    std::vector<winrt::Windows::Storage::StorageFolder> applicationOwnedFolders)
    : selectedRoot_(std::move(selectedRoot)), transactionEngine_(std::move(transactionEngine)),
      applicationOwnedFolders_(std::move(applicationOwnedFolders))
{
}

domain::ImageProcessingResult<domain::ImageSourceDiscovery> WindowsStorageImageSourceCollection::discover(
    const domain::TraversalScope scope, const std::stop_token cancellation,
    const domain::ImageSourceDiscoveryObserver &observer) const
{
    try
    {
        requireDiscoveryWorker();
        if (!selectedRoot_ || !transactionEngine_ ||
            (scope != domain::TraversalScope::SelectedFolderOnly &&
             scope != domain::TraversalScope::SelectedFolderAndDescendants))
            throw winrt::hresult_invalid_argument();
        domain::ImageSourceDiscovery discovery;
        if (cancellation.stop_requested())
        {
            discovery.cancellationRequested = true;
            return DiscoveryResult::success(std::move(discovery));
        }
        const auto rootAncestry = std::make_shared<SourceFolderAncestry>(
            internal::inspectItem(selectedRoot_.Path(), true), selectedRoot_, nullptr);
        std::vector<internal::StorageItemProof> excludedFolders;
        for (const auto &folder : applicationOwnedFolders_)
            excludedFolders.push_back(internal::inspectItem(folder.Path(), true));

        struct PendingFolder final
        {
            StorageFolder folder;
            std::wstring relativePath;
            std::shared_ptr<const SourceFolderAncestry> ancestry;
        };
        std::vector<PendingFolder> pending{{selectedRoot_, L"", rootAncestry}};
        std::size_t inspectedItems = 0;
        constexpr std::uint32_t maximumItemsPerPage = 500;
        while (!pending.empty() && !cancellation.stop_requested())
        {
            auto current = std::move(pending.back());
            pending.pop_back();
            try
            {
                std::uint32_t firstItem = 0;
                while (!cancellation.stop_requested())
                {
                    // Query the granted folder itself, not a desktop iterator or
                    // a deep query that could traverse reparse points implicitly.
                    const auto page = current.folder.GetItemsAsync(firstItem, maximumItemsPerPage).get();
                    if (page.Size() == 0)
                        break;
                    if (page.Size() > maximumItemsPerPage ||
                        page.Size() > std::numeric_limits<std::uint32_t>::max() - firstItem)
                        throw winrt::hresult_invalid_argument();
                    firstItem += page.Size();
                    for (const auto &item : page)
                    {
                        if (cancellation.stop_requested())
                            break;
                        ++inspectedItems;
                        std::wstring name{item.Name()};
                        auto relativePath = current.relativePath.empty() ? name : current.relativePath + L"/" + name;
                        try
                        {
                            const bool directory = item.IsOfType(StorageItemTypes::Folder);
                            // FromApp observes the item itself. Reparse points are
                            // reported before obtaining a folder/traversal capability.
                            const auto attributes = internal::storageItemAttributes(item.Path(), directory);
                            if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
                            {
                                discovery.issues.push_back(
                                    {relativePath, domain::ImageSourceDiscoveryIssueKind::ReparsePointSkipped, {}});
                            }
                            else
                            {
                                auto proof = internal::inspectItem(item.Path(), directory);
                                // A long-path Storage item can expose its 8.3 alias
                                // in Name. Use the normalized name returned by the
                                // existing native handle proof, never a guessed
                                // expansion or an alias as the displayed source name.
                                const auto leaf = proof.canonicalPath.filename();
                                if (leaf.has_parent_path() || leaf.has_root_path() || leaf.empty() || leaf == L"." ||
                                    leaf == L".." ||
                                    proof.canonicalPath.parent_path() != current.ancestry->proof.canonicalPath)
                                    throw winrt::hresult_invalid_argument();
                                name = leaf.wstring();
                                relativePath = current.relativePath.empty() ? name : current.relativePath + L"/" + name;
                                if (directory)
                                {
                                    const bool excluded = std::any_of(
                                        excludedFolders.begin(), excludedFolders.end(), [&](const auto &owned) {
                                            return internal::isSameIdentity(proof.identity, owned.identity);
                                        });
                                    if (excluded)
                                        discovery.issues.push_back(
                                            {relativePath,
                                             domain::ImageSourceDiscoveryIssueKind::ApplicationOwnedFolderExcluded,
                                             {}});
                                    else if (scope == domain::TraversalScope::SelectedFolderAndDescendants)
                                        pending.push_back(
                                            {item.as<StorageFolder>(), relativePath,
                                             std::make_shared<SourceFolderAncestry>(
                                                 std::move(proof), item.as<StorageFolder>(), current.ancestry)});
                                }
                                else if (hasCandidateExtension(winrt::hstring{name}))
                                    discovery.candidates.push_back(
                                        {relativePath,
                                         std::make_shared<WindowsStorageImageSource>(
                                             name, proof.identity, current.ancestry, transactionEngine_)});
                            }
                        }
                        catch (const winrt::hresult_error &error)
                        {
                            discovery.issues.push_back({relativePath,
                                                        domain::ImageSourceDiscoveryIssueKind::SourceAccessFailed,
                                                        discoveryError(error.code())});
                        }
                        if (observer)
                            observer({inspectedItems, discovery.candidates.size()});
                    }
                }
            }
            catch (const winrt::hresult_error &error)
            {
                discovery.issues.push_back({current.relativePath,
                                            domain::ImageSourceDiscoveryIssueKind::SourceAccessFailed,
                                            discoveryError(error.code())});
            }
        }
        discovery.cancellationRequested = cancellation.stop_requested();
        return DiscoveryResult::success(std::move(discovery));
    }
    catch (const winrt::hresult_error &error)
    {
        return DiscoveryResult::failure(discoveryError(error.code()));
    }
    catch (const std::bad_alloc &)
    {
        return DiscoveryResult::failure({domain::ImageProcessingErrorCode::WorkingMemoryAllocationFailed,
                                         domain::ImageProcessingStage::CandidateDiscovery});
    }
}
} // namespace jpg_spinner::storage
