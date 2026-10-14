#pragma once

#include <winrt/Windows.Storage.AccessCache.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <utility>

namespace jpg_spinner::presentation
{
/// Owns one recoverable batch's FutureAccessList entry. The caller serializes
/// calls and proves terminal transaction reconciliation before explicit release.
/// Destruction deliberately preserves an unresolved entry for startup recovery;
/// it neither revokes a picker grant nor deletes a file, backup or journal.
class SourceFolderAccessRetention final
{
  public:
    explicit SourceFolderAccessRetention(winrt::Windows::Storage::AccessCache::IStorageItemAccessList accessList)
        : accessList_(std::move(accessList))
    {
        if (!accessList_)
            throw winrt::hresult_invalid_argument();
    }
    SourceFolderAccessRetention(const SourceFolderAccessRetention &) = delete;
    SourceFolderAccessRetention &operator=(const SourceFolderAccessRetention &) = delete;

    /// Retains an actual granted folder under the complete batch UUID, not a
    /// display path or token per image. Repeated calls for the same folder/batch
    /// are idempotent; switching an unresolved batch is an illegal method call.
    /// Native cache failures propagate, and capacity never evicts older entries.
    void retainSourceFolder(const winrt::Windows::Storage::StorageFolder &sourceFolder,
                            const winrt::guid &batchIdentity)
    {
        if (!sourceFolder || batchIdentity == winrt::guid{})
            throw winrt::hresult_invalid_argument();
        if (hasRetainedSource())
        {
            if (batchIdentity_ != batchIdentity || !sourceFolder_.IsEqual(sourceFolder))
                throw winrt::hresult_illegal_method_call();
            return;
        }
        // FutureAccessList has an OS-owned limit. Refuse at that limit rather
        // than replacing a token that an older unresolved journal may require.
        if (accessList_.Entries().Size() >= accessList_.MaximumItemsAllowed())
            throw winrt::hresult_error{E_OUTOFMEMORY};
        sourceAccessToken_ = accessList_.Add(sourceFolder, winrt::to_hstring(batchIdentity));
        sourceFolder_ = sourceFolder;
        batchIdentity_ = batchIdentity;
    }

    /// Removes only this owner's observed entry after the caller has reconciled
    /// the transaction. Native removal failure leaves ownership intact for retry.
    /// Repeated release does not clear or evict other recoverable batch entries.
    void releaseReconciledSource()
    {
        if (!hasRetainedSource())
            return;
        if (accessList_.ContainsItem(sourceAccessToken_))
            accessList_.Remove(sourceAccessToken_);
        sourceAccessToken_.clear();
        sourceFolder_ = nullptr;
        batchIdentity_ = {};
    }

    /// Reports this owner's retained entry, not the native cache's global size.
    [[nodiscard]] bool hasRetainedSource() const noexcept
    {
        return !sourceAccessToken_.empty();
    }

  private:
    winrt::Windows::Storage::AccessCache::IStorageItemAccessList accessList_;
    winrt::Windows::Storage::StorageFolder sourceFolder_{nullptr};
    winrt::guid batchIdentity_{};
    winrt::hstring sourceAccessToken_;
};
} // namespace jpg_spinner::presentation
