#pragma once

#include <jpg_spinner/domain/ImageProcessingError.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Storage.h>
#include <chrono>
#include <optional>
#include <string>
#include <mutex>

namespace jpg_spinner::diagnostics
{
/// Architecture of this application binary, not a device identifier or an
/// inferred host architecture. Values serialize through a fixed whitelist.
enum class ApplicationArchitecture
{
    X86,
    X64,
    Arm64
};

/// Privacy boundary for local diagnostics. No arbitrary string, file path,
/// image bytes, metadata, native message, endpoint or device ID can be supplied.
/// Correlation is an operation UUID; elapsed time is monotonic measured duration.
struct ImageProcessingDiagnosticRecord final
{
    std::chrono::sys_time<std::chrono::milliseconds> recordedAtUtc;
    winrt::Windows::ApplicationModel::PackageVersion applicationVersion;
    ApplicationArchitecture applicationArchitecture;
    winrt::guid correlationIdentifier;
    domain::ImageProcessingStage stage;
    std::optional<domain::ImageProcessingErrorCode> errorCode;
    std::chrono::milliseconds elapsedTime;
    std::optional<domain::JpegResourceLimit> resourceLimit;
};

/// One deterministic UTF-8 JSON Lines record, with exactly one final LF and
/// no BOM. Windows' native JSON serializer owns escaping and wire syntax.
/// Unknown enum values map to fixed Unknown symbols, never caller prose.
/// Invalid UUID/duration values are rejected before a record is produced.
[[nodiscard]] std::string serializeImageProcessingDiagnosticRecord(const ImageProcessingDiagnosticRecord &record);
/// Capture the current package version, binary architecture and nominal UTC
/// wall time for typed operation facts. Requires package identity; no file I/O.
[[nodiscard]] ImageProcessingDiagnosticRecord captureImageProcessingDiagnosticRecord(
    winrt::guid correlationIdentifier, domain::ImageProcessingStage observedStage,
    std::chrono::milliseconds elapsedTime, const domain::ImageProcessingError *failure = nullptr);

/// Limits apply to retained diagnostic-record files, excluding the empty native
/// writer lease. One reserved pending slot contains at most 4096 bytes and is
/// budgeted before publication; it is never exported. Each published file is
/// one immutable JSON Lines record (at most 4096 bytes).
struct DiagnosticLogRetentionLimits final
{
    std::uint64_t maximumRetainedLengthBytes{256 * 1024};
    std::uint32_t maximumRetainedFileCount{64};
};

/// Local-only log in a dedicated application-owned folder. Construction performs
/// no I/O; call append/snapshot on an owned MTA, never on the UI thread. Writes
/// serialize through a native exclusive lease across instances/processes. Only
/// recognized, validated diagnostic records are rotated; observed unknown/corrupt
/// objects are preserved and refuse the operation. Native Storage can omit
/// unsupported objects; this is not a physical inventory or a path-based audit.
/// Omitted objects are never opened, exported or deleted. Images are not inputs.
/// The caller grants ownership of the reserved pending diagnostic scratch slot;
/// only that slot can be replaced after a crash, under the native writer lease.
class LocalDiagnosticLog final
{
  public:
    explicit LocalDiagnosticLog(winrt::Windows::Storage::StorageFolder privateLogFolder,
                                DiagnosticLogRetentionLimits limits = {});
    /// Best-effort diagnostics never fail image processing. False means the
    /// record was not confirmed; no native message/path is returned or logged.
    [[nodiscard]] bool append(const ImageProcessingDiagnosticRecord &record) noexcept;
    /// A bounded sanitized UTF-8 JSON Lines snapshot; native/schema failures
    /// propagate for the caller to map to safe UI. Existing records are rechecked
    /// through native consumers and a closed field vocabulary, never copied raw.
    [[nodiscard]] std::string snapshot() const;

  private:
    const winrt::Windows::Storage::StorageFolder privateLogFolder_;
    const DiagnosticLogRetentionLimits limits_;
    mutable std::mutex mutex_;
};
} // namespace jpg_spinner::diagnostics
