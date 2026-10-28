#pragma once

#include "AnalyzedImageBatch.h"
#include "BatchProcessingRequest.h"
#include "BatchProcessingProgress.h"
#include "BatchProcessingSummary.h"
#include <jpg_spinner/jpeg/JpegTransformationEngine.h>
#include <mutex>
#include <functional>

namespace jpg_spinner::batch_processing
{
/// Bounded parallel composition of granted source capabilities and the JPEG
/// analyzer/transformer. Analysis is serial; execution admits independent images
/// under the reviewed concurrency and memory-estimate policy. Saves serialize.
/// No native-C abort, raw output constructor or path-derived authority exists.
/// The transformation engine must outlive the coordinator; shared ownership
/// makes that lifetime explicit. Calls on one coordinator serialize.
class BatchProcessingCoordinator final
{
  public:
    /// The provider samples current system headroom, independently of the user's
    /// optional processing budget. Calls are serialized by this coordinator.
    using WorkingMemoryBudgetProvider = std::function<std::uint64_t()>;
    explicit BatchProcessingCoordinator(std::shared_ptr<const jpeg::JpegTransformationEngine> transformationEngine,
                                        WorkingMemoryBudgetProvider workingMemoryBudgetProvider =
                                            BatchExecutionLimits::recommendedWorkingMemoryBudgetBytes);
    [[nodiscard]] domain::ImageProcessingResult<AnalyzedImageBatch> analyze(
        const BatchProcessingRequest &request, std::stop_token cancellation = {},
        const BatchProcessingProgressObserver &observer = {}) const;
    /// Transfers the reviewed artifact. All result rows are allocated before the
    /// first transformation/commit and retain input order. Observer calls serialize
    /// but may run on workers; observers must not require the caller's apartment.
    /// Per-file errors remain in the successful summary.
    [[nodiscard]] domain::ImageProcessingResult<BatchProcessingSummary> execute(
        AnalyzedImageBatch reviewedBatch, std::stop_token cancellation = {},
        const BatchProcessingProgressObserver &observer = {}) const;

  private:
    std::shared_ptr<const jpeg::JpegTransformationEngine> transformationEngine_;
    WorkingMemoryBudgetProvider workingMemoryBudgetProvider_;
    mutable std::mutex operationMutex_;
};
} // namespace jpg_spinner::batch_processing
