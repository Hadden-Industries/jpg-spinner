#pragma once

#include <filesystem>
#include <system_error>

namespace jpg_spinner::test_support
{
    // Owns one uniquely named directory immediately beneath a caller-supplied
    // test-output root. The class deliberately has no move operation: one
    // stable object remains the sole authority allowed to remove the child.
    class TemporaryDirectory final
    {
    public:
        explicit TemporaryDirectory(const std::filesystem::path& testOutputRoot);
        ~TemporaryDirectory() noexcept;

        TemporaryDirectory(const TemporaryDirectory&) = delete;
        TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
        TemporaryDirectory(TemporaryDirectory&&) = delete;
        TemporaryDirectory& operator=(TemporaryDirectory&&) = delete;

        [[nodiscard]] const std::filesystem::path& directoryPath() const noexcept;

        // Explicit cleanup lets a test assert an error that a noexcept
        // destructor could otherwise only retain for diagnostic inspection.
        [[nodiscard]] std::error_code removeOwnedDirectory() noexcept;
        [[nodiscard]] std::error_code cleanupError() const noexcept;

    private:
        [[nodiscard]] bool hasExpectedCanonicalParent(std::error_code& error) const noexcept;

        std::filesystem::path canonicalTestOutputRoot_;
        std::filesystem::path ownedDirectoryPath_;
        std::error_code cleanupError_;
        bool ownsDirectory_{false};
    };
}
