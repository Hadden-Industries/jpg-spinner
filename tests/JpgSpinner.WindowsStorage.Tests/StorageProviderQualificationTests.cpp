#define NOMINMAX
#include <windows.h>
#include "TemporaryDirectory.h"
#include "DeterministicJpegFixtureFactory.h"
#include <jpg_spinner/jpeg/JpegImageAnalyzer.h>
#include <jpg_spinner/jpeg/LibJpegTurboTransformationEngine.h>
#include <jpg_spinner/storage/WindowsStorageImageFileTransactionEngine.h>
#include <jpg_spinner/storage/SourceFileRevisionCalculator.h>
#include <catch2/catch_test_macros.hpp>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace jpg_spinner;

namespace
{
/// Default qualification is the OS temporary volume. An explicit provider root
/// selects only where an exclusive UUID-owned generated fixture is created; no
/// existing images or root-level volume settings are changed. An unavailable
/// requested root is failure, not fallback to a different tested filesystem.
std::filesystem::path qualificationRoot()
{
    const auto length = GetEnvironmentVariableW(L"JPG_SPINNER_PROVIDER_QUALIFICATION_ROOT", nullptr, 0);
    if (length == 0)
        return std::filesystem::temp_directory_path() / "jpg-spinner-provider-tests";
    std::wstring path(length, L'\0');
    const auto copied = GetEnvironmentVariableW(L"JPG_SPINNER_PROVIDER_QUALIFICATION_ROOT", path.data(), length);
    REQUIRE(copied > 0);
    REQUIRE(copied < length);
    path.resize(copied);
    REQUIRE(std::filesystem::is_directory(std::filesystem::path{path}));
    return std::filesystem::path{path};
}
} // namespace

TEST_CASE("a physical provider is either qualified for identity-bound transactions or refused before image writes",
          "[storage][transaction][provider]")
{
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    struct ApartmentLifetime final
    {
        ~ApartmentLifetime()
        {
            winrt::uninit_apartment();
        }
    } apartment;
    test_support::TemporaryDirectory directory{qualificationRoot()};
    test_support::TemporaryDirectory journalDirectory{std::filesystem::temp_directory_path() /
                                                      "jpg-spinner-provider-journals"};
    const auto sourceBytes = test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
        test_support::JpegChromaSubsampling::ycbcr444, 3);
    const auto sourcePath = directory.directoryPath() / "camera.jpg";
    {
        std::ofstream stream{sourcePath, std::ios::binary};
        stream.write(reinterpret_cast<const char *>(sourceBytes.data()),
                     static_cast<std::streamsize>(sourceBytes.size()));
        stream.close();
        REQUIRE(stream.good());
    }
    const auto handle = CreateFileW(directory.directoryPath().c_str(), FILE_READ_ATTRIBUTES,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                    FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    REQUIRE(handle != INVALID_HANDLE_VALUE);
    winrt::handle volumeHandle{handle};
    std::array<wchar_t, MAX_PATH + 1> fileSystemName{};
    REQUIRE(GetVolumeInformationByHandleW(handle, nullptr, 0, nullptr, nullptr, nullptr, fileSystemName.data(),
                                          static_cast<DWORD>(fileSystemName.size())));
    const std::wstring fileSystem{fileSystemName.data()};
    INFO("Observed physical provider filesystem: " << winrt::to_string(fileSystem));
    const auto source = winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(sourcePath.wstring()).get();
    const auto selectedRoot =
        winrt::Windows::Storage::StorageFolder::GetFolderFromPathAsync(directory.directoryPath().wstring()).get();
    const auto store =
        winrt::Windows::Storage::StorageFolder::GetFolderFromPathAsync(journalDirectory.directoryPath().wstring())
            .get();
    const auto sourceRevision = storage::SourceFileRevisionCalculator{}.calculate(source);
    REQUIRE(sourceRevision.valueIfPresent());
    const auto analysis =
        jpeg::JpegImageAnalyzer::analyze(sourceBytes, domain::EdgeHandlingPolicy::TrimPartialMinimumCodedUnits);
    REQUIRE(analysis.valueIfPresent());
    const auto output =
        jpeg::LibJpegTurboTransformationEngine{}.createValidatedOutput(sourceBytes, *analysis.valueIfPresent());
    REQUIRE(output.valueIfPresent());
    const storage::WindowsStorageImageFileTransactionEngine engine{selectedRoot, store};
    const auto result = engine.execute(
        {source, *sourceRevision.valueIfPresent(), domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup},
        *output.valueIfPresent());
    if (result.errorIfPresent())
    {
        INFO("Observed transaction phase: " << static_cast<unsigned>(result.errorIfPresent()->stage));
        const auto native = std::get_if<domain::WindowsHResult>(&result.errorIfPresent()->nativeErrorProjection);
        INFO("Observed native HRESULT: " << (native ? native->signedValue : 0));
    }
    if (fileSystem == L"NTFS")
    {
        REQUIRE(result.valueIfPresent());
        const auto committed =
            storage::SourceFileRevisionCalculator{}.calculate(result.valueIfPresent()->committedFile);
        REQUIRE(committed.valueIfPresent());
        CHECK(committed.valueIfPresent()->encodedSha256 == output.valueIfPresent()->encodedSha256());
        const auto recovery = engine.recoverIncompleteTransactions();
        REQUIRE(recovery.valueIfPresent());
        CHECK(recovery.valueIfPresent()->recoveredTransactions.empty());
    }
    else
    {
        // NTFS is the current qualified physical filesystem. FAT identifiers can
        // change on rename; unavailable ReFS/cloud environments are not inferred
        // qualified merely because a driver can return a file-ID-shaped value.
        REQUIRE(result.errorIfPresent());
        CHECK(result.errorIfPresent()->code ==
              domain::ImageProcessingErrorCode::StorageProviderRecoveryContractNotEstablished);
        CHECK(result.errorIfPresent()->stage == domain::ImageProcessingStage::TransactionRecoverabilityPreflight);
        CHECK_FALSE(std::filesystem::exists(directory.directoryPath() / "JPG Spinner Output"));
        CHECK_FALSE(std::filesystem::exists(directory.directoryPath() / "JPG Spinner Backups"));
        std::ifstream stream{sourcePath, std::ios::binary};
        const std::vector<char> retained{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
        REQUIRE(retained.size() == sourceBytes.size());
        CHECK(std::equal(retained.begin(), retained.end(), sourceBytes.begin(), [](char observed, std::byte expected) {
            return static_cast<unsigned char>(observed) == std::to_integer<unsigned char>(expected);
        }));
        CHECK(store.GetFoldersAsync().get().Size() == 0);
    }
}
