#pragma once

#include <jpg_spinner/domain/ValidatedJpegOutput.h>

namespace jpg_spinner::jpeg::internal
{
/// Native Windows SHA-256 adapter for one immutable bounded byte sequence.
/// No filesystem access or cryptographic implementation. Failure covers the
/// native API's ULONG length bound or an unsuccessful provider operation.
[[nodiscard]] std::optional<jpg_spinner::domain::Sha256Digest> calculateSha256Digest(std::span<const std::byte> bytes);
} // namespace jpg_spinner::jpeg::internal
