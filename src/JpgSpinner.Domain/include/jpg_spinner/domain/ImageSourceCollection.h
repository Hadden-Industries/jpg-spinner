#pragma once

#include "ImageProcessingResult.h"
#include "OutputDisposition.h"
#include "SourceFileRevision.h"
#include "TraversalScope.h"
#include "ValidatedJpegOutput.h"
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace jpg_spinner::domain
{
/// Exactly the bytes and revision observed together, not independently opened files.
struct CapturedImageSource final
{
    std::vector<std::byte> encodedBytes;
    SourceFileRevision revision;
};

/// Successful existing transaction observation; this value grants no file authority.
struct CommittedImageSource final
{
    std::wstring outputDisplayName;
};

/// Selected-root capability for one discovered image. Concrete storage owns identity
/// proof, bounded reads, revision revalidation and the existing commit engine.
/// Capture owns at most maximumEncodedLengthBytes. Commit never accepts raw output.
/// Call serially on a platform-initialized background worker. Cancellation is
/// cooperative; it cannot undo a native commit already admitted by the engine.
class ImageSource
{
  public:
    virtual ~ImageSource() = default;
    [[nodiscard]] virtual ImageProcessingResult<CapturedImageSource> capture(
        std::uint64_t maximumEncodedLengthBytes, std::stop_token cancellation = {}) const = 0;
    [[nodiscard]] virtual ImageProcessingResult<CommittedImageSource> commit(
        const SourceFileRevision &expectedRevision, const ValidatedJpegOutput &validatedOutput,
        OutputDisposition disposition, std::stop_token cancellation = {}) const = 0;
};

/// Original relative name and a capability acquired from the granted root. A path
/// is only a display/order key, never a substitute for this capability's authority.
struct DiscoveredImageSource final
{
    std::wstring relativePath;
    std::shared_ptr<const ImageSource> source;
};

/// Non-image traversal observations are distinct from terminal candidate outcomes.
enum class ImageSourceDiscoveryIssueKind
{
    ReparsePointSkipped,
    SourceAccessFailed,
    ApplicationOwnedFolderExcluded,
};

/// Relative display context only; access errors retain their typed native projection.
struct ImageSourceDiscoveryIssue final
{
    std::wstring relativePath;
    ImageSourceDiscoveryIssueKind kind;
    std::optional<ImageProcessingError> error;
};

/// Bounded metadata discovery; cancellation preserves the candidates already seen.
/// Images are extension candidates, not JPEG proof. No file bytes are opened here.
struct ImageSourceDiscovery final
{
    std::vector<DiscoveredImageSource> candidates;
    std::vector<ImageSourceDiscoveryIssue> issues;
    bool cancellationRequested{false};
};

/// Observation while the final denominator is unknown. Counts are monotonic.
struct ImageSourceDiscoveryProgress final
{
    std::size_t inspectedItems;
    std::size_t discoveredCandidates;
};
/// Called synchronously on the discovery worker; observers must not throw or block.
using ImageSourceDiscoveryObserver = std::function<void(const ImageSourceDiscoveryProgress &)>;

/// Read-only discovery through the selected platform capability, not a free path.
/// The adapter owns paging, exact owned-folder exclusion and reparse refusal.
/// Collection order is unspecified; the batch consumer applies its ordering policy.
class ImageSourceCollection
{
  public:
    virtual ~ImageSourceCollection() = default;
    [[nodiscard]] virtual ImageProcessingResult<ImageSourceDiscovery> discover(
        TraversalScope scope, std::stop_token cancellation = {},
        const ImageSourceDiscoveryObserver &observer = {}) const = 0;
};
} // namespace jpg_spinner::domain
