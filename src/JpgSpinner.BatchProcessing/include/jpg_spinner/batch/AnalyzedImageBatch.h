#pragma once

#include "BatchIdentifier.h"
#include <jpg_spinner/domain/ImageSourceCollection.h>
#include <jpg_spinner/domain/JpegImageAnalysis.h>
#include <jpg_spinner/domain/JpegResourceLimits.h>
#include <span>

namespace jpg_spinner::batch_processing
{
class BatchProcessingCoordinator;

/// Review facts for one candidate, retaining no encoded source bytes. A failed
/// capture/analysis has an error instead of approved JPEG analysis. Source
/// capabilities and revision observations remain bound to their original name.
struct BatchFileAnalysis final
{
    std::wstring relativePath;
    std::shared_ptr<const domain::ImageSource> source;
    std::optional<domain::SourceFileRevision> sourceRevision;
    std::shared_ptr<const domain::JpegImageAnalysis> jpegAnalysis;
    std::optional<domain::ImageProcessingError> error;
};

/// Coordinator-produced review artifact, not a user approval or output proof.
/// Move-only transfer into execute prevents accidental reuse of a reviewed batch.
/// Public access is read-only. The single encoded buffer is released after each
/// analysis; execution recaptures and checks the reviewed revision before native work.
class AnalyzedImageBatch final
{
  public:
    AnalyzedImageBatch(const AnalyzedImageBatch &) = delete;
    AnalyzedImageBatch &operator=(const AnalyzedImageBatch &) = delete;
    AnalyzedImageBatch(AnalyzedImageBatch &&) = default;
    AnalyzedImageBatch &operator=(AnalyzedImageBatch &&) = delete;
    [[nodiscard]] const BatchIdentifier &identifier() const noexcept
    {
        return identifier_;
    }
    [[nodiscard]] std::span<const BatchFileAnalysis> files() const noexcept
    {
        return files_;
    }
    [[nodiscard]] std::span<const domain::ImageSourceDiscoveryIssue> discoveryIssues() const noexcept
    {
        return issues_;
    }
    [[nodiscard]] bool discoveryComplete() const noexcept
    {
        return discoveryComplete_;
    }

  private:
    friend class BatchProcessingCoordinator;
    AnalyzedImageBatch(BatchIdentifier identifier, domain::OutputDisposition disposition,
                       domain::JpegResourceLimits limits, std::vector<BatchFileAnalysis> files,
                       std::vector<domain::ImageSourceDiscoveryIssue> issues, bool complete, std::size_t analyzedCount)
        : identifier_(std::move(identifier)), disposition_(disposition), resourceLimits_(limits),
          files_(std::move(files)), issues_(std::move(issues)), discoveryComplete_(complete),
          analyzedCount_(analyzedCount)
    {
    }
    BatchIdentifier identifier_;
    domain::OutputDisposition disposition_;
    domain::JpegResourceLimits resourceLimits_;
    std::vector<BatchFileAnalysis> files_;
    std::vector<domain::ImageSourceDiscoveryIssue> issues_;
    bool discoveryComplete_;
    std::size_t analyzedCount_;
};
} // namespace jpg_spinner::batch_processing
