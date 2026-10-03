#include "JpegOutputValidator.h"
#include "JpegSegmentScanner.h"
#include "MetadataReconciler.h"
#include "Sha256DigestCalculator.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <jpeglib.h>
#include <csetjmp>
#include <limits>
#include <memory>

namespace jpg_spinner::jpeg::internal
{
using namespace jpg_spinner::domain;
namespace
{
using Result = ImageProcessingResult<ValidatedJpegOutput>;
Result reject(JpegOutputValidationRule rule)
{
    return Result::failure(
        {ImageProcessingErrorCode::OutputValidationFailed, ImageProcessingStage::OutputValidation, {}, rule});
}
bool isProgressive(JpegCodingProcess process) noexcept
{
    return process == JpegCodingProcess::ProgressiveDctHuffman ||
           process == JpegCodingProcess::ProgressiveDctArithmetic;
}
bool isArithmetic(JpegCodingProcess process) noexcept
{
    return process == JpegCodingProcess::ExtendedSequentialDctArithmetic ||
           process == JpegCodingProcess::ProgressiveDctArithmetic;
}

// All mutable native state lives on the heap: longjmp makes modified automatic
// locals indeterminate. No nontrivial automatic lifetime begins across a call
// that can jump. The enclosing owner also handles C++ allocation failures.
#pragma warning(push)
#pragma warning(disable : 4324)
struct DecodeState final
{
    jpeg_decompress_struct decoder{};
    jpeg_compress_struct criticalParameters{};
    jpeg_error_mgr errors{};
    jpeg_progress_mgr progress{};
    std::jmp_buf failure{};
    bool created{};
    bool criticalParametersCreated{};
    bool cancelled{};
    std::stop_token cancellation;
    std::vector<JSAMPLE> row8;
    std::vector<J12SAMPLE> row12;
    ~DecodeState()
    {
        if (criticalParametersCreated)
            jpeg_destroy_compress(&criticalParameters);
        if (created)
            jpeg_destroy_decompress(&decoder);
    }
};
#pragma warning(pop)

enum class DecodeOutcome
{
    Complete,
    Failed,
    Cancelled
};
struct JpegCriticalCodingParameters final
{
    std::vector<std::array<UINT16, DCTSIZE2>> componentQuantizationTables;
};
std::optional<unsigned int> constantRestartInterval(std::span<const std::byte> bytes,
                                                    const JpegMarkerInventory &inventory)
{
    unsigned int intervalMcus = 0;
    bool scanStarted = false;
    for (const auto &marker : inventory.markers())
    {
        if (marker.markerCode == 0xda)
            scanStarted = true;
        if (marker.markerCode != 0xdd)
            continue;
        // T.81 B.2.4.4's two-byte interval is an application policy field
        // inside a scanner-proven range, not a second marker/entropy parser.
        // Native restart_interval exposes one current value and can hide a
        // prior scan's interval after progressive input has been preloaded.
        if (marker.payloadRange.lengthBytes != 2 || !marker.payloadRange.isContainedWithin(bytes.size()))
            return std::nullopt;
        const auto offset = static_cast<std::size_t>(marker.payloadRange.offsetBytes);
        const auto next =
            (std::to_integer<unsigned int>(bytes[offset]) << 8) | std::to_integer<unsigned int>(bytes[offset + 1]);
        // Before the first SOS, subsequent definitions replace the previous
        // value. Once a scan starts, this adapter supports only a fixed MCU
        // interval; even a later reset cannot erase an incompatible scan.
        if (scanStarted && next != intervalMcus)
            return std::nullopt;
        intervalMcus = next;
    }
    return intervalMcus;
}
DecodeOutcome readJpegCodingParameters(std::span<const std::byte> bytes, std::stop_token cancellation,
                                       const TurboJpegResourceLimits &limits, bool coefficientsOnly,
                                       JpegCriticalCodingParameters &observedParameters)
{
    if (bytes.size() > std::numeric_limits<unsigned long>::max() ||
        limits.maximumIntermediateBufferMemoryMebibytes <= 0 ||
        limits.maximumIntermediateBufferMemoryMebibytes > std::numeric_limits<long>::max() / 1048576)
        return DecodeOutcome::Failed;
    const auto state = std::make_unique<DecodeState>();
    state->cancellation = cancellation;
    state->decoder.err = jpeg_std_error(&state->errors);
    state->decoder.client_data = state.get();
    state->errors.error_exit = [](j_common_ptr codec) {
        std::longjmp(static_cast<DecodeState *>(codec->client_data)->failure, 1);
    };
    // The native reader may recover corrupt entropy with fabricated samples;
    // any warning invalidates approval, even if finish_decompress would succeed.
    state->errors.emit_message = [](j_common_ptr codec, int level) {
        if (level < 0)
            std::longjmp(static_cast<DecodeState *>(codec->client_data)->failure, 1);
    };
    state->progress.progress_monitor = [](j_common_ptr codec) {
        auto *reader = static_cast<DecodeState *>(codec->client_data);
        if (reader->cancellation.stop_requested())
        {
            reader->cancelled = true;
            std::longjmp(reader->failure, 1);
        }
    };
#pragma warning(suppress : 4611)
    if (setjmp(state->failure) != 0)
        return state->cancelled ? DecodeOutcome::Cancelled : DecodeOutcome::Failed;
    state->created = true;
    jpeg_create_decompress(&state->decoder);
    state->decoder.progress = &state->progress;
    state->decoder.mem->max_memory_to_use =
        static_cast<long>(limits.maximumIntermediateBufferMemoryMebibytes) * 1048576;
    jpeg_mem_src(&state->decoder, reinterpret_cast<const unsigned char *>(bytes.data()),
                 static_cast<unsigned long>(bytes.size()));
    if (jpeg_read_header(&state->decoder, TRUE) != JPEG_HEADER_OK ||
        (state->decoder.data_precision != 8 && state->decoder.data_precision != 12))
        return DecodeOutcome::Failed;
    if (coefficientsOnly)
    {
        if (!jpeg_read_coefficients(&state->decoder))
            return DecodeOutcome::Failed;
    }
    else
    {
        // The official manual recommends the null color transform for validation.
        // This covers grayscale/RGB/YCbCr/CMYK/YCCK without changing color semantics.
        state->decoder.out_color_space = state->decoder.jpeg_color_space;
        state->decoder.do_fancy_upsampling = FALSE;
        if (!jpeg_start_decompress(&state->decoder) || state->decoder.output_components < 1 ||
            state->decoder.output_components > 4)
            return DecodeOutcome::Failed;
        const auto components = static_cast<std::size_t>(state->decoder.output_components);
        if (state->decoder.output_width > std::numeric_limits<std::size_t>::max() / components / sizeof(J12SAMPLE))
            return DecodeOutcome::Failed;
        const auto rowSamples = static_cast<std::size_t>(state->decoder.output_width) * components;
        if (state->decoder.data_precision == 12)
            state->row12.resize(rowSamples);
        else
            state->row8.resize(rowSamples);
        while (state->decoder.output_scanline < state->decoder.output_height)
        {
            if (cancellation.stop_requested())
                return DecodeOutcome::Cancelled;
            if (state->decoder.data_precision == 12)
            {
                auto *row = state->row12.data();
                if (jpeg12_read_scanlines(&state->decoder, &row, 1) != 1)
                    return DecodeOutcome::Failed;
            }
            else
            {
                auto *row = state->row8.data();
                if (jpeg_read_scanlines(&state->decoder, &row, 1) != 1)
                    return DecodeOutcome::Failed;
            }
        }
    }
    // Coefficients, or all requested output scanlines, have been read and the
    // reader awaits finish_decompress. Single-scan sample decoding may leave
    // terminal markers for finish; do not treat its current restart_interval
    // as a history of scan policy. The critical-parameter API checks effective
    // component tables without exposing private saved-table internals.
    state->criticalParameters.err = &state->errors;
    state->criticalParameters.client_data = state.get();
    state->criticalParametersCreated = true;
    jpeg_create_compress(&state->criticalParameters);
    jpeg_copy_critical_parameters(&state->decoder, &state->criticalParameters);
    observedParameters.componentQuantizationTables.resize(
        static_cast<std::size_t>(state->criticalParameters.num_components));
    for (int index = 0; index < state->criticalParameters.num_components; ++index)
    {
        const auto selector = state->criticalParameters.comp_info[index].quant_tbl_no;
        if (selector < 0 || selector >= NUM_QUANT_TBLS || !state->criticalParameters.quant_tbl_ptrs[selector])
            return DecodeOutcome::Failed;
        std::copy_n(state->criticalParameters.quant_tbl_ptrs[selector]->quantval, DCTSIZE2,
                    observedParameters.componentQuantizationTables[static_cast<std::size_t>(index)].begin());
    }
    return jpeg_finish_decompress(&state->decoder) ? DecodeOutcome::Complete : DecodeOutcome::Failed;
}
} // namespace

ImageProcessingResult<ValidatedJpegOutput> JpegOutputValidator::validate(
    std::span<const std::byte> source, const JpegMarkerInventory &sourceInventory, const JpegTransformPlan &plan,
    std::vector<std::byte> output, std::stop_token cancellation, const JpegResourceLimits &limits,
    const TurboJpegResourceLimits &codecLimits)
{
    const auto cancelled = [] {
        return Result::failure({ImageProcessingErrorCode::Cancelled, ImageProcessingStage::OutputValidation});
    };
    if (cancellation.stop_requested())
        return cancelled();
    if (sourceInventory.encodedSourceLengthBytes() != source.size())
        return reject(JpegOutputValidationRule::CompleteJpegStructure);
    // A fresh reader consumes only the completed immutable sequence, not native
    // writer state or its assertion of success. Storage later verifies these bytes.
    const auto scan = JpegSegmentScanner::scan(output, limits);
    if (const auto *error = scan.errorIfPresent())
    {
        if (error->code == ImageProcessingErrorCode::MultiPictureJpegNotSupported ||
            error->code == ImageProcessingErrorCode::ContentCredentialsWouldBeInvalidated ||
            error->code == ImageProcessingErrorCode::UnsupportedJumbfMetadata)
            return reject(JpegOutputValidationRule::NoUnsupportedAssetBindings);
        return reject(JpegOutputValidationRule::CompleteJpegStructure);
    }
    const auto &inventory = *scan.valueIfPresent();
    if (inventory.trailingDataRange().lengthBytes != 0)
        return reject(JpegOutputValidationRule::CompleteJpegStructure);
    const auto &frame = inventory.frameHeader();
    const auto &sourceFrame = sourceInventory.frameHeader();
    if (frame.dimensions != plan.outputDimensions)
        return reject(JpegOutputValidationRule::PlannedDimensions);
    if (frame.samplePrecisionBits != sourceFrame.samplePrecisionBits)
        return reject(JpegOutputValidationRule::SamplePrecision);
    auto expectedComponents = sourceFrame.components;
    const bool exchangesAxes = plan.transform == LosslessTransform::Transpose ||
                               plan.transform == LosslessTransform::Transverse ||
                               plan.transform == LosslessTransform::Rotate90Clockwise ||
                               plan.transform == LosslessTransform::Rotate270Clockwise;
    // Native single-component transforms normalize relative sampling to 1x1.
    for (auto &component : expectedComponents)
        if (expectedComponents.size() == 1)
            component.horizontalSamplingFactor = component.verticalSamplingFactor = 1;
        else if (exchangesAxes)
            std::swap(component.horizontalSamplingFactor, component.verticalSamplingFactor);
    if (frame.components != expectedComponents &&
        !(expectedComponents.size() == 1 && plan.transform == LosslessTransform::None &&
          frame.components == sourceFrame.components))
        return reject(JpegOutputValidationRule::ComponentOrganization);
    const bool expectedProgressive = plan.outputScanOrganization == OutputScanOrganization::ProgressiveDct ||
                                     (plan.outputScanOrganization == OutputScanOrganization::PreserveSource &&
                                      isProgressive(sourceFrame.codingProcess));
    if (isProgressive(frame.codingProcess) != expectedProgressive)
        return reject(JpegOutputValidationRule::ScanOrganization);
    if (isArithmetic(frame.codingProcess) != isArithmetic(sourceFrame.codingProcess))
        return reject(JpegOutputValidationRule::EntropyCodingMode);
    const auto sourceRestartInterval = constantRestartInterval(source, sourceInventory);
    const auto outputRestartInterval = constantRestartInterval(output, inventory);
    if (!sourceRestartInterval || !outputRestartInterval || sourceRestartInterval != outputRestartInterval)
        return reject(JpegOutputValidationRule::RestartIntervalPreservation);
    const auto metadataEvidence =
        MetadataReconciler::validateReconciledMetadata(source, sourceInventory, output, inventory, plan);
    if (const auto *error = metadataEvidence.errorIfPresent())
        return Result::failure(*error);
    if (!MetadataReconciler::validateSourceAssetBindings(output, inventory).valueIfPresent())
        return reject(JpegOutputValidationRule::NoUnsupportedAssetBindings);
    JpegCriticalCodingParameters sourceParameters, outputParameters;
    // Read sequentially, releasing each native coefficient store before the
    // next operation. The source needs no sample reconstruction; the completed
    // output must still undergo the full 8/12-bit sample decode below.
    const auto sourceDecoded = readJpegCodingParameters(source, cancellation, codecLimits, true, sourceParameters);
    if (sourceDecoded == DecodeOutcome::Cancelled || cancellation.stop_requested())
        return cancelled();
    if (sourceDecoded != DecodeOutcome::Complete)
        return reject(JpegOutputValidationRule::FullDecode);
    const auto decoded = readJpegCodingParameters(output, cancellation, codecLimits, false, outputParameters);
    if (decoded == DecodeOutcome::Cancelled || cancellation.stop_requested())
        return cancelled();
    if (decoded != DecodeOutcome::Complete)
        return reject(JpegOutputValidationRule::FullDecode);
    if (sourceParameters.componentQuantizationTables.size() != outputParameters.componentQuantizationTables.size())
        return reject(JpegOutputValidationRule::QuantizationTablePreservation);
    for (std::size_t component = 0; component < sourceParameters.componentQuantizationTables.size(); ++component)
        for (std::size_t verticalFrequency = 0; verticalFrequency < DCTSIZE; ++verticalFrequency)
            for (std::size_t horizontalFrequency = 0; horizontalFrequency < DCTSIZE; ++horizontalFrequency)
            {
                const auto outputIndex = verticalFrequency * DCTSIZE + horizontalFrequency;
                const auto sourceIndex =
                    exchangesAxes ? horizontalFrequency * DCTSIZE + verticalFrequency : outputIndex;
                if (sourceParameters.componentQuantizationTables[component][sourceIndex] !=
                    outputParameters.componentQuantizationTables[component][outputIndex])
                    return reject(JpegOutputValidationRule::QuantizationTablePreservation);
            }
    std::vector<std::byte> metadata;
    std::uint32_t markerCount = 0;
    for (const auto &marker : inventory.markers())
        if ((marker.markerCode >= 0xe0 && marker.markerCode <= 0xef) || marker.markerCode == 0xfe)
        {
            const auto bytes =
                std::span<const std::byte>{output}.subspan(static_cast<std::size_t>(marker.encodedRange.offsetBytes),
                                                           static_cast<std::size_t>(marker.encodedRange.lengthBytes));
            metadata.insert(metadata.end(), bytes.begin(), bytes.end());
            ++markerCount;
        }
    const auto digest = calculateSha256Digest(output);
    const auto markerDigest = calculateSha256Digest(metadata);
    std::vector<std::byte> profile;
    for (const auto &chunk : inventory.iccProfileChunks())
    {
        const auto bytes =
            std::span<const std::byte>{output}.subspan(static_cast<std::size_t>(chunk.profileDataRange.offsetBytes),
                                                       static_cast<std::size_t>(chunk.profileDataRange.lengthBytes));
        profile.insert(profile.end(), bytes.begin(), bytes.end());
    }
    const auto profileDigest =
        inventory.iccProfileChunks().empty() ? std::optional<Sha256Digest>{} : calculateSha256Digest(profile);
    if (!digest || !markerDigest || (!inventory.iccProfileChunks().empty() && !profileDigest))
        return reject(JpegOutputValidationRule::EncodedOutputDigest);
    if (cancellation.stop_requested())
        return cancelled();
    return Result::success(
        ValidatedJpegOutput{std::move(output),
                            *digest,
                            {frame.codingProcess, frame.samplePrecisionBits, frame.dimensions, frame.components},
                            *metadataEvidence.valueIfPresent(),
                            {markerCount, *markerDigest, profileDigest}});
}
} // namespace jpg_spinner::jpeg::internal
