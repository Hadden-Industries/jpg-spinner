#define NOMINMAX
#include <windows.h>
#include <jpg_spinner/storage/WindowsStorageImageFileTransactionRecoveryCatalog.h>
#include "TemporaryDirectory.h"
#include "internal/ImageFileTransactionJournal.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <cstring>
#include <filesystem>

namespace
{
/// All catalog operations run on an explicitly owned MTA; no UI is activated.
struct CatalogTestApartment final
{
    CatalogTestApartment()
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    }
    ~CatalogTestApartment()
    {
        winrt::uninit_apartment();
    }
};

/// Journal metadata fixtures deliberately have no physical source grants. The
/// production journal writer establishes a valid immutable chain; literal state
/// expectations below are independent of catalog filtering and identity decoding.
struct CatalogJournalFixture final
{
    jpg_spinner::test_support::TemporaryDirectory directory{std::filesystem::temp_directory_path() /
                                                            L"jpg-spinner-recovery-catalog"};
    const winrt::Windows::Storage::StorageFolder store =
        winrt::Windows::Storage::StorageFolder::GetFolderFromPathAsync(directory.directoryPath().wstring()).get();
    const winrt::guid identifier{L"{442E70B0-7396-43C2-AED7-157B9EA2F2D2}"};
    const winrt::Windows::Storage::StorageFolder transactionFolder =
        store.CreateFolderAsync(winrt::to_hstring(identifier)).get();

    jpg_spinner::storage::internal::ImageFileTransactionJournalRecord initialized() const
    {
        using namespace jpg_spinner;
        return {identifier,
                1,
                storage::internal::ImageFileTransactionState::TransactionInitialized,
                domain::OutputDisposition::CreateCorrectedCopy,
                L"C:\\unavailable-source",
                std::wstring(48, L'1'),
                L"C:\\unavailable-source\\camera.jpg",
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
} // namespace

TEST_CASE("Recovery catalog reports validated incomplete source identities without acquiring source access",
          "[storage][startup-recovery][catalog]")
{
    using namespace jpg_spinner;
    using namespace winrt::Windows::Storage;
    const CatalogTestApartment apartment;
    const CatalogJournalFixture fixture;
    // These independently specified record facts are not physical source grants.
    // A catalog can report a needed identity even when its source is unavailable;
    // actual artifact proof and mutation remain the existing recovery engine's job.
    const auto record = fixture.initialized();
    storage::internal::ImageFileTransactionJournal journal{fixture.transactionFolder, fixture.identifier};
    journal.publish(record);
    REQUIRE(fixture.transactionFolder.GetFilesAsync().get().Size() == 1);
    const auto generation = fixture.transactionFolder.GetFileAsync(L"generation-000001.json").get();
    const auto before = FileIO::ReadBufferAsync(generation).get();
    const storage::WindowsStorageImageFileTransactionRecoveryCatalog catalog{fixture.store};
    const auto inspected = catalog.inspectRecoveryRequirements();
    REQUIRE(inspected.valueIfPresent() != nullptr);
    REQUIRE(inspected.valueIfPresent()->size() == 1);
    const auto &requirement = inspected.valueIfPresent()->front();
    CHECK(requirement.transactionIdentifier == fixture.identifier);
    FILE_ID_INFO expectedIdentity{};
    // Native journal identity bytes are 24 repetitions of hexadecimal 0x11.
    // The literal is independent of the catalog's decoding implementation.
    std::memset(&expectedIdentity, 0x11, sizeof(expectedIdentity));
    CHECK(requirement.selectedSourceRootIdentity.VolumeSerialNumber == expectedIdentity.VolumeSerialNumber);
    CHECK(std::memcmp(requirement.selectedSourceRootIdentity.FileId.Identifier, expectedIdentity.FileId.Identifier,
                      sizeof(expectedIdentity.FileId.Identifier)) == 0);
    CHECK(fixture.store.GetFoldersAsync().get().Size() == 1);
    CHECK(fixture.store.GetFilesAsync().get().Size() == 0); // No recovery lease was created.
    CHECK(fixture.transactionFolder.GetFilesAsync().get().Size() == 1);
    CHECK(winrt::Windows::Security::Cryptography::CryptographicBuffer::Compare(
        before, FileIO::ReadBufferAsync(generation).get()));
}

TEST_CASE("Recovery catalog distinguishes every incomplete and terminal journal milestone",
          "[storage][startup-recovery][catalog]")
{
    using namespace jpg_spinner;
    using State = storage::internal::ImageFileTransactionState;
    const CatalogTestApartment apartment;
    const CatalogJournalFixture fixture;
    struct Expectation final
    {
        State lastState;
        std::size_t requirementCount;
    };
    const auto expectation =
        GENERATE(Expectation{State::TransactionInitialized, 1}, Expectation{State::StagedOutputWritten, 1},
                 Expectation{State::StagedOutputHashVerified, 1}, Expectation{State::VerifiedBackupCreated, 1},
                 Expectation{State::OutputCommitted, 1}, Expectation{State::OwnedStagingArtifactsCleaned, 0},
                 Expectation{State::TransactionAbandonedBeforeCommit, 0});
    auto record = fixture.initialized();
    if (expectation.lastState == State::VerifiedBackupCreated)
        record.outputDisposition = domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup;
    storage::internal::ImageFileTransactionJournal journal{fixture.transactionFolder, fixture.identifier};
    journal.publish(record);
    if (expectation.lastState == State::TransactionAbandonedBeforeCommit)
    {
        record.generation = 2;
        record.state = State::TransactionAbandonedBeforeCommit;
        journal.publish(record);
    }
    else if (expectation.lastState != State::TransactionInitialized)
    {
        const std::vector<State> sequence =
            expectation.lastState == State::VerifiedBackupCreated
                ? std::vector<State>{State::StagedOutputWritten, State::StagedOutputHashVerified,
                                     State::VerifiedBackupCreated}
                : std::vector<State>{State::StagedOutputWritten, State::StagedOutputHashVerified,
                                     State::OutputCommitted, State::OwnedStagingArtifactsCleaned};
        for (const auto milestone : sequence)
        {
            ++record.generation;
            record.state = milestone;
            if (milestone == State::VerifiedBackupCreated)
            {
                record.backupPath = L"C:\\backup\\camera.jpg";
                record.backupIdentity = std::wstring(48, L'4');
            }
            journal.publish(record);
            if (milestone == expectation.lastState)
                break;
        }
    }
    const auto latest = journal.readLatest();
    REQUIRE(latest.has_value());
    REQUIRE(latest->state == expectation.lastState);
    const storage::WindowsStorageImageFileTransactionRecoveryCatalog catalog{fixture.store};
    const auto inspected = catalog.inspectRecoveryRequirements();
    REQUIRE(inspected.valueIfPresent() != nullptr);
    CHECK(inspected.valueIfPresent()->size() == expectation.requirementCount);
    CHECK(fixture.store.GetFilesAsync().get().Size() == 0);
}

TEST_CASE("Recovery catalog refuses unpublished or malformed journals without modifying their store",
          "[storage][startup-recovery][catalog]")
{
    using namespace jpg_spinner;
    using namespace winrt::Windows::Storage;
    const CatalogTestApartment apartment;
    const CatalogJournalFixture fixture;
    SECTION("No published generation")
    {
    }
    SECTION("Invalid published schema")
    {
        const auto generation = fixture.transactionFolder.CreateFileAsync(L"generation-000001.json").get();
        FileIO::WriteTextAsync(generation, L"[2]").get();
        REQUIRE(FileIO::ReadTextAsync(generation).get() == L"[2]");
    }
    const auto originalFileCount = fixture.transactionFolder.GetFilesAsync().get().Size();
    const storage::WindowsStorageImageFileTransactionRecoveryCatalog catalog{fixture.store};
    const auto inspected = catalog.inspectRecoveryRequirements();
    REQUIRE(inspected.errorIfPresent() != nullptr);
    CHECK(inspected.errorIfPresent()->code == domain::ImageProcessingErrorCode::RecoveryConflict);
    CHECK(fixture.store.GetFoldersAsync().get().Size() == 1);
    CHECK(fixture.store.GetFilesAsync().get().Size() == 0);
    CHECK(fixture.transactionFolder.GetFilesAsync().get().Size() == originalFileCount);
}

TEST_CASE("Empty startup journal stores and cancellation remain read-only observations",
          "[storage][startup-recovery][catalog]")
{
    using namespace jpg_spinner;
    using namespace winrt::Windows::Storage;
    const CatalogTestApartment apartment;
    const test_support::TemporaryDirectory fixture{std::filesystem::temp_directory_path() /
                                                   L"jpg-spinner-recovery-catalog"};
    const auto store = StorageFolder::GetFolderFromPathAsync(fixture.directoryPath().wstring()).get();
    const storage::WindowsStorageImageFileTransactionRecoveryCatalog catalog{store};
    const auto empty = catalog.inspectRecoveryRequirements();
    REQUIRE(empty.valueIfPresent() != nullptr);
    CHECK(empty.valueIfPresent()->empty());
    std::stop_source cancellation;
    cancellation.request_stop();
    const auto cancelled = catalog.inspectRecoveryRequirements(cancellation.get_token());
    REQUIRE(cancelled.errorIfPresent() != nullptr);
    CHECK(cancelled.errorIfPresent()->code == domain::ImageProcessingErrorCode::Cancelled);
    CHECK(store.GetItemsAsync().get().Size() == 0);
}

TEST_CASE("Unexpected transaction-directory names produce a structured startup conflict",
          "[storage][startup-recovery][catalog][invalid-directory]")
{
    using namespace jpg_spinner;
    using namespace winrt::Windows::Storage;
    const CatalogTestApartment apartment;
    const test_support::TemporaryDirectory fixture{std::filesystem::temp_directory_path() /
                                                   L"jpg-spinner-recovery-catalog"};
    const auto store = StorageFolder::GetFolderFromPathAsync(fixture.directoryPath().wstring()).get();
    store.CreateFolderAsync(L"not-a-transaction").get();
    REQUIRE(store.GetFoldersAsync().get().Size() == 1);
    const storage::WindowsStorageImageFileTransactionRecoveryCatalog catalog{store};
    const auto inspected = catalog.inspectRecoveryRequirements();
    REQUIRE(inspected.errorIfPresent() != nullptr);
    CHECK(inspected.errorIfPresent()->code == domain::ImageProcessingErrorCode::RecoveryConflict);
    CHECK(store.GetFoldersAsync().get().Size() == 1);
    CHECK(store.GetFilesAsync().get().Size() == 0);
}
