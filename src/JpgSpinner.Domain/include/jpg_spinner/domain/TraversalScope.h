#pragma once

namespace jpg_spinner::domain
{
/// Discovery extent beneath the selected root; never permission to follow reparse points.
enum class TraversalScope
{
    SelectedFolderOnly,
    SelectedFolderAndDescendants,
};
} // namespace jpg_spinner::domain
