#include <jpg_spinner/storage/DirectChildStorageFileLease.h>
#include "internal/StorageItemProof.h"

namespace jpg_spinner::storage
{
struct DirectChildStorageFileLease::Proof final
{
    internal::StorageItemProof parent;
    internal::StorageItemProof file;
};
DirectChildStorageFileLease::DirectChildStorageFileLease(const winrt::Windows::Storage::StorageFolder &parent,
                                                         const winrt::Windows::Storage::StorageFile &file)
    : proof_(std::make_unique<Proof>(internal::inspectItem(parent, true), internal::inspectItem(file, false)))
{
    if (proof_->file.canonicalPath.parent_path() != proof_->parent.canonicalPath ||
        proof_->file.canonicalPath.filename() != std::filesystem::path{file.Name().c_str()})
        throw winrt::hresult_invalid_argument();
}
DirectChildStorageFileLease::~DirectChildStorageFileLease() = default;
} // namespace jpg_spinner::storage
