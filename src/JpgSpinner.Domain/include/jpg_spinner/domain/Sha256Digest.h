#pragma once

#include <array>
#include <cstddef>

namespace jpg_spinner::domain
{
/// Fixed-size cryptographic identity of bytes, independent of text format.
using Sha256Digest = std::array<std::byte, 32>;
} // namespace jpg_spinner::domain
