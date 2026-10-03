#define NOMINMAX
#include <windows.h>
#include "DeterministicJpegFixtureFactory.h"
#include "TemporaryDirectory.h"
#include "DeterministicImageFileTransactionEngine.h"
#include <jpg_spinner/jpeg/JpegImageAnalyzer.h>
#include <jpg_spinner/jpeg/LibJpegTurboTransformationEngine.h>
#include <jpg_spinner/storage/SourceFileRevisionCalculator.h>
#include <jpg_spinner/storage/WindowsStorageImageFileTransactionEngine.h>
#include "internal/FileTransactionOperations.h"
#include "internal/RecoverableImageFileTransaction.h"
#include "internal/CorrectedCopyPathPolicy.h"
#include "internal/ImageFileTransactionJournal.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <array>
#include <span>
#include <filesystem>
#include <fstream>
#include <future>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

using namespace jpg_spinner;

namespace
{
// These are generated, UUID-owned fixture bytes, not user data. Default deletion
// delegates to Explorer policy and can accumulate fixtures in the Recycle Bin.
constexpr auto ownedTestFixtureDeletionOption = winrt::Windows::Storage::StorageDeleteOption::PermanentDelete;

/// Projection objects must die before this test-owned explicit MTA lifetime.
struct TransactionTestApartment final
{
    explicit TransactionTestApartment(winrt::apartment_type type = winrt::apartment_type::multi_threaded)
    {
        winrt::init_apartment(type);
    }
    ~TransactionTestApartment()
    {
        winrt::uninit_apartment();
    }
    TransactionTestApartment(const TransactionTestApartment &) = delete;
    TransactionTestApartment &operator=(const TransactionTestApartment &) = delete;
};

void writeBytes(const std::filesystem::path &path, std::span<const std::byte> bytes)
{
    std::ofstream stream{path, std::ios::binary};
    stream.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    stream.close();
    REQUIRE(stream.good());
    REQUIRE(std::filesystem::file_size(path) == bytes.size());
}

std::vector<std::byte> readBytes(const std::filesystem::path &path)
{
    const auto length = std::filesystem::file_size(path);
    REQUIRE(length <= 1024 * 1024);
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    std::ifstream stream{path, std::ios::binary};
    stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    REQUIRE(stream.good());
    return bytes;
}

/// Real source, analyzer and validator setup. This owns resources, not an oracle.
struct TransactionFixture final
{
    // Fault fixtures need the particular native effect to be reachable: a
    // deep runner directory can make their ordinary Win32/standard-library
    // probes fail on length before the injected fault. Use the OS-owned temp
    // location with one UUID-owned child. The public integration test retains
    // the runner directory, and [long-path] separately proves extended stages.
    test_support::TemporaryDirectory directory{std::filesystem::temp_directory_path() /
                                               "jpg-spinner-transaction-tests"};
    const std::vector<std::byte> sourceBytes = test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
        test_support::JpegChromaSubsampling::ycbcr444, 3);
    const domain::ImageProcessingResult<domain::JpegImageAnalysis> analysis =
        jpeg::JpegImageAnalyzer::analyze(sourceBytes, domain::EdgeHandlingPolicy::TrimPartialMinimumCodedUnits);
    const domain::ImageProcessingResult<domain::ValidatedJpegOutput> output = createOutput();
    const winrt::Windows::Storage::StorageFolder root =
        winrt::Windows::Storage::StorageFolder::GetFolderFromPathAsync(directory.directoryPath().wstring()).get();
    const winrt::Windows::Storage::StorageFolder journalStore = root.CreateFolderAsync(L"app-journal-store").get();

    domain::ImageProcessingResult<domain::ValidatedJpegOutput> createOutput() const
    {
        REQUIRE(analysis.valueIfPresent() != nullptr);
        return jpeg::LibJpegTurboTransformationEngine{}.createValidatedOutput(sourceBytes, *analysis.valueIfPresent());
    }
    auto createSource(const std::filesystem::path &relativePath) const
    {
        // std::filesystem accepts '/', but WinRT's absolute-path consumer
        // requires the native separator. Use the standard library conversion.
        const auto path = (directory.directoryPath() / relativePath).make_preferred();
        std::filesystem::create_directories(path.parent_path());
        writeBytes(path, sourceBytes);
        try
        {
            return winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(path.wstring()).get();
        }
        catch (const winrt::hresult_error &error)
        {
            FAIL_CHECK("Native fixture open failed with HRESULT " << error.code().value);
            throw;
        }
    }
    storage::ImageFileTransactionRequest requestFor(const winrt::Windows::Storage::StorageFile &source) const
    {
        const auto revision = storage::SourceFileRevisionCalculator{}.calculate(source);
        REQUIRE(revision.valueIfPresent() != nullptr);
        REQUIRE(output.valueIfPresent() != nullptr);
        return {source, *revision.valueIfPresent(), domain::OutputDisposition::CreateCorrectedCopy};
    }
    void requireNoStages() const
    {
        for (const auto &item : std::filesystem::recursive_directory_iterator(directory.directoryPath()))
            REQUIRE(!item.path().filename().wstring().starts_with(L".jpg-spinner-staged-"));
    }
};
} // namespace

TEST_CASE("a corrected copy commits the exact real validator capability without modifying its source",
          "[storage][transaction][copy]")
{
    TransactionTestApartment apartment;
    test_support::TemporaryDirectory directory{std::filesystem::current_path() / "artifacts/tests/storage"};
    const auto sourcePath = directory.directoryPath() / "camera.jpg";
    const auto sourceBytes = test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
        test_support::JpegChromaSubsampling::ycbcr444, 3);
    writeBytes(sourcePath, sourceBytes);
    // This established 31x19 fixture has incomplete edge MCUs. Explicitly select
    // the accepted trim policy rather than weakening perfect-transform analysis.
    const auto analysis =
        jpeg::JpegImageAnalyzer::analyze(sourceBytes, domain::EdgeHandlingPolicy::TrimPartialMinimumCodedUnits);
    REQUIRE(analysis.valueIfPresent() != nullptr);
    const auto output =
        jpeg::LibJpegTurboTransformationEngine{}.createValidatedOutput(sourceBytes, *analysis.valueIfPresent());
    REQUIRE(output.valueIfPresent() != nullptr);
    const auto sourceFile = winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(sourcePath.wstring()).get();
    const auto revision = storage::SourceFileRevisionCalculator{}.calculate(sourceFile);
    REQUIRE(revision.valueIfPresent() != nullptr);
    const auto root =
        winrt::Windows::Storage::StorageFolder::GetFolderFromPathAsync(directory.directoryPath().wstring()).get();
    const auto journalStore = root.CreateFolderAsync(L"app-journal-store").get();
    storage::WindowsStorageImageFileTransactionEngine nativeEngine{root, journalStore};
    const storage::ImageFileTransactionEngine &engine = nativeEngine;
    const auto result =
        engine.execute({sourceFile, *revision.valueIfPresent(), domain::OutputDisposition::CreateCorrectedCopy},
                       *output.valueIfPresent());
    if (const auto error = result.errorIfPresent())
    {
        // Keep native context at this real integration boundary: fixture path
        // depth can expose Windows I/O constraints that short-path runs miss.
        INFO("Source test path length: " << sourcePath.wstring().size());
        INFO("Failure stage: " << static_cast<unsigned>(error->stage));
        const auto nativeError = std::get_if<domain::WindowsHResult>(&error->nativeErrorProjection);
        INFO("Native HRESULT: " << (nativeError ? nativeError->signedValue : 0));
        REQUIRE(result.valueIfPresent() != nullptr);
    }
    REQUIRE(result.valueIfPresent() != nullptr);
    const auto committedPath = std::filesystem::path{result.valueIfPresent()->committedFile.Path().c_str()};
    CHECK(committedPath.filename() == "camera.jpg");
    CHECK(committedPath.parent_path().parent_path().filename() == "JPG Spinner Output");
    CHECK(readBytes(sourcePath) == sourceBytes);
    CHECK(readBytes(committedPath) == std::vector<std::byte>(output.valueIfPresent()->encodedBytes().begin(),
                                                             output.valueIfPresent()->encodedBytes().end()));
    // Observe the actual directory, not a transaction's claim that it cleaned up.
    for (const auto &item : std::filesystem::recursive_directory_iterator(directory.directoryPath()))
        CHECK(!item.path().filename().wstring().starts_with(L".jpg-spinner-staged-"));
}

TEST_CASE("one batch preserves nested relative paths and refuses an existing final filename",
          "[storage][transaction][copy]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    storage::WindowsStorageImageFileTransactionEngine engine{fixture.root, fixture.journalStore};
    const auto firstSource = fixture.createSource("first/camera.jpg");
    const auto secondSource = fixture.createSource("second/camera.jpg");
    const auto firstRequest = fixture.requestFor(firstSource);
    const auto first = engine.execute(firstRequest, *fixture.output.valueIfPresent());
    REQUIRE(first.valueIfPresent() != nullptr);
    const auto second = engine.execute(fixture.requestFor(secondSource), *fixture.output.valueIfPresent());
    REQUIRE(second.valueIfPresent() != nullptr);
    const std::filesystem::path firstPath{first.valueIfPresent()->committedFile.Path().c_str()};
    const std::filesystem::path secondPath{second.valueIfPresent()->committedFile.Path().c_str()};
    CHECK(firstPath.parent_path().filename() == "first");
    CHECK(secondPath.parent_path().filename() == "second");
    CHECK(firstPath.parent_path().parent_path() == secondPath.parent_path().parent_path());
    const auto committedBytes = readBytes(firstPath);
    const auto collision = engine.execute(firstRequest, *fixture.output.valueIfPresent());
    REQUIRE(collision.errorIfPresent() != nullptr);
    CHECK(collision.errorIfPresent()->code == domain::ImageProcessingErrorCode::OutputRelativePathCollision);
    CHECK(readBytes(firstPath) == committedBytes);
    CHECK(readBytes(fixture.directory.directoryPath() / "first/camera.jpg") == fixture.sourceBytes);
    fixture.requireNoStages();
    CHECK(std::distance(std::filesystem::directory_iterator(firstPath.parent_path()),
                        std::filesystem::directory_iterator{}) == 1);
}

TEST_CASE("a Unicode stage beyond MAX_PATH commits exact bytes without requiring machine opt-in",
          "[storage][transaction][copy][long-path]")
{
    CHECK(ownedTestFixtureDeletionOption == winrt::Windows::Storage::StorageDeleteOption::PermanentDelete);
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    // Keep source/final paths below MAX_PATH so fixture I/O cannot fail before
    // the production boundary. The unique native stage name alone crosses it.
    const auto rootLength = fixture.directory.directoryPath().wstring().size();
    REQUIRE(rootLength < 189);
    const auto relativePath = std::filesystem::path{std::wstring(189 - rootLength, L'\u753b')} / L"camera.jpg";
    const auto source = fixture.createSource(relativePath);
    REQUIRE(source.Path().size() < MAX_PATH);
    const auto request = fixture.requestFor(source);
    struct ObservedNativeStage final : storage::internal::FileTransactionOperations
    {
        mutable std::size_t stagePathLength{};
        winrt::Windows::Storage::StorageFile createStage(const winrt::Windows::Storage::StorageFolder &parent,
                                                         const winrt::hstring &name) const override
        {
            const auto stage = FileTransactionOperations::createStage(parent, name);
            stagePathLength = stage.Path().size();
            return stage;
        }
    } operations;
    struct OwnedFixtureCleanup final
    {
        // This is the TemporaryDirectory-owned test root, never a user folder.
        // Windows Storage can remove an intentionally failed long-path fixture
        // even when the standard test filesystem consumer cannot enumerate it.
        winrt::Windows::Storage::StorageFolder folder;
        ~OwnedFixtureCleanup()
        {
            try
            {
                folder.DeleteAsync(ownedTestFixtureDeletionOption).get();
            }
            catch (const winrt::hresult_error &)
            { /* TemporaryDirectory retains its ordinary cleanup fallback. */
            }
        }
    } cleanup{fixture.root};
    storage::internal::ImageFileTransactionBatch batch{fixture.root, fixture.journalStore};
    const auto result = storage::internal::RecoverableImageFileTransaction::execute(
        batch, request, *fixture.output.valueIfPresent(), {}, operations);
    REQUIRE(operations.stagePathLength > MAX_PATH);
    REQUIRE(result.valueIfPresent() != nullptr);
    const std::filesystem::path committedPath{result.valueIfPresent()->committedFile.Path().c_str()};
    CHECK(committedPath.wstring().size() < MAX_PATH);
    CHECK(committedPath.filename() == relativePath.filename());
    CHECK(committedPath.parent_path().filename() == relativePath.parent_path().filename());
    CHECK(readBytes(fixture.directory.directoryPath() / relativePath) == fixture.sourceBytes);
    CHECK(readBytes(committedPath) == std::vector<std::byte>(fixture.output.valueIfPresent()->encodedBytes().begin(),
                                                             fixture.output.valueIfPresent()->encodedBytes().end()));
    fixture.requireNoStages();
}

TEST_CASE("different batch instances create distinct roots without suffixing source filenames",
          "[storage][transaction][copy]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    const auto source = fixture.createSource("camera.jpg");
    const auto request = fixture.requestFor(source);
    const auto first = storage::WindowsStorageImageFileTransactionEngine{fixture.root, fixture.journalStore}.execute(
        request, *fixture.output.valueIfPresent());
    const auto second = storage::WindowsStorageImageFileTransactionEngine{fixture.root, fixture.journalStore}.execute(
        request, *fixture.output.valueIfPresent());
    REQUIRE(first.valueIfPresent() != nullptr);
    REQUIRE(second.valueIfPresent() != nullptr);
    const std::filesystem::path firstPath{first.valueIfPresent()->committedFile.Path().c_str()};
    const std::filesystem::path secondPath{second.valueIfPresent()->committedFile.Path().c_str()};
    CHECK(firstPath.filename() == secondPath.filename());
    CHECK(firstPath.parent_path() != secondPath.parent_path());
    fixture.requireNoStages();
}

TEST_CASE("cancel before staging leaves even the output root absent", "[storage][transaction][copy]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    const auto source = fixture.createSource("camera.jpg");
    const auto request = fixture.requestFor(source);
    std::stop_source cancellation;
    cancellation.request_stop();
    const auto result = storage::WindowsStorageImageFileTransactionEngine{fixture.root, fixture.journalStore}.execute(
        request, *fixture.output.valueIfPresent(), cancellation.get_token());
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::Cancelled);
    CHECK(!std::filesystem::exists(fixture.directory.directoryPath() / "JPG Spinner Output"));
    CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") == fixture.sourceBytes);
}

TEST_CASE("authoritative source digest mismatch blocks a copy even with equal length and timestamp",
          "[storage][transaction][copy]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    const auto source = fixture.createSource("camera.jpg");
    auto request = fixture.requestFor(source);
    request.expectedSourceRevision.encodedSha256[0] ^= std::byte{1};
    const auto result = storage::WindowsStorageImageFileTransactionEngine{fixture.root, fixture.journalStore}.execute(
        request, *fixture.output.valueIfPresent());
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::SourceChangedAfterAnalysis);
    CHECK(result.errorIfPresent()->stage == domain::ImageProcessingStage::SourceRevisionRevalidation);
    CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") == fixture.sourceBytes);
    fixture.requireNoStages();
    for (const auto &item :
         std::filesystem::recursive_directory_iterator(fixture.directory.directoryPath() / "JPG Spinner Output"))
        CHECK(!item.is_regular_file());
}

TEST_CASE("a partial native stage write is rejected and only its owned stage is removed",
          "[storage][transaction][copy][fault]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    const auto source = fixture.createSource("camera.jpg");
    const auto request = fixture.requestFor(source);
    class ShortWrite final : public storage::internal::FileTransactionOperations
    {
      public:
        mutable unsigned calls{};
        std::uint32_t writeStagedBytes(const winrt::Windows::Storage::Streams::IRandomAccessStream &stream,
                                       const winrt::Windows::Storage::Streams::IBuffer &buffer) const override
        {
            ++calls;
            REQUIRE(buffer.Length() > 1);
            buffer.Length(buffer.Length() / 2);
            return FileTransactionOperations::writeStagedBytes(stream, buffer);
        }
    } operations;
    storage::internal::ImageFileTransactionBatch batch{fixture.root, fixture.journalStore};
    const auto result = storage::internal::RecoverableImageFileTransaction::execute(
        batch, request, *fixture.output.valueIfPresent(), {}, operations);
    CHECK(operations.calls == 1);
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::StagingWriteFailed);
    CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") == fixture.sourceBytes);
    fixture.requireNoStages();
    for (const auto &item :
         std::filesystem::recursive_directory_iterator(fixture.directory.directoryPath() / "JPG Spinner Output"))
        CHECK(!item.is_regular_file());
}

TEST_CASE("a close failure is distinct from a successful flush and cannot commit",
          "[storage][transaction][copy][close-fault]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    const auto request = fixture.requestFor(fixture.createSource("camera.jpg"));
    class FailedClose final : public storage::internal::FileTransactionOperations
    {
      public:
        mutable unsigned calls{};
        void closeStagedOutput(const winrt::Windows::Storage::Streams::IRandomAccessStream &stream) const override
        {
            ++calls;
            FileTransactionOperations::closeStagedOutput(stream);
            throw winrt::hresult_error{E_FAIL};
        }
    } operations;
    storage::internal::ImageFileTransactionBatch batch{fixture.root, fixture.journalStore};
    const auto result = storage::internal::RecoverableImageFileTransaction::execute(
        batch, request, *fixture.output.valueIfPresent(), {}, operations);
    CHECK(operations.calls == 1);
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::StagingCloseFailed);
    CHECK(result.errorIfPresent()->stage == domain::ImageProcessingStage::StagingClose);
    CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") == fixture.sourceBytes);
    fixture.requireNoStages();
    CHECK(batch.correctedCopyFolder.GetFilesAsync().get().Size() == 0);
}

TEST_CASE("a real stage reopen denial is not reported as an observed hash mismatch",
          "[storage][transaction][copy][reopen-fault]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    const auto request = fixture.requestFor(fixture.createSource("camera.jpg"));
    class DeniedStageReader final : public storage::internal::FileTransactionOperations
    {
      public:
        mutable unsigned calls{};
        mutable std::int32_t observedNativeFailure{};
        domain::ImageProcessingResult<domain::SourceFileRevision> captureStagedRevision(
            const winrt::Windows::Storage::StorageFile &stage, std::stop_token token) const override
        {
            ++calls;
            // Own a real incompatible writer during the real revision calculator
            // call. Release it before transaction cleanup, not by fabricating an error.
            const auto rawHandle = CreateFileW(stage.Path().c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            REQUIRE(rawHandle != INVALID_HANDLE_VALUE);
            const winrt::handle writer{rawHandle};
            auto observed = FileTransactionOperations::captureStagedRevision(stage, token);
            REQUIRE(observed.errorIfPresent() != nullptr);
            REQUIRE((observed.errorIfPresent()->code == domain::ImageProcessingErrorCode::SourceSharingViolation ||
                     observed.errorIfPresent()->code == domain::ImageProcessingErrorCode::SourceAccessDenied));
            const auto *native = std::get_if<domain::WindowsHResult>(&observed.errorIfPresent()->nativeErrorProjection);
            REQUIRE(native != nullptr);
            observedNativeFailure = native->signedValue;
            REQUIRE((observedNativeFailure == HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION) ||
                     observedNativeFailure == E_ACCESSDENIED));
            return observed;
        }
    } operations;
    storage::internal::ImageFileTransactionBatch batch{fixture.root, fixture.journalStore};
    const auto result = storage::internal::RecoverableImageFileTransaction::execute(
        batch, request, *fixture.output.valueIfPresent(), {}, operations);
    CHECK(operations.calls == 1);
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::StagedOutputVerificationFailed);
    CHECK(result.errorIfPresent()->stage == domain::ImageProcessingStage::StagedOutputVerification);
    const auto *native = std::get_if<domain::WindowsHResult>(&result.errorIfPresent()->nativeErrorProjection);
    REQUIRE(native != nullptr);
    CHECK(native->signedValue == operations.observedNativeFailure);
    CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") == fixture.sourceBytes);
    fixture.requireNoStages();
    CHECK(batch.correctedCopyFolder.GetFilesAsync().get().Size() == 0);
}

TEST_CASE("a replaced batch folder is not adopted merely because its canonical pathname matches",
          "[storage][transaction][copy][batch-identity]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    storage::WindowsStorageImageFileTransactionEngine engine{fixture.root, fixture.journalStore};
    const auto first =
        engine.execute(fixture.requestFor(fixture.createSource("first.jpg")), *fixture.output.valueIfPresent());
    REQUIRE(first.valueIfPresent() != nullptr);
    const std::filesystem::path firstPath{first.valueIfPresent()->committedFile.Path().c_str()};
    const auto originalBatchPath = firstPath.parent_path();
    const auto preservedBatchPath = fixture.directory.directoryPath() / "preserved-batch";
    std::filesystem::rename(originalBatchPath, preservedBatchPath);
    REQUIRE(std::filesystem::create_directory(originalBatchPath));
    writeBytes(originalBatchPath / "foreign.jpg", fixture.sourceBytes);
    REQUIRE(std::filesystem::exists(preservedBatchPath / "first.jpg"));
    const auto second =
        engine.execute(fixture.requestFor(fixture.createSource("second.jpg")), *fixture.output.valueIfPresent());
    REQUIRE(second.errorIfPresent() != nullptr);
    CHECK(second.errorIfPresent()->code == domain::ImageProcessingErrorCode::RecoveryConflict);
    CHECK(second.errorIfPresent()->stage == domain::ImageProcessingStage::TransactionRecovery);
    CHECK(!std::filesystem::exists(originalBatchPath / "second.jpg"));
    CHECK(readBytes(originalBatchPath / "foreign.jpg") == fixture.sourceBytes);
    CHECK(readBytes(fixture.directory.directoryPath() / "second.jpg") == fixture.sourceBytes);
    fixture.requireNoStages();
}

TEST_CASE("an interrupted move cannot make cleanup delete an already committed copy",
          "[storage][transaction][copy][move-outcome]")
{
    const bool usesStaleProjection = GENERATE(false, true);
    CAPTURE(usesStaleProjection);
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    const auto request = fixture.requestFor(fixture.createSource("camera.jpg"));
    class InterruptedMove final : public storage::internal::FileTransactionOperations
    {
      public:
        explicit InterruptedMove(bool usesStaleProjection) : usesStaleProjection_(usesStaleProjection)
        {
        }
        mutable unsigned calls{};
        void moveStageToCorrectedCopy(const winrt::Windows::Storage::StorageFile &stage,
                                      const winrt::Windows::Storage::StorageFolder &destination,
                                      const winrt::hstring &name) const override
        {
            ++calls;
            if (usesStaleProjection_)
            {
                const auto otherProjection =
                    winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(stage.Path()).get();
                const auto previousPath = stage.Path();
                FileTransactionOperations::moveStageToCorrectedCopy(otherProjection, destination, name);
                REQUIRE(stage.Path() == previousPath);
                REQUIRE(otherProjection.Path() != previousPath);
            }
            else
                FileTransactionOperations::moveStageToCorrectedCopy(stage, destination, name);
            // Model loss of completion after the observable native move. The
            // transaction must not infer that its stage pathname still exists.
            throw winrt::hresult_error{E_FAIL};
        }

      private:
        const bool usesStaleProjection_;
    } operations{usesStaleProjection};
    storage::internal::ImageFileTransactionBatch batch{fixture.root, fixture.journalStore};
    const auto result = storage::internal::RecoverableImageFileTransaction::execute(
        batch, request, *fixture.output.valueIfPresent(), {}, operations);
    CHECK(operations.calls == 1);
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::RecoveryConflict);
    CHECK(result.errorIfPresent()->stage == domain::ImageProcessingStage::TransactionRecovery);
    const std::filesystem::path committedPath =
        std::filesystem::path{batch.correctedCopyFolder.Path().c_str()} / "camera.jpg";
    REQUIRE(std::filesystem::exists(committedPath));
    CHECK(readBytes(committedPath) == std::vector<std::byte>(fixture.output.valueIfPresent()->encodedBytes().begin(),
                                                             fixture.output.valueIfPresent()->encodedBytes().end()));
    CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") == fixture.sourceBytes);
    fixture.requireNoStages();
}

TEST_CASE("different real staged bytes never reach commit", "[storage][transaction][copy][hash-mismatch]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    const auto request = fixture.requestFor(fixture.createSource("camera.jpg"));
    class ChangedStage final : public storage::internal::FileTransactionOperations
    {
      public:
        mutable unsigned hashCalls{};
        mutable unsigned moveCalls{};
        domain::ImageProcessingResult<domain::SourceFileRevision> captureStagedRevision(
            const winrt::Windows::Storage::StorageFile &stage, std::stop_token token) const override
        {
            ++hashCalls;
            const auto stream = stage.OpenAsync(winrt::Windows::Storage::FileAccessMode::ReadWrite).get();
            const auto originalLength = stream.Size();
            REQUIRE(originalLength > 1);
            const std::array<std::uint8_t, 1> changed{0};
            const auto buffer =
                winrt::Windows::Security::Cryptography::CryptographicBuffer::CreateFromByteArray(changed);
            REQUIRE(stream.WriteAsync(buffer).get() == 1);
            REQUIRE(stream.FlushAsync().get());
            stream.Close();
            auto observed = FileTransactionOperations::captureStagedRevision(stage, token);
            REQUIRE(observed.valueIfPresent() != nullptr);
            REQUIRE(observed.valueIfPresent()->encodedLengthBytes == originalLength);
            return observed;
        }
        void moveStageToCorrectedCopy(const winrt::Windows::Storage::StorageFile &,
                                      const winrt::Windows::Storage::StorageFolder &,
                                      const winrt::hstring &) const override
        {
            ++moveCalls;
        }
    } operations;
    storage::internal::ImageFileTransactionBatch batch{fixture.root, fixture.journalStore};
    const auto result = storage::internal::RecoverableImageFileTransaction::execute(
        batch, request, *fixture.output.valueIfPresent(), {}, operations);
    CHECK(operations.hashCalls == 1);
    CHECK(operations.moveCalls == 0);
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::StagedOutputHashMismatch);
    CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") == fixture.sourceBytes);
    fixture.requireNoStages();
    CHECK(batch.correctedCopyFolder.GetFilesAsync().get().Size() == 0);
}

TEST_CASE("cancel after stage verification preserves unrelated stages and uses distinct native transaction UUIDs",
          "[storage][transaction][copy][cancel-after-hash]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    std::stop_source cancellation;
    const auto request = fixture.requestFor(fixture.createSource("camera.jpg"));
    class CancelAfterHash final : public storage::internal::FileTransactionOperations
    {
      public:
        explicit CancelAfterHash(std::stop_source &cancellation) : cancellation_(cancellation)
        {
        }
        mutable std::vector<std::wstring> stageNames;
        mutable unsigned hashCalls{};
        winrt::Windows::Storage::StorageFile createStage(const winrt::Windows::Storage::StorageFolder &destination,
                                                         const winrt::hstring &name) const override
        {
            const std::wstring stageName{name};
            const std::wstring prefix{L".jpg-spinner-staged-"};
            REQUIRE(stageName.starts_with(prefix));
            REQUIRE(stageName.ends_with(L".jpg"));
            const auto uuidText = stageName.substr(prefix.size(), stageName.size() - prefix.size() - 4);
            GUID identifier{};
            REQUIRE(SUCCEEDED(IIDFromString(uuidText.c_str(), &identifier)));
            REQUIRE(!IsEqualGUID(identifier, GUID_NULL));
            stageNames.push_back(stageName);
            if (stageNames.size() == 1)
            {
                const auto foreign =
                    destination
                        .CreateFileAsync(L".jpg-spinner-staged-{00000000-0000-0000-0000-000000000001}.jpg",
                                         winrt::Windows::Storage::CreationCollisionOption::FailIfExists)
                        .get();
                const std::array<std::uint8_t, 3> sentinel{1, 2, 3};
                winrt::Windows::Storage::FileIO::WriteBytesAsync(foreign, sentinel).get();
            }
            return FileTransactionOperations::createStage(destination, name);
        }
        domain::ImageProcessingResult<domain::SourceFileRevision> captureStagedRevision(
            const winrt::Windows::Storage::StorageFile &stage, std::stop_token token) const override
        {
            ++hashCalls;
            auto observed = FileTransactionOperations::captureStagedRevision(stage, token);
            REQUIRE(observed.valueIfPresent() != nullptr);
            cancellation_.request_stop();
            return observed;
        }

      private:
        std::stop_source &cancellation_;
    } operations{cancellation};
    storage::internal::ImageFileTransactionBatch batch{fixture.root, fixture.journalStore};
    for (unsigned attempt = 0; attempt < 2; ++attempt)
    {
        cancellation = std::stop_source{};
        const auto result = storage::internal::RecoverableImageFileTransaction::execute(
            batch, request, *fixture.output.valueIfPresent(), cancellation.get_token(), operations);
        REQUIRE(result.errorIfPresent() != nullptr);
        CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::Cancelled);
        CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") == fixture.sourceBytes);
    }
    CHECK(operations.hashCalls == 2);
    REQUIRE(operations.stageNames.size() == 2);
    CHECK(operations.stageNames[0] != operations.stageNames[1]);
    const std::filesystem::path batchPath{batch.correctedCopyFolder.Path().c_str()};
    for (const auto &ownedName : operations.stageNames)
        CHECK(!std::filesystem::exists(batchPath / ownedName));
    CHECK(!std::filesystem::exists(batchPath / "camera.jpg"));
    const std::vector<std::byte> sentinel{std::byte{1}, std::byte{2}, std::byte{3}};
    CHECK(readBytes(batchPath / L".jpg-spinner-staged-{00000000-0000-0000-0000-000000000001}.jpg") == sentinel);
    CHECK(batch.correctedCopyFolder.GetFilesAsync().get().Size() == 1);
}

TEST_CASE("destination creation failure is not mislabeled as a stage creation failure",
          "[storage][transaction][copy][destination-fault]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    const auto request = fixture.requestFor(fixture.createSource("camera.jpg"));
    class FailedDestination final : public storage::internal::FileTransactionOperations
    {
      public:
        mutable unsigned calls{};
        winrt::Windows::Storage::StorageFolder createCorrectedCopyOutputRoot(
            const winrt::Windows::Storage::StorageFolder &) const override
        {
            ++calls;
            throw winrt::hresult_error{E_FAIL};
        }
    } operations;
    storage::internal::ImageFileTransactionBatch batch{fixture.root, fixture.journalStore};
    const auto result = storage::internal::RecoverableImageFileTransaction::execute(
        batch, request, *fixture.output.valueIfPresent(), {}, operations);
    CHECK(operations.calls == 1);
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::CorrectedCopyDestinationCreationFailed);
    CHECK(result.errorIfPresent()->stage == domain::ImageProcessingStage::CorrectedCopyDestinationCreation);
    CHECK(!std::filesystem::exists(fixture.directory.directoryPath() / "JPG Spinner Output"));
    CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") == fixture.sourceBytes);
    fixture.requireNoStages();
}

TEST_CASE("each unavailable native effect fails closed without changing source bytes",
          "[storage][transaction][copy][fault-matrix]")
{
    enum class FailedEffect
    {
        StageCreation,
        StageWrite,
        StageFlush,
        StageHash,
        SourceHash,
        CopyMove,
        DiskFull,
        DestinationAccess
    };
    struct FailureCase
    {
        FailedEffect effect;
        domain::ImageProcessingErrorCode code;
        domain::ImageProcessingStage stage;
    };
    const auto failureCase =
        GENERATE(FailureCase{FailedEffect::StageCreation, domain::ImageProcessingErrorCode::StagingFileCreationFailed,
                             domain::ImageProcessingStage::StagingFileCreation},
                 FailureCase{FailedEffect::StageWrite, domain::ImageProcessingErrorCode::StagingWriteFailed,
                             domain::ImageProcessingStage::StagingWrite},
                 FailureCase{FailedEffect::StageFlush, domain::ImageProcessingErrorCode::StagingFlushFailed,
                             domain::ImageProcessingStage::StagingFlush},
                 FailureCase{FailedEffect::StageHash, domain::ImageProcessingErrorCode::StagedOutputVerificationFailed,
                             domain::ImageProcessingStage::StagedOutputVerification},
                 FailureCase{FailedEffect::SourceHash, domain::ImageProcessingErrorCode::SourceRevisionCaptureFailed,
                             domain::ImageProcessingStage::SourceRevisionRevalidation},
                 FailureCase{FailedEffect::CopyMove, domain::ImageProcessingErrorCode::CorrectedCopyCommitFailed,
                             domain::ImageProcessingStage::CorrectedCopyCommit},
                 FailureCase{FailedEffect::DiskFull, domain::ImageProcessingErrorCode::InsufficientStorageSpace,
                             domain::ImageProcessingStage::StagingWrite},
                 FailureCase{FailedEffect::DestinationAccess, domain::ImageProcessingErrorCode::DestinationAccessDenied,
                             domain::ImageProcessingStage::CorrectedCopyDestinationCreation});
    CAPTURE(static_cast<unsigned>(failureCase.effect));
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    const auto request = fixture.requestFor(fixture.createSource("camera.jpg"));
    // These are documented native failure outcomes that cannot be forced safely
    // and deterministically (especially disk-full/flush/crypto-provider faults).
    // The decisive transaction policy, all unaffected effects and filesystem
    // observations remain real. This does not qualify an actually full volume.
    class FailedNativeEffect final : public storage::internal::FileTransactionOperations
    {
      public:
        explicit FailedNativeEffect(FailedEffect effect) : effect_(effect)
        {
        }
        mutable unsigned failureCalls{};
        winrt::Windows::Storage::StorageFile createStage(const winrt::Windows::Storage::StorageFolder &folder,
                                                         const winrt::hstring &name) const override
        {
            if (effect_ == FailedEffect::StageCreation)
                fail();
            return FileTransactionOperations::createStage(folder, name);
        }
        std::uint32_t writeStagedBytes(const winrt::Windows::Storage::Streams::IRandomAccessStream &stream,
                                       const winrt::Windows::Storage::Streams::IBuffer &buffer) const override
        {
            if (effect_ == FailedEffect::DiskFull)
                fail(HRESULT_FROM_WIN32(ERROR_DISK_FULL));
            if (effect_ == FailedEffect::StageWrite)
                fail();
            return FileTransactionOperations::writeStagedBytes(stream, buffer);
        }
        bool flushStagedBytes(const winrt::Windows::Storage::Streams::IRandomAccessStream &stream) const override
        {
            const bool flushed = FileTransactionOperations::flushStagedBytes(stream);
            REQUIRE(flushed);
            if (effect_ == FailedEffect::StageFlush)
            {
                ++failureCalls;
                return false;
            }
            return flushed;
        }
        domain::ImageProcessingResult<domain::SourceFileRevision> captureStagedRevision(
            const winrt::Windows::Storage::StorageFile &stage, std::stop_token token) const override
        {
            auto observed = FileTransactionOperations::captureStagedRevision(stage, token);
            REQUIRE(observed.valueIfPresent() != nullptr);
            if (effect_ == FailedEffect::StageHash)
                fail();
            return observed;
        }
        domain::ImageProcessingResult<domain::SourceFileRevision> captureSourceRevision(
            const winrt::Windows::Storage::StorageFile &source, std::stop_token token) const override
        {
            auto observed = FileTransactionOperations::captureSourceRevision(source, token);
            REQUIRE(observed.valueIfPresent() != nullptr);
            if (effect_ == FailedEffect::SourceHash)
            {
                ++failureCalls;
                return domain::ImageProcessingResult<domain::SourceFileRevision>::failure(
                    {domain::ImageProcessingErrorCode::SourceRevisionCaptureFailed,
                     domain::ImageProcessingStage::SourceRevisionCapture, domain::WindowsHResult{E_FAIL}});
            }
            return observed;
        }
        winrt::Windows::Storage::StorageFolder createCorrectedCopyOutputRoot(
            const winrt::Windows::Storage::StorageFolder &root) const override
        {
            if (effect_ == FailedEffect::DestinationAccess)
                fail(E_ACCESSDENIED);
            return FileTransactionOperations::createCorrectedCopyOutputRoot(root);
        }
        void moveStageToCorrectedCopy(const winrt::Windows::Storage::StorageFile &stage,
                                      const winrt::Windows::Storage::StorageFolder &folder,
                                      const winrt::hstring &name) const override
        {
            if (effect_ == FailedEffect::CopyMove)
                fail();
            FileTransactionOperations::moveStageToCorrectedCopy(stage, folder, name);
        }

      private:
        [[noreturn]] void fail(HRESULT error = E_FAIL) const
        {
            ++failureCalls;
            throw winrt::hresult_error{error};
        }
        const FailedEffect effect_;
    } operations{failureCase.effect};
    storage::internal::ImageFileTransactionBatch batch{fixture.root, fixture.journalStore};
    const auto result = storage::internal::RecoverableImageFileTransaction::execute(
        batch, request, *fixture.output.valueIfPresent(), {}, operations);
    CHECK(operations.failureCalls == 1);
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == failureCase.code);
    CHECK(result.errorIfPresent()->stage == failureCase.stage);
    CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") == fixture.sourceBytes);
    fixture.requireNoStages();
    if (batch.correctedCopyFolder)
        CHECK(batch.correctedCopyFolder.GetFilesAsync().get().Size() == 0);
    const auto *native = std::get_if<domain::WindowsHResult>(&result.errorIfPresent()->nativeErrorProjection);
    REQUIRE(native != nullptr);
    if (failureCase.effect == FailedEffect::DiskFull)
        CHECK(native->signedValue == HRESULT_FROM_WIN32(ERROR_DISK_FULL));
}

TEST_CASE("a sibling source root is not authorized by a common textual path prefix",
          "[storage][transaction][copy][containment]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    const auto selectedPath = fixture.directory.directoryPath() / "selected";
    std::filesystem::create_directory(selectedPath);
    const auto source = fixture.createSource("selected-other/camera.jpg");
    const auto selected = winrt::Windows::Storage::StorageFolder::GetFolderFromPathAsync(selectedPath.wstring()).get();
    const auto result = storage::WindowsStorageImageFileTransactionEngine{selected, fixture.journalStore}.execute(
        fixture.requestFor(source), *fixture.output.valueIfPresent());
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::SourceRevisionCaptureFailed);
    CHECK(result.errorIfPresent()->stage == domain::ImageProcessingStage::SourceRevisionRevalidation);
    CHECK(!std::filesystem::exists(selectedPath / "JPG Spinner Output"));
    CHECK(readBytes(fixture.directory.directoryPath() / "selected-other/camera.jpg") == fixture.sourceBytes);
}

TEST_CASE("a native volume root's trailing separator does not reject its exact output child",
          "[storage][transaction][copy][volume-root]")
{
    // Inspect an existing volume read-only; never create output at a drive root.
    // This uses the actual native spelling, not a custom Windows path grammar.
    const auto volumeRoot = std::filesystem::current_path().root_path();
    const auto rawHandle =
        CreateFileW(volumeRoot.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    REQUIRE(rawHandle != INVALID_HANDLE_VALUE);
    const winrt::handle handle{rawHandle};
    std::wstring finalPath(32768, L'\0');
    const auto length =
        GetFinalPathNameByHandleW(handle.get(), finalPath.data(), static_cast<DWORD>(finalPath.size()), VOLUME_NAME_NT);
    REQUIRE(length > 0);
    REQUIRE(length < finalPath.size());
    finalPath.resize(length);
    REQUIRE(finalPath.ends_with(L"\\"));
    const std::filesystem::path nativeRoot{finalPath};
    const auto expectedChild = nativeRoot / L"JPG Spinner Output";
    REQUIRE(expectedChild.parent_path() != nativeRoot);
    CHECK(storage::internal::isCorrectedCopyOutputRootPath(expectedChild, nativeRoot));
    CHECK(!storage::internal::isCorrectedCopyOutputRootPath(nativeRoot / L"foreign", nativeRoot));
    CHECK(!storage::internal::isCorrectedCopyOutputRootPath(expectedChild / L"nested", nativeRoot));
}

TEST_CASE("an in-root directory symlink is not silently followed while deriving an output path",
          "[storage][transaction][copy][reparse]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    fixture.createSource("physical/camera.jpg");
    const auto physicalPath = fixture.directory.directoryPath() / "physical";
    const auto aliasPath = fixture.directory.directoryPath() / "alias";
    // Native fixture creation requires Developer Mode or the OS symlink privilege;
    // fail the precondition rather than silently skip this containment claim.
    REQUIRE(CreateSymbolicLinkW(aliasPath.c_str(), physicalPath.c_str(),
                                SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE));
    REQUIRE(std::filesystem::is_symlink(aliasPath));
    const auto aliasedSourcePath = (aliasPath / "camera.jpg").make_preferred();
    const auto source = winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(aliasedSourcePath.wstring()).get();
    REQUIRE(std::filesystem::path{source.Path().c_str()}.parent_path() == aliasPath);
    const auto result = storage::WindowsStorageImageFileTransactionEngine{fixture.root, fixture.journalStore}.execute(
        fixture.requestFor(source), *fixture.output.valueIfPresent());
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::SourceRevisionCaptureFailed);
    CHECK(result.errorIfPresent()->stage == domain::ImageProcessingStage::SourceRevisionRevalidation);
    CHECK(!std::filesystem::exists(fixture.directory.directoryPath() / "JPG Spinner Output"));
    CHECK(readBytes(physicalPath / "camera.jpg") == fixture.sourceBytes);
}

TEST_CASE("missing or denied sources preserve source-side diagnostics and never commit a copy",
          "[storage][transaction][copy][source-open-failure]")
{
    const bool deniesSharing = GENERATE(false, true);
    CAPTURE(deniesSharing);
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    const auto source = fixture.createSource("camera.jpg");
    const auto request = fixture.requestFor(source);
    winrt::handle incompatibleWriter;
    std::int32_t observedNativeFailure = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    auto expectedCode = domain::ImageProcessingErrorCode::SourceChangedAfterAnalysis;
    if (deniesSharing)
    {
        const auto rawHandle = CreateFileW(source.Path().c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                           FILE_ATTRIBUTE_NORMAL, nullptr);
        REQUIRE(rawHandle != INVALID_HANDLE_VALUE);
        incompatibleWriter = winrt::handle{rawHandle};
        // Metadata-only native inspection need not conflict with an open writer.
        // Observe the real content-reader failure rather than inventing its HRESULT
        // or requiring that no empty output directories were created before recheck.
        const auto observed = storage::SourceFileRevisionCalculator{}.calculate(source);
        REQUIRE(observed.errorIfPresent() != nullptr);
        REQUIRE((observed.errorIfPresent()->code == domain::ImageProcessingErrorCode::SourceAccessDenied ||
                 observed.errorIfPresent()->code == domain::ImageProcessingErrorCode::SourceSharingViolation));
        const auto *native = std::get_if<domain::WindowsHResult>(&observed.errorIfPresent()->nativeErrorProjection);
        REQUIRE(native != nullptr);
        observedNativeFailure = native->signedValue;
        expectedCode = observed.errorIfPresent()->code;
    }
    else
        REQUIRE(std::filesystem::remove(fixture.directory.directoryPath() / "camera.jpg"));
    const auto result = storage::WindowsStorageImageFileTransactionEngine{fixture.root, fixture.journalStore}.execute(
        request, *fixture.output.valueIfPresent());
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == expectedCode);
    CHECK(result.errorIfPresent()->stage == domain::ImageProcessingStage::SourceRevisionRevalidation);
    const auto *native = std::get_if<domain::WindowsHResult>(&result.errorIfPresent()->nativeErrorProjection);
    REQUIRE(native != nullptr);
    CHECK(native->signedValue == observedNativeFailure);
    if (deniesSharing)
    {
        incompatibleWriter.close();
        CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") == fixture.sourceBytes);
        for (const auto &item :
             std::filesystem::recursive_directory_iterator(fixture.directory.directoryPath() / "JPG Spinner Output"))
            CHECK(!item.is_regular_file());
    }
    else
        CHECK(!std::filesystem::exists(fixture.directory.directoryPath() / "JPG Spinner Output"));
    fixture.requireNoStages();
}

TEST_CASE("a moved-from validation capability is rejected before any filesystem write",
          "[storage][transaction][copy][moved-capability]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    const auto request = fixture.requestFor(fixture.createSource("camera.jpg"));
    auto produced = fixture.createOutput();
    REQUIRE(produced.valueIfPresent() != nullptr);
    const auto retained = std::move(*produced.valueIfPresent());
    REQUIRE(!retained.encodedBytes().empty());
    REQUIRE(produced.valueIfPresent()->encodedBytes().empty());
    const auto result = storage::WindowsStorageImageFileTransactionEngine{fixture.root, fixture.journalStore}.execute(
        request, *produced.valueIfPresent());
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::OutputValidationFailed);
    CHECK(result.errorIfPresent()->stage == domain::ImageProcessingStage::OutputValidation);
    CHECK(!std::filesystem::exists(fixture.directory.directoryPath() / "JPG Spinner Output"));
    CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") == fixture.sourceBytes);
}

TEST_CASE("the batch test adapter preserves a completed transaction when cancellation arrives afterwards",
          "[storage][transaction][copy][adapter]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    const auto request = fixture.requestFor(fixture.createSource("camera.jpg"));
    storage::WindowsStorageImageFileTransactionEngine nativeEngine{fixture.root, fixture.journalStore};
    std::stop_source cancellation;
    unsigned factoryCalls = 0;
    test_support::DeterministicImageFileTransactionEngine adapter{
        [&](const auto &sourceRequest, const auto &validatedOutput, auto token) {
            ++factoryCalls;
            auto result = nativeEngine.execute(sourceRequest, validatedOutput, token);
            REQUIRE(result.valueIfPresent() != nullptr);
            cancellation.request_stop();
            return result;
        }};
    const storage::ImageFileTransactionEngine &engine = adapter;
    const auto completed = engine.execute(request, *fixture.output.valueIfPresent(), cancellation.get_token());
    REQUIRE(completed.valueIfPresent() != nullptr);
    CHECK(std::filesystem::exists(std::filesystem::path{completed.valueIfPresent()->committedFile.Path().c_str()}));
    const auto cancelled = engine.execute(request, *fixture.output.valueIfPresent(), cancellation.get_token());
    REQUIRE(cancelled.errorIfPresent() != nullptr);
    CHECK(cancelled.errorIfPresent()->code == domain::ImageProcessingErrorCode::Cancelled);
    CHECK(adapter.invocationCount() == 2);
    CHECK(adapter.maximumConcurrentCalls() == 1);
    CHECK(factoryCalls == 1);
}

TEST_CASE("a real source edit after stage hashing is detected despite restored length and modification time",
          "[storage][transaction][copy][source-recheck]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    const auto source = fixture.createSource("camera.jpg");
    const auto request = fixture.requestFor(source);
    const auto sourcePath = fixture.directory.directoryPath() / "camera.jpg";
    const auto originalWriteTime = std::filesystem::last_write_time(sourcePath);
    auto externallyChangedBytes = fixture.sourceBytes;
    externallyChangedBytes[0] ^= std::byte{1};
    class EditAfterStageHash final : public storage::internal::FileTransactionOperations
    {
      public:
        EditAfterStageHash(winrt::Windows::Storage::StorageFile source, std::filesystem::path sourcePath,
                           std::filesystem::file_time_type originalWriteTime)
            : source_(std::move(source)), sourcePath_(std::move(sourcePath)), originalWriteTime_(originalWriteTime)
        {
        }
        mutable unsigned calls{};
        domain::ImageProcessingResult<domain::SourceFileRevision> captureStagedRevision(
            const winrt::Windows::Storage::StorageFile &stage, std::stop_token token) const override
        {
            ++calls;
            auto observed = FileTransactionOperations::captureStagedRevision(stage, token);
            REQUIRE(observed.valueIfPresent() != nullptr);
            const auto writer = source_.OpenAsync(winrt::Windows::Storage::FileAccessMode::ReadWrite).get();
            const std::array<std::uint8_t, 1> changed{0xfe};
            const auto buffer =
                winrt::Windows::Security::Cryptography::CryptographicBuffer::CreateFromByteArray(changed);
            REQUIRE(writer.WriteAsync(buffer).get() == 1);
            REQUIRE(writer.FlushAsync().get());
            writer.Close();
            std::filesystem::last_write_time(sourcePath_, originalWriteTime_);
            return observed;
        }

      private:
        const winrt::Windows::Storage::StorageFile source_;
        const std::filesystem::path sourcePath_;
        const std::filesystem::file_time_type originalWriteTime_;
    } operations{source, sourcePath, originalWriteTime};
    REQUIRE(fixture.sourceBytes[0] == std::byte{0xff});
    storage::internal::ImageFileTransactionBatch batch{fixture.root, fixture.journalStore};
    const auto result = storage::internal::RecoverableImageFileTransaction::execute(
        batch, request, *fixture.output.valueIfPresent(), {}, operations);
    CHECK(operations.calls == 1);
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::SourceChangedAfterAnalysis);
    CHECK(result.errorIfPresent()->stage == domain::ImageProcessingStage::SourceRevisionRevalidation);
    CHECK(std::filesystem::last_write_time(sourcePath) == originalWriteTime);
    CHECK(readBytes(sourcePath) == externallyChangedBytes);
    fixture.requireNoStages();
    CHECK(batch.correctedCopyFolder.GetFilesAsync().get().Size() == 0);
}

TEST_CASE("an implicit process MTA cannot supply the transaction worker's COM lifetime",
          "[storage][transaction][copy][implicit-mta]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    const auto request = fixture.requestFor(fixture.createSource("camera.jpg"));
    storage::WindowsStorageImageFileTransactionEngine engine{fixture.root, fixture.journalStore};
    const auto observed = std::async(std::launch::async, [&] {
                              APTTYPE type;
                              APTTYPEQUALIFIER qualifier;
                              const auto nativeApartment = CoGetApartmentType(&type, &qualifier);
                              auto result = engine.execute(request, *fixture.output.valueIfPresent());
                              return std::tuple{nativeApartment, type, qualifier, std::move(result)};
                          }).get();
    // Assertions run only on the joined test thread, not through Catch on a worker.
    REQUIRE(std::get<0>(observed) == S_OK);
    REQUIRE(std::get<1>(observed) == APTTYPE_MTA);
    REQUIRE(std::get<2>(observed) == APTTYPEQUALIFIER_IMPLICIT_MTA);
    const auto &result = std::get<3>(observed);
    REQUIRE(result.errorIfPresent() != nullptr);
    const auto *native = std::get_if<domain::WindowsHResult>(&result.errorIfPresent()->nativeErrorProjection);
    REQUIRE(native != nullptr);
    CHECK(native->signedValue == CO_E_NOTINITIALIZED);
    CHECK(!std::filesystem::exists(fixture.directory.directoryPath() / "JPG Spinner Output"));
    CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") == fixture.sourceBytes);
}

TEST_CASE("replacement retains the exact nested source backup before committing validated bytes",
          "[storage][transaction][replace][backup]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    auto request = fixture.requestFor(fixture.createSource("nested/camera.jpg"));
    request.outputDisposition = domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup;
    const auto result = storage::WindowsStorageImageFileTransactionEngine{fixture.root, fixture.journalStore}.execute(
        request, *fixture.output.valueIfPresent());
    REQUIRE(result.valueIfPresent() != nullptr);
    CHECK(readBytes(fixture.directory.directoryPath() / "nested/camera.jpg") ==
          std::vector<std::byte>(fixture.output.valueIfPresent()->encodedBytes().begin(),
                                 fixture.output.valueIfPresent()->encodedBytes().end()));
    const auto committedRevision =
        storage::SourceFileRevisionCalculator{}.calculate(result.valueIfPresent()->committedFile);
    REQUIRE(committedRevision.valueIfPresent());
    CHECK(committedRevision.valueIfPresent()->encodedSha256 == fixture.output.valueIfPresent()->encodedSha256());
    const auto backupRoot = fixture.directory.directoryPath() / "JPG Spinner Backups";
    REQUIRE(std::filesystem::is_directory(backupRoot));
    unsigned backups{};
    for (const auto &item : std::filesystem::recursive_directory_iterator(backupRoot))
        if (item.is_regular_file())
        {
            ++backups;
            CHECK(item.path().filename() == "camera.jpg");
            CHECK(item.path().parent_path().filename() == "nested");
            CHECK(item.path().parent_path().parent_path().parent_path() == backupRoot);
            CHECK(readBytes(item.path()) == fixture.sourceBytes);
            const auto backup = winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(item.path().wstring()).get();
            const auto revision = storage::SourceFileRevisionCalculator{}.calculate(backup);
            REQUIRE(revision.valueIfPresent() != nullptr);
            CHECK(revision.valueIfPresent()->encodedSha256 == request.expectedSourceRevision.encodedSha256);
            CHECK(revision.valueIfPresent()->encodedLengthBytes == request.expectedSourceRevision.encodedLengthBytes);
        }
    CHECK(backups == 1);
    fixture.requireNoStages();
    const auto transactions = fixture.journalStore.GetFoldersAsync().get();
    REQUIRE(transactions.Size() == 1);
    const winrt::guid identifier{std::wstring_view{transactions.GetAt(0).Name()}};
    const auto terminal =
        storage::internal::ImageFileTransactionJournal{transactions.GetAt(0), identifier}.readLatest();
    REQUIRE(terminal.has_value());
    CHECK(terminal->generation == 6);
    CHECK(terminal->state == storage::internal::ImageFileTransactionState::OwnedStagingArtifactsCleaned);
}

TEST_CASE("a native backup failure never changes the original or reports commit",
          "[storage][transaction][replace][backup-fault]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    auto request = fixture.requestFor(fixture.createSource("camera.jpg"));
    request.outputDisposition = domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup;
    class DeniedBackup final : public storage::internal::FileTransactionOperations
    {
      public:
        mutable unsigned calls{};
        winrt::Windows::Storage::StorageFile copySourceToBackup(const winrt::Windows::Storage::StorageFile &,
                                                                const winrt::Windows::Storage::StorageFolder &,
                                                                const winrt::hstring &) const override
        {
            ++calls;
            throw winrt::hresult_access_denied();
        }
    } operations;
    storage::internal::ImageFileTransactionBatch batch{fixture.root, fixture.journalStore};
    const auto result = storage::internal::RecoverableImageFileTransaction::execute(
        batch, request, *fixture.output.valueIfPresent(), {}, operations);
    REQUIRE(operations.calls == 1);
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::BackupCreationFailed);
    CHECK(result.errorIfPresent()->stage == domain::ImageProcessingStage::BackupCreation);
    CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") == fixture.sourceBytes);
    fixture.requireNoStages();
    const auto recovery = storage::WindowsStorageImageFileTransactionEngine{fixture.root, fixture.journalStore}
                              .recoverIncompleteTransactions();
    REQUIRE(recovery.valueIfPresent());
    CHECK(recovery.valueIfPresent()->recoveredTransactions.empty());
}

TEST_CASE("backup flush close reopen and digest failures retain the backup and original",
          "[storage][transaction][replace][backup-verification-fault]")
{
    TransactionTestApartment apartment;
    enum class BackupFault
    {
        Flush,
        Close,
        Reopen,
        Digest
    };
    const auto fault = GENERATE(BackupFault::Flush, BackupFault::Close, BackupFault::Reopen, BackupFault::Digest);
    TransactionFixture fixture;
    auto request = fixture.requestFor(fixture.createSource("camera.jpg"));
    request.outputDisposition = domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup;
    struct FailedBackupVerification final : storage::internal::FileTransactionOperations
    {
        BackupFault fault;
        mutable unsigned calls{};
        mutable winrt::Windows::Storage::StorageFile backup{nullptr};
        explicit FailedBackupVerification(BackupFault selectedFault) : fault(selectedFault)
        {
        }
        winrt::Windows::Storage::StorageFile copySourceToBackup(const winrt::Windows::Storage::StorageFile &source,
                                                                const winrt::Windows::Storage::StorageFolder &parent,
                                                                const winrt::hstring &name) const override
        {
            backup = FileTransactionOperations::copySourceToBackup(source, parent, name);
            return backup;
        }
        bool flushBackup(const winrt::Windows::Storage::Streams::IRandomAccessStream &stream) const override
        {
            if (fault == BackupFault::Flush)
            {
                ++calls;
                return false;
            }
            return FileTransactionOperations::flushBackup(stream);
        }
        void closeBackup(const winrt::Windows::Storage::Streams::IRandomAccessStream &stream) const override
        {
            FileTransactionOperations::closeBackup(stream);
            if (fault == BackupFault::Close)
            {
                ++calls;
                throw winrt::hresult_error{E_FAIL};
            }
        }
        domain::ImageProcessingResult<domain::SourceFileRevision> captureBackupRevision(
            const winrt::Windows::Storage::StorageFile &file, std::stop_token token) const override
        {
            ++calls;
            if (fault == BackupFault::Reopen)
                return domain::ImageProcessingResult<domain::SourceFileRevision>::failure(
                    {domain::ImageProcessingErrorCode::SourceAccessDenied,
                     domain::ImageProcessingStage::SourceRevisionCapture, domain::WindowsHResult{E_ACCESSDENIED}});
            // Change actual persisted bytes, not the expected digest or calculator.
            if (fault == BackupFault::Digest)
                writeBytes(std::filesystem::path{file.Path().c_str()}, std::array{std::byte{0x31}});
            return FileTransactionOperations::captureBackupRevision(file, token);
        }
    } operations{fault};
    storage::internal::ImageFileTransactionBatch batch{fixture.root, fixture.journalStore};
    const auto result = storage::internal::RecoverableImageFileTransaction::execute(
        batch, request, *fixture.output.valueIfPresent(), {}, operations);
    REQUIRE(operations.calls == 1);
    REQUIRE(operations.backup != nullptr);
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::BackupVerificationFailed);
    CHECK(result.errorIfPresent()->stage == domain::ImageProcessingStage::BackupVerification);
    CHECK(std::filesystem::exists(std::filesystem::path{operations.backup.Path().c_str()}));
    CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") == fixture.sourceBytes);
    fixture.requireNoStages();
}

TEST_CASE("source mutation during backup verification is detected without deleting the verified copy",
          "[storage][transaction][replace][backup-source-mutation]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    auto request = fixture.requestFor(fixture.createSource("camera.jpg"));
    request.outputDisposition = domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup;
    struct SourceChangedDuringBackup final : storage::internal::FileTransactionOperations
    {
        std::filesystem::path sourcePath;
        mutable winrt::Windows::Storage::StorageFile backup{nullptr};
        explicit SourceChangedDuringBackup(std::filesystem::path path) : sourcePath(std::move(path))
        {
        }
        domain::ImageProcessingResult<domain::SourceFileRevision> captureBackupRevision(
            const winrt::Windows::Storage::StorageFile &file, std::stop_token token) const override
        {
            backup = file;
            const auto revision = FileTransactionOperations::captureBackupRevision(file, token);
            writeBytes(sourcePath, std::array{std::byte{0x37}});
            return revision;
        }
    } operations{fixture.directory.directoryPath() / "camera.jpg"};
    storage::internal::ImageFileTransactionBatch batch{fixture.root, fixture.journalStore};
    const auto result = storage::internal::RecoverableImageFileTransaction::execute(
        batch, request, *fixture.output.valueIfPresent(), {}, operations);
    REQUIRE(operations.backup != nullptr);
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::SourceChangedAfterAnalysis);
    CHECK(readBytes(operations.sourcePath) == std::vector<std::byte>{std::byte{0x37}});
    CHECK(readBytes(std::filesystem::path{operations.backup.Path().c_str()}) == fixture.sourceBytes);
    fixture.requireNoStages();
    const auto recovery = storage::WindowsStorageImageFileTransactionEngine{fixture.root, fixture.journalStore}
                              .recoverIncompleteTransactions();
    REQUIRE(recovery.valueIfPresent());
    CHECK(recovery.valueIfPresent()->recoveredTransactions.empty());
    CHECK(readBytes(operations.sourcePath) == std::vector<std::byte>{std::byte{0x37}});
}

TEST_CASE("cancellation while hashing a retained backup is cancellation before commit",
          "[storage][transaction][replace][backup-cancellation]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    const auto source = fixture.createSource("camera.jpg");
    auto request = fixture.requestFor(source);
    request.outputDisposition = domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup;
    std::stop_source cancellation;
    struct CancelBackupRead final : storage::internal::FileTransactionOperations
    {
        std::stop_source &cancellation;
        mutable unsigned replacementCalls{};
        explicit CancelBackupRead(std::stop_source &source) : cancellation(source)
        {
        }
        domain::ImageProcessingResult<domain::SourceFileRevision> captureBackupRevision(
            const winrt::Windows::Storage::StorageFile &backup, std::stop_token token) const override
        {
            cancellation.request_stop();
            return FileTransactionOperations::captureBackupRevision(backup, token);
        }
        void replaceOriginalWithStage(const winrt::Windows::Storage::StorageFile &stage,
                                      const winrt::Windows::Storage::StorageFile &original) const override
        {
            ++replacementCalls;
            FileTransactionOperations::replaceOriginalWithStage(stage, original);
        }
    } operations{cancellation};
    storage::internal::ImageFileTransactionBatch batch{fixture.root, fixture.journalStore};
    const auto result = storage::internal::RecoverableImageFileTransaction::execute(
        batch, request, *fixture.output.valueIfPresent(), cancellation.get_token(), operations);
    REQUIRE(result.errorIfPresent());
    CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::Cancelled);
    CHECK(result.errorIfPresent()->stage == domain::ImageProcessingStage::BackupVerification);
    CHECK(operations.replacementCalls == 0);
    CHECK(readBytes(std::filesystem::path{source.Path().c_str()}) == fixture.sourceBytes);
    fixture.requireNoStages();
}

TEST_CASE("uncertain native replacement and postcommit allocation failure require persisted recovery",
          "[storage][transaction][replace][commit-uncertainty]")
{
    TransactionTestApartment apartment;
    const auto afterNativeReplacement = GENERATE(true, false);
    TransactionFixture fixture;
    const auto source = fixture.createSource("camera.jpg");
    auto request = fixture.requestFor(source);
    request.outputDisposition = domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup;
    struct InterruptedCommit final : storage::internal::FileTransactionOperations
    {
        bool afterNativeReplacement;
        mutable unsigned failureCalls{};
        explicit InterruptedCommit(bool afterReplacement) : afterNativeReplacement(afterReplacement)
        {
        }
        void replaceOriginalWithStage(const winrt::Windows::Storage::StorageFile &stage,
                                      const winrt::Windows::Storage::StorageFile &original) const override
        {
            FileTransactionOperations::replaceOriginalWithStage(stage, original);
            if (afterNativeReplacement)
            {
                ++failureCalls;
                throw winrt::hresult_error{E_FAIL};
            }
        }
        domain::ImageProcessingResult<domain::SourceFileRevision> captureCommittedRevision(
            const winrt::Windows::Storage::StorageFile &) const override
        {
            ++failureCalls;
            throw std::bad_alloc{};
        }
    } operations{afterNativeReplacement};
    storage::internal::ImageFileTransactionBatch batch{fixture.root, fixture.journalStore};
    const auto result = storage::internal::RecoverableImageFileTransaction::execute(
        batch, request, *fixture.output.valueIfPresent(), {}, operations);
    REQUIRE(result.errorIfPresent());
    CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::RecoveryConflict);
    CHECK(result.errorIfPresent()->stage == domain::ImageProcessingStage::TransactionRecovery);
    CHECK(operations.failureCalls == 1);
    const std::vector<std::byte> expected{fixture.output.valueIfPresent()->encodedBytes().begin(),
                                          fixture.output.valueIfPresent()->encodedBytes().end()};
    CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") == expected);
    unsigned backups{};
    for (const auto &item :
         std::filesystem::recursive_directory_iterator(fixture.directory.directoryPath() / "JPG Spinner Backups"))
        if (item.is_regular_file())
        {
            ++backups;
            CHECK(readBytes(item.path()) == fixture.sourceBytes);
        }
    CHECK(backups == 1);
    // A fresh public engine must recover from native files alone, not fault-seam
    // counters, the old batch object, or an earlier return value.
    storage::WindowsStorageImageFileTransactionEngine restarted{fixture.root, fixture.journalStore};
    const auto recovered = restarted.recoverIncompleteTransactions();
    REQUIRE(recovered.valueIfPresent());
    REQUIRE(recovered.valueIfPresent()->recoveredTransactions.size() == 1);
    CHECK(recovered.valueIfPresent()->recoveredTransactions.front().outcome ==
          storage::ImageFileTransactionRecoveryOutcome::OutputCommitted);
    fixture.requireNoStages();
}

TEST_CASE("failed replacement requires unchanged original proof before abandonment",
          "[storage][transaction][replace][commit-uncertainty][abandonment-proof]")
{
    TransactionTestApartment apartment;
    enum class OriginalAfterFailure
    {
        Unchanged,
        ChangedBytes,
        Renamed,
        SameBytesDifferentIdentity
    };
    const auto observation = GENERATE(OriginalAfterFailure::Unchanged, OriginalAfterFailure::ChangedBytes,
                                      OriginalAfterFailure::Renamed, OriginalAfterFailure::SameBytesDifferentIdentity);
    TransactionFixture fixture;
    auto request = fixture.requestFor(fixture.createSource("camera.jpg"));
    request.outputDisposition = domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup;
    struct FailedReplacement final : storage::internal::FileTransactionOperations
    {
        const OriginalAfterFailure observation;
        const std::vector<std::byte> sourceBytes;
        mutable std::filesystem::path stagePath;
        FailedReplacement(OriginalAfterFailure selected, const std::vector<std::byte> &bytes)
            : observation(selected), sourceBytes(bytes)
        {
        }
        void replaceOriginalWithStage(const winrt::Windows::Storage::StorageFile &stage,
                                      const winrt::Windows::Storage::StorageFile &original) const override
        {
            stagePath = std::filesystem::path{stage.Path().c_str()};
            const auto originalPath = std::filesystem::path{original.Path().c_str()};
            // Model ambiguous observations, not an undocumented WinRT internal
            // implementation. The owned stage remains at its original identity.
            if (observation == OriginalAfterFailure::ChangedBytes)
                winrt::Windows::Storage::FileIO::WriteTextAsync(original, L"changed during failed replacement").get();
            if (observation == OriginalAfterFailure::Renamed ||
                observation == OriginalAfterFailure::SameBytesDifferentIdentity)
                original.RenameAsync(L"camera-before-failure.jpg").get();
            if (observation == OriginalAfterFailure::SameBytesDifferentIdentity)
                writeBytes(originalPath, sourceBytes);
            throw winrt::hresult_error{E_FAIL};
        }
    } operations{observation, fixture.sourceBytes};
    storage::internal::ImageFileTransactionBatch batch{fixture.root, fixture.journalStore};
    const auto result = storage::internal::RecoverableImageFileTransaction::execute(
        batch, request, *fixture.output.valueIfPresent(), {}, operations);
    REQUIRE(result.errorIfPresent());
    const auto folders = fixture.journalStore.GetFoldersAsync().get();
    REQUIRE(folders.Size() == 1);
    storage::internal::ImageFileTransactionJournal journal{folders.GetAt(0),
                                                           winrt::guid{std::wstring_view{folders.GetAt(0).Name()}}};
    const auto persisted = journal.readLatest();
    REQUIRE(persisted);
    if (observation == OriginalAfterFailure::Unchanged)
    {
        CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::OriginalReplacementFailed);
        CHECK(persisted->state == storage::internal::ImageFileTransactionState::TransactionAbandonedBeforeCommit);
        fixture.requireNoStages();
        CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") == fixture.sourceBytes);
    }
    else
    {
        CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::RecoveryConflict);
        CHECK(persisted->state == storage::internal::ImageFileTransactionState::VerifiedBackupCreated);
        REQUIRE(std::filesystem::is_regular_file(operations.stagePath));
        const std::vector<std::byte> expectedStage{fixture.output.valueIfPresent()->encodedBytes().begin(),
                                                   fixture.output.valueIfPresent()->encodedBytes().end()};
        CHECK(readBytes(operations.stagePath) == expectedStage);
    }
    // Every refusal retains the independently verified backup, even when the
    // source's current bytes or identity no longer explain a safe no-commit result.
    for (const auto &item :
         std::filesystem::recursive_directory_iterator(fixture.directory.directoryPath() / "JPG Spinner Backups"))
        if (item.is_regular_file())
            CHECK(readBytes(item.path()) == fixture.sourceBytes);
}

TEST_CASE("another engine cannot recover or replace a live transaction's closed stage",
          "[storage][transaction][copy][journal-store-ownership]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    const auto source = fixture.createSource("camera.jpg");
    const auto request = fixture.requestFor(source);
    struct HeldStageVerification final : storage::internal::FileTransactionOperations
    {
        mutable std::promise<void> boundary;
        std::shared_future<void> release;
        explicit HeldStageVerification(std::shared_future<void> releaseBoundary) : release(std::move(releaseBoundary))
        {
        }
        domain::ImageProcessingResult<domain::SourceFileRevision> captureStagedRevision(
            const winrt::Windows::Storage::StorageFile &stage, std::stop_token token) const override
        {
            const auto revision = FileTransactionOperations::captureStagedRevision(stage, token);
            boundary.set_value();
            release.wait();
            return revision;
        }
    };
    std::promise<void> release;
    HeldStageVerification operations{release.get_future().share()};
    auto reached = operations.boundary.get_future();
    auto running = std::async(std::launch::async, [&]() {
        TransactionTestApartment workerApartment;
        storage::internal::ImageFileTransactionBatch batch{fixture.root, fixture.journalStore};
        return storage::internal::RecoverableImageFileTransaction::execute(
            batch, request, *fixture.output.valueIfPresent(), {}, operations);
    });
    struct ReleaseOnFailure final
    {
        std::promise<void> &release;
        bool released{};
        ~ReleaseOnFailure()
        {
            if (!released)
                release.set_value();
        }
    } unblock{release};
    REQUIRE(reached.wait_for(std::chrono::seconds{10}) == std::future_status::ready);
    storage::WindowsStorageImageFileTransactionEngine otherEngine{fixture.root, fixture.journalStore};
    const auto recoveryWhileActive = otherEngine.recoverIncompleteTransactions();
    const auto transactionWhileActive = otherEngine.execute(request, *fixture.output.valueIfPresent());
    release.set_value();
    unblock.released = true;
    const auto finished = running.get();
    REQUIRE(recoveryWhileActive.errorIfPresent());
    CHECK(recoveryWhileActive.errorIfPresent()->code == domain::ImageProcessingErrorCode::JournalStoreBusy);
    CHECK(recoveryWhileActive.errorIfPresent()->stage == domain::ImageProcessingStage::JournalStoreLeaseAcquisition);
    REQUIRE(transactionWhileActive.errorIfPresent());
    CHECK(transactionWhileActive.errorIfPresent()->code == domain::ImageProcessingErrorCode::JournalStoreBusy);
    CHECK(transactionWhileActive.errorIfPresent()->stage == domain::ImageProcessingStage::JournalStoreLeaseAcquisition);
    REQUIRE(finished.valueIfPresent());
    CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") == fixture.sourceBytes);
    const auto recoveredAfterExit = otherEngine.recoverIncompleteTransactions();
    REQUIRE(recoveredAfterExit.valueIfPresent());
    CHECK(recoveredAfterExit.valueIfPresent()->recoveredTransactions.empty());
}

TEST_CASE("only the two authorized output dispositions admit a transaction",
          "[storage][transaction][closed-dispositions]")
{
    static_assert(static_cast<int>(domain::OutputDisposition::CreateCorrectedCopy) == 0);
    static_assert(static_cast<int>(domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup) == 1);
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    const auto source = fixture.createSource("camera.jpg");
    auto request = fixture.requestFor(source);
    request.outputDisposition = static_cast<domain::OutputDisposition>(GENERATE(-1, 2, 3, 2147483647));
    const auto result = storage::WindowsStorageImageFileTransactionEngine{fixture.root, fixture.journalStore}.execute(
        request, *fixture.output.valueIfPresent());
    REQUIRE(result.errorIfPresent());
    CHECK_FALSE(result.valueIfPresent());
    CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") == fixture.sourceBytes);
    CHECK_FALSE(fixture.root.TryGetItemAsync(L"JPG Spinner Output").get());
    CHECK_FALSE(fixture.root.TryGetItemAsync(L"JPG Spinner Backups").get());
    CHECK(fixture.journalStore.GetItemsAsync().get().Size() == 0);
}

TEST_CASE("native store exclusion survives exception cleanup and terminal abandonment publication",
          "[storage][transaction][copy][journal-store-ownership][abandonment-lease]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    const auto request = fixture.requestFor(fixture.createSource("camera.jpg"));
    struct HeldAbandonment final : storage::internal::FileTransactionOperations
    {
        mutable std::promise<void> boundary;
        std::shared_future<void> release;
        mutable unsigned journalWrites{};
        explicit HeldAbandonment(std::shared_future<void> signal) : release(std::move(signal))
        {
        }
        bool flushStagedBytes(const winrt::Windows::Storage::Streams::IRandomAccessStream &) const override
        {
            // Real writing completed, but this controlled native failure enters
            // exception cleanup rather than the ordinary successful-commit path.
            return false;
        }
        std::uint32_t writeJournalBytes(const winrt::Windows::Storage::Streams::IRandomAccessStream &stream,
                                        const winrt::Windows::Storage::Streams::IBuffer &bytes) const override
        {
            if (++journalWrites == 2)
            {
                boundary.set_value();
                release.wait();
            }
            return FileTransactionOperations::writeJournalBytes(stream, bytes);
        }
    };
    std::promise<void> release;
    HeldAbandonment operations{release.get_future().share()};
    auto reached = operations.boundary.get_future();
    auto running = std::async(std::launch::async, [&]() {
        TransactionTestApartment workerApartment;
        storage::internal::ImageFileTransactionBatch batch{fixture.root, fixture.journalStore};
        return storage::internal::RecoverableImageFileTransaction::execute(
            batch, request, *fixture.output.valueIfPresent(), {}, operations);
    });
    struct ReleaseOnFailure final
    {
        std::promise<void> &release;
        bool released{};
        ~ReleaseOnFailure()
        {
            if (!released)
                release.set_value();
        }
    } unblock{release};
    REQUIRE(reached.wait_for(std::chrono::seconds{10}) == std::future_status::ready);
    fixture.requireNoStages();
    const storage::WindowsStorageImageFileTransactionEngine other{fixture.root, fixture.journalStore};
    const auto whileClosing = other.recoverIncompleteTransactions();
    release.set_value();
    unblock.released = true;
    const auto finished = running.get();
    REQUIRE(whileClosing.errorIfPresent());
    CHECK(whileClosing.errorIfPresent()->code == domain::ImageProcessingErrorCode::JournalStoreBusy);
    REQUIRE(finished.errorIfPresent());
    CHECK(finished.errorIfPresent()->code == domain::ImageProcessingErrorCode::StagingFlushFailed);
    REQUIRE(operations.journalWrites == 2);
    CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") == fixture.sourceBytes);
    const auto closed = other.recoverIncompleteTransactions();
    REQUIRE(closed.valueIfPresent());
    CHECK(closed.valueIfPresent()->recoveredTransactions.empty());
}

TEST_CASE("identical bytes in a substituted committed file do not prove stage identity transfer",
          "[storage][transaction][replace][committed-identity]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    auto request = fixture.requestFor(fixture.createSource("camera.jpg"));
    request.outputDisposition = domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup;
    struct SubstituteCommittedFile final : storage::internal::FileTransactionOperations
    {
        mutable unsigned calls{};
        domain::ImageProcessingResult<domain::SourceFileRevision> captureCommittedRevision(
            const winrt::Windows::Storage::StorageFile &committed) const override
        {
            ++calls;
            const std::filesystem::path path{committed.Path().c_str()};
            const auto bytes = readBytes(path);
            // The original stage object is retained under another name. An
            // external, same-byte file is not the journal's committed identity.
            std::filesystem::rename(path, path.parent_path() / "retained-stage.bin");
            writeBytes(path, bytes);
            const auto substituted = winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(path.wstring()).get();
            return FileTransactionOperations::captureCommittedRevision(substituted);
        }
    } operations;
    storage::internal::ImageFileTransactionBatch batch{fixture.root, fixture.journalStore};
    const auto result = storage::internal::RecoverableImageFileTransaction::execute(
        batch, request, *fixture.output.valueIfPresent(), {}, operations);
    REQUIRE(result.errorIfPresent());
    CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::RecoveryConflict);
    CHECK(result.errorIfPresent()->stage == domain::ImageProcessingStage::TransactionRecovery);
    CHECK(operations.calls == 1);
    const std::vector<std::byte> expected{fixture.output.valueIfPresent()->encodedBytes().begin(),
                                          fixture.output.valueIfPresent()->encodedBytes().end()};
    CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") == expected);
    CHECK(readBytes(fixture.directory.directoryPath() / "retained-stage.bin") == expected);
    fixture.requireNoStages();
}

TEST_CASE("every immutable publication failure preserves originals or reports committed-state uncertainty",
          "[storage][transaction][journal-publication-matrix]")
{
    TransactionTestApartment apartment;
    const auto replacement = GENERATE(false, true);
    const auto targetGeneration = GENERATE_COPY(Catch::Generators::range(1u, replacement ? 7u : 6u));
    enum class PublicationFault
    {
        Write,
        Flush,
        Close,
        BeforePublish,
        AfterPublish
    };
    const auto selectedFault = GENERATE(PublicationFault::Write, PublicationFault::Flush, PublicationFault::Close,
                                        PublicationFault::BeforePublish, PublicationFault::AfterPublish);
    TransactionFixture fixture;
    auto request = fixture.requestFor(fixture.createSource("camera.jpg"));
    if (replacement)
        request.outputDisposition = domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup;
    struct FailedPublication final : storage::internal::FileTransactionOperations
    {
        unsigned target;
        PublicationFault fault;
        mutable unsigned generation{};
        mutable unsigned faultCalls{};
        mutable unsigned replacementCalls{};
        mutable unsigned publishedGeneration{};
        FailedPublication(unsigned targetGeneration, PublicationFault selected)
            : target(targetGeneration), fault(selected)
        {
        }
        winrt::Windows::Storage::StorageFile createPendingJournalGeneration(
            const winrt::Windows::Storage::StorageFolder &folder, const winrt::hstring &name) const override
        {
            ++generation;
            return FileTransactionOperations::createPendingJournalGeneration(folder, name);
        }
        void failIf(PublicationFault observed) const
        {
            // This matrix selects exactly one interrupted publication boundary.
            // Terminal-abandonment recovery also reads and closes generations;
            // those calls must not accidentally inject additional failures.
            if (faultCalls == 0 && generation == target && fault == observed)
            {
                ++faultCalls;
                throw winrt::hresult_error{E_FAIL};
            }
        }
        std::uint32_t writeJournalBytes(const winrt::Windows::Storage::Streams::IRandomAccessStream &stream,
                                        const winrt::Windows::Storage::Streams::IBuffer &bytes) const override
        {
            failIf(PublicationFault::Write);
            return FileTransactionOperations::writeJournalBytes(stream, bytes);
        }
        bool flushJournalBytes(const winrt::Windows::Storage::Streams::IRandomAccessStream &stream) const override
        {
            failIf(PublicationFault::Flush);
            return FileTransactionOperations::flushJournalBytes(stream);
        }
        void closeJournalGeneration(const winrt::Windows::Storage::Streams::IRandomAccessStream &stream) const override
        {
            failIf(PublicationFault::Close);
            FileTransactionOperations::closeJournalGeneration(stream);
        }
        void publishJournalGeneration(const winrt::Windows::Storage::StorageFile &pending,
                                      const winrt::hstring &name) const override
        {
            failIf(PublicationFault::BeforePublish);
            FileTransactionOperations::publishJournalGeneration(pending, name);
            publishedGeneration = generation;
            failIf(PublicationFault::AfterPublish);
        }
        void replaceOriginalWithStage(const winrt::Windows::Storage::StorageFile &stage,
                                      const winrt::Windows::Storage::StorageFile &source) const override
        {
            // Count actual admission, not a claimed flag. Replacement cannot be
            // reached through the failing backup-generation publication.
            CHECK(publishedGeneration == 4);
            ++replacementCalls;
            FileTransactionOperations::replaceOriginalWithStage(stage, source);
        }
    } operations{targetGeneration, selectedFault};
    storage::internal::ImageFileTransactionBatch batch{fixture.root, fixture.journalStore};
    const auto result = storage::internal::RecoverableImageFileTransaction::execute(
        batch, request, *fixture.output.valueIfPresent(), {}, operations);
    REQUIRE(operations.faultCalls == 1);
    REQUIRE(result.errorIfPresent());
    const bool committed = targetGeneration >= (replacement ? 5u : 4u);
    CHECK(result.errorIfPresent()->code == (committed ? domain::ImageProcessingErrorCode::RecoveryConflict
                                                      : domain::ImageProcessingErrorCode::JournalPersistenceFailed));
    CHECK(operations.replacementCalls == (replacement && committed ? 1u : 0u));
    const std::vector<std::byte> expected{fixture.output.valueIfPresent()->encodedBytes().begin(),
                                          fixture.output.valueIfPresent()->encodedBytes().end()};
    CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") ==
          (replacement && committed ? expected : fixture.sourceBytes));
    for (const auto &item : std::filesystem::recursive_directory_iterator(fixture.directory.directoryPath()))
        if (item.is_regular_file() &&
            item.path().lexically_relative(fixture.directory.directoryPath()).begin()->wstring() ==
                L"JPG Spinner Backups")
            CHECK(readBytes(item.path()) == fixture.sourceBytes);
    // Restart against persisted files, not this seam's publication counters.
    storage::WindowsStorageImageFileTransactionEngine restarted{fixture.root, fixture.journalStore};
    const auto recovered = restarted.recoverIncompleteTransactions();
    if (targetGeneration == 1 && selectedFault != PublicationFault::AfterPublish)
    {
        REQUIRE(recovered.errorIfPresent());
        CHECK(recovered.errorIfPresent()->code == domain::ImageProcessingErrorCode::RecoveryConflict);
    }
    else
    {
        REQUIRE(recovered.valueIfPresent());
        const bool alreadyClosed = !committed || (targetGeneration == (replacement ? 6u : 5u) &&
                                                  selectedFault == PublicationFault::AfterPublish);
        if (alreadyClosed)
            CHECK(recovered.valueIfPresent()->recoveredTransactions.empty());
        else
        {
            REQUIRE(recovered.valueIfPresent()->recoveredTransactions.size() == 1);
            CHECK(recovered.valueIfPresent()->recoveredTransactions.front().outcome ==
                  storage::ImageFileTransactionRecoveryOutcome::OutputCommitted);
        }
    }
}
