#pragma once

namespace jpg_spinner::domain
{
/// User-selected persistence policy. Both replacement modes stage and verify
/// corrected bytes; only the explicit backup mode retains the original bytes.
enum class OutputDisposition
{
    CreateCorrectedCopy,
    ReplaceOriginalWithVerifiedBackup,
    ReplaceOriginalWithoutRetainedBackup,
};
} // namespace jpg_spinner::domain
