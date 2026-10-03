#pragma once

namespace jpg_spinner::domain
{
/// User-selected persistence policy. There is deliberately no unbacked replacement.
enum class OutputDisposition
{
    CreateCorrectedCopy,
    ReplaceOriginalWithVerifiedBackup,
};
} // namespace jpg_spinner::domain
