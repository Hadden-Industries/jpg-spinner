#include "pch.h"
#include "Composition/SelectedFolderBatchComposition.h"
#include "DeterministicJpegFixtureFactory.h"
#include "PresentationTestApartment.h"
#include "TemporaryDirectory.h"
#include <jpg_spinner/jpeg/LibJpegTurboTransformationEngine.h>
#include <jpg_spinner/batch/BatchProcessingCoordinator.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <winrt/Windows.Foundation.Collections.h>
#include <filesystem>
#include <fstream>

namespace
{
void writeCompositionFixture(const std::filesystem::path &path)
{
    const auto bytes = jpg_spinner::test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
        jpg_spinner::test_support::JpegChromaSubsampling::ycbcr444, 5);
    std::ofstream file{path, std::ios::binary};
    file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    file.close();
    REQUIRE(file.good());
}
jpg_spinner::storage::IdentityBoundStorageFolder identityBoundTestFolder(
    const winrt::Windows::Storage::StorageFolder &folder)
{
    const winrt::handle handle{CreateFileW(folder.Path().c_str(), FILE_READ_ATTRIBUTES,
                                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                           OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr)};
    REQUIRE(handle.get() != INVALID_HANDLE_VALUE);
    FILE_ID_INFO identity{};
    REQUIRE(GetFileInformationByHandleEx(handle.get(), FileIdInfo, &identity, sizeof(identity)));
    return {folder, identity};
}
} // namespace

TEST_CASE("Selected-folder composition retains exact grants and non-destructive defaults",
          "[presentation][composition][selected-folder]")
{
    using namespace jpg_spinner;
    using namespace winrt::Windows::Storage;
    test_support::ensurePresentationTestApartment();
    const test_support::TemporaryDirectory fixture{std::filesystem::temp_directory_path() /
                                                   L"jpg-spinner-selected-folder-composition"};
    const auto owner = StorageFolder::GetFolderFromPathAsync(fixture.directoryPath().wstring()).get();
    const auto selected = owner.CreateFolderAsync(L"selected").get();
    const auto sibling = owner.CreateFolderAsync(L"sibling").get();
    const auto journal = owner.CreateFolderAsync(L"journal").get();
    const auto owned = selected.CreateFolderAsync(L"owned-output").get();
    writeCompositionFixture(std::filesystem::path{selected.Path().c_str()} / L"chosen.jpg");
    writeCompositionFixture(std::filesystem::path{sibling.Path().c_str()} / L"not-granted.jpg");
    writeCompositionFixture(std::filesystem::path{owned.Path().c_str()} / L"excluded.jpg");

    auto composition = presentation::composeSelectedFolderBatch(selected, journal, {identityBoundTestFolder(owned)});
    CHECK(composition.sourceFolderDisplayName == selected.DisplayName());
    CHECK(composition.request.outputDisposition == domain::OutputDisposition::CreateCorrectedCopy);
    CHECK(composition.request.traversalScope == domain::TraversalScope::SelectedFolderOnly);
    CHECK(composition.request.edgeHandlingPolicy == domain::EdgeHandlingPolicy::RequirePerfectCoefficientTransform);
    CHECK(composition.request.outputScanOrganization == domain::OutputScanOrganization::PreserveSource);
    REQUIRE(composition.transactionEngine != nullptr);
    REQUIRE(composition.request.sourceCollection != nullptr);
    const auto expectedBackupPath = std::filesystem::path{selected.Path().c_str()} / L"JPG Spinner Backups" /
                                    composition.request.identifier.displayDirectoryName();
    CHECK(composition.backupRootDisplayPath == expectedBackupPath.native());

    const auto discovery = composition.request.sourceCollection->discover(domain::TraversalScope::SelectedFolderOnly);
    REQUIRE(discovery.valueIfPresent() != nullptr);
    REQUIRE(discovery.valueIfPresent()->candidates.size() == 1);
    CHECK(discovery.valueIfPresent()->candidates[0].relativePath == L"chosen.jpg");
    const auto recursive =
        composition.request.sourceCollection->discover(domain::TraversalScope::SelectedFolderAndDescendants);
    REQUIRE(recursive.valueIfPresent() != nullptr);
    REQUIRE(recursive.valueIfPresent()->candidates.size() == 1);
    CHECK(recursive.valueIfPresent()->candidates[0].relativePath == L"chosen.jpg");
    REQUIRE(recursive.valueIfPresent()->issues.size() == 1);
    CHECK(recursive.valueIfPresent()->issues[0].kind ==
          domain::ImageSourceDiscoveryIssueKind::ApplicationOwnedFolderExcluded);
    CHECK(journal.GetItemsAsync().get().Size() == 0);
    CHECK_FALSE(std::filesystem::exists(std::filesystem::path{selected.Path().c_str()} / L"JPG Spinner Output"));
    CHECK_FALSE(std::filesystem::exists(expectedBackupPath));
    // This headless host is full-trust. These observations establish exact
    // collection consumption, not denial of arbitrary paths in AppContainer.
}

TEST_CASE("Composition passes completed native batch folders to a fresh recursive review",
          "[presentation][composition][owned-folder-identity]")
{
    using namespace jpg_spinner;
    using namespace winrt::Windows::Storage;
    test_support::ensurePresentationTestApartment();
    const test_support::TemporaryDirectory fixture{std::filesystem::temp_directory_path() /
                                                   L"jpg-spinner-selected-folder-composition"};
    const auto owner = StorageFolder::GetFolderFromPathAsync(fixture.directoryPath().wstring()).get();
    const auto selected = owner.CreateFolderAsync(L"selected").get();
    const auto journal = owner.CreateFolderAsync(L"journal").get();
    writeCompositionFixture(std::filesystem::path{selected.Path().c_str()} / L"source.jpg");
    auto first = presentation::composeSelectedFolderBatch(selected, journal);
    const auto disposition = GENERATE(domain::OutputDisposition::CreateCorrectedCopy,
                                      domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup);
    first.request.outputDisposition = disposition;
    CHECK(first.transactionEngine->applicationOwnedFolders().empty()); // Composition alone does not create output.
    const auto directoryName = first.request.identifier.displayDirectoryName();
    const batch_processing::BatchProcessingCoordinator coordinator{
        std::make_shared<jpeg::LibJpegTurboTransformationEngine>()};
    auto analysis = coordinator.analyze(first.request);
    REQUIRE(analysis.valueIfPresent() != nullptr);
    auto summary = coordinator.execute(std::move(*analysis.valueIfPresent()));
    REQUIRE(summary.valueIfPresent() != nullptr);
    REQUIRE_FALSE(summary.valueIfPresent()->transactionIntegrityUncertain);
    REQUIRE(summary.valueIfPresent()->fileResults.size() == 1);
    const auto expectedOutcome = disposition == domain::OutputDisposition::CreateCorrectedCopy
                                     ? batch_processing::BatchFileOutcome::CorrectedCopyCreated
                                     : batch_processing::BatchFileOutcome::OriginalReplacedWithVerifiedBackup;
    REQUIRE(summary.valueIfPresent()->fileResults[0].outcome == expectedOutcome);
    const auto exclusions = first.transactionEngine->applicationOwnedFolders();
    // Replacement stages in its owned output batch before the final move and
    // also retains an owned backup batch. Both are actual created identities.
    const std::size_t expectedFolderCount = disposition == domain::OutputDisposition::CreateCorrectedCopy ? 1 : 2;
    REQUIRE(exclusions.size() == expectedFolderCount);
    for (const auto &exclusion : exclusions)
        CHECK(exclusion.folder.Name() == directoryName);
    // Same-looking user containers are not owned. Exclude only the UUID-bound
    // child that the transaction created, not the whole shared display-name root.
    const auto displayRoot =
        disposition == domain::OutputDisposition::CreateCorrectedCopy ? L"JPG Spinner Output" : L"JPG Spinner Backups";
    writeCompositionFixture(std::filesystem::path{selected.Path().c_str()} / displayRoot / L"user.jpg");
    auto next = presentation::composeSelectedFolderBatch(selected, journal, exclusions);
    CHECK(next.request.identifier.fullIdentity() != first.request.identifier.fullIdentity());
    const auto discovery =
        next.request.sourceCollection->discover(domain::TraversalScope::SelectedFolderAndDescendants);
    REQUIRE(discovery.valueIfPresent() != nullptr);
    CHECK(discovery.valueIfPresent()->candidates.size() == 2);
    REQUIRE(discovery.valueIfPresent()->issues.size() == expectedFolderCount);
    for (const auto &issue : discovery.valueIfPresent()->issues)
        CHECK(issue.kind == domain::ImageSourceDiscoveryIssueKind::ApplicationOwnedFolderExcluded);
}

TEST_CASE("Selected-folder composition rejects absent source or journal grants",
          "[presentation][composition][selected-folder]")
{
    using namespace jpg_spinner;
    using namespace winrt::Windows::Storage;
    test_support::ensurePresentationTestApartment();
    const test_support::TemporaryDirectory fixture{std::filesystem::temp_directory_path() /
                                                   L"jpg-spinner-selected-folder-composition"};
    const auto owner = StorageFolder::GetFolderFromPathAsync(fixture.directoryPath().wstring()).get();
    CHECK_THROWS_AS(presentation::composeSelectedFolderBatch(nullptr, owner), winrt::hresult_invalid_argument);
    CHECK_THROWS_AS(presentation::composeSelectedFolderBatch(owner, nullptr), winrt::hresult_invalid_argument);
    CHECK(owner.GetItemsAsync().get().Size() == 0);
}
