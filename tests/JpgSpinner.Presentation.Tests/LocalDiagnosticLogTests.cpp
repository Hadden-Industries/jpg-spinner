#include "pch.h"
#include "Diagnostics/LocalDiagnosticLog.h"
#include "PresentationTestApartment.h"
#include <catch2/catch_test_macros.hpp>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <type_traits>
#include "TemporaryDirectory.h"
#include <filesystem>
#include <catch2/generators/catch_generators.hpp>
#include <jpg_spinner/storage/DirectChildStorageFileLease.h>

TEST_CASE("Diagnostics serialize typed safe fields as one native UTF-8 JSON Lines record",
          "[presentation][diagnostics]")
{
    using namespace jpg_spinner;
    using namespace std::chrono;
    test_support::ensurePresentationTestApartment();
    const diagnostics::ImageProcessingDiagnosticRecord record{sys_days{2026y / October / 14} + 12h + 34min + 56s +
                                                                  789ms,
                                                              {2, 0, 12, 3},
                                                              diagnostics::ApplicationArchitecture::X64,
                                                              winrt::guid{L"{442E70B0-7396-43C2-AED7-157B9EA2F2D2}"},
                                                              domain::ImageProcessingStage::JpegStructureValidation,
                                                              domain::ImageProcessingErrorCode::PixelCountLimitExceeded,
                                                              123ms,
                                                              domain::JpegResourceLimit::PixelCount};
    const auto encoded = diagnostics::serializeImageProcessingDiagnosticRecord(record);
    REQUIRE_FALSE(encoded.empty());
    CHECK(encoded.back() == '\n');
    CHECK(encoded.find('\n') == encoded.size() - 1);
    CHECK_FALSE(encoded.starts_with("\xef\xbb\xbf"));
    const auto json = winrt::Windows::Data::Json::JsonObject::Parse(winrt::to_hstring(encoded));
    REQUIRE(json.Size() == 8);
    CHECK(json.GetNamedString(L"recordedAtUtc") == L"2026-10-14T12:34:56.789Z");
    CHECK(json.GetNamedString(L"applicationVersion") == L"2.0.12.3");
    CHECK(json.GetNamedString(L"applicationArchitecture") == L"x64");
    CHECK(winrt::guid{std::wstring_view{json.GetNamedString(L"correlationIdentifier")}} ==
          record.correlationIdentifier);
    CHECK(json.GetNamedString(L"stage") == L"JpegStructureValidation");
    CHECK(json.GetNamedString(L"errorCode") == L"PixelCountLimitExceeded");
    CHECK(json.GetNamedNumber(L"elapsedMilliseconds") == 123);
    CHECK(json.GetNamedString(L"resourceLimit") == L"PixelCount");
    CHECK(encoded == diagnostics::serializeImageProcessingDiagnosticRecord(record));
    // A private path or native message cannot enter the API. This compile-time
    // input-domain check complements the real serializer/schema assertions;
    // it does not claim to test image metadata redaction after unsafe admission.
    STATIC_REQUIRE_FALSE(std::is_constructible_v<diagnostics::ImageProcessingDiagnosticRecord, std::wstring>);
    STATIC_REQUIRE_FALSE(
        std::is_invocable_v<decltype(diagnostics::serializeImageProcessingDiagnosticRecord), std::wstring>);
}

TEST_CASE("Diagnostics reject invalid UUIDs and durations before admitting a record", "[presentation][diagnostics]")
{
    using namespace jpg_spinner;
    using namespace std::chrono;
    test_support::ensurePresentationTestApartment();
    diagnostics::ImageProcessingDiagnosticRecord record{sys_days{2026y / October / 14},
                                                        {2, 0, 0, 0},
                                                        diagnostics::ApplicationArchitecture::X86,
                                                        winrt::guid{L"{442E70B0-7396-43C2-AED7-157B9EA2F2D2}"},
                                                        domain::ImageProcessingStage::TransactionRecovery,
                                                        std::nullopt,
                                                        0ms,
                                                        std::nullopt};
    const auto invalidCase = GENERATE(0, 1, 2, 3);
    if (invalidCase == 0)
        record.correlationIdentifier = {};
    if (invalidCase == 1)
        record.elapsedTime = -1ms;
    if (invalidCase == 2)
        record.elapsedTime = milliseconds{9'007'199'254'740'992LL};
    if (invalidCase == 3)
        record.recordedAtUtc = sys_days{year{0} / January / 1};
    CHECK_THROWS_AS(diagnostics::serializeImageProcessingDiagnosticRecord(record), winrt::hresult_invalid_argument);
}

TEST_CASE("Diagnostic retention bounds bytes independently of file count",
          "[presentation][diagnostics][diagnostic-storage]")
{
    using namespace jpg_spinner;
    using namespace std::chrono;
    test_support::ensurePresentationTestApartment();
    const test_support::TemporaryDirectory directory{std::filesystem::temp_directory_path() /
                                                     L"jpg-spinner-diagnostic-tests"};
    const auto folder =
        winrt::Windows::Storage::StorageFolder::GetFolderFromPathAsync(directory.directoryPath().native()).get();
    diagnostics::ImageProcessingDiagnosticRecord record{sys_days{2026y / October / 14},
                                                        {2, 0, 0, 0},
                                                        diagnostics::ApplicationArchitecture::X64,
                                                        winrt::guid{L"{442E70B0-7396-43C2-AED7-157B9EA2F2D2}"},
                                                        domain::ImageProcessingStage::TransactionRecovery,
                                                        std::nullopt,
                                                        0ms,
                                                        std::nullopt};
    const auto line = diagnostics::serializeImageProcessingDiagnosticRecord(record);
    diagnostics::LocalDiagnosticLog log{folder, {line.size() + 1, 64}};
    REQUIRE(log.append(record));
    record.elapsedTime = 1ms;
    REQUIRE(log.append(record));
    const auto second = diagnostics::serializeImageProcessingDiagnosticRecord(record);
    CHECK(log.snapshot() == second); // Byte quota forces rotation before count quota.
    diagnostics::LocalDiagnosticLog tooSmall{folder, {1, 64}};
    CHECK_FALSE(tooSmall.append(record));
    CHECK(log.snapshot() == second); // Refusal cannot delete the earlier record.
}

TEST_CASE("An interrupted diagnostic publication preserves retained records and permits a later append",
          "[presentation][diagnostics][diagnostic-storage]")
{
    using namespace jpg_spinner;
    using namespace std::chrono;
    test_support::ensurePresentationTestApartment();
    const test_support::TemporaryDirectory directory{std::filesystem::temp_directory_path() /
                                                     L"jpg-spinner-diagnostic-tests"};
    const auto folder =
        winrt::Windows::Storage::StorageFolder::GetFolderFromPathAsync(directory.directoryPath().native()).get();
    diagnostics::ImageProcessingDiagnosticRecord record{sys_days{2026y / October / 14},
                                                        {2, 0, 0, 0},
                                                        diagnostics::ApplicationArchitecture::X64,
                                                        winrt::guid{L"{442E70B0-7396-43C2-AED7-157B9EA2F2D2}"},
                                                        domain::ImageProcessingStage::TransactionRecovery,
                                                        std::nullopt,
                                                        0ms,
                                                        std::nullopt};
    diagnostics::LocalDiagnosticLog log{folder};
    REQUIRE(log.append(record));
    const auto first = diagnostics::serializeImageProcessingDiagnosticRecord(record);
    // The dedicated store owns this reserved single scratch slot, not arbitrary
    // filenames. A crash before publication can leave partial bytes here.
    const auto pending = folder.CreateFileAsync(L".pending-diagnostic-record.jsonl").get();
    winrt::Windows::Storage::FileIO::WriteTextAsync(pending, L"{partial").get();
    CHECK(log.snapshot() == first); // Never export incomplete scratch bytes.
    record.elapsedTime = 1ms;
    REQUIRE(log.append(record));
    CHECK(log.snapshot() == first + diagnostics::serializeImageProcessingDiagnosticRecord(record));
    CHECK(folder.TryGetItemAsync(L".pending-diagnostic-record.jsonl").get() == nullptr);
}

TEST_CASE("Diagnostic export refuses substituted private content and leaves it intact",
          "[presentation][diagnostics][diagnostic-storage]")
{
    using namespace jpg_spinner;
    test_support::ensurePresentationTestApartment();
    const test_support::TemporaryDirectory directory{std::filesystem::temp_directory_path() /
                                                     L"jpg-spinner-diagnostic-tests"};
    const auto folder =
        winrt::Windows::Storage::StorageFolder::GetFolderFromPathAsync(directory.directoryPath().native()).get();
    const auto file =
        folder.CreateFileAsync(L"diagnostic-00000000000000000001-{442E70B0-7396-43C2-AED7-157B9EA2F2D2}.jsonl").get();
    const auto payload = GENERATE(L"{\"sourcePath\":\"C:\\\\Users\\\\Private\\\\secret.jpg\"}\n",
                                  L"{\"metadata\":\"private Exif/XMP value\"}\n", L"[]\n",
                                  L"{\"nativeMessage\":\"private native message\"}\n");
    winrt::Windows::Storage::FileIO::WriteTextAsync(file, payload).get();
    diagnostics::LocalDiagnosticLog log{folder};
    CHECK_THROWS(log.snapshot());
    CHECK(winrt::Windows::Storage::FileIO::ReadTextAsync(file).get() == payload);
}

TEST_CASE("Every diagnostic field rejects private text substituted into an otherwise valid record",
          "[presentation][diagnostics][diagnostic-storage]")
{
    using namespace jpg_spinner;
    using namespace std::chrono;
    test_support::ensurePresentationTestApartment();
    const test_support::TemporaryDirectory directory{std::filesystem::temp_directory_path() /
                                                     L"jpg-spinner-diagnostic-tests"};
    const auto folder =
        winrt::Windows::Storage::StorageFolder::GetFolderFromPathAsync(directory.directoryPath().native()).get();
    const diagnostics::ImageProcessingDiagnosticRecord record{sys_days{2026y / October / 14},
                                                              {2, 0, 0, 0},
                                                              diagnostics::ApplicationArchitecture::X64,
                                                              winrt::guid{L"{442E70B0-7396-43C2-AED7-157B9EA2F2D2}"},
                                                              domain::ImageProcessingStage::TransactionRecovery,
                                                              std::nullopt,
                                                              0ms,
                                                              std::nullopt};
    const auto field =
        GENERATE(L"recordedAtUtc", L"applicationVersion", L"applicationArchitecture", L"correlationIdentifier",
                 L"stage", L"errorCode", L"elapsedMilliseconds", L"resourceLimit");
    const auto encoded = diagnostics::serializeImageProcessingDiagnosticRecord(record);
    auto tampered = winrt::Windows::Data::Json::JsonObject::Parse(winrt::to_hstring(encoded));
    tampered.Insert(field, winrt::Windows::Data::Json::JsonValue::CreateStringValue(
                               L"C:\\private\\source.jpg; private Exif/XMP/native message"));
    const auto file =
        folder.CreateFileAsync(L"diagnostic-00000000000000000001-{442E70B0-7396-43C2-AED7-157B9EA2F2D2}.jsonl").get();
    const auto payload = tampered.Stringify() + L"\n";
    winrt::Windows::Storage::FileIO::WriteTextAsync(file, payload).get();
    diagnostics::LocalDiagnosticLog log{folder};
    CHECK_THROWS(log.snapshot());
    CHECK_FALSE(log.append(record));
    CHECK(winrt::Windows::Storage::FileIO::ReadTextAsync(file).get() == payload);
}

TEST_CASE("Diagnostic storage never exports or changes an escaping link target",
          "[presentation][diagnostics][diagnostic-storage][reparse-control]")
{
    using namespace jpg_spinner;
    using namespace std::chrono;
    test_support::ensurePresentationTestApartment();
    const test_support::TemporaryDirectory directory{std::filesystem::temp_directory_path() /
                                                     L"jpg-spinner-diagnostic-tests"};
    const auto root =
        winrt::Windows::Storage::StorageFolder::GetFolderFromPathAsync(directory.directoryPath().native()).get();
    const auto folder = root.CreateFolderAsync(L"private-log-store").get();
    const auto foreign = root.CreateFileAsync(L"foreign-record.jsonl").get();
    const diagnostics::ImageProcessingDiagnosticRecord record{sys_days{2026y / October / 14},
                                                              {2, 0, 0, 0},
                                                              diagnostics::ApplicationArchitecture::X64,
                                                              winrt::guid{L"{442E70B0-7396-43C2-AED7-157B9EA2F2D2}"},
                                                              domain::ImageProcessingStage::TransactionRecovery,
                                                              std::nullopt,
                                                              0ms,
                                                              std::nullopt};
    const auto line = diagnostics::serializeImageProcessingDiagnosticRecord(record);
    winrt::Windows::Storage::FileIO::WriteTextAsync(foreign, winrt::to_hstring(line)).get();
    const auto link = std::filesystem::path{folder.Path().c_str()} /
                      L"diagnostic-00000000000000000001-{442E70B0-7396-43C2-AED7-157B9EA2F2D2}.jsonl";
    REQUIRE(CreateSymbolicLinkW(link.c_str(), foreign.Path().c_str(), SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE));
    REQUIRE((GetFileAttributesW(link.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) != 0);
    const auto listedFiles = folder.GetFilesAsync().get();
    const auto listedItems = folder.GetItemsAsync().get();
    INFO("Native listed files=" << listedFiles.Size() << "; items=" << listedItems.Size());
    diagnostics::LocalDiagnosticLog log{folder};
    if (listedFiles.Size() == 0)
    {
        // Native Storage omits unsupported file links on this host. Omission
        // isn't evidence of a complete physical inventory or a reason to add
        // a path-based fallback. Only newly published owned records may appear.
        CHECK(log.snapshot().empty());
        REQUIRE(log.append(record));
        CHECK(log.snapshot() == line);
    }
    else
    {
        // When a provider reports the entry, native containment must refuse
        // its escaping target before export or diagnostic-record rotation.
        CHECK_THROWS(log.snapshot());
        CHECK_FALSE(log.append(record));
    }
    CHECK((GetFileAttributesW(link.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) != 0);
    CHECK(winrt::Windows::Storage::FileIO::ReadTextAsync(foreign).get() == winrt::to_hstring(line));
}

TEST_CASE("A direct-child storage lease rejects an actual foreign file grant",
          "[presentation][diagnostics][diagnostic-storage][containment-control]")
{
    using namespace jpg_spinner;
    test_support::ensurePresentationTestApartment();
    const test_support::TemporaryDirectory directory{std::filesystem::temp_directory_path() /
                                                     L"jpg-spinner-diagnostic-tests"};
    const auto root =
        winrt::Windows::Storage::StorageFolder::GetFolderFromPathAsync(directory.directoryPath().native()).get();
    const auto folder = root.CreateFolderAsync(L"private-log-store").get();
    const auto foreign = root.CreateFileAsync(L"foreign.txt").get();
    winrt::Windows::Storage::FileIO::WriteTextAsync(foreign, L"independent foreign bytes").get();
    const auto child = folder.CreateFileAsync(L"owned.txt").get();
    CHECK_NOTHROW(storage::DirectChildStorageFileLease(folder, child));
    CHECK_THROWS_AS(storage::DirectChildStorageFileLease(folder, foreign), winrt::hresult_invalid_argument);
    CHECK(winrt::Windows::Storage::FileIO::ReadTextAsync(foreign).get() == L"independent foreign bytes");
}

TEST_CASE("Local diagnostics publish bounded immutable records and rotate oldest records only",
          "[presentation][diagnostics][diagnostic-storage]")
{
    using namespace jpg_spinner;
    using namespace std::chrono;
    test_support::ensurePresentationTestApartment();
    const test_support::TemporaryDirectory directory{std::filesystem::temp_directory_path() /
                                                     L"jpg-spinner-diagnostic-tests"};
    const auto folder =
        winrt::Windows::Storage::StorageFolder::GetFolderFromPathAsync(directory.directoryPath().native()).get();
    const diagnostics::DiagnosticLogRetentionLimits limits{1024, 2};
    diagnostics::LocalDiagnosticLog log{folder, limits};
    diagnostics::ImageProcessingDiagnosticRecord record{sys_days{2026y / October / 14},
                                                        {2, 0, 0, 0},
                                                        diagnostics::ApplicationArchitecture::X64,
                                                        winrt::guid{L"{442E70B0-7396-43C2-AED7-157B9EA2F2D2}"},
                                                        domain::ImageProcessingStage::TransactionRecovery,
                                                        std::nullopt,
                                                        0ms,
                                                        std::nullopt};
    REQUIRE(log.append(record));
    const auto first = diagnostics::serializeImageProcessingDiagnosticRecord(record);
    CHECK(log.snapshot() == first);
    record.elapsedTime = 1ms;
    REQUIRE(log.append(record));
    const auto second = diagnostics::serializeImageProcessingDiagnosticRecord(record);
    record.elapsedTime = 2ms;
    REQUIRE(log.append(record));
    const auto third = diagnostics::serializeImageProcessingDiagnosticRecord(record);
    CHECK(log.snapshot() == second + third);
    CHECK(log.snapshot().size() <= limits.maximumRetainedLengthBytes);
    CHECK(folder.GetFilesAsync().get().Size() <= limits.maximumRetainedFileCount + 1); // Empty writer lease.
    diagnostics::LocalDiagnosticLog reopened{folder, limits};
    CHECK(reopened.snapshot() == second + third); // Real persisted data, not an in-memory substitute.
}
