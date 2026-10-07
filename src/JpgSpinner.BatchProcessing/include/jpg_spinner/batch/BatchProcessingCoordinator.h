#pragma once

#include "AnalyzedImageBatch.h"
#include "BatchProcessingRequest.h"
#include "BatchProcessingProgress.h"
#include "BatchProcessingSummary.h"
#include <jpg_spinner/jpeg/JpegTransformationEngine.h>
#include <mutex>

namespace jpg_spinner::batch_processing
{
/// Serial background-worker composition of granted source capabilities and the
/// one-operation JPEG analyzer/transformer. No worker pool, native-C abort, raw
/// output constructor or filesystem-path authority exists in this layer.
/// The transformation engine must outlive the coordinator; shared ownership
/// makes that lifetime explicit. Calls on one coordinator serialize.
class BatchProcessingCoordinator final
{
  public:
    explicit BatchProcessingCoordinator(std::shared_ptr<const jpeg::JpegTransformationEngine> transformationEngine);
    [[nodiscard]] domain::ImageProcessingResult<AnalyzedImageBatch> analyze(
        const BatchProcessingRequest &request, std::stop_token cancellation = {},
        const BatchProcessingProgressObserver &observer = {}) const;
    /// Transfers the reviewed artifact. Exactly one source buffer and codec call
    /// are active at a time. All result storage is allocated before the first
    /// transformation/commit; per-file errors remain in the successful summary.
    [[nodiscard]] domain::ImageProcessingResult<BatchProcessingSummary> execute(
        AnalyzedImageBatch reviewedBatch, std::stop_token cancellation = {},
        const BatchProcessingProgressObserver &observer = {}) const;

  private:
    std::shared_ptr<const jpeg::JpegTransformationEngine> transformationEngine_;
    mutable std::mutex operationMutex_;
};
} // namespace jpg_spinner::batch_processing
