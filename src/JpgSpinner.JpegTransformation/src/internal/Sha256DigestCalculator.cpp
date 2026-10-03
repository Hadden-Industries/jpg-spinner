#include "Sha256DigestCalculator.h"
#define NOMINMAX
#include <Windows.h>
#include <bcrypt.h>
#include <limits>

// This static module carries its platform dependency into each native consumer.
#pragma comment(lib, "bcrypt.lib")

namespace jpg_spinner::jpeg::internal
{
std::optional<jpg_spinner::domain::Sha256Digest> calculateSha256Digest(std::span<const std::byte> bytes)
{
    if (bytes.size() > std::numeric_limits<ULONG>::max())
        return std::nullopt;
    jpg_spinner::domain::Sha256Digest digest{};
    // The predefined algorithm handle needs no open/close lifetime and is
    // supported above our Windows minimum. BCryptHash does not modify its input.
    if (BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0,
                   reinterpret_cast<PUCHAR>(const_cast<std::byte *>(bytes.data())), static_cast<ULONG>(bytes.size()),
                   reinterpret_cast<PUCHAR>(digest.data()), static_cast<ULONG>(digest.size())) < 0)
        return std::nullopt;
    return digest;
}
} // namespace jpg_spinner::jpeg::internal
