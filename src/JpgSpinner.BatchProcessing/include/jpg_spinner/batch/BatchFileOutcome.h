#pragma once

#include <jpg_spinner/domain/ImageProcessingError.h>
#include <optional>
#include <string>

namespace jpg_spinner::batch_processing
{
/// Exact terminal observations. A successful native commit wins over a token
/// requested afterwards. Cancellation is before transformation if that boundary
/// was not entered, otherwise before commit; it never claims to undo a commit.
enum class BatchFileOutcome
{
    CorrectedCopyCreated,
    OriginalReplacedWithVerifiedBackup,
    NoOrientationNormalizationRequired,
    UnsupportedSourceSkipped,
    ProcessingFailed,
    CancelledBeforeTransformation,
    CancelledBeforeCommit,
};

/// One terminal result for every discovered extension candidate. Successful
/// commits and no-op orientation results have no error. Unsupported, failed and
/// cancelled outcomes carry their exact structured reason. ProcessingFailed can
/// also mean an earlier uncertain transaction prevented safely starting this file.
struct BatchFileProcessingResult final
{
    std::wstring relativePath;
    BatchFileOutcome outcome;
    std::optional<domain::ImageProcessingError> error;
};
} // namespace jpg_spinner::batch_processing
