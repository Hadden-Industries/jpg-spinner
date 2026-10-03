#define NOMINMAX
#include <windows.h>
#include "DeterministicJpegFixtureFactory.h"
#include "internal/RecoverableImageFileTransaction.h"
#include <jpg_spinner/jpeg/JpegImageAnalyzer.h>
#include <jpg_spinner/jpeg/LibJpegTurboTransformationEngine.h>
#include <jpg_spinner/storage/WindowsStorageImageFileTransactionEngine.h>
#include <catch2/catch_test_macros.hpp>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <utility>
#include <vector>

using namespace jpg_spinner;

namespace
{
std::wstring requiredEnvironment(const wchar_t *name)
{
    const auto length = GetEnvironmentVariableW(name, nullptr, 0);
    REQUIRE(length > 1);
    std::wstring value(length, L'\0');
    const auto copied = GetEnvironmentVariableW(name, value.data(), length);
    REQUIRE(copied > 0);
    REQUIRE(copied < length);
    value.resize(copied);
    return value;
}

void writeFixture(const std::filesystem::path &path, std::span<const std::byte> bytes)
{
    std::ofstream stream{path, std::ios::binary};
    stream.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    stream.close();
    REQUIRE(stream.good());
}

std::vector<std::byte> readFixture(const std::filesystem::path &path)
{
    const auto length = std::filesystem::file_size(path);
    REQUIRE(length < 1024 * 1024);
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    std::ifstream stream{path, std::ios::binary};
    stream.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    REQUIRE(stream.good());
    return bytes;
}

/// Only the parent harness invokes these hidden test cases, with a new exclusive
/// fixture directory and UUID-named event. The child never cleans up: process
/// death must leave real artifacts for a separate recovery process to inspect.
struct ProcessFixture final
{
    std::filesystem::path directory{requiredEnvironment(L"JPG_SPINNER_TERMINATION_FIXTURE")};
    const bool replacement{requiredEnvironment(L"JPG_SPINNER_TERMINATION_DISPOSITION") == L"replace"};
    const winrt::Windows::Storage::StorageFolder root =
        winrt::Windows::Storage::StorageFolder::GetFolderFromPathAsync(directory.wstring()).get();
    const winrt::Windows::Storage::StorageFolder store = root.GetFolderAsync(L"journal-store").get();
};

/// This is a real effect observer, not a simulated journal or recovery oracle.
/// PartialWrite leaves a real truncated pending file; every other boundary uses
/// the native operation unmodified. The signaled process blocks until its owner
/// kills it, avoiding sleep races or graceful exception cleanup.
struct TerminationBoundary final : storage::internal::FileTransactionOperations
{
    const unsigned targetGeneration;
    const std::wstring boundary;
    const winrt::handle reached;
    mutable unsigned generation{};

    TerminationBoundary(unsigned target, std::wstring selectedBoundary, const std::wstring &eventName)
        : targetGeneration(target), boundary(std::move(selectedBoundary)),
          reached(OpenEventW(EVENT_MODIFY_STATE, FALSE, eventName.c_str()))
    {
        REQUIRE(reached.get() != nullptr);
    }
    void stopAt(std::wstring_view observed) const
    {
        if (generation != targetGeneration || observed != boundary)
            return;
        winrt::check_bool(SetEvent(reached.get()));
        // An unsignaled child-owned event keeps this exact boundary stationary.
        // The parent owns process termination and must prove exit before restart.
        winrt::handle neverSignaled{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
        winrt::check_bool(neverSignaled.get() != nullptr);
        REQUIRE(WaitForSingleObject(neverSignaled.get(), INFINITE) == WAIT_OBJECT_0);
    }
    winrt::Windows::Storage::StorageFile createPendingJournalGeneration(
        const winrt::Windows::Storage::StorageFolder &folder, const winrt::hstring &name) const override
    {
        stopAt(L"before-next-effect");
        ++generation;
        return FileTransactionOperations::createPendingJournalGeneration(folder, name);
    }
    std::uint32_t writeJournalBytes(const winrt::Windows::Storage::Streams::IRandomAccessStream &stream,
                                    const winrt::Windows::Storage::Streams::IBuffer &bytes) const override
    {
        stopAt(L"before-write");
        if (generation == targetGeneration && boundary == L"partial-write")
        {
            const auto fullLength = bytes.Length();
            bytes.Length(fullLength / 2);
            FileTransactionOperations::writeJournalBytes(stream, bytes);
            stopAt(L"partial-write");
            bytes.Length(fullLength);
        }
        const auto written = FileTransactionOperations::writeJournalBytes(stream, bytes);
        stopAt(L"after-write");
        return written;
    }
    winrt::Windows::Storage::Streams::IRandomAccessStream openStagedOutput(
        const winrt::Windows::Storage::StorageFile &stage) const override
    {
        stopAt(L"before-next-effect");
        return FileTransactionOperations::openStagedOutput(stage);
    }
    bool flushJournalBytes(const winrt::Windows::Storage::Streams::IRandomAccessStream &stream) const override
    {
        const auto flushed = FileTransactionOperations::flushJournalBytes(stream);
        stopAt(L"after-flush");
        return flushed;
    }
    void closeJournalGeneration(const winrt::Windows::Storage::Streams::IRandomAccessStream &stream) const override
    {
        FileTransactionOperations::closeJournalGeneration(stream);
        stopAt(L"after-close");
    }
    void publishJournalGeneration(const winrt::Windows::Storage::StorageFile &pending,
                                  const winrt::hstring &name) const override
    {
        stopAt(L"before-publish");
        FileTransactionOperations::publishJournalGeneration(pending, name);
        stopAt(L"after-publish");
    }
    winrt::Windows::Storage::StorageFolder createBackupRoot(
        const winrt::Windows::Storage::StorageFolder &root) const override
    {
        stopAt(L"before-next-effect");
        return FileTransactionOperations::createBackupRoot(root);
    }
    void moveStageToCorrectedCopy(const winrt::Windows::Storage::StorageFile &stage,
                                  const winrt::Windows::Storage::StorageFolder &parent,
                                  const winrt::hstring &name) const override
    {
        stopAt(L"before-next-effect");
        FileTransactionOperations::moveStageToCorrectedCopy(stage, parent, name);
    }
    void replaceOriginalWithStage(const winrt::Windows::Storage::StorageFile &stage,
                                  const winrt::Windows::Storage::StorageFile &source) const override
    {
        stopAt(L"before-next-effect");
        FileTransactionOperations::replaceOriginalWithStage(stage, source);
    }
};
} // namespace

TEST_CASE("the transaction termination child holds a real native journal boundary", "[.][termination-child]")
{
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    struct ApartmentLifetime final
    {
        ~ApartmentLifetime()
        {
            winrt::uninit_apartment();
        }
    } apartment;
    ProcessFixture fixture;
    const auto sourceBytes = test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
        test_support::JpegChromaSubsampling::ycbcr444, 3);
    const auto analysis =
        jpeg::JpegImageAnalyzer::analyze(sourceBytes, domain::EdgeHandlingPolicy::TrimPartialMinimumCodedUnits);
    REQUIRE(analysis.valueIfPresent());
    const auto output =
        jpeg::LibJpegTurboTransformationEngine{}.createValidatedOutput(sourceBytes, *analysis.valueIfPresent());
    REQUIRE(output.valueIfPresent());
    writeFixture(fixture.directory / "camera.jpg", sourceBytes);
    writeFixture(fixture.directory / "expected-source.bin", sourceBytes);
    writeFixture(fixture.directory / "expected-output.bin", output.valueIfPresent()->encodedBytes());
    const auto source = fixture.root.GetFileAsync(L"camera.jpg").get();
    const auto revision = storage::SourceFileRevisionCalculator{}.calculate(source);
    REQUIRE(revision.valueIfPresent());
    storage::internal::ImageFileTransactionBatch batch{fixture.root, fixture.store};
    const auto generationText = requiredEnvironment(L"JPG_SPINNER_TERMINATION_GENERATION");
    REQUIRE(generationText.size() == 1);
    REQUIRE(generationText.front() >= L'1');
    REQUIRE(generationText.front() <= L'6');
    TerminationBoundary operations{static_cast<unsigned>(generationText.front() - L'0'),
                                   requiredEnvironment(L"JPG_SPINNER_TERMINATION_BOUNDARY"),
                                   requiredEnvironment(L"JPG_SPINNER_TERMINATION_EVENT")};
    const auto result = storage::internal::RecoverableImageFileTransaction::execute(
        batch,
        {source, *revision.valueIfPresent(),
         fixture.replacement ? domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup
                             : domain::OutputDisposition::CreateCorrectedCopy},
        *output.valueIfPresent(), {}, operations);
    REQUIRE(result.valueIfPresent());
    operations.stopAt(L"before-next-effect");
    FAIL("The requested termination boundary was not reached.");
}

TEST_CASE("a fresh recovery child reconciles terminated native transaction artifacts", "[.][termination-recovery]")
{
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    struct ApartmentLifetime final
    {
        ~ApartmentLifetime()
        {
            winrt::uninit_apartment();
        }
    } apartment;
    ProcessFixture fixture;
    const auto expectedSource = readFixture(fixture.directory / "expected-source.bin");
    const auto expectedOutput = readFixture(fixture.directory / "expected-output.bin");
    const auto expectation = requiredEnvironment(L"JPG_SPINNER_TERMINATION_EXPECTATION");
    const auto sourceBefore = readFixture(fixture.directory / "camera.jpg");
    CHECK(sourceBefore == (fixture.replacement && expectation == L"committed" ? expectedOutput : expectedSource));
    // A conflict is not permission to discard an unexplained stage. Capture the
    // actual interrupted bytes, including an empty stage before its first write,
    // rather than assuming that it already contains the validated output.
    std::vector<std::pair<std::filesystem::path, std::vector<std::byte>>> stagesBefore;
    if (expectation == L"conflict")
    {
        for (const auto &item : std::filesystem::recursive_directory_iterator(fixture.directory))
            if (item.is_regular_file() && item.path().filename().wstring().starts_with(L".jpg-spinner-staged-"))
                stagesBefore.emplace_back(item.path(), readFixture(item.path()));
        REQUIRE(stagesBefore.size() == 1);
    }
    storage::WindowsStorageImageFileTransactionEngine engine{fixture.root, fixture.store};
    for (unsigned recoveryAttempt = 0; recoveryAttempt != 2; ++recoveryAttempt)
    {
        const auto result = engine.recoverIncompleteTransactions();
        if (expectation == L"conflict")
        {
            REQUIRE(result.errorIfPresent());
            CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::RecoveryConflict);
            for (const auto &[path, bytes] : stagesBefore)
            {
                REQUIRE(std::filesystem::is_regular_file(path));
                CHECK(readFixture(path) == bytes);
            }
        }
        else
        {
            REQUIRE(result.valueIfPresent());
            const auto generation = std::stoul(requiredEnvironment(L"JPG_SPINNER_TERMINATION_GENERATION"));
            const auto boundary = requiredEnvironment(L"JPG_SPINNER_TERMINATION_BOUNDARY");
            const bool alreadyClosed =
                recoveryAttempt != 0 || (generation == (fixture.replacement ? 6u : 5u) &&
                                         (boundary == L"after-publish" || boundary == L"before-next-effect"));
            if (alreadyClosed)
                CHECK(result.valueIfPresent()->recoveredTransactions.empty());
            else
            {
                REQUIRE(result.valueIfPresent()->recoveredTransactions.size() == 1);
                CHECK(result.valueIfPresent()->recoveredTransactions.front().outcome ==
                      (expectation == L"committed" ? storage::ImageFileTransactionRecoveryOutcome::OutputCommitted
                                                   : storage::ImageFileTransactionRecoveryOutcome::OriginalPreserved));
            }
        }
        CHECK(readFixture(fixture.directory / "camera.jpg") == sourceBefore);
        for (const auto &item : std::filesystem::recursive_directory_iterator(fixture.directory))
        {
            if (!item.is_regular_file())
                continue;
            const auto relative = item.path().lexically_relative(fixture.directory);
            if (*relative.begin() == "JPG Spinner Backups")
                CHECK(readFixture(item.path()) == expectedSource);
            if (*relative.begin() == "JPG Spinner Output" && item.path().filename() == "camera.jpg")
                CHECK(readFixture(item.path()) == expectedOutput);
            if (expectation != L"conflict")
                CHECK_FALSE(item.path().filename().wstring().starts_with(L".jpg-spinner-staged-"));
        }
    }
}
