#define NOMINMAX

#include "TemporaryDirectory.h"

#include <Windows.h>
#include <objbase.h>

#include <array>
#include <stdexcept>
#include <string>

#pragma comment(lib, "ole32.lib")

namespace jpg_spinner::test_support
{
    namespace
    {
        constexpr int maximumUniqueNameAttempts = 16;

        std::filesystem::path createUniqueChildPath(
            const std::filesystem::path& canonicalTestOutputRoot)
        {
            for (int attempt = 0; attempt < maximumUniqueNameAttempts; ++attempt)
            {
                GUID uniqueIdentifier{};
                if (FAILED(CoCreateGuid(&uniqueIdentifier)))
                {
                    throw std::runtime_error(
                        "CoCreateGuid could not create a temporary-directory identifier.");
                }

                // StringFromGUID2 writes 38 visible characters plus the null
                // terminator. Retaining the braces makes the UUID boundary
                // visually unambiguous in test artifacts.
                std::array<wchar_t, 39> identifierText{};
                if (StringFromGUID2(
                        uniqueIdentifier,
                        identifierText.data(),
                        static_cast<int>(identifierText.size())) == 0)
                {
                    throw std::runtime_error(
                        "StringFromGUID2 could not format a temporary-directory identifier.");
                }

                const auto candidatePath = canonicalTestOutputRoot /
                    (std::wstring(L"jpg-spinner-test-") + identifierText.data());
                std::error_code creationError;
                if (std::filesystem::create_directory(candidatePath, creationError))
                {
                    return candidatePath;
                }
                if (creationError != std::errc::file_exists)
                {
                    throw std::filesystem::filesystem_error(
                        "Could not create the owned temporary directory",
                        candidatePath,
                        creationError);
                }
            }

            throw std::runtime_error(
                "Could not create a unique temporary directory after repeated UUID attempts.");
        }
    }

    TemporaryDirectory::TemporaryDirectory(const std::filesystem::path& testOutputRoot)
    {
        if (testOutputRoot.empty())
        {
            throw std::invalid_argument("The test output root must not be empty.");
        }

        std::error_code filesystemError;
        std::filesystem::create_directories(testOutputRoot, filesystemError);
        if (filesystemError)
        {
            throw std::filesystem::filesystem_error(
                "Could not create the test output root",
                testOutputRoot,
                filesystemError);
        }
        if (!std::filesystem::is_directory(testOutputRoot, filesystemError))
        {
            if (filesystemError)
            {
                throw std::filesystem::filesystem_error(
                    "Could not inspect the test output root",
                    testOutputRoot,
                    filesystemError);
            }
            throw std::invalid_argument("The test output root is not a directory.");
        }

        canonicalTestOutputRoot_ =
            std::filesystem::weakly_canonical(testOutputRoot, filesystemError);
        if (filesystemError)
        {
            throw std::filesystem::filesystem_error(
                "Could not canonicalize the test output root",
                testOutputRoot,
                filesystemError);
        }

        ownedDirectoryPath_ = createUniqueChildPath(canonicalTestOutputRoot_);
        ownsDirectory_ = true;

        // Validate containment immediately as well as before deletion. If the
        // filesystem resolves the new child anywhere unexpected, remove only
        // the just-created empty directory and refuse to assume ownership.
        if (!hasExpectedCanonicalParent(filesystemError))
        {
            std::error_code removalError;
            std::filesystem::remove(ownedDirectoryPath_, removalError);
            ownsDirectory_ = false;
            throw std::filesystem::filesystem_error(
                "The temporary directory did not resolve beneath its test output root",
                ownedDirectoryPath_,
                filesystemError ? filesystemError : std::make_error_code(std::errc::permission_denied));
        }
    }

    TemporaryDirectory::~TemporaryDirectory() noexcept
    {
        static_cast<void>(removeOwnedDirectory());
    }

    const std::filesystem::path& TemporaryDirectory::directoryPath() const noexcept
    {
        return ownedDirectoryPath_;
    }

    std::error_code TemporaryDirectory::removeOwnedDirectory() noexcept
    {
        if (!ownsDirectory_)
        {
            return cleanupError_;
        }

        std::error_code containmentError;
        if (!hasExpectedCanonicalParent(containmentError))
        {
            cleanupError_ = containmentError ?
                containmentError :
                std::make_error_code(std::errc::permission_denied);
            return cleanupError_;
        }

        // Rechecking the canonical parent immediately before remove_all is the
        // destructive-operation boundary. It prevents a renamed/reparse-point
        // child from widening cleanup beyond the directory this object owns.
        std::filesystem::remove_all(ownedDirectoryPath_, cleanupError_);
        if (!cleanupError_)
        {
            ownsDirectory_ = false;
        }
        return cleanupError_;
    }

    std::error_code TemporaryDirectory::cleanupError() const noexcept
    {
        return cleanupError_;
    }

    bool TemporaryDirectory::hasExpectedCanonicalParent(std::error_code& error) const noexcept
    {
        error.clear();
        const auto currentCanonicalRoot =
            std::filesystem::weakly_canonical(canonicalTestOutputRoot_, error);
        if (error)
        {
            return false;
        }

        const auto currentCanonicalOwnedPath =
            std::filesystem::weakly_canonical(ownedDirectoryPath_, error);
        if (error)
        {
            return false;
        }

        return std::filesystem::equivalent(
            currentCanonicalOwnedPath.parent_path(),
            currentCanonicalRoot,
            error);
    }
}
