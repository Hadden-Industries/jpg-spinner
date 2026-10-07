#define NOMINMAX
#include <windows.h>
#include <jpg_spinner/batch/BatchProcessingCoordinator.h>
#include <jpg_spinner/jpeg/LibJpegTurboTransformationEngine.h>
#include <jpg_spinner/storage/WindowsStorageImageSourceCollection.h>
#include <jpg_spinner/storage/WindowsStorageImageFileTransactionEngine.h>
#include "DeterministicJpegFixtureFactory.h"
#include "../JpgSpinner.JpegTransformation.Tests/CoefficientTransformFixture.h"
#include <exiv2/exiv2.hpp>
#include "TemporaryDirectory.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>
#include <filesystem>
#include <fstream>
#include <bit>

namespace
{
struct BatchStorageApartment final
{
    BatchStorageApartment()
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    }
    ~BatchStorageApartment()
    {
        winrt::uninit_apartment();
    }
};
void writeImage(const std::filesystem::path &path, const std::vector<std::byte> &bytes)
{
    std::ofstream file{path, std::ios::binary};
    file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    file.close();
    REQUIRE(file.good());
    REQUIRE(std::filesystem::file_size(path) == bytes.size());
}
std::vector<std::byte> readImage(const std::filesystem::path &path)
{
    const auto length = std::filesystem::file_size(path);
    REQUIRE(length < 1024 * 1024);
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    std::ifstream file{path, std::ios::binary};
    file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    REQUIRE(file.good());
    return bytes;
}
} // namespace

TEST_CASE("A source without orientation metadata creates neither an output nor a backup",
          "[storage][batch][integration][no-orientation]")
{
    using namespace jpg_spinner;
    using namespace batch_processing;
    using namespace winrt::Windows::Storage;
    const BatchStorageApartment apartment;
    const test_support::TemporaryDirectory fixture{std::filesystem::temp_directory_path() /
                                                   L"jpg-spinner-batch-integration"};
    // Reuse the existing native encoder fixture, which adds no Exif/XMP marker.
    // Independently establish that precondition through the public metadata API.
    const auto bytes = test_support::createTransformFixture(8, TJSAMP_444, TJCS_YCbCr);
    const auto metadata = Exiv2::ImageFactory::open(reinterpret_cast<const Exiv2::byte *>(bytes.data()), bytes.size());
    REQUIRE(metadata != nullptr);
    metadata->readMetadata();
    REQUIRE(metadata->exifData().empty());
    REQUIRE(metadata->xmpData().empty());
    const auto path = fixture.directoryPath() / L"no-orientation.jpg";
    writeImage(path, bytes);
    const auto selected = StorageFolder::GetFolderFromPathAsync(fixture.directoryPath().wstring()).get();
    const auto journal = selected.CreateFolderAsync(L"journal").get();
    const auto transactions = std::make_shared<storage::WindowsStorageImageFileTransactionEngine>(selected, journal);
    const auto collection = std::make_shared<storage::WindowsStorageImageSourceCollection>(selected, transactions);
    const BatchProcessingCoordinator coordinator{std::make_shared<jpeg::LibJpegTurboTransformationEngine>()};
    auto analyzed = coordinator.analyze({collection});
    REQUIRE(analyzed.valueIfPresent() != nullptr);
    REQUIRE(analyzed.valueIfPresent()->files().size() == 1);
    REQUIRE(analyzed.valueIfPresent()->files()[0].jpegAnalysis->authoritativeOrientation ==
            domain::ExifOrientation::TopLeft);
    const auto summary = coordinator.execute(std::move(*analyzed.valueIfPresent()));
    REQUIRE(summary.valueIfPresent() != nullptr);
    REQUIRE(summary.valueIfPresent()->fileResults[0].outcome == BatchFileOutcome::NoOrientationNormalizationRequired);
    REQUIRE_FALSE(summary.valueIfPresent()->fileResults[0].error.has_value());
    REQUIRE(summary.valueIfPresent()->finalProgress.skipped == 1);
    REQUIRE_FALSE(std::filesystem::exists(fixture.directoryPath() / L"JPG Spinner Output"));
    REQUIRE_FALSE(std::filesystem::exists(fixture.directoryPath() / L"JPG Spinner Backups"));
    REQUIRE(journal.GetItemsAsync().get().Size() == 0);
    REQUIRE(readImage(path) == bytes);
}

TEST_CASE("Batch composition uses real native capture JPEG validation and corrected-copy or backup commits",
          "[storage][batch][integration]")
{
    using namespace jpg_spinner;
    using namespace batch_processing;
    using namespace winrt::Windows::Storage;
    const auto disposition = GENERATE(domain::OutputDisposition::CreateCorrectedCopy,
                                      domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup);
    const BatchStorageApartment apartment;
    const test_support::TemporaryDirectory fixture{std::filesystem::temp_directory_path() /
                                                   L"jpg-spinner-batch-integration"};
    const auto inputPath = fixture.directoryPath() / L"input";
    REQUIRE(std::filesystem::create_directory(inputPath));
    REQUIRE(std::filesystem::create_directory(inputPath / L"nested"));
    const auto rotated = test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
        test_support::JpegChromaSubsampling::ycbcr444, 3);
    const auto upright = test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
        test_support::JpegChromaSubsampling::ycbcr444, 1);
    writeImage(inputPath / L"nested" / L"a.JpEg", rotated);
    writeImage(inputPath / L"b.jpg", upright);
    writeImage(inputPath / L"c-malformed.jpg", {std::byte{0}});
    writeImage(inputPath / L"unrelated.txt", rotated); // real JPEG, intentionally not a candidate
    const auto selected = StorageFolder::GetFolderFromPathAsync(inputPath.wstring()).get();
    const auto owner = StorageFolder::GetFolderFromPathAsync(fixture.directoryPath().wstring()).get();
    const auto journal = owner.CreateFolderAsync(L"journal").get();
    BatchProcessingRequest request;
    const auto transactions = std::make_shared<storage::WindowsStorageImageFileTransactionEngine>(
        selected, journal, std::bit_cast<winrt::guid>(request.identifier.fullIdentity()),
        request.identifier.creationTime());
    const auto collection = std::make_shared<storage::WindowsStorageImageSourceCollection>(selected, transactions);
    const BatchProcessingCoordinator coordinator{std::make_shared<jpeg::LibJpegTurboTransformationEngine>()};
    request.sourceCollection = collection;
    request.traversalScope = domain::TraversalScope::SelectedFolderAndDescendants;
    request.edgeHandlingPolicy = domain::EdgeHandlingPolicy::TrimPartialMinimumCodedUnits;
    request.outputDisposition = disposition;
    auto analyzed = coordinator.analyze(request);
    REQUIRE(analyzed.valueIfPresent() != nullptr);
    const auto review = analyzed.valueIfPresent()->files();
    REQUIRE(review.size() == 3);
    REQUIRE(review[0].relativePath == L"b.jpg");
    REQUIRE(review[1].relativePath == L"c-malformed.jpg");
    REQUIRE(review[2].relativePath == L"nested/a.JpEg");
    REQUIRE(review[2].jpegAnalysis->authoritativeOrientation == domain::ExifOrientation::BottomRight);
    REQUIRE_FALSE(std::filesystem::exists(inputPath / L"JPG Spinner Output"));
    REQUIRE_FALSE(std::filesystem::exists(inputPath / L"JPG Spinner Backups"));
    const auto expected =
        jpeg::LibJpegTurboTransformationEngine{}.createValidatedOutput(rotated, *review[2].jpegAnalysis);
    REQUIRE(expected.valueIfPresent() != nullptr);
    const std::vector<std::byte> expectedBytes{expected.valueIfPresent()->encodedBytes().begin(),
                                               expected.valueIfPresent()->encodedBytes().end()};
    const auto summary = coordinator.execute(std::move(*analyzed.valueIfPresent()));
    REQUIRE(summary.valueIfPresent() != nullptr);
    REQUIRE(summary.valueIfPresent()->fileResults.size() == 3);
    REQUIRE(summary.valueIfPresent()->fileResults[0].outcome == BatchFileOutcome::NoOrientationNormalizationRequired);
    REQUIRE(summary.valueIfPresent()->fileResults[1].outcome == BatchFileOutcome::UnsupportedSourceSkipped);
    INFO("Rotated-source error code: " << (summary.valueIfPresent()->fileResults[2].error
                                               ? static_cast<unsigned>(
                                                     summary.valueIfPresent()->fileResults[2].error->code)
                                               : 0));
    REQUIRE(summary.valueIfPresent()->fileResults[2].outcome ==
            (disposition == domain::OutputDisposition::CreateCorrectedCopy
                 ? BatchFileOutcome::CorrectedCopyCreated
                 : BatchFileOutcome::OriginalReplacedWithVerifiedBackup));
    REQUIRE_FALSE(summary.valueIfPresent()->transactionIntegrityUncertain);
    REQUIRE(summary.valueIfPresent()->finalProgress.completed == 3);
    REQUIRE(summary.valueIfPresent()->finalProgress.skipped == 2);
    REQUIRE(summary.valueIfPresent()->finalProgress.failed == 0);
    REQUIRE(readImage(inputPath / L"b.jpg") == upright);
    REQUIRE(readImage(inputPath / L"unrelated.txt") == rotated);
    REQUIRE(readImage(inputPath / L"nested" / L"a.JpEg") ==
            (disposition == domain::OutputDisposition::CreateCorrectedCopy ? rotated : expectedBytes));
    std::size_t retainedOutputs = 0;
    std::size_t retainedBackups = 0;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(inputPath))
    {
        REQUIRE_FALSE(entry.path().filename().wstring().starts_with(L".jpg-spinner-staged-"));
        if (!entry.is_regular_file() || entry.path().filename() != L"a.JpEg" ||
            entry.path().parent_path() == inputPath / L"nested")
            continue;
        REQUIRE(entry.path().parent_path().parent_path().filename() ==
                summary.valueIfPresent()->identifier.displayDirectoryName());
        const auto bytes = readImage(entry.path());
        if (bytes == rotated)
            ++retainedBackups;
        else
        {
            REQUIRE(bytes == expectedBytes);
            ++retainedOutputs;
        }
    }
    REQUIRE(retainedOutputs == (disposition == domain::OutputDisposition::CreateCorrectedCopy ? 1U : 0U));
    REQUIRE(retainedBackups == (disposition == domain::OutputDisposition::CreateCorrectedCopy ? 0U : 1U));
    const auto recovered = transactions->recoverIncompleteTransactions();
    REQUIRE(recovered.valueIfPresent() != nullptr);
    REQUIRE(recovered.valueIfPresent()->recoveredTransactions.empty());
}
