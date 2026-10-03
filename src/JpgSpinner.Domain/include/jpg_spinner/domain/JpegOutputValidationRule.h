#pragma once

namespace jpg_spinner::domain
{
/// Stable rule identities for failed output approval; never native prose.
enum class JpegOutputValidationRule
{
    CompleteJpegStructure,
    PlannedDimensions,
    SamplePrecision,
    ComponentOrganization,
    ScanOrganization,
    EntropyCodingMode,
    QuantizationTablePreservation,
    RestartIntervalPreservation,
    FullDecode,
    CanonicalExifOrientation,
    CanonicalStandardXmpOrientation,
    DerivedDimensions,
    MetadataPreservation,
    PreservedMarkerSequence,
    IccProfilePreservation,
    ExtendedXmpPreservation,
    RemovedEmbeddedThumbnails,
    NoUnsupportedAssetBindings,
    EncodedOutputDigest,
};
} // namespace jpg_spinner::domain
