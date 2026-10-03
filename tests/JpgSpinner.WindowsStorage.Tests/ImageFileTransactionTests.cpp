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
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
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
    storage::WindowsStorageImageFileTransactionEngine nativeEngine{root};
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
    storage::WindowsStorageImageFileTransactionEngine engine{fixture.root};
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
    storage::internal::CorrectedCopyBatch batch{fixture.root};
    const auto result = storage::internal::RecoverableImageFileTransaction::executeCorrectedCopy(
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
    const auto first = storage::WindowsStorageImageFileTransactionEngine{fixture.root}.execute(
        request, *fixture.output.valueIfPresent());
    const auto second = storage::WindowsStorageImageFileTransactionEngine{fixture.root}.execute(
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
    const auto result = storage::WindowsStorageImageFileTransactionEngine{fixture.root}.execute(
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
    const auto result = storage::WindowsStorageImageFileTransactionEngine{fixture.root}.execute(
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
    storage::internal::CorrectedCopyBatch batch{fixture.root};
    const auto result = storage::internal::RecoverableImageFileTransaction::executeCorrectedCopy(
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
    storage::internal::CorrectedCopyBatch batch{fixture.root};
    const auto result = storage::internal::RecoverableImageFileTransaction::executeCorrectedCopy(
        batch, request, *fixture.output.valueIfPresent(), {}, operations);
    CHECK(operations.calls == 1);
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::StagingCloseFailed);
    CHECK(result.errorIfPresent()->stage == domain::ImageProcessingStage::StagingClose);
    CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") == fixture.sourceBytes);
    fixture.requireNoStages();
    CHECK(batch.folder.GetFilesAsync().get().Size() == 0);
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
    storage::internal::CorrectedCopyBatch batch{fixture.root};
    const auto result = storage::internal::RecoverableImageFileTransaction::executeCorrectedCopy(
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
    CHECK(batch.folder.GetFilesAsync().get().Size() == 0);
}

TEST_CASE("a replaced batch folder is not adopted merely because its canonical pathname matches",
          "[storage][transaction][copy][batch-identity]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    storage::WindowsStorageImageFileTransactionEngine engine{fixture.root};
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
    storage::internal::CorrectedCopyBatch batch{fixture.root};
    const auto result = storage::internal::RecoverableImageFileTransaction::executeCorrectedCopy(
        batch, request, *fixture.output.valueIfPresent(), {}, operations);
    CHECK(operations.calls == 1);
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::RecoveryConflict);
    CHECK(result.errorIfPresent()->stage == domain::ImageProcessingStage::TransactionRecovery);
    const std::filesystem::path committedPath = std::filesystem::path{batch.folder.Path().c_str()} / "camera.jpg";
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
    storage::internal::CorrectedCopyBatch batch{fixture.root};
    const auto result = storage::internal::RecoverableImageFileTransaction::executeCorrectedCopy(
        batch, request, *fixture.output.valueIfPresent(), {}, operations);
    CHECK(operations.hashCalls == 1);
    CHECK(operations.moveCalls == 0);
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::StagedOutputHashMismatch);
    CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") == fixture.sourceBytes);
    fixture.requireNoStages();
    CHECK(batch.folder.GetFilesAsync().get().Size() == 0);
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
    storage::internal::CorrectedCopyBatch batch{fixture.root};
    for (unsigned attempt = 0; attempt < 2; ++attempt)
    {
        cancellation = std::stop_source{};
        const auto result = storage::internal::RecoverableImageFileTransaction::executeCorrectedCopy(
            batch, request, *fixture.output.valueIfPresent(), cancellation.get_token(), operations);
        REQUIRE(result.errorIfPresent() != nullptr);
        CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::Cancelled);
        CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") == fixture.sourceBytes);
    }
    CHECK(operations.hashCalls == 2);
    REQUIRE(operations.stageNames.size() == 2);
    CHECK(operations.stageNames[0] != operations.stageNames[1]);
    const std::filesystem::path batchPath{batch.folder.Path().c_str()};
    for (const auto &ownedName : operations.stageNames)
        CHECK(!std::filesystem::exists(batchPath / ownedName));
    CHECK(!std::filesystem::exists(batchPath / "camera.jpg"));
    const std::vector<std::byte> sentinel{std::byte{1}, std::byte{2}, std::byte{3}};
    CHECK(readBytes(batchPath / L".jpg-spinner-staged-{00000000-0000-0000-0000-000000000001}.jpg") == sentinel);
    CHECK(batch.folder.GetFilesAsync().get().Size() == 1);
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
    storage::internal::CorrectedCopyBatch batch{fixture.root};
    const auto result = storage::internal::RecoverableImageFileTransaction::executeCorrectedCopy(
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
    storage::internal::CorrectedCopyBatch batch{fixture.root};
    const auto result = storage::internal::RecoverableImageFileTransaction::executeCorrectedCopy(
        batch, request, *fixture.output.valueIfPresent(), {}, operations);
    CHECK(operations.failureCalls == 1);
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == failureCase.code);
    CHECK(result.errorIfPresent()->stage == failureCase.stage);
    CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") == fixture.sourceBytes);
    fixture.requireNoStages();
    if (batch.folder)
        CHECK(batch.folder.GetFilesAsync().get().Size() == 0);
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
    const auto result = storage::WindowsStorageImageFileTransactionEngine{selected}.execute(
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
    const auto result = storage::WindowsStorageImageFileTransactionEngine{fixture.root}.execute(
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
    const auto result = storage::WindowsStorageImageFileTransactionEngine{fixture.root}.execute(
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
    const auto result =
        storage::WindowsStorageImageFileTransactionEngine{fixture.root}.execute(request, *produced.valueIfPresent());
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
    storage::WindowsStorageImageFileTransactionEngine nativeEngine{fixture.root};
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
    storage::internal::CorrectedCopyBatch batch{fixture.root};
    const auto result = storage::internal::RecoverableImageFileTransaction::executeCorrectedCopy(
        batch, request, *fixture.output.valueIfPresent(), {}, operations);
    CHECK(operations.calls == 1);
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::SourceChangedAfterAnalysis);
    CHECK(result.errorIfPresent()->stage == domain::ImageProcessingStage::SourceRevisionRevalidation);
    CHECK(std::filesystem::last_write_time(sourcePath) == originalWriteTime);
    CHECK(readBytes(sourcePath) == externallyChangedBytes);
    fixture.requireNoStages();
    CHECK(batch.folder.GetFilesAsync().get().Size() == 0);
}

TEST_CASE("an implicit process MTA cannot supply the transaction worker's COM lifetime",
          "[storage][transaction][copy][implicit-mta]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    const auto request = fixture.requestFor(fixture.createSource("camera.jpg"));
    storage::WindowsStorageImageFileTransactionEngine engine{fixture.root};
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

TEST_CASE("replacement is explicitly unavailable until the verified-backup transaction is implemented",
          "[storage][transaction][copy][replacement-unavailable]")
{
    TransactionTestApartment apartment;
    TransactionFixture fixture;
    auto request = fixture.requestFor(fixture.createSource("camera.jpg"));
    request.outputDisposition = domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup;
    const auto result = storage::WindowsStorageImageFileTransactionEngine{fixture.root}.execute(
        request, *fixture.output.valueIfPresent());
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::OriginalReplacementFailed);
    CHECK(result.errorIfPresent()->stage == domain::ImageProcessingStage::OriginalReplacement);
    const auto *native = std::get_if<domain::WindowsHResult>(&result.errorIfPresent()->nativeErrorProjection);
    REQUIRE(native != nullptr);
    CHECK(native->signedValue == E_NOTIMPL);
    CHECK(readBytes(fixture.directory.directoryPath() / "camera.jpg") == fixture.sourceBytes);
    CHECK(!std::filesystem::exists(fixture.directory.directoryPath() / "JPG Spinner Output"));
    CHECK(!std::filesystem::exists(fixture.directory.directoryPath() / "JPG Spinner Backups"));
}
