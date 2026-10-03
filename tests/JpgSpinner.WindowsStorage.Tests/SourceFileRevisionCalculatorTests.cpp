#define NOMINMAX
#include <windows.h>
#include <sddl.h>

#include "TemporaryDirectory.h"
#include <jpg_spinner/storage/SourceFileRevisionCalculator.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <string_view>
#include <string>
#include <tuple>
#include <vector>

#pragma comment(lib, "advapi32.lib")

namespace
{
using jpg_spinner::domain::ImageProcessingErrorCode;
using jpg_spinner::domain::ImageProcessingStage;
using jpg_spinner::storage::SourceFileRevisionCalculator;
using jpg_spinner::test_support::TemporaryDirectory;

/// Each case owns its apartment; projection objects are destroyed before uninitialization.
class TestApartment final
{
  public:
    explicit TestApartment(const winrt::apartment_type type = winrt::apartment_type::multi_threaded)
    {
        winrt::init_apartment(type);
    }
    ~TestApartment()
    {
        winrt::uninit_apartment();
    }
    TestApartment(const TestApartment &) = delete;
    TestApartment &operator=(const TestApartment &) = delete;
};

void writeFile(const std::filesystem::path &path, const std::string_view bytes)
{
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    REQUIRE(output.good());
    REQUIRE(std::filesystem::file_size(path) == bytes.size());
}

auto openStorageFile(const std::filesystem::path &path)
{
    return winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(path.wstring()).get();
}

auto capture(const std::filesystem::path &path, const std::stop_token cancellationToken = {},
             const jpg_spinner::storage::SourceFileRevisionProgressObserver &progressObserver = {})
{
    return SourceFileRevisionCalculator{}.calculate(openStorageFile(path), cancellationToken, progressObserver);
}

std::string digestHex(const jpg_spinner::domain::Sha256Digest &digest)
{
    constexpr std::string_view hexDigits = "0123456789abcdef";
    std::string result;
    for (const auto byte : digest)
    {
        const auto value = std::to_integer<unsigned char>(byte);
        result += hexDigits[value >> 4];
        result += hexDigits[value & 15];
    }
    return result;
}

void requireFailure(const auto &result, const ImageProcessingErrorCode expectedCode)
{
    REQUIRE(result.valueIfPresent() == nullptr);
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == expectedCode);
    CHECK(result.errorIfPresent()->stage == ImageProcessingStage::SourceRevisionCapture);
}

/// Test-owned ACL mutation is restored even when an assertion unwinds this scope.
class FileReadDenial final
{
  public:
    explicit FileReadDenial(std::filesystem::path path) : path_(std::move(path))
    {
        DWORD requiredBytes = 0;
        GetFileSecurityW(path_.c_str(), DACL_SECURITY_INFORMATION, nullptr, 0, &requiredBytes);
        REQUIRE(GetLastError() == ERROR_INSUFFICIENT_BUFFER);
        originalSecurity_.resize(requiredBytes);
        REQUIRE(GetFileSecurityW(path_.c_str(), DACL_SECURITY_INFORMATION, originalSecurity_.data(), requiredBytes,
                                 &requiredBytes));
        PSECURITY_DESCRIPTOR deniedSecurity = nullptr;
        REQUIRE(ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:(D;;FR;;;WD)(A;;FA;;;OW)", SDDL_REVISION_1,
                                                                     &deniedSecurity, nullptr));
        const auto changed = SetFileSecurityW(path_.c_str(), DACL_SECURITY_INFORMATION, deniedSecurity);
        LocalFree(deniedSecurity);
        REQUIRE(changed);
    }
    ~FileReadDenial()
    {
        SetFileSecurityW(path_.c_str(), DACL_SECURITY_INFORMATION, originalSecurity_.data());
    }
    FileReadDenial(const FileReadDenial &) = delete;
    FileReadDenial &operator=(const FileReadDenial &) = delete;

  private:
    std::filesystem::path path_;
    std::vector<std::byte> originalSecurity_;
};
} // namespace

TEST_CASE("a real source revision contains its length UTC instant and known SHA-256", "[storage][revision]")
{
    TestApartment apartment;
    TemporaryDirectory directory{std::filesystem::current_path() / "artifacts" / "tests" / "storage"};
    const auto path = directory.directoryPath() / "source.jpg";
    writeFile(path, "abc");

    // Set an independently chosen UTC instant through the standard filesystem clock,
    // rather than deriving the expectation from the adapter's WinRT conversion.
    using namespace std::chrono;
    const auto expectedInstant = sys_days{year{2026} / October / 3} + hours{12} + seconds{34};
    std::filesystem::last_write_time(path, clock_cast<std::chrono::file_clock>(expectedInstant));
    REQUIRE(clock_cast<system_clock>(std::filesystem::last_write_time(path)) == expectedInstant);

    const auto result = SourceFileRevisionCalculator{}.calculate(openStorageFile(path));
    REQUIRE(result.errorIfPresent() == nullptr);
    REQUIRE(result.valueIfPresent() != nullptr);
    CHECK(result.valueIfPresent()->encodedLengthBytes == 3);
    CHECK(result.valueIfPresent()->lastWriteTimeUtc == expectedInstant);

    // NIST/FIPS SHA-256 vector for the three encoded bytes "abc".
    const std::array<unsigned char, 32> expectedDigest{0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40,
                                                       0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17,
                                                       0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
    for (std::size_t index = 0; index < expectedDigest.size(); ++index)
        CHECK(std::to_integer<unsigned char>(result.valueIfPresent()->encodedSha256[index]) == expectedDigest[index]);
    CHECK_FALSE(directory.removeOwnedDirectory());
}

TEST_CASE("same-length edits with restored timestamps still change the source revision", "[storage][revision]")
{
    TestApartment apartment;
    TemporaryDirectory directory{std::filesystem::current_path() / "artifacts" / "tests" / "storage"};
    const auto path = directory.directoryPath() / "source.jpg";
    writeFile(path, "abc");
    const auto modificationTime = std::filesystem::last_write_time(path);
    const auto original = capture(path);
    REQUIRE(original.valueIfPresent() != nullptr);
    writeFile(path, "abd");
    std::filesystem::last_write_time(path, modificationTime);
    REQUIRE(std::filesystem::last_write_time(path) == modificationTime);
    const auto edited = capture(path);
    REQUIRE(edited.valueIfPresent() != nullptr);
    CHECK(original.valueIfPresent()->encodedLengthBytes == edited.valueIfPresent()->encodedLengthBytes);
    CHECK(original.valueIfPresent()->lastWriteTimeUtc == edited.valueIfPresent()->lastWriteTimeUtc);
    CHECK(original.valueIfPresent()->encodedSha256 != edited.valueIfPresent()->encodedSha256);
    CHECK(*original.valueIfPresent() != *edited.valueIfPresent());
    CHECK_FALSE(directory.removeOwnedDirectory());
}

TEST_CASE("empty and multi-block files use known SHA-256 vectors and bounded progress", "[storage][revision]")
{
    TestApartment apartment;
    TemporaryDirectory directory{std::filesystem::current_path() / "artifacts" / "tests" / "storage"};
    const auto path = directory.directoryPath() / "source.jpg";
    const bool empty = GENERATE(true, false);
    writeFile(path, empty ? std::string{} : std::string(1'000'000, 'a'));
    std::vector<jpg_spinner::storage::SourceFileRevisionProgress> notifications;
    const auto result = capture(path, {}, [&](const auto &progress) { notifications.push_back(progress); });
    REQUIRE(result.valueIfPresent() != nullptr);
    CHECK(result.valueIfPresent()->encodedLengthBytes == (empty ? 0 : 1'000'000));
    CHECK(digestHex(result.valueIfPresent()->encodedSha256) ==
          (empty ? "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
                 : "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
    REQUIRE_FALSE(notifications.empty());
    CHECK(notifications.front().encodedBytesHashed == 0);
    CHECK(notifications.back().encodedBytesHashed == result.valueIfPresent()->encodedLengthBytes);
    if (!empty)
        REQUIRE(notifications.size() > 2);
    for (std::size_t index = 1; index < notifications.size(); ++index)
    {
        CHECK(notifications[index].encodedBytesHashed > notifications[index - 1].encodedBytesHashed);
        CHECK(notifications[index].encodedBytesHashed - notifications[index - 1].encodedBytesHashed <= 64 * 1024);
        CHECK(notifications[index].totalEncodedBytes == result.valueIfPresent()->encodedLengthBytes);
    }
    CHECK_FALSE(directory.removeOwnedDirectory());
}

TEST_CASE("cancellation before open and after a hashed block never publishes a revision", "[storage][revision]")
{
    TestApartment apartment;
    TemporaryDirectory directory{std::filesystem::current_path() / "artifacts" / "tests" / "storage"};
    const auto path = directory.directoryPath() / "source.jpg";
    writeFile(path, std::string(200'000, 'a'));
    const bool cancelBeforeOpen = GENERATE(true, false);
    std::stop_source cancellation;
    std::uint64_t observedLength = 0;
    if (cancelBeforeOpen)
        cancellation.request_stop();
    const auto result = capture(path, cancellation.get_token(), [&](const auto &progress) {
        observedLength = progress.encodedBytesHashed;
        if (observedLength > 0)
            cancellation.request_stop();
    });
    requireFailure(result, ImageProcessingErrorCode::Cancelled);
    if (cancelBeforeOpen)
        CHECK(observedLength == 0);
    else
    {
        // Partial reads may return less than the requested block capacity.
        CHECK(observedLength > 0);
        CHECK(observedLength <= 64 * 1024);
    }
    // A writer would be denied by our reader-only sharing. Successful truncation
    // proves that the failed capture released its reader, not exclusive writing.
    writeFile(path, "abc");
    CHECK_FALSE(directory.removeOwnedDirectory());
}

TEST_CASE("an existing writer produces a structured sharing failure", "[storage][revision]")
{
    TestApartment apartment;
    TemporaryDirectory directory{std::filesystem::current_path() / "artifacts" / "tests" / "storage"};
    const auto path = directory.directoryPath() / "source.jpg";
    writeFile(path, "abc");
    const auto file = openStorageFile(path);
    const auto writerHandle =
        CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    REQUIRE(writerHandle != INVALID_HANDLE_VALUE);
    winrt::handle writer{writerHandle};
    const auto result = SourceFileRevisionCalculator{}.calculate(file);
    REQUIRE(result.valueIfPresent() == nullptr);
    REQUIRE(result.errorIfPresent() != nullptr);
    // The broker reports E_ACCESSDENIED on this local filesystem even though
    // the independently established cause is conflicting write access. Preserve
    // the HRESULT rather than inventing a more precise native diagnosis.
    CHECK((result.errorIfPresent()->code == ImageProcessingErrorCode::SourceSharingViolation ||
           result.errorIfPresent()->code == ImageProcessingErrorCode::SourceAccessDenied));
    CHECK(result.errorIfPresent()->stage == ImageProcessingStage::SourceRevisionCapture);
    REQUIRE(
        std::holds_alternative<jpg_spinner::domain::WindowsHResult>(result.errorIfPresent()->nativeErrorProjection));
    writer.close();
    REQUIRE(capture(path).valueIfPresent() != nullptr);
    CHECK_FALSE(directory.removeOwnedDirectory());
}

TEST_CASE("real denied read permissions and missing files preserve native error context", "[storage][revision]")
{
    TestApartment apartment;
    TemporaryDirectory directory{std::filesystem::current_path() / "artifacts" / "tests" / "storage"};
    const auto path = directory.directoryPath() / "source.jpg";
    writeFile(path, "abc");
    const auto file = openStorageFile(path);
    {
        FileReadDenial denial{path};
        const auto deniedHandle =
            CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        REQUIRE(deniedHandle == INVALID_HANDLE_VALUE);
        REQUIRE(GetLastError() == ERROR_ACCESS_DENIED);
        const auto result = SourceFileRevisionCalculator{}.calculate(file);
        requireFailure(result, ImageProcessingErrorCode::SourceAccessDenied);
        REQUIRE(std::holds_alternative<jpg_spinner::domain::WindowsHResult>(
            result.errorIfPresent()->nativeErrorProjection));
    }
    REQUIRE(capture(path).valueIfPresent() != nullptr);
    REQUIRE(std::filesystem::remove(path));
    const auto missing = SourceFileRevisionCalculator{}.calculate(file);
    requireFailure(missing, ImageProcessingErrorCode::SourceRevisionCaptureFailed);
    REQUIRE(
        std::holds_alternative<jpg_spinner::domain::WindowsHResult>(missing.errorIfPresent()->nativeErrorProjection));
    CHECK_FALSE(directory.removeOwnedDirectory());
}

TEST_CASE("replacement between streamed blocks cannot return a mixed-content revision", "[storage][revision]")
{
    TestApartment apartment;
    TemporaryDirectory directory{std::filesystem::current_path() / "artifacts" / "tests" / "storage"};
    const auto path = directory.directoryPath() / "source.jpg";
    const auto replacementPath = directory.directoryPath() / "replacement.jpg";
    writeFile(path, std::string(200'000, 'a'));
    writeFile(replacementPath, std::string(200'000, 'b'));
    // Make the change oracle independent of filesystem timestamp granularity.
    // Same-length/time replacement needs a later independent digest comparison;
    // this capture test specifically exercises observable metadata change.
    std::filesystem::last_write_time(replacementPath, std::filesystem::last_write_time(path) + std::chrono::hours{1});
    REQUIRE(std::filesystem::last_write_time(replacementPath) != std::filesystem::last_write_time(path));
    bool replacementAttempted = false;
    bool replacementSucceeded = false;
    DWORD replacementError = ERROR_SUCCESS;
    const auto result = capture(path, {}, [&](const auto &progress) {
        if (progress.encodedBytesHashed > 0 && !replacementAttempted)
        {
            replacementAttempted = true;
            replacementSucceeded = MoveFileExW(replacementPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
            if (!replacementSucceeded)
                replacementError = GetLastError();
        }
    });
    REQUIRE(replacementAttempted);
    INFO("Replacement HRESULT source: " << replacementError);
    // Reader-only sharing can reject replacement outright. That access failure
    // is the stronger safe outcome, not a reason to loosen the production lock
    // merely to exercise a post-mutation rejection branch.
    if (replacementSucceeded)
    {
        REQUIRE(result.valueIfPresent() == nullptr);
        REQUIRE(result.errorIfPresent() != nullptr);
        CHECK((result.errorIfPresent()->code == ImageProcessingErrorCode::SourceChangedAfterAnalysis ||
               result.errorIfPresent()->code == ImageProcessingErrorCode::SourceAccessDenied ||
               result.errorIfPresent()->code == ImageProcessingErrorCode::SourceSharingViolation));
        CHECK(result.errorIfPresent()->stage == ImageProcessingStage::SourceRevisionCapture);
    }
    else
    {
        CHECK((replacementError == ERROR_ACCESS_DENIED || replacementError == ERROR_SHARING_VIOLATION));
        REQUIRE(result.valueIfPresent() != nullptr);
        REQUIRE(std::filesystem::exists(replacementPath));
        const auto after = capture(path);
        REQUIRE(after.valueIfPresent() != nullptr);
        CHECK(*after.valueIfPresent() == *result.valueIfPresent());
        std::ifstream input{path, std::ios::binary};
        const std::string retainedBytes{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
        CHECK(retainedBytes == std::string(200'000, 'a'));
    }
    CHECK_FALSE(directory.removeOwnedDirectory());
}

TEST_CASE("a modification instant changed during streaming discards the digest", "[storage][revision]")
{
    TestApartment apartment;
    TemporaryDirectory directory{std::filesystem::current_path() / "artifacts" / "tests" / "storage"};
    const auto path = directory.directoryPath() / "source.jpg";
    writeFile(path, std::string(200'000, 'a'));
    const auto originalTime = std::filesystem::last_write_time(path);
    bool changed = false;
    const auto result = capture(path, {}, [&](const auto &progress) {
        if (progress.encodedBytesHashed > 0 && !changed)
        {
            std::filesystem::last_write_time(path, originalTime + std::chrono::hours{1});
            changed = true;
        }
    });
    REQUIRE(changed);
    REQUIRE(std::filesystem::last_write_time(path) != originalTime);
    requireFailure(result, ImageProcessingErrorCode::SourceChangedAfterAnalysis);
    CHECK_FALSE(directory.removeOwnedDirectory());
}

TEST_CASE("STA callers and null file capabilities fail before stream access", "[storage][revision]")
{
    TestApartment apartment;
    TemporaryDirectory directory{std::filesystem::current_path() / "artifacts" / "tests" / "storage"};
    const auto path = directory.directoryPath() / "source.jpg";
    writeFile(path, "abc");
    const auto file = openStorageFile(path);
    bool notified = false;
    const auto staResult =
        std::async(std::launch::async, [file, &notified] {
            TestApartment sta{winrt::apartment_type::single_threaded};
            return SourceFileRevisionCalculator{}.calculate(file, {}, [&](const auto &) { notified = true; });
        }).get();
    requireFailure(staResult, ImageProcessingErrorCode::SourceRevisionCaptureFailed);
    CHECK_FALSE(notified);
    REQUIRE(
        std::holds_alternative<jpg_spinner::domain::WindowsHResult>(staResult.errorIfPresent()->nativeErrorProjection));
    CHECK(
        std::get<jpg_spinner::domain::WindowsHResult>(staResult.errorIfPresent()->nativeErrorProjection).signedValue ==
        RPC_E_WRONG_THREAD);
    const auto nullResult = SourceFileRevisionCalculator{}.calculate(nullptr);
    requireFailure(nullResult, ImageProcessingErrorCode::SourceRevisionCaptureFailed);
    CHECK_FALSE(directory.removeOwnedDirectory());
}

TEST_CASE("an uninitialized thread in a process with an MTA fails before stream access", "[storage][revision]")
{
    // The main thread keeps an MTA alive. COM then reports an implicit MTA on
    // this fresh native worker even though that worker never initialized COM.
    TestApartment apartment;
    TemporaryDirectory directory{std::filesystem::current_path() / "artifacts" / "tests" / "storage"};
    const auto path = directory.directoryPath() / "source.jpg";
    writeFile(path, "abc");
    const auto file = openStorageFile(path);
    bool notified = false;
    const auto [apartmentResult, apartmentType, apartmentQualifier, result] =
        std::async(std::launch::async, [file, &notified] {
            APTTYPE workerApartmentType = APTTYPE_CURRENT;
            APTTYPEQUALIFIER workerApartmentQualifier = APTTYPEQUALIFIER_NONE;
            const auto workerApartmentResult = CoGetApartmentType(&workerApartmentType, &workerApartmentQualifier);
            return std::tuple{
                workerApartmentResult, workerApartmentType, workerApartmentQualifier,
                SourceFileRevisionCalculator{}.calculate(file, {}, [&](const auto &) { notified = true; })};
        }).get();
    // Catch2 assertions stay on the test thread, after the worker has joined.
    REQUIRE(apartmentResult == S_OK);
    REQUIRE(apartmentType == APTTYPE_MTA);
    REQUIRE(apartmentQualifier == APTTYPEQUALIFIER_IMPLICIT_MTA);
    requireFailure(result, ImageProcessingErrorCode::SourceRevisionCaptureFailed);
    CHECK_FALSE(notified);
    REQUIRE(
        std::holds_alternative<jpg_spinner::domain::WindowsHResult>(result.errorIfPresent()->nativeErrorProjection));
    CHECK(std::get<jpg_spinner::domain::WindowsHResult>(result.errorIfPresent()->nativeErrorProjection).signedValue ==
          CO_E_NOTINITIALIZED);
    CHECK_FALSE(directory.removeOwnedDirectory());
}
