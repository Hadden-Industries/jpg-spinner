#include "MetadataReconciler.h"
#include "JpegSegmentScanner.h"

#include <exiv2/exiv2.hpp>
#include <exiv2/photoshop.hpp>
#include <charconv>
#include <algorithm>
#include <mutex>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace jpg_spinner::jpeg::internal
{
using namespace jpg_spinner::domain;
namespace
{
using OrientationResult = ImageProcessingResult<OrientationMetadata>;

// Exiv2's diagnostic callback and XMP initialization are process-global. All
// production Exiv2 use belongs to this module and shares this lock. Do not throw
// from its log callback (called from a destructor), or retain private messages.
std::mutex metadataLibraryMutex;
thread_local bool nativeDiagnosticObserved = false;

class MetadataDiagnostics final
{
  public:
    MetadataDiagnostics() : previousHandler_(Exiv2::LogMsg::handler()), previousLevel_(Exiv2::LogMsg::level())
    {
        nativeDiagnosticObserved = false;
        Exiv2::LogMsg::setHandler([](int level, const char *) noexcept {
            if (level >= Exiv2::LogMsg::warn)
                nativeDiagnosticObserved = true;
        });
        Exiv2::LogMsg::setLevel(Exiv2::LogMsg::warn);
    }
    ~MetadataDiagnostics()
    {
        Exiv2::LogMsg::setHandler(previousHandler_);
        Exiv2::LogMsg::setLevel(previousLevel_);
    }
    MetadataDiagnostics(const MetadataDiagnostics &) = delete;
    MetadataDiagnostics &operator=(const MetadataDiagnostics &) = delete;

  private:
    Exiv2::LogMsg::Handler previousHandler_;
    Exiv2::LogMsg::Level previousLevel_;
};

OrientationResult fail(ImageProcessingErrorCode code)
{
    return OrientationResult::failure({code, ImageProcessingStage::MetadataAnalysis});
}

// XMP Integer is not Exiv2's permissive numeric conversion (which can round a
// float or coerce a Boolean). Parse the already-decoded scalar, never its XML.
std::optional<ExifOrientation> parseXmpOrientation(const std::string &text)
{
    std::string_view scalar{text};
    const auto first = scalar.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos)
        return std::nullopt;
    scalar = scalar.substr(first, scalar.find_last_not_of(" \t\r\n") - first + 1);
    if (scalar.starts_with('+'))
        scalar.remove_prefix(1);
    unsigned int value{};
    const auto converted = std::from_chars(scalar.data(), scalar.data() + scalar.size(), value);
    if (converted.ec != std::errc{} || converted.ptr != scalar.data() + scalar.size() || value > 8)
        return std::nullopt;
    return tryParseExifOrientation(static_cast<std::uint16_t>(value));
}

// The public parser has already interpreted TIFF. Compare the complete exposed
// metadata multiset, including opaque value/data-area bytes, not display strings.
// Only the three standard IFD linkage offsets may move during serialization;
// their target facts are compared individually. Unknown fields are not exempt.
auto exifPreservationSnapshot(const Exiv2::ExifData &exif)
{
    using Entry =
        std::tuple<std::string, Exiv2::TypeId, std::size_t, std::vector<Exiv2::byte>, std::vector<Exiv2::byte>>;
    std::vector<Entry> entries;
    for (const auto &entry : exif)
    {
        const auto key = entry.key();
        if (key == "Exif.Image.ExifTag" || key == "Exif.Image.GPSTag" || key == "Exif.Photo.InteroperabilityTag")
            continue;
        std::vector<Exiv2::byte> value(entry.size());
        if (!value.empty())
            entry.copy(value.data(), Exiv2::littleEndian);
        const auto area = entry.dataArea();
        std::vector<Exiv2::byte> dataArea;
        if (area.size() != 0)
            dataArea.assign(area.c_data(), area.c_data() + area.size());
        entries.emplace_back(key, entry.typeId(), entry.count(), std::move(value), std::move(dataArea));
    }
    std::ranges::sort(entries);
    return entries;
}

// Preserve array order, language keys, value types and structure flags. A single
// display string is not a semantic snapshot of an RDF container. Keys include
// the native parser's paths for structured members and qualifiers.
auto xmpPreservationSnapshot(const Exiv2::XmpData &xmp)
{
    using Values = std::vector<std::pair<std::string, std::string>>;
    using Entry =
        std::tuple<std::string, Exiv2::TypeId, Exiv2::XmpValue::XmpArrayType, Exiv2::XmpValue::XmpStruct, Values>;
    std::vector<Entry> entries;
    for (const auto &entry : xmp)
    {
        const auto &value = dynamic_cast<const Exiv2::XmpValue &>(entry.value());
        Values values;
        if (const auto *languages = dynamic_cast<const Exiv2::LangAltValue *>(&value))
            for (const auto &[language, text] : languages->value_)
                values.emplace_back(language, text);
        else
            for (std::size_t index = 0; index < entry.count(); ++index)
                values.emplace_back(std::to_string(index), entry.toString(index));
        entries.emplace_back(entry.key(), entry.typeId(), value.xmpArrayType(), value.xmpStruct(), std::move(values));
    }
    std::ranges::sort(entries);
    return entries;
}

bool hasIdentifier(std::span<const std::byte> payload, std::string_view identifier)
{
    return payload.size() >= identifier.size() &&
           std::equal(identifier.begin(), identifier.end(), payload.begin(), [](char text, std::byte byte) {
               return static_cast<unsigned char>(text) == std::to_integer<unsigned char>(byte);
           });
}

std::optional<std::uint64_t> xmpUnsignedInteger(const Exiv2::Xmpdatum *entry)
{
    if (!entry || entry->typeId() != Exiv2::xmpText)
        return std::nullopt;
    const auto *value = dynamic_cast<const Exiv2::XmpValue *>(&entry->value());
    if (!value || value->xmpStruct() != Exiv2::XmpValue::xsNone || value->xmpArrayType() != Exiv2::XmpValue::xaNone)
        return std::nullopt;
    const auto text = entry->toString();
    std::string_view scalar{text};
    const auto first = scalar.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos)
        return std::nullopt;
    scalar = scalar.substr(first, scalar.find_last_not_of(" \t\r\n") - first + 1);
    if (scalar.starts_with('+'))
        scalar.remove_prefix(1);
    std::uint64_t number{};
    const auto parsed = std::from_chars(scalar.data(), scalar.data() + scalar.size(), number);
    if (parsed.ec != std::errc{} || parsed.ptr != scalar.data() + scalar.size())
        return std::nullopt;
    return number;
}

bool describesMotionPhotoContainer(const Exiv2::XmpData &xmp, std::uint64_t trailingLength)
{
    // Namespace identity comes from the native registry, so alternate producer
    // prefixes work. A dictionary indexes native RDF paths; it does not parse XML.
    const auto camera = Exiv2::XmpProperties::prefix("http://ns.google.com/photos/1.0/camera/");
    const auto container = Exiv2::XmpProperties::prefix("http://ns.google.com/photos/1.0/container/");
    if (camera.empty() || container.empty() || trailingLength == 0)
        return false;
    // Nested-only schemas do not create a top-level Exiv2 registry entry.
    // Query its public combined toolkit registry for those RDF path prefixes.
    Exiv2::Dictionary namespaces;
    Exiv2::XmpProperties::registeredNamespaces(namespaces);
    const auto itemNamespace = std::ranges::find_if(namespaces, [](const auto &entry) {
        return entry.second == "http://ns.google.com/photos/1.0/container/item/";
    });
    const auto item = itemNamespace == namespaces.end() ? std::string{} : itemNamespace->first;
    if (camera.empty() || container.empty() || item.empty() || trailingLength == 0)
        return false;
    std::map<std::string, const Exiv2::Xmpdatum *> properties;
    for (const auto &entry : xmp)
        if (!properties.emplace(entry.key(), &entry).second)
            return false;
    const auto lookup = [&](const std::string &key) -> const Exiv2::Xmpdatum * {
        const auto found = properties.find(key);
        return found == properties.end() ? nullptr : found->second;
    };
    const auto scalar = [&](const std::string &key) {
        const auto *entry = lookup(key);
        return entry && entry->typeId() == Exiv2::xmpText ? entry->toString() : std::string{};
    };
    if (xmpUnsignedInteger(lookup("Xmp." + camera + ".MotionPhoto")) != 1 ||
        xmpUnsignedInteger(lookup("Xmp." + camera + ".MotionPhotoVersion")) != 1)
        return false;
    const auto directory = "Xmp." + container + ".Directory";
    const auto *root = lookup(directory);
    const auto *rootValue = root ? dynamic_cast<const Exiv2::XmpValue *>(&root->value()) : nullptr;
    if (!rootValue || rootValue->xmpArrayType() != Exiv2::XmpValue::xaSeq)
        return false;
    std::uint64_t accounted = 0;
    bool motionPhotoObserved = false;
    std::size_t itemCount = 0;
    for (std::size_t index = 1; index <= properties.size(); ++index)
    {
        const auto path = directory + "[" + std::to_string(index) + "]/" + container + ":Item/" + item + ":";
        const auto mime = scalar(path + "Mime");
        if (mime.empty())
            break;
        if (motionPhotoObserved)
            return false; // MotionPhoto must be the last resource in the file.
        ++itemCount;
        const auto semantic = scalar(path + "Semantic");
        if (index == 1)
        {
            if (mime != "image/jpeg" || semantic != "Primary")
                return false;
            if (const auto *length = lookup(path + "Length"); length && xmpUnsignedInteger(length) != 0)
                return false;
            if (const auto *padding = lookup(path + "Padding"))
            {
                const auto bytes = xmpUnsignedInteger(padding);
                if (!bytes || *bytes == 0 || *bytes > trailingLength)
                    return false;
                accounted = *bytes;
            }
        }
        else
        {
            if (semantic.empty() || semantic == "Primary" || lookup(path + "Padding"))
                return false;
            const auto bytes = xmpUnsignedInteger(lookup(path + "Length"));
            if (!bytes || *bytes > trailingLength - accounted)
                return false;
            accounted += *bytes;
            if (semantic == "MotionPhoto")
            {
                if (*bytes == 0 || (mime != "video/mp4" && mime != "video/quicktime"))
                    return false;
                motionPhotoObserved = true;
            }
        }
    }
    // This classifies a declared unsupported container; it neither decodes nor
    // attests to the video. Every appended byte must still be accounted for.
    return itemCount >= 2 && motionPhotoObserved && accounted == trailingLength;
}

ImageProcessingResult<ReconciledMetadataBytes> reconcilePhotoshopResources(std::span<const std::byte> payload,
                                                                           bool transformed)
{
    using Result = ImageProcessingResult<ReconciledMetadataBytes>;
    const auto reject = [] {
        return Result::failure({ImageProcessingErrorCode::UnsupportedEmbeddedPreviewMetadata,
                                ImageProcessingStage::MetadataReconciliation});
    };
    const std::lock_guard lock{metadataLibraryMutex};
    const MetadataDiagnostics diagnostics;
    try
    {
        std::vector<std::byte> output{payload.begin(), payload.begin() + 14};
        bool removedThumbnail = false;
        auto remaining = payload.subspan(14);
        while (!remaining.empty())
        {
            // Public locateIrb supplies record/header/data boundaries. Its
            // legacy byte-sized Pascal-name calculation cannot represent 254/
            // 255-byte names; reject those before calling it, without creating
            // a competing resource parser or silently discarding a container.
            if (remaining.size() < 12 || std::to_integer<unsigned>(remaining[6]) >= 254)
                return reject();
            const auto *begin = reinterpret_cast<const Exiv2::byte *>(remaining.data());
            const auto resourceId = Exiv2::getUShort(begin + 4, Exiv2::bigEndian);
            const Exiv2::byte *record = nullptr;
            std::uint32_t headerLength = 0, dataLength = 0;
            if (Exiv2::Photoshop::locateIrb(begin, remaining.size(), resourceId, &record, headerLength, dataLength) !=
                    0 ||
                record != begin || nativeDiagnosticObserved)
                return reject();
            const auto recordLength = static_cast<std::uint64_t>(headerLength) + dataLength + (dataLength & 1U);
            if (recordLength > remaining.size() || recordLength < 12)
                return reject();
            if (!transformed || (resourceId != 1033 && resourceId != 1036))
                output.insert(output.end(), remaining.begin(),
                              remaining.begin() + static_cast<std::ptrdiff_t>(recordLength));
            else
                removedThumbnail = true;
            remaining = remaining.subspan(static_cast<std::size_t>(recordLength));
        }
        return Result::success({std::move(output), removedThumbnail});
    }
    catch (const Exiv2::Error &)
    {
        return reject();
    }
}
} // namespace

ImageProcessingResult<std::monostate> MetadataReconciler::validateSourceAssetBindings(
    std::span<const std::byte> source, const JpegMarkerInventory &inventory)
{
    using Result = ImageProcessingResult<std::monostate>;
    const auto reject = [](ImageProcessingErrorCode code) {
        return Result::failure({code, ImageProcessingStage::MetadataAnalysis});
    };
    const auto extended = validateExtendedXmpPreservation(source, inventory, LosslessTransform::None);
    if (const auto *error = extended.errorIfPresent())
        return Result::failure(*error);
    const std::lock_guard lock{metadataLibraryMutex};
    const MetadataDiagnostics diagnostics;
    try
    {
        std::vector<std::string> packets;
        for (const auto &range : inventory.standardXmpPacketRanges())
            packets.emplace_back(reinterpret_cast<const char *>(source.data() + range.offsetBytes),
                                 static_cast<std::size_t>(range.lengthBytes));
        if (!inventory.extendedXmpChunks().empty())
        {
            std::string extension;
            for (const auto &chunk : inventory.extendedXmpChunks())
                extension.append(reinterpret_cast<const char *>(source.data() + chunk.packetDataRange.offsetBytes),
                                 static_cast<std::size_t>(chunk.packetDataRange.lengthBytes));
            packets.push_back(std::move(extension));
        }
        bool declaredMotionPhoto = false;
        for (const auto &packet : packets)
        {
            Exiv2::XmpData xmp;
            if (Exiv2::XmpParser::decode(xmp, packet) != 0 || nativeDiagnosticObserved)
                return reject(ImageProcessingErrorCode::MalformedImageMetadata);
            for (const auto &entry : xmp)
                if (entry.tagName() == "provenance" &&
                    Exiv2::XmpProperties::ns(entry.groupName()) == "http://purl.org/dc/terms/")
                    // Changing coefficients invalidates asset bindings. We own
                    // no signing identity and must not drop or fetch a manifest.
                    return reject(ImageProcessingErrorCode::ContentCredentialsWouldBeInvalidated);
            declaredMotionPhoto |= describesMotionPhotoContainer(xmp, inventory.trailingDataRange().lengthBytes);
        }
        if (declaredMotionPhoto)
            return reject(ImageProcessingErrorCode::MotionPhotoNotSupported);
        if (inventory.trailingDataRange().lengthBytes != 0)
            return reject(ImageProcessingErrorCode::UnsupportedTrailingPayload);
        return Result::success({});
    }
    catch (const Exiv2::Error &)
    {
        return reject(ImageProcessingErrorCode::MalformedImageMetadata);
    }
}

ImageProcessingResult<ReconciledMetadataBytes> MetadataReconciler::reconcileMarkerSegments(
    std::span<const std::byte> source, const JpegMarkerInventory &inventory, const JpegTransformPlan &plan,
    const JpegResourceLimits &resourceLimits)
{
    using Result = ImageProcessingResult<ReconciledMetadataBytes>;
    const auto unsupportedPreview = [] {
        return Result::failure({ImageProcessingErrorCode::UnsupportedEmbeddedPreviewMetadata,
                                ImageProcessingStage::MetadataReconciliation});
    };
    const auto extended = validateExtendedXmpPreservation(source, inventory, plan.transform);
    if (const auto *error = extended.errorIfPresent())
        return Result::failure(*error);
    const bool transformed = plan.transform != LosslessTransform::None;
    std::vector<std::byte> output;
    bool removedThumbnail = false;
    for (const auto &marker : inventory.markers())
        if ((marker.markerCode >= 0xe0 && marker.markerCode <= 0xef) || marker.markerCode == 0xfe)
        {
            const auto payload = source.subspan(static_cast<std::size_t>(marker.payloadRange.offsetBytes),
                                                static_cast<std::size_t>(marker.payloadRange.lengthBytes));
            std::vector<std::byte> replacement;
            bool changed = false;
            if (marker.markerCode == 0xe1 && hasIdentifier(payload, {"Exif\0\0", 6}))
            {
                const auto reconciled = reconcileExifPayload(payload.subspan(6), plan);
                if (const auto *error = reconciled.errorIfPresent())
                    return Result::failure(*error);
                replacement.assign(payload.begin(), payload.begin() + 6);
                const auto &bytes = reconciled.valueIfPresent()->encodedBytes;
                replacement.insert(replacement.end(), bytes.begin(), bytes.end());
                removedThumbnail |= reconciled.valueIfPresent()->removedEmbeddedThumbnail;
                changed = true;
            }
            else if (marker.markerCode == 0xe1 && hasIdentifier(payload, {"http://ns.adobe.com/xap/1.0/\0", 29}))
            {
                const auto reconciled = reconcileStandardXmpPacket(payload.subspan(29), plan);
                if (const auto *error = reconciled.errorIfPresent())
                    return Result::failure(*error);
                replacement.assign(payload.begin(), payload.begin() + 29);
                const auto &bytes = reconciled.valueIfPresent()->encodedBytes;
                replacement.insert(replacement.end(), bytes.begin(), bytes.end());
                removedThumbnail |= reconciled.valueIfPresent()->removedEmbeddedThumbnail;
                changed = true;
            }
            else if (marker.markerCode == 0xe0 && hasIdentifier(payload, {"JFIF\0", 5}))
            {
                // JFIF 1.02 section "File Interchange Format Specification":
                // the only variable tail is exactly 3 * Xthumbnail * Ythumbnail.
                if (payload.size() < 14 || payload.size() != 14U + 3U * std::to_integer<unsigned>(payload[12]) *
                                                                       std::to_integer<unsigned>(payload[13]))
                    return unsupportedPreview();
                if (transformed)
                {
                    replacement.assign(payload.begin(), payload.begin() + 14);
                    removedThumbnail |= payload[12] != std::byte{0} && payload[13] != std::byte{0};
                    replacement[12] = replacement[13] = std::byte{0};
                    changed = true;
                }
            }
            else if (marker.markerCode == 0xe0 && hasIdentifier(payload, {"JFXX\0", 5}))
            {
                if (payload.size() < 6)
                    return unsupportedPreview();
                const auto format = std::to_integer<unsigned>(payload[5]);
                if (format == 0x10)
                {
                    const auto thumbnail = JpegSegmentScanner::scan(payload.subspan(6));
                    if (!thumbnail.valueIfPresent() || thumbnail.valueIfPresent()->trailingDataRange().lengthBytes != 0)
                        return unsupportedPreview();
                    // JFIF 1.02's JPEG-coded extension explicitly forbids JFIF
                    // and JFXX inside the thumbnail. Reuse scanner boundaries;
                    // do not recursively interpret nested preview containers.
                    for (const auto &nested : thumbnail.valueIfPresent()->markers())
                        if (nested.markerCode == 0xe0)
                        {
                            const auto nestedPayload =
                                payload.subspan(6).subspan(static_cast<std::size_t>(nested.payloadRange.offsetBytes),
                                                           static_cast<std::size_t>(nested.payloadRange.lengthBytes));
                            if (hasIdentifier(nestedPayload, {"JFIF\0", 5}) ||
                                hasIdentifier(nestedPayload, {"JFXX\0", 5}))
                                return unsupportedPreview();
                        }
                }
                else if (format == 0x11 || format == 0x13)
                {
                    if (payload.size() < 8)
                        return unsupportedPreview();
                    const auto pixels = std::to_integer<unsigned>(payload[6]) * std::to_integer<unsigned>(payload[7]);
                    const auto expectedLength = format == 0x11 ? 8U + 768U + pixels : 8U + 3U * pixels;
                    if (pixels == 0 || payload.size() != expectedLength)
                        return unsupportedPreview();
                }
                else
                    return unsupportedPreview();
                if (transformed)
                {
                    removedThumbnail = true;
                    continue;
                }
            }
            else if (marker.markerCode == 0xed && hasIdentifier(payload, {"Photoshop 3.0\0", 14}))
            {
                const auto reconciled = reconcilePhotoshopResources(payload, transformed);
                if (const auto *error = reconciled.errorIfPresent())
                    return Result::failure(*error);
                replacement = reconciled.valueIfPresent()->encodedBytes;
                removedThumbnail |= reconciled.valueIfPresent()->removedEmbeddedThumbnail;
                changed = true;
            }
            // Serialization can grow a packet that satisfied the source budget.
            // Bound the aggregate before extending the output allocation, not
            // just each APP segment independently.
            const std::uint64_t appendedLength = changed ? replacement.size() + 4 : marker.encodedRange.lengthBytes;
            const std::uint64_t outputLength = output.size() + appendedLength;
            if (outputLength > resourceLimits.maximumMetadataLengthBytes)
                return Result::failure({ImageProcessingErrorCode::MetadataLengthLimitExceeded,
                                        ImageProcessingStage::MetadataReconciliation,
                                        {},
                                        JpegResourceLimitViolation{JpegResourceLimit::MetadataLengthBytes, outputLength,
                                                                   resourceLimits.maximumMetadataLengthBytes}});
            if (changed)
            {
                const auto length = replacement.size() + 2;
                if (length > 65535)
                    return Result::failure({ImageProcessingErrorCode::MetadataPreservationFailed,
                                            ImageProcessingStage::MetadataReconciliation});
                output.insert(output.end(),
                              {std::byte{0xff}, static_cast<std::byte>(marker.markerCode),
                               static_cast<std::byte>(length >> 8), static_cast<std::byte>(length & 255)});
                output.insert(output.end(), replacement.begin(), replacement.end());
            }
            else
            {
                // ICC chunks and unknown APPn/COM are already legally framed by
                // the scanner. Keeping entire segments preserves both logical
                // ICC bytes and source-relative ordering without needless work.
                const auto bytes = source.subspan(static_cast<std::size_t>(marker.encodedRange.offsetBytes),
                                                  static_cast<std::size_t>(marker.encodedRange.lengthBytes));
                output.insert(output.end(), bytes.begin(), bytes.end());
            }
        }
    return Result::success({std::move(output), removedThumbnail});
}

ImageProcessingResult<std::monostate> MetadataReconciler::validateExtendedXmpPreservation(
    std::span<const std::byte> source, const JpegMarkerInventory &inventory, LosslessTransform transform)
{
    using Result = ImageProcessingResult<std::monostate>;
    const auto reject = [](ImageProcessingErrorCode code) {
        return Result::failure({code, ImageProcessingStage::MetadataAnalysis});
    };
    if (inventory.standardXmpPacketRanges().size() > 1)
        return reject(ImageProcessingErrorCode::MalformedImageMetadata);
    const std::lock_guard lock{metadataLibraryMutex};
    const MetadataDiagnostics diagnostics;
    try
    {
        Exiv2::XmpData standard;
        for (const auto &range : inventory.standardXmpPacketRanges())
        {
            if (!range.isContainedWithin(source.size()))
                return reject(ImageProcessingErrorCode::MalformedImageMetadata);
            const std::string packet{reinterpret_cast<const char *>(source.data() + range.offsetBytes),
                                     static_cast<std::size_t>(range.lengthBytes)};
            if (Exiv2::XmpParser::decode(standard, packet) != 0 || nativeDiagnosticObserved)
                return reject(ImageProcessingErrorCode::MalformedImageMetadata);
        }
        // Unlike TIFF, Exiv2 does not pre-register this namespace. Decode owns
        // registration of the producer's prefix; compare its URI, never assume
        // a particular prefix or construct a key in an unregistered namespace.
        const auto declaration = std::ranges::find_if(standard, [](const auto &entry) {
            return entry.tagName() == "HasExtendedXMP" &&
                   Exiv2::XmpProperties::ns(entry.groupName()) == "http://ns.adobe.com/xmp/note/";
        });
        const auto &chunks = inventory.extendedXmpChunks();
        if (chunks.empty())
            return declaration == standard.end() ? Result::success({})
                                                 : reject(ImageProcessingErrorCode::MalformedImageMetadata);
        const auto &guid = chunks.front().packetGuid.uppercaseMd5HexadecimalCharacters;
        if (declaration == standard.end() || declaration->typeId() != Exiv2::xmpText ||
            declaration->toString() != std::string{guid.begin(), guid.end()})
            return reject(ImageProcessingErrorCode::MalformedImageMetadata);
        // Scanner-proven ranges and logical offsets bound this owned allocation.
        // Never concatenate physical marker order or hand the parser a JPEG.
        std::string packet;
        for (const auto &chunk : chunks)
        {
            if (!chunk.packetDataRange.isContainedWithin(source.size()) || chunk.chunkOffsetBytes != packet.size())
                return reject(ImageProcessingErrorCode::MalformedImageMetadata);
            packet.append(reinterpret_cast<const char *>(source.data() + chunk.packetDataRange.offsetBytes),
                          static_cast<std::size_t>(chunk.packetDataRange.lengthBytes));
        }
        if (packet.size() != chunks.front().completePacketLengthBytes)
            return reject(ImageProcessingErrorCode::MalformedImageMetadata);
        Exiv2::XmpData extension;
        if (Exiv2::XmpParser::decode(extension, packet) != 0 || nativeDiagnosticObserved)
            return reject(ImageProcessingErrorCode::ExtendedXmpMutationNotSupported);
        for (const auto &entry : extension)
        {
            const auto key = entry.key();
            // Overlapping properties have no unambiguous authoritative value.
            if (standard.findKey(Exiv2::XmpKey{key}) != standard.end())
                return reject(ImageProcessingErrorCode::ExtendedXmpMutationNotSupported);
            // Planning reads Exif and standard XMP only. An extension-held
            // orientation cannot justify the provisional identity plan: it may
            // require a transform and extension serialization we do not support.
            // Reject before using that provisional plan as preservation proof.
            if (key == "Xmp.tiff.Orientation")
                return reject(ImageProcessingErrorCode::ExtendedXmpMutationNotSupported);
            if (transform != LosslessTransform::None &&
                (key == "Xmp.tiff.ImageWidth" || key == "Xmp.tiff.ImageLength" || key == "Xmp.exif.PixelXDimension" ||
                 key == "Xmp.exif.PixelYDimension" || key == "Xmp.xmp.Thumbnails" ||
                 key.starts_with("Xmp.xmp.Thumbnails[") || key.starts_with("Xmp.xmp.Thumbnails/")))
                return reject(ImageProcessingErrorCode::ExtendedXmpMutationNotSupported);
        }
        return Result::success({});
    }
    catch (const Exiv2::Error &)
    {
        return reject(ImageProcessingErrorCode::ExtendedXmpMutationNotSupported);
    }
}

ImageProcessingResult<ReconciledMetadataBytes> MetadataReconciler::reconcileStandardXmpPacket(
    std::span<const std::byte> packet, const JpegTransformPlan &plan)
{
    using PayloadResult = ImageProcessingResult<ReconciledMetadataBytes>;
    const auto reject = [](ImageProcessingErrorCode code) {
        return PayloadResult::failure({code, ImageProcessingStage::MetadataReconciliation});
    };
    // The APP1 length includes two length bytes and the 29-byte XMP identifier.
    // Extended XMP has a different preservation policy; never synthesize it here.
    if (packet.empty() || packet.size() > 65504)
        return reject(ImageProcessingErrorCode::MalformedImageMetadata);
    const std::lock_guard lock{metadataLibraryMutex};
    const MetadataDiagnostics diagnostics;
    try
    {
        const std::string ownedPacket{reinterpret_cast<const char *>(packet.data()), packet.size()};
        Exiv2::XmpData xmp;
        if (Exiv2::XmpParser::decode(xmp, ownedPacket) != 0 || nativeDiagnosticObserved)
            return reject(ImageProcessingErrorCode::MalformedImageMetadata);
        bool requiresMetadataRewrite = plan.transform != LosslessTransform::None;
        for (auto &entry : xmp)
        {
            const auto key = entry.key();
            // TIFF/Exif dimensions are non-negative integer properties. Do not
            // overwrite malformed source values and thereby hide invalid input.
            const bool width = key == "Xmp.tiff.ImageWidth" || key == "Xmp.exif.PixelXDimension";
            const bool height = key == "Xmp.tiff.ImageLength" || key == "Xmp.exif.PixelYDimension";
            if (width || height)
            {
                const auto pixels = xmpUnsignedInteger(&entry);
                if (!pixels)
                    return reject(ImageProcessingErrorCode::MalformedImageMetadata);
                requiresMetadataRewrite |=
                    *pixels != (width ? plan.outputDimensions.width.pixels : plan.outputDimensions.height.pixels);
            }
            if (key == "Xmp.tiff.Orientation")
            {
                const auto *value = dynamic_cast<const Exiv2::XmpValue *>(&entry.value());
                if (entry.typeId() != Exiv2::xmpText || !value || value->xmpStruct() != Exiv2::XmpValue::xsNone ||
                    value->xmpArrayType() != Exiv2::XmpValue::xaNone || !parseXmpOrientation(entry.toString()))
                    return reject(ImageProcessingErrorCode::InvalidOrientationMetadata);
                if (plan.transform != LosslessTransform::None ||
                    parseXmpOrientation(entry.toString()) != ExifOrientation::TopLeft)
                {
                    requiresMetadataRewrite = true;
                    entry = "1";
                }
            }
        }
        // Identity describes the pixels, not a guarantee that all derived
        // metadata already agrees. Avoid rewriting an already-correct packet.
        if (!requiresMetadataRewrite)
            return PayloadResult::success({{packet.begin(), packet.end()}, false});
        if (plan.outputDimensions.width.pixels == 0 || plan.outputDimensions.width.pixels > 65535 ||
            plan.outputDimensions.height.pixels == 0 || plan.outputDimensions.height.pixels > 65535)
            return reject(ImageProcessingErrorCode::OutputValidationFailed);
        bool removedThumbnail = false;
        for (auto entry = xmp.begin(); entry != xmp.end();)
        {
            const auto key = entry->key();
            // Remove the complete native property family, not unrelated keys
            // that happen to start with "Thumbnails". This is an RDF path, not XML.
            if (plan.transform != LosslessTransform::None &&
                (key == "Xmp.xmp.Thumbnails" || key.starts_with("Xmp.xmp.Thumbnails[") ||
                 key.starts_with("Xmp.xmp.Thumbnails/")))
            {
                entry = xmp.erase(entry);
                removedThumbnail = true;
                continue;
            }
            const bool width = key == "Xmp.tiff.ImageWidth" || key == "Xmp.exif.PixelXDimension";
            const bool height = key == "Xmp.tiff.ImageLength" || key == "Xmp.exif.PixelYDimension";
            if (width || height)
            {
                const auto *value = dynamic_cast<const Exiv2::XmpValue *>(&entry->value());
                if (entry->typeId() != Exiv2::xmpText || !value || value->xmpStruct() != Exiv2::XmpValue::xsNone ||
                    value->xmpArrayType() != Exiv2::XmpValue::xaNone)
                    return reject(ImageProcessingErrorCode::MalformedImageMetadata);
                *entry =
                    std::to_string(width ? plan.outputDimensions.width.pixels : plan.outputDimensions.height.pixels);
            }
            ++entry;
        }
        const auto expectedMetadata = xmpPreservationSnapshot(xmp);
        std::string output;
        if (Exiv2::XmpParser::encode(output, xmp, Exiv2::XmpParser::useCompactFormat) != 0 ||
            nativeDiagnosticObserved || output.empty() || output.size() > 65504)
            return reject(ImageProcessingErrorCode::MetadataPreservationFailed);
        Exiv2::XmpData observedMetadata;
        if (Exiv2::XmpParser::decode(observedMetadata, output) != 0 || nativeDiagnosticObserved ||
            xmpPreservationSnapshot(observedMetadata) != expectedMetadata)
            return reject(ImageProcessingErrorCode::MetadataPreservationFailed);
        const auto *begin = reinterpret_cast<const std::byte *>(output.data());
        return PayloadResult::success({{begin, begin + output.size()}, removedThumbnail});
    }
    catch (const Exiv2::Error &)
    {
        return reject(ImageProcessingErrorCode::MalformedImageMetadata);
    }
}

ImageProcessingResult<ReconciledMetadataBytes> MetadataReconciler::reconcileExifPayload(
    std::span<const std::byte> tiffPayload, const JpegTransformPlan &plan)
{
    using PayloadResult = ImageProcessingResult<ReconciledMetadataBytes>;
    const auto reject = [](ImageProcessingErrorCode code) {
        return PayloadResult::failure({code, ImageProcessingStage::MetadataReconciliation});
    };
    // JPEG APP1's maximum 65535-byte length includes its length field and the
    // six-byte Exif identifier. Bound even this internal payload-only entry.
    if (tiffPayload.empty() || tiffPayload.size() > 65527)
        return reject(ImageProcessingErrorCode::MalformedImageMetadata);
    const std::lock_guard lock{metadataLibraryMutex};
    const MetadataDiagnostics diagnostics;
    try
    {
        std::vector<Exiv2::byte> ownedTiff{
            reinterpret_cast<const Exiv2::byte *>(tiffPayload.data()),
            reinterpret_cast<const Exiv2::byte *>(tiffPayload.data() + tiffPayload.size())};
        Exiv2::ExifData exif;
        const auto byteOrder = Exiv2::ExifParser::decode(exif, ownedTiff.data(), ownedTiff.size());
        if (byteOrder == Exiv2::invalidByteOrder || nativeDiagnosticObserved)
            return reject(ImageProcessingErrorCode::MalformedImageMetadata);
        bool orientationObserved = false;
        bool requiresMetadataRewrite = plan.transform != LosslessTransform::None;
        for (auto &entry : exif)
            if (entry.key() == "Exif.Image.Orientation")
            {
                if (orientationObserved || entry.typeId() != Exiv2::unsignedShort || entry.count() != 1 ||
                    !tryParseExifOrientation(static_cast<std::uint16_t>(entry.toInt64())))
                    return reject(ImageProcessingErrorCode::InvalidOrientationMetadata);
                orientationObserved = true;
                if (entry.toInt64() != 1)
                {
                    requiresMetadataRewrite = true;
                    entry = static_cast<std::uint16_t>(1);
                }
            }
        if (plan.outputDimensions.width.pixels == 0 || plan.outputDimensions.width.pixels > 65535 ||
            plan.outputDimensions.height.pixels == 0 || plan.outputDimensions.height.pixels > 65535)
            return reject(ImageProcessingErrorCode::OutputValidationFailed);
        // CIPA DC-008-2026 4.6.5/4.6.6: preserve each present field's legal
        // SHORT/LONG representation; do not invent unrelated dimension tags.
        for (auto &entry : exif)
        {
            const auto key = entry.key();
            const bool width = key == "Exif.Image.ImageWidth" || key == "Exif.Photo.PixelXDimension";
            const bool height = key == "Exif.Image.ImageLength" || key == "Exif.Photo.PixelYDimension";
            if (width || height)
            {
                if (entry.count() != 1 ||
                    (entry.typeId() != Exiv2::unsignedShort && entry.typeId() != Exiv2::unsignedLong))
                    return reject(ImageProcessingErrorCode::MalformedImageMetadata);
                const auto pixels = width ? plan.outputDimensions.width.pixels : plan.outputDimensions.height.pixels;
                requiresMetadataRewrite |= entry.toInt64() != static_cast<std::int64_t>(pixels);
                if (entry.setValue(std::to_string(pixels)) != 0)
                    return reject(ImageProcessingErrorCode::MalformedImageMetadata);
            }
        }
        // A transformed image cannot truthfully retain an untransformed IFD1.
        // The supported Exiv2 operation removes both its tags and owned bytes.
        if (!requiresMetadataRewrite)
            return PayloadResult::success({{tiffPayload.begin(), tiffPayload.end()}, false});
        const bool removedThumbnail =
            plan.transform != LosslessTransform::None &&
            std::ranges::any_of(exif, [](const auto &entry) { return entry.groupName() == "Thumbnail"; });
        if (plan.transform != LosslessTransform::None)
            Exiv2::ExifThumb{exif}.erase();
        const auto expectedMetadata = exifPreservationSnapshot(exif);
        const bool hasOpaqueMakerNote = exif.findKey(Exiv2::ExifKey{"Exif.Photo.MakerNote"}) != exif.end() &&
                                        exif.findKey(Exiv2::ExifKey{"Exif.MakerNote.Offset"}) == exif.end();
        Exiv2::Blob rebuiltTiff;
        const auto writeMethod =
            Exiv2::ExifParser::encode(rebuiltTiff, ownedTiff.data(), ownedTiff.size(), byteOrder, exif);
        if (nativeDiagnosticObserved)
            return reject(ImageProcessingErrorCode::MetadataPreservationFailed);
        // An unknown MakerNote may contain TIFF-relative offsets. Byte equality
        // does not prove those references survive an intrusive layout rebuild.
        // Exiv2 synthesizes MakerNote.Offset only for an interpreted MakerNote;
        // do not invent a proprietary parser to authorize relocating opaque data.
        if (writeMethod == Exiv2::wmIntrusive && hasOpaqueMakerNote)
            return reject(ImageProcessingErrorCode::MetadataPreservationFailed);
        if (writeMethod == Exiv2::wmIntrusive)
            ownedTiff = std::move(rebuiltTiff);
        if (ownedTiff.empty() || ownedTiff.size() > 65527)
            return reject(ImageProcessingErrorCode::MetadataPreservationFailed);
        // ExifParser::encode explicitly documents lossy fallback. Native success
        // is insufficient: independently decode its bytes and reject any loss.
        Exiv2::ExifData observedMetadata;
        if (Exiv2::ExifParser::decode(observedMetadata, ownedTiff.data(), ownedTiff.size()) != byteOrder ||
            nativeDiagnosticObserved || exifPreservationSnapshot(observedMetadata) != expectedMetadata)
            return reject(ImageProcessingErrorCode::MetadataPreservationFailed);
        const auto *begin = reinterpret_cast<const std::byte *>(ownedTiff.data());
        return PayloadResult::success({{begin, begin + ownedTiff.size()}, removedThumbnail});
    }
    catch (const Exiv2::Error &)
    {
        return reject(ImageProcessingErrorCode::MalformedImageMetadata);
    }
}

ImageProcessingResult<OrientationMetadata> MetadataReconciler::analyzeOrientation(std::span<const std::byte> source,
                                                                                  const JpegMarkerInventory &inventory)
{
    // Exif and standard XMP each define one authoritative payload. Combining
    // separate packets could fabricate a metadata set no producer authored.
    if (inventory.exifTiffDataRanges().size() > 1 || inventory.standardXmpPacketRanges().size() > 1)
        return fail(ImageProcessingErrorCode::MalformedImageMetadata);
    const std::lock_guard lock{metadataLibraryMutex};
    const MetadataDiagnostics diagnostics;
    std::optional<ExifOrientation> exifOrientation;
    std::optional<ExifOrientation> xmpOrientation;
    try
    {
        for (const auto &range : inventory.exifTiffDataRanges())
        {
            if (!range.isContainedWithin(source.size()))
                return fail(ImageProcessingErrorCode::MalformedImageMetadata);
            const auto bytes = source.subspan(static_cast<std::size_t>(range.offsetBytes),
                                              static_cast<std::size_t>(range.lengthBytes));
            // The public Exif parser receives only an owned TIFF payload, never
            // the JPEG container or a path it could reopen outside AppContainer.
            const std::vector<Exiv2::byte> ownedTiff{
                reinterpret_cast<const Exiv2::byte *>(bytes.data()),
                reinterpret_cast<const Exiv2::byte *>(bytes.data() + bytes.size())};
            Exiv2::ExifData exif;
            const auto byteOrder = Exiv2::ExifParser::decode(exif, ownedTiff.data(), ownedTiff.size());
            if (byteOrder == Exiv2::invalidByteOrder || nativeDiagnosticObserved)
                return fail(ImageProcessingErrorCode::MalformedImageMetadata);
            for (const auto &entry : exif)
                if (entry.key() == "Exif.Image.Orientation")
                {
                    if (exifOrientation || entry.typeId() != Exiv2::unsignedShort || entry.count() != 1)
                        return fail(ImageProcessingErrorCode::InvalidOrientationMetadata);
                    exifOrientation = tryParseExifOrientation(static_cast<std::uint16_t>(entry.toInt64()));
                    if (!exifOrientation)
                        return fail(ImageProcessingErrorCode::InvalidOrientationMetadata);
                }
        }
        for (const auto &range : inventory.standardXmpPacketRanges())
        {
            if (!range.isContainedWithin(source.size()))
                return fail(ImageProcessingErrorCode::MalformedImageMetadata);
            const auto bytes = source.subspan(static_cast<std::size_t>(range.offsetBytes),
                                              static_cast<std::size_t>(range.lengthBytes));
            const std::string packet{reinterpret_cast<const char *>(bytes.data()), bytes.size()};
            Exiv2::XmpData xmp;
            if (Exiv2::XmpParser::decode(xmp, packet) != 0 || nativeDiagnosticObserved)
                return fail(ImageProcessingErrorCode::MalformedImageMetadata);
            for (const auto &entry : xmp)
                if (entry.key() == "Xmp.tiff.Orientation")
                {
                    if (xmpOrientation || entry.typeId() != Exiv2::xmpText)
                        return fail(ImageProcessingErrorCode::InvalidOrientationMetadata);
                    xmpOrientation = parseXmpOrientation(entry.toString());
                    if (!xmpOrientation)
                        return fail(ImageProcessingErrorCode::InvalidOrientationMetadata);
                }
        }
        const bool conflict = exifOrientation && xmpOrientation && exifOrientation != xmpOrientation;
        return OrientationResult::success({exifOrientation.value_or(xmpOrientation.value_or(ExifOrientation::TopLeft)),
                                           exifOrientation, xmpOrientation, conflict});
    }
    catch (const Exiv2::Error &)
    {
        return fail(ImageProcessingErrorCode::MalformedImageMetadata);
    }
}
namespace
{
using MetadataValidationResult = ImageProcessingResult<std::monostate>;
MetadataValidationResult rejectMetadata(JpegOutputValidationRule rule)
{
    return MetadataValidationResult::failure(
        {ImageProcessingErrorCode::OutputValidationFailed, ImageProcessingStage::OutputValidation, {}, rule});
}

// These predicates identify the application's permitted semantic delta; native
// Exiv2 still owns all TIFF/RDF decoding, types, links and value representation.
bool isExifWidth(std::string_view key)
{
    return key == "Exif.Image.ImageWidth" || key == "Exif.Photo.PixelXDimension";
}
bool isExifHeight(std::string_view key)
{
    return key == "Exif.Image.ImageLength" || key == "Exif.Photo.PixelYDimension";
}
bool isXmpWidth(std::string_view key)
{
    return key == "Xmp.tiff.ImageWidth" || key == "Xmp.exif.PixelXDimension";
}
bool isXmpHeight(std::string_view key)
{
    return key == "Xmp.tiff.ImageLength" || key == "Xmp.exif.PixelYDimension";
}
bool isXmpThumbnail(std::string_view key)
{
    return key == "Xmp.xmp.Thumbnails" || key.starts_with("Xmp.xmp.Thumbnails[") ||
           key.starts_with("Xmp.xmp.Thumbnails/");
}

MetadataValidationResult comparePhotoshopResources(std::span<const std::byte> source, std::span<const std::byte> output,
                                                   bool &removedThumbnail)
{
    const std::lock_guard lock{metadataLibraryMutex};
    const MetadataDiagnostics diagnostics;
    try
    {
        if (!hasIdentifier(output, {"Photoshop 3.0\0", 14}))
            return rejectMetadata(JpegOutputValidationRule::MetadataPreservation);
        // Inspect native record boundaries in both immutable sequences. Do not
        // call the writer or synthesize its expected serialization: a faulty
        // writer must not be able to authorize its own thumbnail omissions.
        struct Resource final
        {
            std::uint16_t identifier;
            std::span<const std::byte> encodedBytes;
        };
        const auto read = [](std::span<const std::byte> payload) -> std::optional<std::vector<Resource>> {
            std::vector<Resource> resources;
            auto remaining = payload.subspan(14);
            while (!remaining.empty())
            {
                // The public Exiv2 locator has a byte-sized Pascal-name
                // calculation. Reject the two overflowing lengths before
                // entering it; it remains the owner of IRB parsing semantics.
                if (remaining.size() < 12 || std::to_integer<unsigned>(remaining[6]) >= 254)
                    return std::nullopt;
                const auto *begin = reinterpret_cast<const Exiv2::byte *>(remaining.data());
                const auto identifier = Exiv2::getUShort(begin + 4, Exiv2::bigEndian);
                const Exiv2::byte *record = nullptr;
                std::uint32_t headerLength = 0, dataLength = 0;
                if (Exiv2::Photoshop::locateIrb(begin, remaining.size(), identifier, &record, headerLength,
                                                dataLength) != 0 ||
                    record != begin || nativeDiagnosticObserved)
                    return std::nullopt;
                const auto length = static_cast<std::uint64_t>(headerLength) + dataLength + (dataLength & 1U);
                if (length < 12 || length > remaining.size())
                    return std::nullopt;
                resources.push_back({identifier, remaining.first(static_cast<std::size_t>(length))});
                remaining = remaining.subspan(static_cast<std::size_t>(length));
            }
            return resources;
        };
        const auto original = read(source);
        const auto observed = read(output);
        if (!original || !observed)
            return rejectMetadata(JpegOutputValidationRule::MetadataPreservation);
        const auto isThumbnail = [](const Resource &resource) {
            return resource.identifier == 1033 || resource.identifier == 1036;
        };
        if (std::ranges::any_of(*observed, isThumbnail))
            return rejectMetadata(JpegOutputValidationRule::RemovedEmbeddedThumbnails);
        std::size_t index = 0;
        for (const auto &resource : *original)
        {
            if (isThumbnail(resource))
            {
                removedThumbnail = true;
                continue;
            }
            if (index == observed->size() ||
                !std::ranges::equal(resource.encodedBytes, (*observed)[index++].encodedBytes))
                return rejectMetadata(JpegOutputValidationRule::MetadataPreservation);
        }
        return index == observed->size() ? MetadataValidationResult::success({})
                                         : rejectMetadata(JpegOutputValidationRule::MetadataPreservation);
    }
    catch (const Exiv2::Error &)
    {
        return rejectMetadata(JpegOutputValidationRule::MetadataPreservation);
    }
}

MetadataValidationResult compareExifMetadata(std::span<const std::byte> source, std::span<const std::byte> output,
                                             const JpegTransformPlan &plan, bool &removedThumbnail)
{
    const std::lock_guard lock{metadataLibraryMutex};
    const MetadataDiagnostics diagnostics;
    try
    {
        Exiv2::ExifData original, observed;
        if (Exiv2::ExifParser::decode(original, reinterpret_cast<const Exiv2::byte *>(source.data()), source.size()) ==
                Exiv2::invalidByteOrder ||
            Exiv2::ExifParser::decode(observed, reinterpret_cast<const Exiv2::byte *>(output.data()), output.size()) ==
                Exiv2::invalidByteOrder ||
            nativeDiagnosticObserved)
            return rejectMetadata(JpegOutputValidationRule::MetadataPreservation);
        for (const auto &entry : observed)
            if (entry.key() == "Exif.Image.Orientation" &&
                (entry.typeId() != Exiv2::unsignedShort || entry.count() != 1 || entry.toInt64() != 1))
                return rejectMetadata(JpegOutputValidationRule::CanonicalExifOrientation);
        {
            // Every present derived field must describe the validated pixels,
            // including identity transforms. Only non-identity transforms may
            // discard thumbnails; identity must preserve their native facts.
            for (const auto key : {"Exif.Image.Orientation", "Exif.Image.ImageWidth", "Exif.Image.ImageLength",
                                   "Exif.Photo.PixelXDimension", "Exif.Photo.PixelYDimension"})
            {
                const auto originalCount =
                    std::ranges::count_if(original, [=](const auto &entry) { return entry.key() == key; });
                const auto observedCount =
                    std::ranges::count_if(observed, [=](const auto &entry) { return entry.key() == key; });
                if (originalCount != observedCount || originalCount > 1)
                    return rejectMetadata(key == std::string_view{"Exif.Image.Orientation"}
                                              ? JpegOutputValidationRule::CanonicalExifOrientation
                                              : JpegOutputValidationRule::DerivedDimensions);
            }
            for (const auto &entry : observed)
            {
                const auto key = entry.key();
                if (key == "Exif.Image.Orientation" &&
                    (entry.typeId() != Exiv2::unsignedShort || entry.count() != 1 || entry.toInt64() != 1))
                    return rejectMetadata(JpegOutputValidationRule::CanonicalExifOrientation);
                if (isExifWidth(key) || isExifHeight(key))
                {
                    const auto originalEntry = original.findKey(Exiv2::ExifKey{key});
                    const auto expected =
                        isExifWidth(key) ? plan.outputDimensions.width.pixels : plan.outputDimensions.height.pixels;
                    if (entry.count() != 1 || originalEntry == original.end() ||
                        originalEntry->typeId() != entry.typeId() ||
                        (entry.typeId() != Exiv2::unsignedShort && entry.typeId() != Exiv2::unsignedLong) ||
                        entry.toInt64() != static_cast<std::int64_t>(expected))
                        return rejectMetadata(JpegOutputValidationRule::DerivedDimensions);
                }
                if (plan.transform != LosslessTransform::None && entry.groupName() == "Thumbnail")
                    return rejectMetadata(JpegOutputValidationRule::RemovedEmbeddedThumbnails);
            }
            removedThumbnail |=
                plan.transform != LosslessTransform::None &&
                std::ranges::any_of(original, [](const auto &entry) { return entry.groupName() == "Thumbnail"; });
            // Compare all other native facts after removing only explicitly
            // permitted fields. No writer-derived expected bytes are involved.
            for (auto *metadata : {&original, &observed})
                for (auto entry = metadata->begin(); entry != metadata->end();)
                    if (entry->key() == "Exif.Image.Orientation" || isExifWidth(entry->key()) ||
                        isExifHeight(entry->key()) ||
                        (plan.transform != LosslessTransform::None && entry->groupName() == "Thumbnail"))
                        entry = metadata->erase(entry);
                    else
                        ++entry;
        }
        if (exifPreservationSnapshot(original) != exifPreservationSnapshot(observed))
            return rejectMetadata(JpegOutputValidationRule::MetadataPreservation);
        return MetadataValidationResult::success({});
    }
    catch (const Exiv2::Error &)
    {
        return rejectMetadata(JpegOutputValidationRule::MetadataPreservation);
    }
}

MetadataValidationResult compareXmpMetadata(std::span<const std::byte> source, std::span<const std::byte> output,
                                            const JpegTransformPlan &plan, bool &removedThumbnail)
{
    const std::lock_guard lock{metadataLibraryMutex};
    const MetadataDiagnostics diagnostics;
    try
    {
        Exiv2::XmpData original, observed;
        if (Exiv2::XmpParser::decode(original, {reinterpret_cast<const char *>(source.data()), source.size()}) != 0 ||
            Exiv2::XmpParser::decode(observed, {reinterpret_cast<const char *>(output.data()), output.size()}) != 0 ||
            nativeDiagnosticObserved)
            return rejectMetadata(JpegOutputValidationRule::MetadataPreservation);
        for (const auto &entry : observed)
            if (entry.key() == "Xmp.tiff.Orientation" && xmpUnsignedInteger(&entry) != 1)
                return rejectMetadata(JpegOutputValidationRule::CanonicalStandardXmpOrientation);
        {
            for (const auto key : {"Xmp.tiff.Orientation", "Xmp.tiff.ImageWidth", "Xmp.tiff.ImageLength",
                                   "Xmp.exif.PixelXDimension", "Xmp.exif.PixelYDimension"})
            {
                const auto originalCount =
                    std::ranges::count_if(original, [=](const auto &entry) { return entry.key() == key; });
                const auto observedCount =
                    std::ranges::count_if(observed, [=](const auto &entry) { return entry.key() == key; });
                if (originalCount != observedCount || originalCount > 1)
                    return rejectMetadata(key == std::string_view{"Xmp.tiff.Orientation"}
                                              ? JpegOutputValidationRule::CanonicalStandardXmpOrientation
                                              : JpegOutputValidationRule::DerivedDimensions);
            }
            for (const auto &entry : observed)
            {
                const auto key = entry.key();
                if (key == "Xmp.tiff.Orientation" && xmpUnsignedInteger(&entry) != 1)
                    return rejectMetadata(JpegOutputValidationRule::CanonicalStandardXmpOrientation);
                if ((isXmpWidth(key) || isXmpHeight(key)) &&
                    xmpUnsignedInteger(&entry) !=
                        (isXmpWidth(key) ? plan.outputDimensions.width.pixels : plan.outputDimensions.height.pixels))
                    return rejectMetadata(JpegOutputValidationRule::DerivedDimensions);
                if (plan.transform != LosslessTransform::None && isXmpThumbnail(key))
                    return rejectMetadata(JpegOutputValidationRule::RemovedEmbeddedThumbnails);
            }
            removedThumbnail |=
                plan.transform != LosslessTransform::None &&
                std::ranges::any_of(original, [](const auto &entry) { return isXmpThumbnail(entry.key()); });
            for (auto *metadata : {&original, &observed})
                for (auto entry = metadata->begin(); entry != metadata->end();)
                    if (entry->key() == "Xmp.tiff.Orientation" || isXmpWidth(entry->key()) ||
                        isXmpHeight(entry->key()) ||
                        (plan.transform != LosslessTransform::None && isXmpThumbnail(entry->key())))
                        entry = metadata->erase(entry);
                    else
                        ++entry;
        }
        if (xmpPreservationSnapshot(original) != xmpPreservationSnapshot(observed))
            return rejectMetadata(JpegOutputValidationRule::MetadataPreservation);
        return MetadataValidationResult::success({});
    }
    catch (const Exiv2::Error &)
    {
        return rejectMetadata(JpegOutputValidationRule::MetadataPreservation);
    }
}

std::vector<JpegMarkerReference> metadataMarkers(const JpegMarkerInventory &inventory)
{
    std::vector<JpegMarkerReference> markers;
    for (const auto &marker : inventory.markers())
        if ((marker.markerCode >= 0xe0 && marker.markerCode <= 0xef) || marker.markerCode == 0xfe)
            markers.push_back(marker);
    return markers;
}
} // namespace

ImageProcessingResult<JpegOutputMetadataEvidence> MetadataReconciler::validateReconciledMetadata(
    std::span<const std::byte> source, const JpegMarkerInventory &sourceInventory, std::span<const std::byte> output,
    const JpegMarkerInventory &outputInventory, const JpegTransformPlan &plan)
{
    using Result = ImageProcessingResult<JpegOutputMetadataEvidence>;
    const auto reject = [](JpegOutputValidationRule rule) {
        return Result::failure(
            {ImageProcessingErrorCode::OutputValidationFailed, ImageProcessingStage::OutputValidation, {}, rule});
    };
    const auto payload = [](std::span<const std::byte> bytes, const JpegMarkerReference &marker) {
        return bytes.subspan(static_cast<std::size_t>(marker.payloadRange.offsetBytes),
                             static_cast<std::size_t>(marker.payloadRange.lengthBytes));
    };
    const auto sourceMarkers = metadataMarkers(sourceInventory);
    const auto outputMarkers = metadataMarkers(outputInventory);
    std::size_t outputIndex = 0;
    bool removedThumbnail = false;
    const bool transformed = plan.transform != LosslessTransform::None;
    for (const auto &sourceMarker : sourceMarkers)
    {
        const auto sourcePayload = payload(source, sourceMarker);
        if (transformed && sourceMarker.markerCode == 0xe0 && hasIdentifier(sourcePayload, {"JFXX\0", 5}))
        {
            removedThumbnail = true;
            continue;
        }
        if (outputIndex == outputMarkers.size())
            return reject(JpegOutputValidationRule::PreservedMarkerSequence);
        const auto &outputMarker = outputMarkers[outputIndex++];
        const auto outputPayload = payload(output, outputMarker);
        if (outputMarker.markerCode != sourceMarker.markerCode)
            return reject(JpegOutputValidationRule::PreservedMarkerSequence);
        if (sourceMarker.markerCode == 0xe1 && hasIdentifier(sourcePayload, {"Exif\0\0", 6}))
        {
            if (!hasIdentifier(outputPayload, {"Exif\0\0", 6}))
                return reject(JpegOutputValidationRule::PreservedMarkerSequence);
            const auto compared =
                compareExifMetadata(sourcePayload.subspan(6), outputPayload.subspan(6), plan, removedThumbnail);
            if (const auto *error = compared.errorIfPresent())
                return Result::failure(*error);
        }
        else if (sourceMarker.markerCode == 0xe1 &&
                 hasIdentifier(sourcePayload, {"http://ns.adobe.com/xap/1.0/\0", 29}))
        {
            if (!hasIdentifier(outputPayload, {"http://ns.adobe.com/xap/1.0/\0", 29}))
                return reject(JpegOutputValidationRule::PreservedMarkerSequence);
            const auto compared =
                compareXmpMetadata(sourcePayload.subspan(29), outputPayload.subspan(29), plan, removedThumbnail);
            if (const auto *error = compared.errorIfPresent())
                return Result::failure(*error);
        }
        else if (transformed && sourceMarker.markerCode == 0xe0 && hasIdentifier(sourcePayload, {"JFIF\0", 5}))
        {
            if (sourcePayload.size() < 14 || outputPayload.size() != 14 || outputPayload[12] != std::byte{0} ||
                outputPayload[13] != std::byte{0})
                return reject(JpegOutputValidationRule::RemovedEmbeddedThumbnails);
            if (!std::equal(sourcePayload.begin(), sourcePayload.begin() + 12, outputPayload.begin()))
                return reject(JpegOutputValidationRule::PreservedMarkerSequence);
            removedThumbnail |= sourcePayload[12] != std::byte{0} && sourcePayload[13] != std::byte{0};
        }
        else if (transformed && sourceMarker.markerCode == 0xed &&
                 hasIdentifier(sourcePayload, {"Photoshop 3.0\0", 14}))
        {
            const auto compared = comparePhotoshopResources(sourcePayload, outputPayload, removedThumbnail);
            if (const auto *error = compared.errorIfPresent())
                return Result::failure(*error);
        }
        else
        {
            const auto original = source.subspan(static_cast<std::size_t>(sourceMarker.encodedRange.offsetBytes),
                                                 static_cast<std::size_t>(sourceMarker.encodedRange.lengthBytes));
            const auto observed = output.subspan(static_cast<std::size_t>(outputMarker.encodedRange.offsetBytes),
                                                 static_cast<std::size_t>(outputMarker.encodedRange.lengthBytes));
            if (!std::ranges::equal(original, observed))
            {
                if (sourceMarker.markerCode == 0xe2 && hasIdentifier(sourcePayload, {"ICC_PROFILE\0", 12}))
                    return reject(JpegOutputValidationRule::IccProfilePreservation);
                if (sourceMarker.markerCode == 0xe1 &&
                    hasIdentifier(sourcePayload, {"http://ns.adobe.com/xmp/extension/\0", 35}))
                    return reject(JpegOutputValidationRule::ExtendedXmpPreservation);
                return reject(JpegOutputValidationRule::PreservedMarkerSequence);
            }
        }
    }
    if (outputIndex != outputMarkers.size())
        return reject(JpegOutputValidationRule::PreservedMarkerSequence);
    const auto orientation = analyzeOrientation(output, outputInventory);
    if (!orientation.valueIfPresent())
        return reject(JpegOutputValidationRule::MetadataPreservation);
    return Result::success({orientation.valueIfPresent()->exifOrientation.has_value(),
                            orientation.valueIfPresent()->xmpOrientation.has_value(), removedThumbnail});
}
} // namespace jpg_spinner::jpeg::internal
