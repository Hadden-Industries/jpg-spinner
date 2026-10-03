#pragma once

#include "JpegMarkerInventory.h"
#include <jpg_spinner/domain/ExifOrientation.h>
#include <jpg_spinner/domain/ImageProcessingResult.h>
#include <jpg_spinner/domain/JpegTransformPlan.h>
#include <optional>
#include <span>
#include <vector>

namespace jpg_spinner::jpeg::internal
{
/// Observed orientation facts, retaining absence and disagreement separately
/// from the orientation used for planning. No native metadata object escapes.
struct OrientationMetadata final
{
    jpg_spinner::domain::ExifOrientation authoritativeOrientation;
    std::optional<jpg_spinner::domain::ExifOrientation> exifOrientation;
    std::optional<jpg_spinner::domain::ExifOrientation> xmpOrientation;
    bool hasOrientationConflict;
};

/// Owned serialization plus the concrete destructive effect requiring review.
/// The enclosing operation defines whether bytes contain a payload or framed
/// marker segments. Merely reformatting XMP does not count as preview removal.
struct ReconciledMetadataBytes final
{
    std::vector<std::byte> encodedBytes;
    bool removedEmbeddedThumbnail;
};

/// Owns interpretation and reconciliation of isolated JPEG metadata payloads.
/// This internal seam grants no filesystem authority or transform permission.
class MetadataReconciler final
{
  public:
    MetadataReconciler() = delete;

    /// Reads orientation without changing the source. inventory must be the
    /// successful bounded scan of these exact bytes; the caller must prevent
    /// concurrent mutation for the synchronous call. Native parse errors become
    /// redacted domain errors. This is not full image/metadata approval: extended
    /// XMP, previews, provenance and trailing-data policy are separate Task 6 gates.
    [[nodiscard]] static jpg_spinner::domain::ImageProcessingResult<OrientationMetadata> analyzeOrientation(
        std::span<const std::byte> source, const JpegMarkerInventory &inventory);

    /// Reconciles one isolated TIFF payload into owned bytes. The bounded
    /// source is immutable; native serialization never touches caller storage.
    /// Only orientation, present dimensions and derived thumbnails may change.
    /// Returns a typed error if preservation cannot be established.
    [[nodiscard]] static jpg_spinner::domain::ImageProcessingResult<ReconciledMetadataBytes> reconcileExifPayload(
        std::span<const std::byte> tiffPayload, const jpg_spinner::domain::JpegTransformPlan &plan);

    /// Rewrites a standard XMP packet, never an Extended XMP packet or JPEG.
    /// Preserves unrelated RDF properties semantically, not XML formatting.
    /// Rejects output that cannot fit a single standard-XMP APP1 segment.
    [[nodiscard]] static jpg_spinner::domain::ImageProcessingResult<ReconciledMetadataBytes> reconcileStandardXmpPacket(
        std::span<const std::byte> packet, const jpg_spinner::domain::JpegTransformPlan &plan);

    /// Correlates native XMP properties with scanner-proven extension chunks.
    /// Reassembly is read-only: this app never serializes Extended XMP. Success
    /// permits preserving chunks only for the supplied transform, not generally.
    [[nodiscard]] static jpg_spinner::domain::ImageProcessingResult<std::monostate> validateExtendedXmpPreservation(
        std::span<const std::byte> source, const JpegMarkerInventory &inventory,
        jpg_spinner::domain::LosslessTransform transform);

    /// Returns framed APPn/COM segments in source-relative order. The caller
    /// inserts these into the metadata-free codec output; no pixel bytes are
    /// accepted from a metadata library. Scanner inventory must match source.
    [[nodiscard]] static jpg_spinner::domain::ImageProcessingResult<ReconciledMetadataBytes> reconcileMarkerSegments(
        std::span<const std::byte> source, const JpegMarkerInventory &inventory,
        const jpg_spinner::domain::JpegTransformPlan &plan,
        const jpg_spinner::domain::JpegResourceLimits &resourceLimits =
            jpg_spinner::domain::JpegResourceLimits::production());

    /// Rejects external Content Credentials and unsupported appended assets.
    /// Motion Photo classification requires container lengths matching the exact
    /// trailing range, never a flag alone. Performs no network or file access.
    [[nodiscard]] static jpg_spinner::domain::ImageProcessingResult<std::monostate> validateSourceAssetBindings(
        std::span<const std::byte> source, const JpegMarkerInventory &inventory);
};
} // namespace jpg_spinner::jpeg::internal
