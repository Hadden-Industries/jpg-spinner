#include "pch.h"
#include "Composition/StartupRecoveryComposition.h"
#include "PresentationTestApartment.h"
#include "../../src/JpgSpinner.WindowsStorage/src/internal/ImageFileTransactionJournal.h"
#include <jpg_spinner/storage/SourceFileRevisionCalculator.h>
#include <catch2/catch_test_macros.hpp>
#include <winrt/Windows.Security.Cryptography.h>
#include <wil/resource.h>
#include <bit>

TEST_CASE("Startup composition rejects an absent native access cache", "[presentation][startup-recovery]")
{
    REQUIRE_THROWS_AS(jpg_spinner::presentation::recoverStartupTransactions(nullptr, nullptr),
                      winrt::hresult_invalid_argument);
}

TEST_CASE("Startup restores an actual native cache grant and reconciles its journal without evicting the token",
          "[presentation][startup-recovery][packaged-runtime][packaged-access-worker][source-access]")
{
    using namespace jpg_spinner;
    using namespace winrt::Windows::Storage;
    test_support::ensurePresentationTestApartment();
    winrt::handle token;
    REQUIRE(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, token.put()));
    DWORD isAppContainer{}, length{};
    REQUIRE(GetTokenInformation(token.get(), TokenIsAppContainer, &isAppContainer, sizeof(isAppContainer), &length));
    REQUIRE(isAppContainer == 1);
    GUID nativeIdentity{};
    REQUIRE(SUCCEEDED(CoCreateGuid(&nativeIdentity)));
    const auto transactionIdentity = std::bit_cast<winrt::guid>(nativeIdentity);
    const auto root =
        ApplicationData::Current().LocalFolder().CreateFolderAsync(winrt::to_hstring(transactionIdentity)).get();
    const auto cleanup = wil::scope_exit([&] { root.DeleteAsync().get(); }); // Only this UUID-owned fixture.
    const auto store = root.CreateFolderAsync(L"journals").get();
    const auto journalFolder = store.CreateFolderAsync(winrt::to_hstring(transactionIdentity)).get();
    const auto source = root.CreateFileAsync(L"original.jpg").get();
    const auto output =
        root.CreateFolderAsync(L"JPG Spinner Output").get().CreateFolderAsync(L"20261014T000000Z-442e70b0").get();
    const winrt::hstring stageName{L".jpg-spinner-staged-" + std::wstring{winrt::to_hstring(transactionIdentity)} +
                                   L".jpg"};
    const auto stage = output.CreateFileAsync(stageName).get();
    FileIO::WriteTextAsync(source, L"independent original payload").get();
    FileIO::WriteTextAsync(stage, L"independent staged payload").get();
    const auto sourceRevision = storage::SourceFileRevisionCalculator{}.calculate(source);
    const auto stageRevision = storage::SourceFileRevisionCalculator{}.calculate(stage);
    REQUIRE(sourceRevision.valueIfPresent());
    REQUIRE(stageRevision.valueIfPresent());
    using storage::internal::inspectItem;
    using storage::internal::storageItemIdentityText;
    storage::internal::ImageFileTransactionJournal journal{journalFolder, transactionIdentity};
    journal.publish({transactionIdentity, 1, storage::internal::ImageFileTransactionState::TransactionInitialized,
                     domain::OutputDisposition::CreateCorrectedCopy, std::wstring{root.Path()},
                     storageItemIdentityText(inspectItem(root, true).identity), std::wstring{source.Path()},
                     storageItemIdentityText(inspectItem(source, false).identity), *sourceRevision.valueIfPresent(),
                     std::wstring{stage.Path()}, storageItemIdentityText(inspectItem(stage, false).identity),
                     stageRevision.valueIfPresent()->encodedLengthBytes, stageRevision.valueIfPresent()->encodedSha256,
                     L"", L"", std::wstring{output.Path()} + L"\\original.jpg"});
    const auto cache = AccessCache::StorageApplicationPermissions::FutureAccessList();
    REQUIRE(cache.Entries().Size() == 0); // Fresh test-package cache, never the user's app cache.
    const auto missingGrant = presentation::recoverStartupTransactions(store, cache);
    REQUIRE(missingGrant.errorIfPresent());
    CHECK(missingGrant.errorIfPresent()->code == domain::ImageProcessingErrorCode::RecoveryConflict);
    CHECK(FileIO::ReadTextAsync(stage).get() == L"independent staged payload");
    // Metadata intentionally isn't the transaction ID. Only the restored grant
    // and its native identity authorize recovery; cache metadata is batch prose.
    const auto sourceToken = cache.Add(root, L"unrelated batch metadata, not a source path");
    const auto release = wil::scope_exit([&] {
        if (cache.ContainsItem(sourceToken))
            cache.Remove(sourceToken);
    });
    const auto result = presentation::recoverStartupTransactions(store, cache);
    REQUIRE(result.valueIfPresent());
    REQUIRE(result.valueIfPresent()->recoveredTransactions.size() == 1);
    CHECK(result.valueIfPresent()->recoveredTransactions.front().transactionIdentifier == transactionIdentity);
    CHECK(result.valueIfPresent()->recoveredTransactions.front().outcome ==
          storage::ImageFileTransactionRecoveryOutcome::OriginalPreserved);
    CHECK(FileIO::ReadTextAsync(source).get() == L"independent original payload");
    CHECK(output.TryGetItemAsync(stageName).get() == nullptr);
    CHECK(output.TryGetItemAsync(L"original.jpg").get() == nullptr);
    CHECK(cache.ContainsItem(sourceToken)); // Inspection is not cache-release authority.
    CHECK(journal.readLatest()->state ==
          storage::internal::ImageFileTransactionState::TransactionAbandonedBeforeCommit);
}
