#pragma once

#include "Sha256Digest.h"

#include <chrono>
#include <cstdint>
#include <ratio>

namespace jpg_spinner::domain
{
/// Observed encoded file state, not a file identity or an access capability.
/// SHA-256 detects edits whose length and modification instant are unchanged;
/// consumers must recapture the revision immediately before committing output.
struct SourceFileRevision final
{
    std::uint64_t encodedLengthBytes;
    std::chrono::sys_time<std::chrono::duration<std::int64_t, std::ratio<1, 10'000'000>>> lastWriteTimeUtc;
    Sha256Digest encodedSha256;

    bool operator==(const SourceFileRevision &) const = default;
};
} // namespace jpg_spinner::domain
