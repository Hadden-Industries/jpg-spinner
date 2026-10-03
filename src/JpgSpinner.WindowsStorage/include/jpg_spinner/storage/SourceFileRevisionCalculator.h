#pragma once

#include <jpg_spinner/domain/ImageProcessingResult.h>
#include <jpg_spinner/domain/SourceFileRevision.h>

#include <winrt/Windows.Storage.h>

#include <cstdint>
#include <functional>
#include <stop_token>

namespace jpg_spinner::storage
{
/// Byte progress for one opened file. A notification is not a completed revision.
struct SourceFileRevisionProgress final
{
    std::uint64_t encodedBytesHashed;
    std::uint64_t totalEncodedBytes;
};

/// Called on a worker after open and each hashed block. Return promptly, do not
/// throw, and marshal/throttle UI notifications. Captures must outlive completion.
using SourceFileRevisionProgressObserver = std::function<void(const SourceFileRevisionProgress &)>;

/// Captures a caller-supplied StorageFile through native bounded streaming SHA-256.
/// Stateless and safe for concurrent calls; never obtains authority from a path.
/// Call from an initialized MTA worker, never a UI/STA thread. The batch layer
/// owns scheduling, as it does for the synchronous JPEG engine.
class SourceFileRevisionCalculator final
{
  public:
    /// Blocks its worker until capture completes. Native I/O/sharing/cancellation
    /// failures become structured results; no digest is published on failure.
    /// An uninitialized (including implicit-MTA), non-MTA or null-file caller
    /// fails before I/O. Observer exceptions other than
    /// native HRESULT/allocation failures propagate as caller contract violations.
    /// Cancellation is checked between native calls, not a hard I/O deadline.
    [[nodiscard]] domain::ImageProcessingResult<domain::SourceFileRevision> calculate(
        const winrt::Windows::Storage::StorageFile &sourceFile, std::stop_token cancellationToken = {},
        const SourceFileRevisionProgressObserver &progressObserver = {}) const;
};
} // namespace jpg_spinner::storage
