#include "pch.h"
#include "Diagnostics/DiagnosticLogExport.h"
#include "PresentationTestApartment.h"
#include "TemporaryDirectory.h"
#include <catch2/catch_test_macros.hpp>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Storage.Provider.h>
#include <filesystem>
#include <vector>
#include <wil/resource.h>
#include <bit>

namespace
{
/// A real caller-supplied snapshot is the export contract's input, not an
/// expected value computed by the export implementation. Its independent
/// field oracle below also prevents empty/unsafe logger output masking a pass.
void requireNativeDiagnosticExport(const winrt::Windows::Storage::StorageFolder &root, bool expectsProviderRefusal)
{
    using namespace jpg_spinner;
    using namespace winrt::Windows::Storage;
    using namespace std::chrono;
    const auto privateLog = root.CreateFolderAsync(L"diagnostics").get();
    const auto log = std::make_shared<diagnostics::LocalDiagnosticLog>(privateLog);
    const diagnostics::ImageProcessingDiagnosticRecord record{sys_days{2026y / October / 14},
                                                              {2, 0, 0, 0},
                                                              diagnostics::ApplicationArchitecture::X64,
                                                              winrt::guid{L"{442E70B0-7396-43C2-AED7-157B9EA2F2D2}"},
                                                              domain::ImageProcessingStage::TransactionRecovery,
                                                              domain::ImageProcessingErrorCode::RecoveryConflict,
                                                              17ms,
                                                              std::nullopt};
    REQUIRE(log->append(record));
    const auto before = log->snapshot();
    REQUIRE_FALSE(before.empty());
    const auto json = winrt::Windows::Data::Json::JsonObject::Parse(winrt::to_hstring(before));
    REQUIRE(json.Size() == 8);
    REQUIRE(json.GetNamedString(L"errorCode") == L"RecoveryConflict");
    REQUIRE(json.GetNamedNumber(L"elapsedMilliseconds") == 17);
    const auto destination = root.CreateFileAsync(L"export.jsonl").get();
    FileIO::WriteTextAsync(destination, L"independent pre-existing destination bytes").get();
    // Native provider registration differs between the unpackaged fixture and
    // real AppContainer. Prove the selected fixture's independent native
    // precondition before testing completed export or refusal propagation.
    const auto requireMissingProvider = [&](auto &&operation) {
        try
        {
            operation();
            FAIL("The native fixture unexpectedly has cached-provider completion metadata.");
        }
        catch (const winrt::hresult_error &error)
        {
            CHECK(error.code() == HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
        }
    };
    if (expectsProviderRefusal)
    {
        requireMissingProvider(
            [&] { winrt::Windows::Storage::CachedFileManager::CompleteUpdatesAsync(destination).get(); });
        requireMissingProvider([&] { diagnostics::exportDiagnosticLogAsync(log, destination).get(); });
    }
    else
    {
        REQUIRE(winrt::Windows::Storage::CachedFileManager::CompleteUpdatesAsync(destination).get() ==
                winrt::Windows::Storage::Provider::FileUpdateStatus::Complete);
        CHECK_NOTHROW(diagnostics::exportDiagnosticLogAsync(log, destination).get());
    }
    // Read bytes rather than text: conversion must not hide an inserted BOM,
    // newline conversion or accidental UTF-16 encoding.
    const auto bytes = FileIO::ReadBufferAsync(destination).get();
    const auto reader = winrt::Windows::Storage::Streams::DataReader::FromBuffer(bytes);
    std::vector<std::uint8_t> actual(bytes.Length());
    reader.ReadBytes(actual);
    REQUIRE(actual.size() == before.size());
    CHECK(std::string(actual.begin(), actual.end()) == before);
    CHECK(log->snapshot() == before); // Export is not retention mutation.
}
} // namespace

TEST_CASE("Diagnostic export writes sanitized bytes without masking unavailable native provider completion",
          "[presentation][diagnostics][diagnostic-export-provider]")
{
    jpg_spinner::test_support::ensurePresentationTestApartment();
    const jpg_spinner::test_support::TemporaryDirectory directory{std::filesystem::temp_directory_path() /
                                                                  L"jpg-spinner-diagnostic-export-tests"};
    const auto root =
        winrt::Windows::Storage::StorageFolder::GetFolderFromPathAsync(directory.directoryPath().native()).get();
    requireNativeDiagnosticExport(root, true);
}

TEST_CASE("Diagnostic export rejects absent or wrong-type grants without changing the destination",
          "[presentation][diagnostics][diagnostic-export-provider]")
{
    using namespace jpg_spinner;
    using namespace winrt::Windows::Storage;
    test_support::ensurePresentationTestApartment();
    const test_support::TemporaryDirectory directory{std::filesystem::temp_directory_path() /
                                                     L"jpg-spinner-diagnostic-export-tests"};
    const auto root = StorageFolder::GetFolderFromPathAsync(directory.directoryPath().native()).get();
    const auto log = std::make_shared<diagnostics::LocalDiagnosticLog>(root.CreateFolderAsync(L"diagnostics").get());
    const auto destination = root.CreateFileAsync(L"original.jpg").get();
    FileIO::WriteTextAsync(destination, L"original must remain unchanged").get();
    CHECK_THROWS_AS(diagnostics::exportDiagnosticLogAsync(log, destination).get(), winrt::hresult_invalid_argument);
    CHECK_THROWS_AS(diagnostics::exportDiagnosticLogAsync(log, nullptr).get(), winrt::hresult_invalid_argument);
    CHECK_THROWS_AS(diagnostics::exportDiagnosticLogAsync(nullptr, destination).get(), winrt::hresult_invalid_argument);
    CHECK(FileIO::ReadTextAsync(destination).get() == L"original must remain unchanged");
}

TEST_CASE("A rejected diagnostic snapshot never overwrites existing destination bytes",
          "[presentation][diagnostics][diagnostic-export-provider]")
{
    using namespace jpg_spinner;
    using namespace winrt::Windows::Storage;
    test_support::ensurePresentationTestApartment();
    const test_support::TemporaryDirectory directory{std::filesystem::temp_directory_path() /
                                                     L"jpg-spinner-diagnostic-export-tests"};
    const auto root = StorageFolder::GetFolderFromPathAsync(directory.directoryPath().native()).get();
    const auto privateLog = root.CreateFolderAsync(L"diagnostics").get();
    const auto corruptRecord = privateLog.CreateFileAsync(L"unknown.jsonl").get();
    FileIO::WriteTextAsync(corruptRecord, L"private metadata must not be exported").get();
    const auto destination = root.CreateFileAsync(L"export.jsonl").get();
    FileIO::WriteTextAsync(destination, L"independent destination sentinel").get();
    const auto log = std::make_shared<diagnostics::LocalDiagnosticLog>(privateLog);
    CHECK_THROWS(diagnostics::exportDiagnosticLogAsync(log, destination).get());
    CHECK(FileIO::ReadTextAsync(destination).get() == L"independent destination sentinel");
    CHECK(FileIO::ReadTextAsync(corruptRecord).get() == L"private metadata must not be exported");
}

TEST_CASE("The actual AppContainer exports exact sanitized bytes with native provider completion",
          "[presentation][diagnostics][packaged-runtime][packaged-access-worker][source-access]")
{
    using namespace winrt::Windows::Storage;
    jpg_spinner::test_support::ensurePresentationTestApartment();
    winrt::handle token;
    REQUIRE(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, token.put()));
    DWORD isAppContainer{}, length{};
    REQUIRE(GetTokenInformation(token.get(), TokenIsAppContainer, &isAppContainer, sizeof(isAppContainer), &length));
    REQUIRE(isAppContainer == 1);
    GUID identity{};
    REQUIRE(SUCCEEDED(CoCreateGuid(&identity)));
    const auto root = ApplicationData::Current()
                          .LocalFolder()
                          .CreateFolderAsync(winrt::to_hstring(std::bit_cast<winrt::guid>(identity)))
                          .get();
    const auto cleanup = wil::scope_exit([&] { root.DeleteAsync().get(); }); // Only this newly created UUID child.
    // This is native application-owned test data, not a source-grant bypass.
    // The independently qualified picker still owns external file selection.
    requireNativeDiagnosticExport(root, false);
}
