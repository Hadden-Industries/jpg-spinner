#include <jpg_spinner/storage/SourceFileRevisionCalculator.h>
#include "internal/SourceFileCapture.h"
#include <utility>

namespace jpg_spinner::storage
{
domain::ImageProcessingResult<domain::SourceFileRevision> SourceFileRevisionCalculator::calculate(
    const winrt::Windows::Storage::StorageFile &sourceFile, const std::stop_token cancellationToken,
    const SourceFileRevisionProgressObserver &progressObserver) const
{
    auto captured = internal::captureSourceFile(sourceFile, cancellationToken, progressObserver);
    using Result = domain::ImageProcessingResult<domain::SourceFileRevision>;
    if (const auto error = captured.errorIfPresent())
        return Result::failure(*error);
    return Result::success(std::move(captured.valueIfPresent()->revision));
}
} // namespace jpg_spinner::storage
