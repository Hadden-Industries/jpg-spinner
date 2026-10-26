#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace jpg_spinner::presentation
{
/// Owned pixels for one actual WIC progressive level of the packaged example.
/// Decode on an initialized COM worker; create XAML image objects on the UI
/// thread afterward. The demonstration owns no user-image processing path.
struct ProgressiveJpegExampleFrame final
{
    std::uint32_t widthInPixels;
    std::uint32_t heightInPixels;
    std::uint32_t progressiveLevel;
    std::uint32_t progressiveLevelCount;
    std::vector<std::uint8_t> premultipliedBgraPixels;
};

/// Select the first, middle and final native levels. All three frames represent
/// the same complete image at different detail levels; no simulated blur,
/// recompression or replacement artwork is introduced.
std::array<ProgressiveJpegExampleFrame, 3> decodeProgressiveJpegExample(const std::filesystem::path &path);
} // namespace jpg_spinner::presentation
