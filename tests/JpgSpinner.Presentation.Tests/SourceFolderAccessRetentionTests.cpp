#include "pch.h"
#include "Composition/SourceFolderAccessRetention.h"
#include "Composition/SelectedFolderBatchComposition.h"
#include "DeterministicJpegFixtureFactory.h"
#include "PresentationTestApartment.h"
#include "InteractiveUiEnvironment.h"
#include <jpg_spinner/batch/BatchProcessingCoordinator.h>
#include <jpg_spinner/jpeg/LibJpegTurboTransformationEngine.h>
#include <catch2/catch_test_macros.hpp>
#include <winrt/Windows.Storage.AccessCache.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.FileProperties.h>
#include <wil/resource.h>
#include <bit>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <cstring>

namespace
{
/// Independently verifies the native token; neither MSIX identity nor the test
/// process's invocation mode is evidence of effective AppContainer trust.
void requireAppContainerToken()
{
    winrt::handle token;
    REQUIRE(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, token.put()));
    DWORD isAppContainer{};
    DWORD returnedLength{};
    REQUIRE(GetTokenInformation(token.get(), TokenIsAppContainer, &isAppContainer, sizeof(isAppContainer),
                                &returnedLength));
    REQUIRE(isAppContainer == 1);
}
/// Native denial control over real, independently created ungranted folders.
/// Reuse the same oracle before and after picking a child; a missing path or
/// broken Storage consumer is not evidence of confined access.
void requireExistingUngrantedFoldersInaccessible(const winrt::Windows::Data::Json::JsonObject &request)
{
    using namespace winrt::Windows;
    for (const auto key : {L"parentFolderPath", L"siblingFolderPath"})
    {
        const auto path = request.GetNamedString(key);
        REQUIRE_FALSE(path.empty());
        bool accessDenied{};
        try
        {
            const auto folder = Storage::StorageFolder::GetFolderFromPathAsync(path).get();
            static_cast<void>(folder.GetItemsAsync().get());
        }
        catch (const winrt::hresult_error &error)
        {
            CHECK(error.code() == E_ACCESSDENIED); // Missing fixtures do not satisfy this contract.
            accessDenied = error.code() == E_ACCESSDENIED;
        }
        CHECK(accessDenied);
    }
}
} // namespace

TEST_CASE("Source access retention rejects an absent native access list", "[presentation][source-access]")
{
    CHECK_THROWS_AS(jpg_spinner::presentation::SourceFolderAccessRetention{nullptr}, winrt::hresult_invalid_argument);
}

TEST_CASE("Recoverable source access retains one native token until explicit reconciliation",
          "[presentation][packaged-runtime][packaged-access-worker][source-access]")
{
    jpg_spinner::test_support::ensurePresentationTestApartment();
    requireAppContainerToken();
    using namespace winrt::Windows::Storage;
    const auto folder = ApplicationData::Current().LocalFolder();
    const auto accessList = AccessCache::StorageApplicationPermissions::FutureAccessList();
    REQUIRE(accessList.Entries().Size() == 0); // This is a fresh UUID-owned package, never the user's app cache.
    GUID nativeBatchIdentity{};
    REQUIRE(SUCCEEDED(CoCreateGuid(&nativeBatchIdentity)));
    const auto batch = std::bit_cast<winrt::guid>(nativeBatchIdentity);
    winrt::hstring retainedToken;
    {
        jpg_spinner::presentation::SourceFolderAccessRetention retention{accessList};
        CHECK_FALSE(retention.hasRetainedSource());
        REQUIRE_THROWS_AS(retention.retainSourceFolder(nullptr, batch), winrt::hresult_invalid_argument);
        REQUIRE_THROWS_AS(retention.retainSourceFolder(folder, {}), winrt::hresult_invalid_argument);
        CHECK(accessList.Entries().Size() == 0);
        retention.retainSourceFolder(folder, batch);
        REQUIRE(accessList.Entries().Size() == 1);
        retainedToken = accessList.Entries().GetAt(0).Token;
        CHECK(accessList.Entries().GetAt(0).Metadata == winrt::to_hstring(batch));
        CHECK(accessList.GetFolderAsync(retainedToken).get().IsEqual(folder));
        retention.retainSourceFolder(folder, batch);
        CHECK(accessList.Entries().Size() == 1);
        CHECK(retention.hasRetainedSource());
        GUID otherNativeBatchIdentity{};
        REQUIRE(SUCCEEDED(CoCreateGuid(&otherNativeBatchIdentity)));
        REQUIRE_THROWS_AS(retention.retainSourceFolder(folder, std::bit_cast<winrt::guid>(otherNativeBatchIdentity)),
                          winrt::hresult_illegal_method_call);
        CHECK(accessList.Entries().Size() == 1);
        retention.releaseReconciledSource();
        CHECK_FALSE(retention.hasRetainedSource());
        CHECK_FALSE(accessList.ContainsItem(retainedToken));
        retention.releaseReconciledSource(); // Repeated reconciliation must not affect another entry.
        CHECK(accessList.Entries().Size() == 0);
        retention.retainSourceFolder(folder, batch);
        retainedToken = accessList.Entries().GetAt(0).Token;
    }
    // Object/process teardown is not successful transaction reconciliation.
    CHECK(accessList.ContainsItem(retainedToken));
    CHECK(accessList.GetFolderAsync(retainedToken).get().IsEqual(folder));
    accessList.Remove(retainedToken); // Only this fixture's observed token is released.
    CHECK(accessList.Entries().Size() == 0);
}

TEST_CASE("Native AppContainer denies existing ungranted parent and sibling fixtures",
          "[presentation][packaged-runtime][packaged-access-worker][source-access]")
{
    jpg_spinner::test_support::ensurePresentationTestApartment();
    requireAppContainerToken();
    using namespace winrt::Windows;
    const auto local = Storage::ApplicationData::Current().LocalFolder();
    const auto request = Data::Json::JsonObject::Parse(
        Storage::FileIO::ReadTextAsync(local.GetFileAsync(L"SourceAccessRequest.json").get()).get());
    // Positive control: the same process can enumerate its actual app-local
    // capability. An uninitialized/broken Storage API must not qualify denial.
    REQUIRE(local.GetItemsAsync().get().Size() >= 1);
    requireExistingUngrantedFoldersInaccessible(request);
}

TEST_CASE("Native AppContainer analyzes an app-local folder through the real storage adapter",
          "[presentation][packaged-runtime][packaged-access-worker][source-access][storage-analysis]")
{
    // This control isolates the metadata/stream adapter from picker acquisition.
    // App-local access is native authority, not a substitute for a picker grant.
    // The independent Exif-5 fixture must yield one eligible review and no writes.
    jpg_spinner::test_support::ensurePresentationTestApartment();
    requireAppContainerToken();
    using namespace jpg_spinner;
    using namespace winrt::Windows::Storage;
    const auto local = ApplicationData::Current().LocalFolder();
    const auto selected = local.CreateFolderAsync(L"AdapterAnalysisSource").get();
    const auto journal = local.CreateFolderAsync(L"AdapterAnalysisJournal").get();
    const auto source = selected.CreateFileAsync(L"orientation-5.jpg").get();
    const auto bytes = test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
        test_support::JpegChromaSubsampling::ycbcr444, 5);
    const auto first = reinterpret_cast<const std::uint8_t *>(bytes.data());
    FileIO::WriteBytesAsync(source, winrt::array_view<const std::uint8_t>{first, first + bytes.size()}).get();
    REQUIRE(selected.GetItemsAsync().get().Size() == 1);
    auto composition = presentation::composeSelectedFolderBatch(selected, journal);
    const batch_processing::BatchProcessingCoordinator coordinator{
        std::make_shared<jpeg::LibJpegTurboTransformationEngine>()};
    const auto analysis = coordinator.analyze(composition.request);
    REQUIRE(analysis.valueIfPresent() != nullptr);
    REQUIRE(analysis.valueIfPresent()->files().size() == 1);
    CHECK(analysis.valueIfPresent()->discoveryIssues().empty());
    CHECK_FALSE(analysis.valueIfPresent()->files()[0].error.has_value());
    REQUIRE(analysis.valueIfPresent()->files()[0].jpegAnalysis != nullptr);
    CHECK(analysis.valueIfPresent()->files()[0].jpegAnalysis->authoritativeOrientation ==
          domain::ExifOrientation::LeftTop);
    CHECK(analysis.valueIfPresent()->files()[0].sourceRevision.has_value());
    CHECK(journal.GetItemsAsync().get().Size() == 0);
    CHECK(selected.GetItemsAsync().get().Size() == 1);
    winrt::com_array<std::uint8_t> observedBytes;
    winrt::Windows::Security::Cryptography::CryptographicBuffer::CopyToByteArray(FileIO::ReadBufferAsync(source).get(),
                                                                                 observedBytes);
    REQUIRE(observedBytes.size() == bytes.size());
    CHECK(std::memcmp(observedBytes.data(), bytes.data(), bytes.size()) == 0);
}

TEST_CASE("A broker-picked external folder supports identity-bound source discovery in AppContainer",
          "[presentation][packaged-runtime][packaged-access-worker][source-access][native-picker][operator-assisted]")
{
    jpg_spinner::test_support::ensurePresentationTestApartment();
    requireAppContainerToken();
    using namespace jpg_spinner;
    using namespace winrt::Windows::Storage;
    const auto local = ApplicationData::Current().LocalFolder();
    const auto request = winrt::Windows::Data::Json::JsonObject::Parse(
        FileIO::ReadTextAsync(local.GetFileAsync(L"SourceAccessRequest.json").get()).get());
    const auto selected = test_support::pickFolderForAccessQualification();
    REQUIRE(selected != nullptr);
    REQUIRE(selected.Path() == request.GetNamedString(L"selectedFolderPath"));
    const auto items = selected.GetItemsAsync().get();
    REQUIRE(items.Size() == 2); // Proves the actual broker grant before native inspection.
    bool observedEscape{};
    for (const auto &item : items)
        if (item.Name() == L"escape")
        {
            const auto properties =
                item.GetBasicPropertiesAsync().get().RetrievePropertiesAsync({L"System.FileAttributes"}).get();
            const auto attributes = winrt::unbox_value<std::uint32_t>(properties.Lookup(L"System.FileAttributes"));
            REQUIRE((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0);
            observedEscape = true;
        }
    REQUIRE(observedEscape);
    requireExistingUngrantedFoldersInaccessible(request);

    const auto journal = local.CreateFolderAsync(L"PickedSourceDiscoveryJournal").get();
    const auto composition = presentation::composeSelectedFolderBatch(selected, journal);
    const auto discovery = composition.request.sourceCollection->discover(domain::TraversalScope::SelectedFolderOnly);
    REQUIRE(discovery.valueIfPresent() != nullptr);
    REQUIRE(discovery.valueIfPresent()->candidates.size() == 1);
    REQUIRE(discovery.valueIfPresent()->candidates[0].relativePath == L"sentinel.jpg");
    REQUIRE(discovery.valueIfPresent()->issues.size() == 1);
    CHECK(discovery.valueIfPresent()->issues[0].kind == domain::ImageSourceDiscoveryIssueKind::ReparsePointSkipped);
    const auto captured = discovery.valueIfPresent()->candidates[0].source->capture(3);
    REQUIRE(captured.valueIfPresent() != nullptr);
    CHECK(captured.valueIfPresent()->encodedBytes ==
          std::vector<std::byte>{std::byte{'a'}, std::byte{'b'}, std::byte{'c'}});
    CHECK(journal.GetItemsAsync().get().Size() == 0);
}
