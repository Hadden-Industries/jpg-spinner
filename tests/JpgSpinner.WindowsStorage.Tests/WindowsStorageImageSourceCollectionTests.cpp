#define NOMINMAX
#include <windows.h>
#include <sddl.h>
#include <fileapifromapp.h>
#include <jpg_spinner/storage/WindowsStorageImageSourceCollection.h>
#include <jpg_spinner/storage/WindowsStorageImageFileTransactionEngine.h>
#include <jpg_spinner/storage/SourceFileRevisionCalculator.h>
#include "TemporaryDirectory.h"
#include "DeterministicJpegFixtureFactory.h"
#include <jpg_spinner/jpeg/JpegImageAnalyzer.h>
#include <jpg_spinner/jpeg/LibJpegTurboTransformationEngine.h>
#include <catch2/catch_test_macros.hpp>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Storage.FileProperties.h>
#include <filesystem>
#include <fstream>
#include <set>
#include <string_view>

namespace
{
/// Every fixture owns the MTA before storage projections are created.
struct DiscoveryTestApartment final
{
    DiscoveryTestApartment()
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    }
    ~DiscoveryTestApartment()
    {
        winrt::uninit_apartment();
    }
};

void writeCandidate(const std::filesystem::path &path)
{
    std::ofstream file{path, std::ios::binary};
    file << "not a JPEG; discovery must not read or silently omit this candidate";
    file.close();
    REQUIRE(file.good());
    REQUIRE(std::filesystem::file_size(path) != 0);
}

std::set<std::wstring> relativeNames(const jpg_spinner::domain::ImageSourceDiscovery &discovery);

/// A real, restored ACL on a UUID-owned fixture; never a fabricated access error.
class DiscoveryReadDenial final
{
  public:
    explicit DiscoveryReadDenial(std::filesystem::path path) : path_(std::move(path))
    {
        DWORD requiredBytes = 0;
        GetFileSecurityW(path_.c_str(), DACL_SECURITY_INFORMATION, nullptr, 0, &requiredBytes);
        REQUIRE(GetLastError() == ERROR_INSUFFICIENT_BUFFER);
        original_.resize(requiredBytes);
        REQUIRE(GetFileSecurityW(path_.c_str(), DACL_SECURITY_INFORMATION, original_.data(), requiredBytes,
                                 &requiredBytes));
        PSECURITY_DESCRIPTOR denied = nullptr;
        REQUIRE(ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:(D;;FR;;;WD)(A;;FA;;;OW)", SDDL_REVISION_1,
                                                                     &denied, nullptr));
        const auto changed = SetFileSecurityW(path_.c_str(), DACL_SECURITY_INFORMATION, denied);
        LocalFree(denied);
        REQUIRE(changed);
    }
    ~DiscoveryReadDenial()
    {
        SetFileSecurityW(path_.c_str(), DACL_SECURITY_INFORMATION, original_.data());
    }
    DiscoveryReadDenial(const DiscoveryReadDenial &) = delete;
    DiscoveryReadDenial &operator=(const DiscoveryReadDenial &) = delete;

  private:
    const std::filesystem::path path_;
    std::vector<std::byte> original_;
};

TEST_CASE("Inaccessible descendants are reported without losing accessible candidates",
          "[storage][batch][discovery][access]")
{
    using namespace jpg_spinner;
    using namespace winrt::Windows::Storage;
    const DiscoveryTestApartment apartment;
    const test_support::TemporaryDirectory fixture{std::filesystem::current_path() / "artifacts/tests"};
    const auto selected = StorageFolder::GetFolderFromPathAsync(fixture.directoryPath().wstring()).get();
    const auto journal = selected.CreateFolderAsync(L"journal").get();
    selected.CreateFolderAsync(L"denied").get();
    writeCandidate(fixture.directoryPath() / L"accessible.jpg");
    writeCandidate(fixture.directoryPath() / L"denied" / L"inaccessible.jpg");
    const DiscoveryReadDenial denial{fixture.directoryPath() / L"denied"};
    // An already-created StorageFolder projection can retain earlier enumeration
    // access. Establish the actual FromApp metadata-open precondition instead;
    // discovery obtains fresh native proofs, never inherited read handles.
    const winrt::handle deniedMetadata{CreateFileFromAppW(
        (fixture.directoryPath() / L"denied").c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr)};
    const auto deniedError = GetLastError();
    REQUIRE(deniedMetadata.get() == INVALID_HANDLE_VALUE);
    REQUIRE(deniedError == ERROR_ACCESS_DENIED);
    const auto transactions = std::make_shared<storage::WindowsStorageImageFileTransactionEngine>(selected, journal);
    const storage::WindowsStorageImageSourceCollection collection{selected, transactions};
    const auto discovered = collection.discover(domain::TraversalScope::SelectedFolderAndDescendants);
    REQUIRE(discovered.valueIfPresent() != nullptr);
    REQUIRE(relativeNames(*discovered.valueIfPresent()) == std::set<std::wstring>{L"accessible.jpg"});
    REQUIRE(discovered.valueIfPresent()->issues.size() == 1);
    const auto &issue = discovered.valueIfPresent()->issues.front();
    REQUIRE(issue.relativePath == L"denied");
    REQUIRE(issue.kind == domain::ImageSourceDiscoveryIssueKind::SourceAccessFailed);
    REQUIRE(issue.error->code == domain::ImageProcessingErrorCode::SourceAccessDenied);
    REQUIRE_FALSE(std::filesystem::exists(fixture.directoryPath() / L"JPG Spinner Output"));
}

TEST_CASE("A discovered source commits through the existing validated transaction boundary", "[storage][batch][commit]")
{
    using namespace jpg_spinner;
    using namespace winrt::Windows::Storage;
    const DiscoveryTestApartment apartment;
    // This successful-copy fixture needs a provider-supported destination, not
    // a path length determined by the build/clone location. Long Unicode
    // discovery has its own fixture; native destination refusals stay failures.
    const test_support::TemporaryDirectory fixture{std::filesystem::temp_directory_path() /
                                                   L"jpg-spinner-source-commit-tests"};
    const auto originalBytes = test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
        test_support::JpegChromaSubsampling::ycbcr444, 3);
    const auto originalPath = fixture.directoryPath() / L"camera.JpEg";
    {
        std::ofstream file{originalPath, std::ios::binary};
        file.write(reinterpret_cast<const char *>(originalBytes.data()),
                   static_cast<std::streamsize>(originalBytes.size()));
        file.close();
        REQUIRE(file.good());
    }
    const auto selected = StorageFolder::GetFolderFromPathAsync(fixture.directoryPath().wstring()).get();
    const auto journal = selected.CreateFolderAsync(L"journal").get();
    const auto transactions = std::make_shared<storage::WindowsStorageImageFileTransactionEngine>(selected, journal);
    const storage::WindowsStorageImageSourceCollection collection{selected, transactions};
    const auto discovery = collection.discover(domain::TraversalScope::SelectedFolderOnly);
    REQUIRE(discovery.valueIfPresent() != nullptr);
    REQUIRE(discovery.valueIfPresent()->candidates.size() == 1);
    const auto &source = discovery.valueIfPresent()->candidates.front().source;
    const auto captured = source->capture(originalBytes.size());
    REQUIRE(captured.valueIfPresent() != nullptr);
    REQUIRE(captured.valueIfPresent()->encodedBytes == originalBytes);
    const auto analysis = jpeg::JpegImageAnalyzer::analyze(captured.valueIfPresent()->encodedBytes,
                                                           domain::EdgeHandlingPolicy::TrimPartialMinimumCodedUnits);
    REQUIRE(analysis.valueIfPresent() != nullptr);
    const auto output = jpeg::LibJpegTurboTransformationEngine{}.createValidatedOutput(
        captured.valueIfPresent()->encodedBytes, *analysis.valueIfPresent());
    REQUIRE(output.valueIfPresent() != nullptr);
    const auto committed = source->commit(captured.valueIfPresent()->revision, *output.valueIfPresent(),
                                          domain::OutputDisposition::CreateCorrectedCopy);
    const auto commitError = committed.errorIfPresent();
    const auto commitNative =
        commitError ? std::get_if<domain::WindowsHResult>(&commitError->nativeErrorProjection) : nullptr;
    INFO("Commit error code: " << (commitError ? static_cast<int>(commitError->code) : -1));
    INFO("Commit error stage: " << (commitError ? static_cast<int>(commitError->stage) : -1));
    INFO("Commit native HRESULT: " << (commitNative ? commitNative->signedValue : 0));
    REQUIRE(committed.valueIfPresent() != nullptr);
    REQUIRE(committed.valueIfPresent()->outputDisplayName == L"camera.JpEg");
    REQUIRE(std::filesystem::file_size(originalPath) == originalBytes.size());
    std::ifstream original{originalPath, std::ios::binary};
    const std::string retained{std::istreambuf_iterator<char>{original}, std::istreambuf_iterator<char>{}};
    REQUIRE(retained == std::string{reinterpret_cast<const char *>(originalBytes.data()), originalBytes.size()});
    std::size_t correctedCopies = 0;
    for (const auto &entry :
         std::filesystem::recursive_directory_iterator(fixture.directoryPath() / L"JPG Spinner Output"))
        if (entry.is_regular_file() && entry.path().filename() == L"camera.JpEg")
        {
            std::ifstream corrected{entry.path(), std::ios::binary};
            const std::string bytes{std::istreambuf_iterator<char>{corrected}, std::istreambuf_iterator<char>{}};
            const auto validated = output.valueIfPresent()->encodedBytes();
            REQUIRE(bytes == std::string{reinterpret_cast<const char *>(validated.data()), validated.size()});
            ++correctedCopies;
        }
    REQUIRE(correctedCopies == 1);
}

TEST_CASE("Discovery preserves long Unicode names without opening encoded bytes",
          "[storage][batch][discovery][long-path]")
{
    using namespace winrt::Windows::Storage;
    const DiscoveryTestApartment apartment;
    const jpg_spinner::test_support::TemporaryDirectory fixture{std::filesystem::current_path() / "artifacts/tests"};
    const auto selected = StorageFolder::GetFolderFromPathAsync(fixture.directoryPath().wstring()).get();
    const auto journal = selected.CreateFolderAsync(L"journal").get();
    const auto childName = std::wstring(130, L'Ж');
    const auto child = selected.CreateFolderAsync(childName).get();
    const auto grandchildName = std::wstring(130, L'ю');
    const auto grandchild = child.CreateFolderAsync(grandchildName).get();
    const auto displayName = std::wstring(130, L'é') + L".JpEg";
    const auto file = grandchild.CreateFileAsync(displayName).get();
    // Cross the directory boundary too, regardless of the runner's working
    // directory: Windows Storage can enumerate a long item by its 8.3 alias.
    const auto physicalFilePath = fixture.directoryPath() / childName / grandchildName / displayName;
    REQUIRE(physicalFilePath.parent_path().wstring().size() > MAX_PATH);
    // The CRT path consumer is not long-path opted in. Establish the created
    // file through its Storage capability instead of that unrelated consumer.
    REQUIRE(file.GetBasicPropertiesAsync().get().Size() == 0);
    const auto transactions =
        std::make_shared<jpg_spinner::storage::WindowsStorageImageFileTransactionEngine>(selected, journal);
    const jpg_spinner::storage::WindowsStorageImageSourceCollection collection{selected, transactions};
    const auto discovered = collection.discover(jpg_spinner::domain::TraversalScope::SelectedFolderAndDescendants);
    REQUIRE(discovered.valueIfPresent() != nullptr);
    for (const auto &issue : discovered.valueIfPresent()->issues)
    {
        const auto native = issue.error
                                ? std::get_if<jpg_spinner::domain::WindowsHResult>(&issue.error->nativeErrorProjection)
                                : nullptr;
        INFO("Discovery issue relative length: " << issue.relativePath.size());
        INFO("Native discovery HRESULT: " << (native ? native->signedValue : 0));
        REQUIRE_FALSE(issue.error.has_value());
    }
    REQUIRE(relativeNames(*discovered.valueIfPresent()) ==
            std::set<std::wstring>{childName + L"/" + grandchildName + L"/" + displayName});
    REQUIRE(discovered.valueIfPresent()->issues.empty());
    const auto captured = discovered.valueIfPresent()->candidates.front().source->capture(1);
    REQUIRE(captured.valueIfPresent() != nullptr);
    REQUIRE(captured.valueIfPresent()->encodedBytes.empty());
}

TEST_CASE("A retained parent capability refuses substitution of an identical source",
          "[storage][batch][capture][identity]")
{
    using namespace jpg_spinner;
    using namespace winrt::Windows::Storage;
    const DiscoveryTestApartment apartment;
    const test_support::TemporaryDirectory fixture{std::filesystem::current_path() / "artifacts/tests"};
    const auto path = fixture.directoryPath() / L"candidate.jpg";
    writeCandidate(path);
    const auto observedWriteTime = std::filesystem::last_write_time(path);
    const auto selected = StorageFolder::GetFolderFromPathAsync(fixture.directoryPath().wstring()).get();
    const auto journal = selected.CreateFolderAsync(L"journal").get();
    const auto transactions = std::make_shared<storage::WindowsStorageImageFileTransactionEngine>(selected, journal);
    const storage::WindowsStorageImageSourceCollection collection{selected, transactions};
    const auto discovered = collection.discover(domain::TraversalScope::SelectedFolderOnly);
    REQUIRE(discovered.valueIfPresent() != nullptr);
    REQUIRE(discovered.valueIfPresent()->candidates.size() == 1);
    const auto &source = discovered.valueIfPresent()->candidates.front().source;
    const auto original = source->capture(1024);
    REQUIRE(original.valueIfPresent() != nullptr);
    std::filesystem::rename(path, fixture.directoryPath() / L"retired.jpg");
    writeCandidate(path);
    std::filesystem::last_write_time(path, observedWriteTime);
    // All revision components match. Only native identity explains refusal.
    const auto replacement = StorageFile::GetFileFromPathAsync(path.wstring()).get();
    const auto revision = storage::SourceFileRevisionCalculator{}.calculate(replacement);
    REQUIRE(revision.valueIfPresent() != nullptr);
    REQUIRE(*revision.valueIfPresent() == original.valueIfPresent()->revision);
    const auto refused = source->capture(1024);
    REQUIRE(refused.errorIfPresent() != nullptr);
    REQUIRE(refused.errorIfPresent()->code == domain::ImageProcessingErrorCode::SourceChangedAfterAnalysis);
    REQUIRE_FALSE(std::filesystem::exists(fixture.directoryPath() / L"JPG Spinner Output"));
    REQUIRE(std::filesystem::exists(fixture.directoryPath() / L"retired.jpg"));
}

TEST_CASE("Discovered source capture returns the same bounded bytes and revision", "[storage][batch][capture]")
{
    using namespace winrt::Windows::Storage;
    using jpg_spinner::domain::TraversalScope;
    const DiscoveryTestApartment apartment;
    const jpg_spinner::test_support::TemporaryDirectory fixture{std::filesystem::current_path() / "artifacts/tests"};
    const auto filePath = fixture.directoryPath() / L"sample.jpg";
    {
        std::ofstream file{filePath, std::ios::binary};
        file << "abc";
        file.close();
        REQUIRE(file.good());
    }
    REQUIRE(std::filesystem::file_size(filePath) == 3);
    const auto selected = StorageFolder::GetFolderFromPathAsync(fixture.directoryPath().wstring()).get();
    const auto journal = selected.CreateFolderAsync(L"journal").get();
    const auto transactions =
        std::make_shared<jpg_spinner::storage::WindowsStorageImageFileTransactionEngine>(selected, journal);
    const jpg_spinner::storage::WindowsStorageImageSourceCollection collection{selected, transactions};
    const auto discovered = collection.discover(TraversalScope::SelectedFolderOnly);
    REQUIRE(discovered.valueIfPresent() != nullptr);
    REQUIRE(discovered.valueIfPresent()->candidates.size() == 1);
    const auto &source = discovered.valueIfPresent()->candidates.front().source;
    const auto captured = source->capture(3);
    REQUIRE(captured.valueIfPresent() != nullptr);
    REQUIRE(captured.valueIfPresent()->encodedBytes ==
            std::vector<std::byte>{std::byte{'a'}, std::byte{'b'}, std::byte{'c'}});
    REQUIRE(captured.valueIfPresent()->revision.encodedLengthBytes == 3);
    // Independent published SHA-256 vector for the literal ASCII string "abc".
    const std::string_view expectedDigest = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    constexpr std::string_view digits = "0123456789abcdef";
    std::string actualDigest;
    for (const auto byte : captured.valueIfPresent()->revision.encodedSha256)
    {
        const auto value = std::to_integer<unsigned int>(byte);
        actualDigest += digits[value >> 4];
        actualDigest += digits[value & 15];
    }
    REQUIRE(actualDigest == expectedDigest);
    const auto refused = source->capture(2);
    REQUIRE(refused.errorIfPresent() != nullptr);
    REQUIRE(refused.errorIfPresent()->code == jpg_spinner::domain::ImageProcessingErrorCode::EncodedFileTooLarge);
    std::stop_source cancellation;
    cancellation.request_stop();
    const auto cancelled = source->capture(3, cancellation.get_token());
    REQUIRE(cancelled.errorIfPresent() != nullptr);
    REQUIRE(cancelled.errorIfPresent()->code == jpg_spinner::domain::ImageProcessingErrorCode::Cancelled);
    REQUIRE(std::filesystem::file_size(filePath) == 3);
}

TEST_CASE("Paged discovery reports and skips escaping and cyclic directory reparse points",
          "[storage][batch][discovery][reparse]")
{
    using namespace winrt::Windows::Storage;
    const DiscoveryTestApartment apartment;
    const jpg_spinner::test_support::TemporaryDirectory fixture{std::filesystem::current_path() / "artifacts/tests"};
    const auto selectedPath = fixture.directoryPath() / L"selected";
    const auto outsidePath = fixture.directoryPath() / L"outside";
    REQUIRE(std::filesystem::create_directory(selectedPath));
    REQUIRE(std::filesystem::create_directory(outsidePath));
    writeCandidate(selectedPath / L"owned.jpg");
    writeCandidate(outsidePath / L"foreign.jpg");
    const auto escapePath = selectedPath / L"escape";
    const auto loopPath = selectedPath / L"loop";
    REQUIRE(CreateSymbolicLinkW(escapePath.c_str(), outsidePath.c_str(),
                                SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE));
    REQUIRE(CreateSymbolicLinkW(loopPath.c_str(), selectedPath.c_str(),
                                SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE));
    REQUIRE(std::filesystem::is_symlink(escapePath));
    REQUIRE(std::filesystem::is_symlink(loopPath));
    const auto selected = StorageFolder::GetFolderFromPathAsync(selectedPath.wstring()).get();
    const auto journal = selected.CreateFolderAsync(L"journal").get();
    const auto transactions =
        std::make_shared<jpg_spinner::storage::WindowsStorageImageFileTransactionEngine>(selected, journal);
    const jpg_spinner::storage::WindowsStorageImageSourceCollection collection{selected, transactions};
    const auto discovered = collection.discover(jpg_spinner::domain::TraversalScope::SelectedFolderAndDescendants);
    REQUIRE(discovered.valueIfPresent() != nullptr);
    REQUIRE(relativeNames(*discovered.valueIfPresent()) == std::set<std::wstring>{L"owned.jpg"});
    REQUIRE(discovered.valueIfPresent()->issues.size() == 2);
    for (const auto &issue : discovered.valueIfPresent()->issues)
        REQUIRE(issue.kind == jpg_spinner::domain::ImageSourceDiscoveryIssueKind::ReparsePointSkipped);
    REQUIRE_FALSE(std::filesystem::exists(selectedPath / L"JPG Spinner Output"));
    REQUIRE(std::filesystem::exists(outsidePath / L"foreign.jpg"));
}

TEST_CASE("Discovery reads no image bytes and stops at an observed cancellation boundary",
          "[storage][batch][discovery][cancellation]")
{
    using namespace winrt::Windows::Storage;
    const DiscoveryTestApartment apartment;
    const jpg_spinner::test_support::TemporaryDirectory fixture{std::filesystem::current_path() / "artifacts/tests"};
    const auto filePath = fixture.directoryPath() / L"candidate.jpg";
    writeCandidate(filePath);
    // A real incompatible reader/writer makes any encoded-file read fail;
    // attribute-only handles remain exempt from the share-access restriction.
    const winrt::handle writeLock{CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr)};
    REQUIRE(writeLock.get() != INVALID_HANDLE_VALUE);
    const auto selected = StorageFolder::GetFolderFromPathAsync(fixture.directoryPath().wstring()).get();
    const auto journal = selected.CreateFolderAsync(L"journal").get();
    const auto transactions =
        std::make_shared<jpg_spinner::storage::WindowsStorageImageFileTransactionEngine>(selected, journal);
    const jpg_spinner::storage::WindowsStorageImageSourceCollection collection{selected, transactions};
    const auto discovered = collection.discover(jpg_spinner::domain::TraversalScope::SelectedFolderOnly);
    REQUIRE(discovered.valueIfPresent() != nullptr);
    REQUIRE(discovered.valueIfPresent()->candidates.size() == 1);
    REQUIRE(discovered.valueIfPresent()->issues.empty());
    std::optional<winrt::hresult> nativeReadFailure;
    try
    {
        const auto file = StorageFile::GetFileFromPathAsync(filePath.wstring()).get();
        auto stream = file.OpenAsync(FileAccessMode::Read, StorageOpenOptions::AllowOnlyReaders).get();
        stream.Close();
    }
    catch (const winrt::hresult_error &error)
    {
        nativeReadFailure = error.code();
    }
    REQUIRE(nativeReadFailure.has_value());
    REQUIRE(
        (*nativeReadFailure == E_ACCESSDENIED || *nativeReadFailure == HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION)));
    const auto deniedRead = discovered.valueIfPresent()->candidates.front().source->capture(1024);
    REQUIRE(deniedRead.errorIfPresent() != nullptr);
    // Windows Storage may report E_ACCESSDENIED for a conflicting writer, as
    // established by the native operation above and the existing revision tests.
    // Preserve its observation rather than inventing a more precise diagnosis.
    REQUIRE(
        std::get<jpg_spinner::domain::WindowsHResult>(deniedRead.errorIfPresent()->nativeErrorProjection).signedValue ==
        nativeReadFailure->value);
    REQUIRE(deniedRead.errorIfPresent()->code ==
            (*nativeReadFailure == E_ACCESSDENIED
                 ? jpg_spinner::domain::ImageProcessingErrorCode::SourceAccessDenied
                 : jpg_spinner::domain::ImageProcessingErrorCode::SourceSharingViolation));
    std::stop_source cancellation;
    std::size_t notifications = 0;
    const auto stopped = collection.discover(jpg_spinner::domain::TraversalScope::SelectedFolderOnly,
                                             cancellation.get_token(), [&](const auto &) {
                                                 ++notifications;
                                                 cancellation.request_stop();
                                             });
    REQUIRE(stopped.valueIfPresent() != nullptr);
    REQUIRE(stopped.valueIfPresent()->cancellationRequested);
    REQUIRE(notifications == 1);
}

std::set<std::wstring> relativeNames(const jpg_spinner::domain::ImageSourceDiscovery &discovery)
{
    std::set<std::wstring> result;
    for (const auto &candidate : discovery.candidates)
    {
        REQUIRE(candidate.source != nullptr);
        REQUIRE(result.insert(candidate.relativePath).second);
    }
    return result;
}
} // namespace

TEST_CASE("Native discovery honors extent and exact owned-folder identity", "[storage][batch][discovery]")
{
    using namespace winrt::Windows::Storage;
    using jpg_spinner::domain::TraversalScope;
    using jpg_spinner::storage::WindowsStorageImageSourceCollection;
    const DiscoveryTestApartment apartment;
    const jpg_spinner::test_support::TemporaryDirectory fixture{std::filesystem::current_path() / "artifacts/tests"};
    const auto root = fixture.directoryPath();
    REQUIRE(std::filesystem::create_directory(root / L"nested"));
    REQUIRE(std::filesystem::create_directory(root / L"JPG Spinner Output"));
    REQUIRE(std::filesystem::create_directory(root / L"JPG Spinner Output Notes"));
    REQUIRE(std::filesystem::create_directory(root / L"backup"));
    writeCandidate(root / L"z.JpG");
    writeCandidate(root / L"a.JPEG");
    writeCandidate(root / L"unrelated.txt");
    writeCandidate(root / L"nested" / L"picture.jPe");
    writeCandidate(root / L"nested" / L"scan.JFIF");
    writeCandidate(root / L"JPG Spinner Output" / L"owned.jpg");
    writeCandidate(root / L"JPG Spinner Output Notes" / L"real.jpg");
    writeCandidate(root / L"backup" / L"owned.jpg");
    const auto selected = StorageFolder::GetFolderFromPathAsync(root.wstring()).get();
    const auto output = selected.GetFolderAsync(L"JPG Spinner Output").get();
    const auto backup = selected.GetFolderAsync(L"backup").get();
    const auto journal = selected.CreateFolderAsync(L"journal").get();
    const auto transactions =
        std::make_shared<jpg_spinner::storage::WindowsStorageImageFileTransactionEngine>(selected, journal);
    const WindowsStorageImageSourceCollection collection{selected, transactions, {output, backup}};
    const auto shallow = collection.discover(TraversalScope::SelectedFolderOnly);
    REQUIRE(shallow.valueIfPresent() != nullptr);
    REQUIRE(relativeNames(*shallow.valueIfPresent()) == std::set<std::wstring>{L"a.JPEG", L"z.JpG"});
    const auto deep = collection.discover(TraversalScope::SelectedFolderAndDescendants);
    REQUIRE(deep.valueIfPresent() != nullptr);
    REQUIRE(relativeNames(*deep.valueIfPresent()) == std::set<std::wstring>{L"a.JPEG", L"z.JpG", L"nested/picture.jPe",
                                                                            L"nested/scan.JFIF",
                                                                            L"JPG Spinner Output Notes/real.jpg"});
    REQUIRE(deep.valueIfPresent()->issues.size() == 2);
    REQUIRE_FALSE(deep.valueIfPresent()->cancellationRequested);
    // Discovery preserves the source tree; excluded names are neither deleted nor processed.
    REQUIRE(std::filesystem::exists(root / L"backup" / L"owned.jpg"));
    REQUIRE(std::filesystem::exists(root / L"JPG Spinner Output" / L"owned.jpg"));
}
