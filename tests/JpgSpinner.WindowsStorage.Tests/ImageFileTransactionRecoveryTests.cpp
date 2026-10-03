#define NOMINMAX
#include <windows.h>
#include "TemporaryDirectory.h"
#include "internal/ImageFileTransactionJournal.h"
#include "internal/RecoverableImageFileTransaction.h"
#include <jpg_spinner/storage/WindowsStorageImageFileTransactionEngine.h>
#include <fstream>
#include <catch2/catch_test_macros.hpp>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Storage.h>
#include <filesystem>
#include <array>
#include <limits>
#include <catch2/generators/catch_generators.hpp>

using namespace jpg_spinner;

namespace
{
/// Test-owned COM lifetime; all projections are destroyed before uninitialization.
struct RecoveryTestApartment final
{
    RecoveryTestApartment()
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    }
    ~RecoveryTestApartment()
    {
        winrt::uninit_apartment();
    }
};

/// Real persisted records with independently specified schema values. No encoder
/// or transition logic from production computes the expected states.
struct JournalFixture final
{
    test_support::TemporaryDirectory directory{std::filesystem::temp_directory_path() / "jpg-spinner-journal-tests"};
    const winrt::guid identifier{L"{442E70B0-7396-43C2-AED7-157B9EA2F2D2}"};
    const winrt::Windows::Storage::StorageFolder folder =
        winrt::Windows::Storage::StorageFolder::GetFolderFromPathAsync(directory.directoryPath().wstring()).get();

    storage::internal::ImageFileTransactionJournalRecord initialized() const
    {
        // These are record semantics, not a file deletion capability. Real
        // transaction/recovery integration separately proves native file IDs.
        return {identifier,
                1,
                storage::internal::ImageFileTransactionState::TransactionInitialized,
                domain::OutputDisposition::CreateCorrectedCopy,
                L"C:\\source",
                std::wstring(48, L'1'),
                L"C:\\source\\camera.jpg",
                std::wstring(48, L'2'),
                {53, {}, {}},
                L"C:\\output\\stage.jpg",
                std::wstring(48, L'3'),
                47,
                {},
                L"",
                L"",
                L"C:\\output\\camera.jpg"};
    }
};

/// Independent known byte payloads plus actual native identities. Recovery does
/// not decode JPEGs or authorize new transforms; its oracle is retained bytes.
struct RecoveryFixture final
{
    test_support::TemporaryDirectory directory{std::filesystem::temp_directory_path() / "jpg-spinner-recovery-tests"};
    const winrt::Windows::Storage::StorageFolder root =
        winrt::Windows::Storage::StorageFolder::GetFolderFromPathAsync(directory.directoryPath().wstring()).get();
    const winrt::Windows::Storage::StorageFolder store = root.CreateFolderAsync(L"app-journal-store").get();
    const winrt::guid identifier{L"{6AE118AF-9508-4D73-AB0A-0BE062CFAF11}"};
    const winrt::Windows::Storage::StorageFolder transactionFolder =
        store.CreateFolderAsync(winrt::to_hstring(identifier)).get();
    const winrt::Windows::Storage::StorageFolder outputFolder =
        root.CreateFolderAsync(L"JPG Spinner Output").get().CreateFolderAsync(L"20261003T000000Z-442e70b0").get();
    const winrt::Windows::Storage::StorageFile source = writeFile(root, L"camera.jpg", "original encoded bytes");
    const winrt::hstring stageName{L".jpg-spinner-staged-" + std::wstring{winrt::to_hstring(identifier)} + L".jpg"};
    const winrt::Windows::Storage::StorageFile stage = writeFile(outputFolder, stageName, "corrected encoded bytes");

    static winrt::Windows::Storage::StorageFile writeFile(const winrt::Windows::Storage::StorageFolder &folder,
                                                          const winrt::hstring &name, const std::string_view bytes)
    {
        const auto file = folder.CreateFileAsync(name).get();
        std::ofstream stream{std::filesystem::path{file.Path().c_str()}, std::ios::binary};
        stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        stream.close();
        REQUIRE(stream.good());
        return file;
    }
    storage::internal::ImageFileTransactionJournalRecord initialized() const
    {
        const auto sourceRevision = storage::SourceFileRevisionCalculator{}.calculate(source);
        const auto outputRevision = storage::SourceFileRevisionCalculator{}.calculate(stage);
        REQUIRE(sourceRevision.valueIfPresent());
        REQUIRE(outputRevision.valueIfPresent());
        using storage::internal::inspectItem;
        using storage::internal::storageItemIdentityText;
        return {identifier,
                1,
                storage::internal::ImageFileTransactionState::TransactionInitialized,
                domain::OutputDisposition::CreateCorrectedCopy,
                std::wstring{root.Path()},
                storageItemIdentityText(inspectItem(root.Path(), true).identity),
                std::wstring{source.Path()},
                storageItemIdentityText(inspectItem(source.Path(), false).identity),
                *sourceRevision.valueIfPresent(),
                std::wstring{stage.Path()},
                storageItemIdentityText(inspectItem(stage.Path(), false).identity),
                outputRevision.valueIfPresent()->encodedLengthBytes,
                outputRevision.valueIfPresent()->encodedSha256,
                L"",
                L"",
                std::wstring{outputFolder.Path()} + L"\\camera.jpg"};
    }

    storage::internal::ImageFileTransactionJournalRecord publishPrepared(const bool replacement) const
    {
        auto record = initialized();
        if (replacement)
        {
            record.outputDisposition = domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup;
            record.destinationPath = record.sourcePath;
        }
        storage::internal::ImageFileTransactionJournal journal{transactionFolder, identifier};
        journal.publish(record);
        record.generation = 2;
        record.state = storage::internal::ImageFileTransactionState::StagedOutputWritten;
        journal.publish(record);
        record.generation = 3;
        record.state = storage::internal::ImageFileTransactionState::StagedOutputHashVerified;
        journal.publish(record);
        if (replacement)
        {
            const auto backupParent = root.CreateFolderAsync(L"JPG Spinner Backups")
                                          .get()
                                          .CreateFolderAsync(L"20261003T000000Z-442e70b0")
                                          .get();
            const auto backup =
                source
                    .CopyAsync(backupParent, L"camera.jpg", winrt::Windows::Storage::NameCollisionOption::FailIfExists)
                    .get();
            record.generation = 4;
            record.state = storage::internal::ImageFileTransactionState::VerifiedBackupCreated;
            record.backupPath = std::wstring{backup.Path()};
            record.backupIdentity = storage::internal::storageItemIdentityText(
                storage::internal::inspectItem(backup.Path(), false).identity);
            journal.publish(record);
        }
        return record;
    }
};
} // namespace

TEST_CASE("a journal generation is independently reopened after publication without mutating its predecessor",
          "[storage][transaction][journal]")
{
    RecoveryTestApartment apartment;
    JournalFixture fixture;
    storage::internal::ImageFileTransactionJournal journal{fixture.folder, fixture.identifier};
    const auto first = fixture.initialized();
    try
    {
        journal.publish(first);
    }
    catch (const winrt::hresult_error &error)
    {
        FAIL("Native journal publication failed with HRESULT " << error.code().value);
    }
    storage::internal::ImageFileTransactionJournal reopened{fixture.folder, fixture.identifier};
    const auto read = reopened.readLatest();
    REQUIRE(read.has_value());
    CHECK(*read == first);
    const auto firstFile = fixture.folder.GetFileAsync(L"generation-000001.json").get();
    const auto firstBytes = winrt::Windows::Storage::FileIO::ReadTextAsync(firstFile).get();
    REQUIRE_FALSE(firstBytes.empty());
    // Replaying the identical generation must not overwrite it, nor generate an
    // extra transition. Restart callers inspect persisted state instead of retry.
    CHECK_THROWS(journal.publish(first));
    CHECK(winrt::Windows::Storage::FileIO::ReadTextAsync(firstFile).get() == firstBytes);
}

TEST_CASE("a contradictory pending predecessor is rejected before its generation can be published",
          "[storage][transaction][journal][predecessor]")
{
    RecoveryTestApartment apartment;
    JournalFixture fixture;
    struct ContradictoryPredecessor final : storage::internal::FileTransactionOperations
    {
        mutable unsigned publicationCalls{};
        mutable bool changed{};
        winrt::Windows::Storage::Streams::IRandomAccessStream openJournalGeneration(
            const winrt::Windows::Storage::StorageFile &file,
            winrt::Windows::Storage::FileAccessMode access) const override
        {
            if (access == winrt::Windows::Storage::FileAccessMode::Read && !changed)
            {
                auto text = winrt::Windows::Storage::FileIO::ReadTextAsync(file).get();
                auto values = winrt::Windows::Data::Json::JsonArray::Parse(text);
                values.SetAt(19, winrt::Windows::Data::Json::JsonValue::CreateStringValue(std::wstring(64, L'1')));
                winrt::Windows::Storage::FileIO::WriteTextAsync(file, values.Stringify()).get();
                changed = true;
            }
            return FileTransactionOperations::openJournalGeneration(file, access);
        }
        void publishJournalGeneration(const winrt::Windows::Storage::StorageFile &pending,
                                      const winrt::hstring &name) const override
        {
            ++publicationCalls;
            FileTransactionOperations::publishJournalGeneration(pending, name);
        }
    } operations;
    storage::internal::ImageFileTransactionJournal journal{fixture.folder, fixture.identifier};
    CHECK_THROWS(journal.publish(fixture.initialized(), operations));
    REQUIRE(operations.changed);
    CHECK(operations.publicationCalls == 0);
    CHECK_FALSE(fixture.folder.TryGetItemAsync(L"generation-000001.json").get());
}

TEST_CASE("only the disposition-specific monotonic journal graph can be published",
          "[storage][transaction][journal][transitions]")
{
    RecoveryTestApartment apartment;
    JournalFixture fixture;
    const auto disposition = GENERATE(domain::OutputDisposition::CreateCorrectedCopy,
                                      domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup);
    storage::internal::ImageFileTransactionJournal journal{fixture.folder, fixture.identifier};
    auto record = fixture.initialized();
    record.outputDisposition = disposition;
    // Values exceeding JSON double's exact integer range must remain exact.
    record.sourceRevision.encodedLengthBytes = std::numeric_limits<std::uint64_t>::max();
    record.sourceRevision.lastWriteTimeUtc = decltype(record.sourceRevision.lastWriteTimeUtc){
        decltype(record.sourceRevision.lastWriteTimeUtc)::duration{std::numeric_limits<std::int64_t>::max()}};
    using State = storage::internal::ImageFileTransactionState;
    for (const auto state :
         std::array{State::TransactionInitialized, State::StagedOutputWritten, State::StagedOutputHashVerified,
                    State::VerifiedBackupCreated, State::OutputCommitted, State::OwnedStagingArtifactsCleaned})
    {
        if (state == State::VerifiedBackupCreated && disposition == domain::OutputDisposition::CreateCorrectedCopy)
            continue;
        record.state = state;
        if (state == State::VerifiedBackupCreated)
        {
            record.backupPath = L"C:\\backups\\camera.jpg";
            record.backupIdentity = std::wstring(48, L'4');
        }
        journal.publish(record);
        const storage::internal::ImageFileTransactionJournal reopened{fixture.folder, fixture.identifier};
        const auto observed = reopened.readLatest();
        REQUIRE(observed.has_value());
        CHECK(*observed == record);
        CHECK_THROWS(journal.publish(record));
        auto changed = record;
        changed.sourcePath = L"C:\\source\\somebody-else.jpg";
        CHECK_THROWS(journal.publish(changed));
        auto backward = fixture.initialized();
        backward.outputDisposition = disposition;
        CHECK_THROWS(journal.publish(backward));
        ++record.generation;
    }
}

TEST_CASE("journal input rejects object roots duplicate object members and non-string tuple facts",
          "[storage][transaction][journal][schema]")
{
    RecoveryTestApartment apartment;
    JournalFixture fixture;
    storage::internal::ImageFileTransactionJournal journal{fixture.folder, fixture.identifier};
    journal.publish(fixture.initialized());
    const auto file = fixture.folder.GetFileAsync(L"generation-000001.json").get();
    const auto original = winrt::Windows::Storage::FileIO::ReadTextAsync(file).get();
    SECTION("objects cannot erase duplicate facts")
    {
        const auto malformed =
            GENERATE(L"null", L"true", L"1", L"{}", L"{\"schemaVersion\":1,\"schemaVersion\":2}", L"[", L"[]");
        winrt::Windows::Storage::FileIO::WriteTextAsync(file, malformed).get();
        CHECK_THROWS(journal.readLatest());
    }
    SECTION("every tuple position has an exact JSON type")
    {
        const auto index =
            GENERATE(0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u);
        auto values = winrt::Windows::Data::Json::JsonArray::Parse(original);
        values.SetAt(index, winrt::Windows::Data::Json::JsonValue::CreateBooleanValue(true));
        winrt::Windows::Storage::FileIO::WriteTextAsync(file, values.Stringify()).get();
        CHECK_THROWS(journal.readLatest());
    }
    SECTION("unknown version and cardinality are not coerced")
    {
        auto values = winrt::Windows::Data::Json::JsonArray::Parse(original);
        values.SetAt(0, winrt::Windows::Data::Json::JsonValue::CreateNumberValue(2));
        winrt::Windows::Storage::FileIO::WriteTextAsync(file, values.Stringify()).get();
        CHECK_THROWS(journal.readLatest());
        values = winrt::Windows::Data::Json::JsonArray::Parse(original);
        values.Append(winrt::Windows::Data::Json::JsonValue::CreateStringValue(L"unexpected"));
        winrt::Windows::Storage::FileIO::WriteTextAsync(file, values.Stringify()).get();
        CHECK_THROWS(journal.readLatest());
    }
}

TEST_CASE("abandonment closes only a non-committed journal without changing artifact facts",
          "[storage][transaction][journal][review-abandonment]")
{
    RecoveryTestApartment apartment;
    JournalFixture fixture;
    storage::internal::ImageFileTransactionJournal journal{fixture.folder, fixture.identifier};
    auto record = fixture.initialized();
    journal.publish(record);
    record.generation = 2;
    record.state = storage::internal::ImageFileTransactionState::TransactionAbandonedBeforeCommit;
    journal.publish(record);
    REQUIRE(journal.readLatest() == record);
    CHECK_THROWS(journal.publish(record));
    auto resurrection = record;
    resurrection.generation = 3;
    resurrection.state = storage::internal::ImageFileTransactionState::StagedOutputHashVerified;
    CHECK_THROWS(journal.publish(resurrection));
    CHECK(journal.readLatest() == record);
}

TEST_CASE("interrupted abandonment publication preserves the valid predecessor or a complete terminal record",
          "[storage][transaction][journal][fault][abandonment-publication]")
{
    RecoveryTestApartment apartment;
    enum class AbandonmentPublicationFault
    {
        Write,
        Flush,
        Close,
        BeforePublication,
        AfterPublication
    };
    const auto fault = GENERATE(AbandonmentPublicationFault::Write, AbandonmentPublicationFault::Flush,
                                AbandonmentPublicationFault::Close, AbandonmentPublicationFault::BeforePublication,
                                AbandonmentPublicationFault::AfterPublication);
    JournalFixture fixture;
    storage::internal::ImageFileTransactionJournal journal{fixture.folder, fixture.identifier};
    const auto initial = fixture.initialized();
    journal.publish(initial);
    const auto predecessor = fixture.folder.GetFileAsync(L"generation-000001.json").get();
    const auto predecessorBytes = winrt::Windows::Storage::FileIO::ReadTextAsync(predecessor).get();
    struct InterruptedAbandonment final : storage::internal::FileTransactionOperations
    {
        AbandonmentPublicationFault fault;
        mutable bool armed{};
        mutable unsigned failures{};
        explicit InterruptedAbandonment(AbandonmentPublicationFault selected) : fault(selected)
        {
        }
        void failAt(AbandonmentPublicationFault boundary) const
        {
            if (armed && failures == 0 && fault == boundary)
            {
                ++failures;
                throw winrt::hresult_error{E_FAIL};
            }
        }
        winrt::Windows::Storage::StorageFile createPendingJournalGeneration(
            const winrt::Windows::Storage::StorageFolder &folder, const winrt::hstring &name) const override
        {
            // Prior-chain reads are not the selected abandonment publication.
            armed = true;
            return FileTransactionOperations::createPendingJournalGeneration(folder, name);
        }
        std::uint32_t writeJournalBytes(const winrt::Windows::Storage::Streams::IRandomAccessStream &stream,
                                        const winrt::Windows::Storage::Streams::IBuffer &bytes) const override
        {
            failAt(AbandonmentPublicationFault::Write);
            return FileTransactionOperations::writeJournalBytes(stream, bytes);
        }
        bool flushJournalBytes(const winrt::Windows::Storage::Streams::IRandomAccessStream &stream) const override
        {
            failAt(AbandonmentPublicationFault::Flush);
            return FileTransactionOperations::flushJournalBytes(stream);
        }
        void closeJournalGeneration(const winrt::Windows::Storage::Streams::IRandomAccessStream &stream) const override
        {
            failAt(AbandonmentPublicationFault::Close);
            FileTransactionOperations::closeJournalGeneration(stream);
        }
        void publishJournalGeneration(const winrt::Windows::Storage::StorageFile &pending,
                                      const winrt::hstring &name) const override
        {
            failAt(AbandonmentPublicationFault::BeforePublication);
            FileTransactionOperations::publishJournalGeneration(pending, name);
            failAt(AbandonmentPublicationFault::AfterPublication);
        }
    } operations{fault};
    auto terminal = initial;
    terminal.generation = 2;
    terminal.state = storage::internal::ImageFileTransactionState::TransactionAbandonedBeforeCommit;
    CHECK_THROWS_AS(journal.publish(terminal, operations), winrt::hresult_error);
    REQUIRE(operations.failures == 1);
    const storage::internal::ImageFileTransactionJournal restarted{fixture.folder, fixture.identifier};
    const auto persisted = restarted.readLatest();
    REQUIRE(persisted.has_value());
    CHECK(*persisted == (fault == AbandonmentPublicationFault::AfterPublication ? terminal : initial));
    CHECK(winrt::Windows::Storage::FileIO::ReadTextAsync(predecessor).get() == predecessorBytes);
    CHECK(restarted.readLatest() == persisted);
}

TEST_CASE("a damaged newer generation cannot erase the valid immutable prefix",
          "[storage][transaction][journal][corruption]")
{
    RecoveryTestApartment apartment;
    JournalFixture fixture;
    storage::internal::ImageFileTransactionJournal journal{fixture.folder, fixture.identifier};
    auto record = fixture.initialized();
    journal.publish(record);
    const auto firstFile = fixture.folder.GetFileAsync(L"generation-000001.json").get();
    const auto firstBytes = winrt::Windows::Storage::FileIO::ReadTextAsync(firstFile).get();
    record.generation = 2;
    record.state = storage::internal::ImageFileTransactionState::StagedOutputWritten;
    journal.publish(record);
    const auto secondFile = fixture.folder.GetFileAsync(L"generation-000002.json").get();
    auto values =
        winrt::Windows::Data::Json::JsonArray::Parse(winrt::Windows::Storage::FileIO::ReadTextAsync(secondFile).get());
    SECTION("truncated JSON")
    {
        winrt::Windows::Storage::FileIO::WriteTextAsync(secondFile, L"[").get();
    }
    SECTION("future schema")
    {
        values.SetAt(0, winrt::Windows::Data::Json::JsonValue::CreateNumberValue(2));
        winrt::Windows::Storage::FileIO::WriteTextAsync(secondFile, values.Stringify()).get();
    }
    SECTION("malformed GUID text is native invalid data rather than an escaping C++ exception")
    {
        values.SetAt(1, winrt::Windows::Data::Json::JsonValue::CreateStringValue(L"not-a-guid"));
        winrt::Windows::Storage::FileIO::WriteTextAsync(secondFile, values.Stringify()).get();
    }
    SECTION("contradictory predecessor")
    {
        values.SetAt(19, winrt::Windows::Data::Json::JsonValue::CreateStringValue(std::wstring(64, L'1')));
        winrt::Windows::Storage::FileIO::WriteTextAsync(secondFile, values.Stringify()).get();
    }
    SECTION("changed immutable source")
    {
        values.SetAt(7, winrt::Windows::Data::Json::JsonValue::CreateStringValue(L"C:\\source\\foreign.jpg"));
        winrt::Windows::Storage::FileIO::WriteTextAsync(secondFile, values.Stringify()).get();
    }
    const auto recovered = journal.readLatest();
    REQUIRE(recovered.has_value());
    CHECK(*recovered == fixture.initialized());
    CHECK(journal.readLatest() == recovered);
    CHECK_THROWS(journal.publish(record));
    CHECK(winrt::Windows::Storage::FileIO::ReadTextAsync(firstFile).get() == firstBytes);
}

TEST_CASE("malformed journal GUID facts return conflict without escaping the result API",
          "[storage][transaction][recovery][review-guid]")
{
    RecoveryTestApartment apartment;
    RecoveryFixture fixture;
    SECTION("unexpected non-GUID transaction directory")
    {
        fixture.transactionFolder.RenameAsync(L"not-a-guid").get();
    }
    SECTION("malformed GUID in the first published generation")
    {
        storage::internal::ImageFileTransactionJournal{fixture.transactionFolder, fixture.identifier}.publish(
            fixture.initialized());
        const auto file = fixture.transactionFolder.GetFileAsync(L"generation-000001.json").get();
        auto values =
            winrt::Windows::Data::Json::JsonArray::Parse(winrt::Windows::Storage::FileIO::ReadTextAsync(file).get());
        values.SetAt(1, winrt::Windows::Data::Json::JsonValue::CreateStringValue(L"not-a-guid"));
        winrt::Windows::Storage::FileIO::WriteTextAsync(file, values.Stringify()).get();
    }
    const auto result =
        storage::WindowsStorageImageFileTransactionEngine{fixture.root, fixture.store}.recoverIncompleteTransactions();
    REQUIRE(result.errorIfPresent());
    CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::RecoveryConflict);
    CHECK(winrt::Windows::Storage::FileIO::ReadTextAsync(fixture.source).get() == L"original encoded bytes");
    CHECK(winrt::Windows::Storage::FileIO::ReadTextAsync(fixture.stage).get() == L"corrected encoded bytes");
}

TEST_CASE("a closed committed transaction is not reopened after subsequent legitimate edits",
          "[storage][transaction][recovery][review-lifecycle]")
{
    RecoveryTestApartment apartment;
    const auto replacement = GENERATE(false, true);
    RecoveryFixture fixture;
    const auto record = fixture.publishPrepared(replacement);
    if (replacement)
        fixture.stage.MoveAndReplaceAsync(fixture.source).get();
    else
        fixture.stage
            .MoveAsync(fixture.outputFolder, L"camera.jpg", winrt::Windows::Storage::NameCollisionOption::FailIfExists)
            .get();
    const storage::WindowsStorageImageFileTransactionEngine engine{fixture.root, fixture.store};
    const auto first = engine.recoverIncompleteTransactions();
    REQUIRE(first.valueIfPresent());
    REQUIRE(first.valueIfPresent()->recoveredTransactions.size() == 1);
    // Closure describes this transaction's observed completion, not an eternal
    // integrity constraint on user-editable images. Later edits are not replay.
    const auto currentSource = winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(record.sourcePath).get();
    winrt::Windows::Storage::FileIO::WriteTextAsync(currentSource, L"later legitimate edit").get();
    const auto repeated = engine.recoverIncompleteTransactions();
    REQUIRE(repeated.valueIfPresent());
    CHECK(repeated.valueIfPresent()->recoveredTransactions.empty());
    CHECK(winrt::Windows::Storage::FileIO::ReadTextAsync(currentSource).get() == L"later legitimate edit");
}

TEST_CASE("missing copy stage and output cannot prove abandonment after commit admission",
          "[storage][transaction][recovery][copy][abandonment-proof]")
{
    RecoveryTestApartment apartment;
    RecoveryFixture fixture;
    fixture.publishPrepared(false);
    SECTION("stage disappeared without an observed move")
    {
        fixture.stage.DeleteAsync(winrt::Windows::Storage::StorageDeleteOption::PermanentDelete).get();
    }
    SECTION("the committed copy was subsequently renamed before recovery")
    {
        fixture.stage
            .MoveAsync(fixture.outputFolder, L"camera.jpg", winrt::Windows::Storage::NameCollisionOption::FailIfExists)
            .get();
        fixture.stage.RenameAsync(L"user-renamed-copy.jpg").get();
    }
    const auto predecessor = fixture.transactionFolder.GetFileAsync(L"generation-000003.json").get();
    const auto predecessorBytes = winrt::Windows::Storage::FileIO::ReadTextAsync(predecessor).get();
    const storage::WindowsStorageImageFileTransactionEngine engine{fixture.root, fixture.store};
    for (unsigned attempt = 0; attempt != 2; ++attempt)
    {
        const auto result = engine.recoverIncompleteTransactions();
        REQUIRE(result.errorIfPresent());
        CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::RecoveryConflict);
        CHECK_FALSE(fixture.transactionFolder.TryGetItemAsync(L"generation-000004.json").get());
        CHECK(winrt::Windows::Storage::FileIO::ReadTextAsync(predecessor).get() == predecessorBytes);
        CHECK(winrt::Windows::Storage::FileIO::ReadTextAsync(fixture.source).get() == L"original encoded bytes");
        if (const auto renamed = fixture.outputFolder.TryGetItemAsync(L"user-renamed-copy.jpg").get())
            CHECK(winrt::Windows::Storage::FileIO::ReadTextAsync(renamed.as<winrt::Windows::Storage::StorageFile>())
                      .get() == L"corrected encoded bytes");
    }
}

TEST_CASE("a conflicting transaction does not prevent independent owned-stage recovery",
          "[storage][transaction][recovery][review-conflict-isolation]")
{
    RecoveryTestApartment apartment;
    RecoveryFixture fixture;
    storage::internal::ImageFileTransactionJournal{fixture.transactionFolder, fixture.identifier}.publish(
        fixture.initialized());
    // An earlier-sorted UUID has no trustworthy generation. It must remain a
    // conflict, but not prevent cleanup of the independently explained stage.
    const auto unresolved = fixture.store.CreateFolderAsync(L"{00000001-0000-0000-0000-000000000001}").get();
    const auto recovery =
        storage::WindowsStorageImageFileTransactionEngine{fixture.root, fixture.store}.recoverIncompleteTransactions();
    REQUIRE(recovery.errorIfPresent());
    CHECK(recovery.errorIfPresent()->code == domain::ImageProcessingErrorCode::RecoveryConflict);
    CHECK_FALSE(fixture.outputFolder.TryGetItemAsync(fixture.stageName).get());
    CHECK(winrt::Windows::Storage::FileIO::ReadTextAsync(fixture.source).get() == L"original encoded bytes");
    CHECK(fixture.store.TryGetItemAsync(unresolved.Name()).get() != nullptr);
}

TEST_CASE("shared application storage does not grant recovery authority over another selected root",
          "[storage][transaction][recovery][review-root-scope]")
{
    RecoveryTestApartment apartment;
    RecoveryFixture selected;
    RecoveryFixture other;
    storage::internal::ImageFileTransactionJournal{selected.transactionFolder, selected.identifier}.publish(
        selected.initialized());
    auto foreign = other.initialized();
    foreign.transactionIdentifier = winrt::guid{L"{00000002-0000-0000-0000-000000000002}"};
    const auto foreignFolder = selected.store.CreateFolderAsync(winrt::to_hstring(foreign.transactionIdentifier)).get();
    storage::internal::ImageFileTransactionJournal{foreignFolder, foreign.transactionIdentifier}.publish(foreign);
    const auto result = storage::WindowsStorageImageFileTransactionEngine{selected.root, selected.store}
                            .recoverIncompleteTransactions();
    REQUIRE(result.valueIfPresent());
    REQUIRE(result.valueIfPresent()->recoveredTransactions.size() == 1);
    CHECK_FALSE(selected.outputFolder.TryGetItemAsync(selected.stageName).get());
    CHECK(winrt::Windows::Storage::FileIO::ReadTextAsync(other.source).get() == L"original encoded bytes");
    CHECK(winrt::Windows::Storage::FileIO::ReadTextAsync(other.stage).get() == L"corrected encoded bytes");
}

TEST_CASE("native journal failures preserve pending evidence and expose only completed publication",
          "[storage][transaction][journal][fault]")
{
    RecoveryTestApartment apartment;
    enum class JournalFault
    {
        Create,
        OpenWrite,
        Write,
        Flush,
        Close,
        PendingRead,
        BeforePublication,
        AfterPublication,
        PublishedRead
    };
    const auto fault = GENERATE(JournalFault::Create, JournalFault::OpenWrite, JournalFault::Write, JournalFault::Flush,
                                JournalFault::Close, JournalFault::PendingRead, JournalFault::BeforePublication,
                                JournalFault::AfterPublication, JournalFault::PublishedRead);
    JournalFixture fixture;
    struct InterruptedJournal final : storage::internal::FileTransactionOperations
    {
        JournalFault fault;
        mutable unsigned faultsObserved{};
        explicit InterruptedJournal(JournalFault selected) : fault(selected)
        {
        }
        void interruptAt(JournalFault operation) const
        {
            if (fault == operation)
            {
                ++faultsObserved;
                throw winrt::hresult_error{E_FAIL};
            }
        }
        winrt::Windows::Storage::StorageFile createPendingJournalGeneration(
            const winrt::Windows::Storage::StorageFolder &folder, const winrt::hstring &name) const override
        {
            interruptAt(JournalFault::Create);
            return FileTransactionOperations::createPendingJournalGeneration(folder, name);
        }
        winrt::Windows::Storage::Streams::IRandomAccessStream openJournalGeneration(
            const winrt::Windows::Storage::StorageFile &file,
            winrt::Windows::Storage::FileAccessMode access) const override
        {
            if (access == winrt::Windows::Storage::FileAccessMode::ReadWrite)
                interruptAt(JournalFault::OpenWrite);
            else if (std::wstring_view{file.Name()}.starts_with(L".pending-"))
                interruptAt(JournalFault::PendingRead);
            else
                interruptAt(JournalFault::PublishedRead);
            return FileTransactionOperations::openJournalGeneration(file, access);
        }
        std::uint32_t writeJournalBytes(const winrt::Windows::Storage::Streams::IRandomAccessStream &stream,
                                        const winrt::Windows::Storage::Streams::IBuffer &bytes) const override
        {
            const auto length = FileTransactionOperations::writeJournalBytes(stream, bytes);
            interruptAt(JournalFault::Write);
            return length;
        }
        bool flushJournalBytes(const winrt::Windows::Storage::Streams::IRandomAccessStream &stream) const override
        {
            const auto flushed = FileTransactionOperations::flushJournalBytes(stream);
            interruptAt(JournalFault::Flush);
            return flushed;
        }
        void closeJournalGeneration(const winrt::Windows::Storage::Streams::IRandomAccessStream &stream) const override
        {
            FileTransactionOperations::closeJournalGeneration(stream);
            interruptAt(JournalFault::Close);
        }
        void publishJournalGeneration(const winrt::Windows::Storage::StorageFile &pending,
                                      const winrt::hstring &name) const override
        {
            interruptAt(JournalFault::BeforePublication);
            FileTransactionOperations::publishJournalGeneration(pending, name);
            interruptAt(JournalFault::AfterPublication);
        }
    } operations{fault};
    storage::internal::ImageFileTransactionJournal journal{fixture.folder, fixture.identifier};
    CHECK_THROWS(journal.publish(fixture.initialized(), operations));
    REQUIRE(operations.faultsObserved == 1);
    // Restart uses actual files and default native operations, not the injected
    // adapter's reported phase or an in-memory state left by the failed call.
    const storage::internal::ImageFileTransactionJournal restarted{fixture.folder, fixture.identifier};
    const auto latest = restarted.readLatest();
    const bool publicationCompleted = fault == JournalFault::AfterPublication || fault == JournalFault::PublishedRead;
    CHECK(latest.has_value() == publicationCompleted);
    if (latest)
        CHECK(*latest == fixture.initialized());
    CHECK(restarted.readLatest() == latest);
    if (fault != JournalFault::Create)
        CHECK(fixture.folder.GetFilesAsync().get().Size() > 0);
}

TEST_CASE("restart recovery preserves the original and removes only the exact owned stage",
          "[storage][transaction][recovery]")
{
    RecoveryTestApartment apartment;
    RecoveryFixture fixture;
    const auto record = fixture.initialized();
    storage::internal::ImageFileTransactionJournal{fixture.transactionFolder, fixture.identifier}.publish(record);
    const storage::WindowsStorageImageFileTransactionEngine engine{fixture.root, fixture.store};
    const storage::ImageFileTransactionEngine &contract = engine;
    const auto result = contract.recoverIncompleteTransactions();
    if (result.errorIfPresent())
    {
        INFO("Recovery error code " << static_cast<unsigned>(result.errorIfPresent()->code));
        const auto native = std::get_if<domain::WindowsHResult>(&result.errorIfPresent()->nativeErrorProjection);
        INFO("Recovery native HRESULT " << (native ? native->signedValue : 0));
        REQUIRE(result.valueIfPresent());
    }
    REQUIRE(result.valueIfPresent());
    REQUIRE(result.valueIfPresent()->recoveredTransactions.size() == 1);
    CHECK(result.valueIfPresent()->recoveredTransactions.front().transactionIdentifier == fixture.identifier);
    CHECK(result.valueIfPresent()->recoveredTransactions.front().outcome ==
          storage::ImageFileTransactionRecoveryOutcome::OriginalPreserved);
    CHECK(winrt::Windows::Storage::FileIO::ReadTextAsync(fixture.source).get() == L"original encoded bytes");
    CHECK_FALSE(fixture.outputFolder.TryGetItemAsync(fixture.stageName).get());
    const auto repeated = contract.recoverIncompleteTransactions();
    REQUIRE(repeated.valueIfPresent());
    CHECK(repeated.valueIfPresent()->recoveredTransactions.empty());
}

TEST_CASE("recovery refuses changed source bytes or a substituted stage without deleting either",
          "[storage][transaction][recovery][conflict]")
{
    RecoveryTestApartment apartment;
    RecoveryFixture fixture;
    fixture.publishPrepared(false);
    SECTION("unexplained source bytes")
    {
        winrt::Windows::Storage::FileIO::WriteTextAsync(fixture.source, L"external source mutation").get();
    }
    SECTION("same stage name and bytes do not transfer ownership")
    {
        const auto originalIdentity = storage::internal::inspectItem(fixture.stage.Path(), false).identity;
        fixture.stage.RenameAsync(L"retained-real-stage.jpg").get();
        const auto substituted =
            RecoveryFixture::writeFile(fixture.outputFolder, fixture.stageName, "corrected encoded bytes");
        REQUIRE_FALSE(storage::internal::isSameIdentity(
            originalIdentity, storage::internal::inspectItem(substituted.Path(), false).identity));
    }
    SECTION("unknown staged bytes after the written milestone")
    {
        winrt::Windows::Storage::FileIO::WriteTextAsync(fixture.stage, L"unexplained stage mutation").get();
    }
    const auto sourceBefore = winrt::Windows::Storage::FileIO::ReadTextAsync(fixture.source).get();
    const auto retained = fixture.outputFolder.GetFileAsync(fixture.stageName).get();
    const auto stageBefore = winrt::Windows::Storage::FileIO::ReadTextAsync(retained).get();
    const auto result =
        storage::WindowsStorageImageFileTransactionEngine{fixture.root, fixture.store}.recoverIncompleteTransactions();
    REQUIRE(result.errorIfPresent());
    CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::RecoveryConflict);
    CHECK(winrt::Windows::Storage::FileIO::ReadTextAsync(fixture.source).get() == sourceBefore);
    CHECK(winrt::Windows::Storage::FileIO::ReadTextAsync(retained).get() == stageBefore);
}

TEST_CASE("restart recovery infers completed copy or replacement from bytes and identities rather than return status",
          "[storage][transaction][recovery][committed]")
{
    RecoveryTestApartment apartment;
    const auto replacement = GENERATE(false, true);
    RecoveryFixture fixture;
    const auto record = fixture.publishPrepared(replacement);
    if (replacement)
        fixture.stage.MoveAndReplaceAsync(fixture.source).get();
    else
        fixture.stage
            .MoveAsync(fixture.outputFolder, L"camera.jpg", winrt::Windows::Storage::NameCollisionOption::FailIfExists)
            .get();
    // No OutputCommitted journal generation was written. This is the native
    // move-completed/process-stopped window, observed without replaying the move.
    REQUIRE_FALSE(fixture.outputFolder.TryGetItemAsync(fixture.stageName).get());
    const auto committed = winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(record.destinationPath).get();
    REQUIRE(winrt::Windows::Storage::FileIO::ReadTextAsync(committed).get() == L"corrected encoded bytes");
    const storage::WindowsStorageImageFileTransactionEngine engine{fixture.root, fixture.store};
    const auto recovered = engine.recoverIncompleteTransactions();
    REQUIRE(recovered.valueIfPresent());
    REQUIRE(recovered.valueIfPresent()->recoveredTransactions.size() == 1);
    CHECK(recovered.valueIfPresent()->recoveredTransactions.front().outcome ==
          storage::ImageFileTransactionRecoveryOutcome::OutputCommitted);
    if (replacement)
    {
        const auto backup = winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(record.backupPath).get();
        CHECK(winrt::Windows::Storage::FileIO::ReadTextAsync(backup).get() == L"original encoded bytes");
    }
    else
        CHECK(winrt::Windows::Storage::FileIO::ReadTextAsync(fixture.source).get() == L"original encoded bytes");
    const auto repeated = engine.recoverIncompleteTransactions();
    REQUIRE(repeated.valueIfPresent());
    CHECK(repeated.valueIfPresent()->recoveredTransactions.empty());
    CHECK(winrt::Windows::Storage::FileIO::ReadTextAsync(committed).get() == L"corrected encoded bytes");
}

TEST_CASE("a stopped recovery hash retains the proof artifacts and reports cancellation",
          "[storage][transaction][recovery][recovery-cancellation]")
{
    RecoveryTestApartment apartment;
    const auto stopAtBackup = GENERATE(false, true);
    RecoveryFixture fixture;
    fixture.publishPrepared(stopAtBackup);
    std::stop_source cancellation;
    struct StoppedRecoveryRead final : storage::internal::FileTransactionOperations
    {
        bool stopAtBackup;
        std::stop_source &cancellation;
        mutable unsigned stopCalls{};
        StoppedRecoveryRead(bool backup, std::stop_source &source) : stopAtBackup(backup), cancellation(source)
        {
        }
        domain::ImageProcessingResult<domain::SourceFileRevision> captureBackupRevision(
            const winrt::Windows::Storage::StorageFile &file, std::stop_token token) const override
        {
            if (stopAtBackup)
            {
                ++stopCalls;
                cancellation.request_stop();
            }
            return FileTransactionOperations::captureBackupRevision(file, token);
        }
        domain::ImageProcessingResult<domain::SourceFileRevision> captureSourceRevision(
            const winrt::Windows::Storage::StorageFile &file, std::stop_token token) const override
        {
            if (!stopAtBackup)
            {
                ++stopCalls;
                cancellation.request_stop();
            }
            return FileTransactionOperations::captureSourceRevision(file, token);
        }
    } operations{stopAtBackup, cancellation};
    storage::internal::ImageFileTransactionBatch batch{fixture.root, fixture.store};
    const auto result = storage::internal::RecoverableImageFileTransaction::recoverIncompleteTransactions(
        batch, cancellation.get_token(), operations);
    REQUIRE(result.errorIfPresent());
    CHECK(result.errorIfPresent()->code == domain::ImageProcessingErrorCode::Cancelled);
    CHECK(result.errorIfPresent()->stage == domain::ImageProcessingStage::TransactionRecovery);
    CHECK(operations.stopCalls == 1);
    CHECK(winrt::Windows::Storage::FileIO::ReadTextAsync(fixture.source).get() == L"original encoded bytes");
    CHECK(winrt::Windows::Storage::FileIO::ReadTextAsync(fixture.stage).get() == L"corrected encoded bytes");
}
