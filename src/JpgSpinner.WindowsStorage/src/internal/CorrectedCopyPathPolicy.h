#pragma once

#include <filesystem>

namespace jpg_spinner::storage::internal
{
/// Native handle-derived paths only. This names one permitted direct child, not
/// a generic path parser or authority for opening/deleting arbitrary paths.
[[nodiscard]] inline bool isCorrectedCopyOutputRootPath(const std::filesystem::path &outputRoot,
                                                        const std::filesystem::path &selectedSourceRoot)
{
    // NT volume roots retain a trailing separator. Comparing parent_path()
    // with that root adds a false inequality from its empty final component.
    // Compose the expected child with the standard path API instead; this
    // also rejects a different direct-child name beneath the selected root.
    return outputRoot == selectedSourceRoot / L"JPG Spinner Output";
}
} // namespace jpg_spinner::storage::internal
