#pragma once

#include "JpegFrameProperties.h"
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace jpg_spinner::jpeg::internal
{
class JpegOutputValidator;
}

namespace jpg_spinner::domain
{
/// Fixed-size cryptographic identity of encoded bytes, independent of text format.
using Sha256Digest = std::array<std::byte, 32>;

/// Facts independently observed in approved metadata, not a writer's claim.
struct JpegOutputMetadataEvidence final
{
    bool hasExifOrientation;
    bool hasStandardXmpOrientation;
    bool removedEmbeddedThumbnail;
};

/// Evidence bound to the approved marker inventory. ICC hashes use logical
/// profile order; the marker digest includes output framing and physical output
/// order. Rewritten metadata is output evidence, not a digest of source bytes.
struct PreservedJpegMarkerEvidence final
{
    std::uint32_t metadataMarkerCount;
    Sha256Digest encodedMetadataSha256;
    std::optional<Sha256Digest> iccProfileSha256;
};

/// Move-only approval capability owning the exact independently validated bytes.
/// Consumers may inspect but cannot mutate or construct it. A moved-from value
/// must not be submitted to storage. Storage must re-hash its closed staged file:
/// this evidence establishes in-memory validity, not persistence or durability.
class ValidatedJpegOutput final
{
  public:
    ValidatedJpegOutput(const ValidatedJpegOutput &) = delete;
    ValidatedJpegOutput &operator=(const ValidatedJpegOutput &) = delete;
    ValidatedJpegOutput(ValidatedJpegOutput &&) noexcept = default;
    ValidatedJpegOutput &operator=(ValidatedJpegOutput &&) = delete;

    [[nodiscard]] std::span<const std::byte> encodedBytes() const noexcept
    {
        return encodedBytes_;
    }
    [[nodiscard]] const Sha256Digest &encodedSha256() const noexcept
    {
        return encodedSha256_;
    }
    [[nodiscard]] const JpegFrameProperties &outputProperties() const noexcept
    {
        return outputProperties_;
    }
    [[nodiscard]] const JpegOutputMetadataEvidence &metadataEvidence() const noexcept
    {
        return metadataEvidence_;
    }
    [[nodiscard]] const PreservedJpegMarkerEvidence &preservedMarkerEvidence() const noexcept
    {
        return markerEvidence_;
    }

  private:
    // This named producer is an ownership boundary, not a native library type
    // leaking into Domain. Test engines obtain capabilities through the real
    // validation path instead of inventing valid-looking digests.
    friend class jpg_spinner::jpeg::internal::JpegOutputValidator;
    ValidatedJpegOutput(std::vector<std::byte> encodedBytes, Sha256Digest encodedSha256, JpegFrameProperties properties,
                        JpegOutputMetadataEvidence metadata, PreservedJpegMarkerEvidence markers) noexcept
        : encodedBytes_(std::move(encodedBytes)), encodedSha256_(encodedSha256),
          outputProperties_(std::move(properties)), metadataEvidence_(metadata), markerEvidence_(markers)
    {
    }

    std::vector<std::byte> encodedBytes_;
    Sha256Digest encodedSha256_;
    JpegFrameProperties outputProperties_;
    JpegOutputMetadataEvidence metadataEvidence_;
    PreservedJpegMarkerEvidence markerEvidence_;
};
} // namespace jpg_spinner::domain
