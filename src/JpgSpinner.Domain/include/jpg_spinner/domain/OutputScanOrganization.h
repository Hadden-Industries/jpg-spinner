#pragma once

namespace jpg_spinner::domain
{
/// Describes the requested organization of JPEG DCT scans. PreserveSource
/// is distinct from both explicit sequential and progressive output; a
/// Boolean "progressive" option could not express that third policy.
enum class OutputScanOrganization
{
    PreserveSource,
    SequentialDct,
    ProgressiveDct,
};
} // namespace jpg_spinner::domain
