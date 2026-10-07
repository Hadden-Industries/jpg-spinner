#pragma once

#include <jpg_spinner/storage/SourceFileRevisionCalculator.h>
#include <optional>
#include <vector>

namespace jpg_spinner::storage::internal
{
/// Revision and optional bytes from one opened read-only stream. Absence of bytes
/// is intentional for revision-only callers, preserving their constant I/O buffer.
struct SourceFileCapture final
{
    domain::SourceFileRevision revision;
    std::optional<std::vector<std::byte>> encodedBytes;
};

/// Shared native capture mechanism. Supplying a maximum requests retained bytes
/// and rejects the observed size before allocation; omission streams SHA-256 only.
/// Both forms preserve the existing explicit-MTA, sharing, close and mutation gates.
[[nodiscard]] domain::ImageProcessingResult<SourceFileCapture> captureSourceFile(
    const winrt::Windows::Storage::StorageFile &sourceFile, std::stop_token cancellationToken,
    const SourceFileRevisionProgressObserver &progressObserver,
    std::optional<std::uint64_t> maximumRetainedEncodedLengthBytes = {});
} // namespace jpg_spinner::storage::internal
