#pragma once

#include "BatchIdentifier.h"
#include <jpg_spinner/domain/ImageSourceCollection.h>
#include <jpg_spinner/domain/EdgeHandlingPolicy.h>
#include <jpg_spinner/domain/OutputScanOrganization.h>
#include <jpg_spinner/domain/JpegResourceLimits.h>

namespace jpg_spinner::batch_processing
{
/// Analysis policy and the granted collection, not an arbitrary filesystem path.
/// Analysis performs no transformation or commit. Presentation must review the
/// returned immutable analysis before explicitly calling execute.
/// Resource bounds may be stricter than production, never larger. Unknown closed
/// enum values or a missing collection fail before discovery or source allocation.
struct BatchProcessingRequest final
{
    std::shared_ptr<const domain::ImageSourceCollection> sourceCollection;
    domain::TraversalScope traversalScope{domain::TraversalScope::SelectedFolderOnly};
    domain::OutputDisposition outputDisposition{domain::OutputDisposition::CreateCorrectedCopy};
    domain::EdgeHandlingPolicy edgeHandlingPolicy{domain::EdgeHandlingPolicy::RequirePerfectCoefficientTransform};
    domain::OutputScanOrganization outputScanOrganization{domain::OutputScanOrganization::PreserveSource};
    domain::JpegResourceLimits resourceLimits{domain::JpegResourceLimits::production()};
    /// Composition supplies this same full identity/time to the native transaction
    /// engine. It is created before analysis so review, results and output/backup
    /// labels refer to one batch; abbreviated labels never establish ownership.
    BatchIdentifier identifier{
        BatchIdentifier::create(std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()))};
};
} // namespace jpg_spinner::batch_processing
