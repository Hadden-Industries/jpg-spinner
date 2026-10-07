#pragma once

#include <guiddef.h>
#include <chrono>
#include <string>

namespace jpg_spinner::batch_processing
{
/// Full native batch identity and UTC creation time. Display names are only labels:
/// an abbreviated prefix must never grant ownership or authorize file cleanup.
class BatchIdentifier final
{
  public:
    /// Capture an already generated identity and a UTC time at second precision.
    BatchIdentifier(GUID fullIdentity, std::chrono::sys_seconds creationTime) noexcept;
    /// Generate a full UUID through CoCreateGuid; native generation failure throws.
    [[nodiscard]] static BatchIdentifier create(std::chrono::sys_seconds creationTime);
    [[nodiscard]] const GUID &fullIdentity() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds creationTime() const noexcept;
    /// Invariant UTC yyyyMMddTHHmmssZ followed by '-' and eight lowercase UUID hex digits.
    [[nodiscard]] std::wstring displayDirectoryName() const;

  private:
    GUID fullIdentity_;
    std::chrono::sys_seconds creationTime_;
};
} // namespace jpg_spinner::batch_processing
