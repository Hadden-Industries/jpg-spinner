#pragma once

#include "BatchIdentifier.h"
#include "BatchFileOutcome.h"
#include "BatchProcessingProgress.h"
#include <jpg_spinner/domain/ImageSourceCollection.h>

namespace jpg_spinner::batch_processing
{
/// Terminal results in reviewed ordinal order, including discovery issues that
/// were not image candidates. Partial discovery is explicit in finalProgress.
/// transactionIntegrityUncertain requires recovery before further transactions;
/// ordinary per-file failures do not prevent later independently eligible files.
struct BatchProcessingSummary final
{
    BatchIdentifier identifier;
    std::vector<BatchFileProcessingResult> fileResults;
    std::vector<domain::ImageSourceDiscoveryIssue> discoveryIssues;
    BatchProcessingProgress finalProgress;
    bool transactionIntegrityUncertain{false};
};
} // namespace jpg_spinner::batch_processing
