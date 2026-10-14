#pragma once

#include <winrt/Windows.Storage.h>
#include <memory>

namespace jpg_spinner::storage
{
/// Pin and verify an actual granted physical file as a direct child of an actual
/// granted physical folder. Uses the existing brokered identity/canonical-path
/// proof, not Storage display paths or parent heuristics. Constructor performs
/// synchronous native inspection on an owned MTA; retain through the operation.
/// Handles prevent rename/deletion substitution, not external content writes.
/// This proves containment only; the caller separately owns payload validation
/// and any decision to delete an application-owned diagnostic record.
class DirectChildStorageFileLease final
{
  public:
    DirectChildStorageFileLease(const winrt::Windows::Storage::StorageFolder &parent,
                                const winrt::Windows::Storage::StorageFile &file);
    ~DirectChildStorageFileLease();
    DirectChildStorageFileLease(const DirectChildStorageFileLease &) = delete;
    DirectChildStorageFileLease &operator=(const DirectChildStorageFileLease &) = delete;

  private:
    struct Proof;
    const std::unique_ptr<Proof> proof_;
};
} // namespace jpg_spinner::storage
