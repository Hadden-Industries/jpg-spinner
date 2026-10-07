#pragma once

#include <jpg_spinner/domain/ImageSourceCollection.h>
#include "ImageFileTransactionEngine.h"
#include <winrt/Windows.Storage.h>

namespace jpg_spinner::storage
{
/// Windows Storage discovery and per-image access, bound to the same selected-root
/// transaction engine. Caller-supplied excluded folders are application-owned
/// capabilities, never guessed names. Call from an explicitly initialized MTA.
/// Native identity/reparse failures are retained observations, not traversal grants.
class WindowsStorageImageSourceCollection final : public domain::ImageSourceCollection
{
  public:
    WindowsStorageImageSourceCollection(
        winrt::Windows::Storage::StorageFolder selectedRoot,
        std::shared_ptr<const ImageFileTransactionEngine> transactionEngine,
        std::vector<winrt::Windows::Storage::StorageFolder> applicationOwnedFolders = {});
    [[nodiscard]] domain::ImageProcessingResult<domain::ImageSourceDiscovery> discover(
        domain::TraversalScope scope, std::stop_token cancellation = {},
        const domain::ImageSourceDiscoveryObserver &observer = {}) const override;

  private:
    winrt::Windows::Storage::StorageFolder selectedRoot_;
    std::shared_ptr<const ImageFileTransactionEngine> transactionEngine_;
    std::vector<winrt::Windows::Storage::StorageFolder> applicationOwnedFolders_;
};
} // namespace jpg_spinner::storage
