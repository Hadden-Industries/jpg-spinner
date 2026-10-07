#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <jpg_spinner/storage/WindowsStorageImageSourceCollection.h>
#include <jpg_spinner/storage/WindowsStorageImageFileTransactionEngine.h>
#include "TemporaryDirectory.h"
#include "DeterministicJpegFixtureFactory.h"
#include <catch2/catch_test_macros.hpp>
#include <winrt/Windows.Foundation.h>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <optional>

namespace
{
struct BenchmarkApartment final
{
    BenchmarkApartment()
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    }
    ~BenchmarkApartment()
    {
        winrt::uninit_apartment();
    }
};

PROCESS_MEMORY_COUNTERS processMemory()
{
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = sizeof(counters);
    REQUIRE(GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)));
    return counters;
}
} // namespace

TEST_CASE("Native paged discovery baseline covers ten thousand real JPEG candidates",
          "[storage][batch][discovery][benchmark]")
{
    using namespace jpg_spinner;
    using namespace winrt::Windows::Storage;
    const BenchmarkApartment apartment;
    const test_support::TemporaryDirectory fixture{std::filesystem::temp_directory_path() /
                                                   L"jpg-spinner-discovery-tests"};
    const auto inputPath = fixture.directoryPath() / L"input";
    REQUIRE(std::filesystem::create_directory(inputPath));
    const auto sourceBytes = test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
        test_support::JpegChromaSubsampling::ycbcr444, 3);
    constexpr std::array<std::wstring_view, 4> extensions{L".JpG", L".JPEG", L".jPe", L".JFIF"};
    for (unsigned folderIndex = 0; folderIndex < 100; ++folderIndex)
    {
        const auto folder = inputPath / std::format(L"folder-{:03}", folderIndex);
        REQUIRE(std::filesystem::create_directory(folder));
        for (unsigned fileIndex = 0; fileIndex < 100; ++fileIndex)
        {
            const auto path =
                folder / std::format(L"image-{:03}{}", fileIndex, extensions[fileIndex % extensions.size()]);
            std::ofstream file{path, std::ios::binary};
            file.write(reinterpret_cast<const char *>(sourceBytes.data()),
                       static_cast<std::streamsize>(sourceBytes.size()));
            file.close();
            REQUIRE(file.good());
        }
    }
    const auto selected = StorageFolder::GetFolderFromPathAsync(inputPath.wstring()).get();
    const auto owner = StorageFolder::GetFolderFromPathAsync(fixture.directoryPath().wstring()).get();
    const auto journal = owner.CreateFolderAsync(L"journal").get();
    const auto engine = std::make_shared<storage::WindowsStorageImageFileTransactionEngine>(selected, journal);
    const storage::WindowsStorageImageSourceCollection collection{selected, engine};
    const auto baseline = processMemory();
    auto observedPeak = baseline.WorkingSetSize;
    const auto started = std::chrono::steady_clock::now();
    std::optional<std::chrono::steady_clock::time_point> firstResult;
    const auto discovery =
        collection.discover(domain::TraversalScope::SelectedFolderAndDescendants, {}, [&](const auto &progress) {
            if (progress.discoveredCandidates != 0 && !firstResult)
                firstResult = std::chrono::steady_clock::now();
            if (progress.inspectedItems % 500 == 0)
                observedPeak = std::max(observedPeak, processMemory().WorkingSetSize);
        });
    const auto completed = std::chrono::steady_clock::now();
    REQUIRE(discovery.valueIfPresent() != nullptr);
    REQUIRE(discovery.valueIfPresent()->candidates.size() == 10'000);
    REQUIRE(discovery.valueIfPresent()->issues.empty());
    REQUIRE_FALSE(discovery.valueIfPresent()->cancellationRequested);
    REQUIRE(firstResult.has_value());
    observedPeak = std::max(observedPeak, processMemory().WorkingSetSize);
    std::stop_source cancellation;
    std::optional<std::chrono::steady_clock::time_point> cancellationRequested;
    const auto partial = collection.discover(domain::TraversalScope::SelectedFolderAndDescendants,
                                             cancellation.get_token(), [&](const auto &progress) {
                                                 if (progress.discoveredCandidates == 50)
                                                 {
                                                     cancellationRequested = std::chrono::steady_clock::now();
                                                     cancellation.request_stop();
                                                 }
                                             });
    const auto cancellationCompleted = std::chrono::steady_clock::now();
    REQUIRE(cancellationRequested.has_value());
    REQUIRE(partial.valueIfPresent() != nullptr);
    REQUIRE(partial.valueIfPresent()->cancellationRequested);
    REQUIRE(partial.valueIfPresent()->candidates.size() == 50);
    REQUIRE_FALSE(std::filesystem::exists(inputPath / L"JPG Spinner Output"));
    const auto microseconds = [](const auto duration) {
        return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    };
    // Raw benchmark observations are retained by the runner, not a fabricated
    // performance threshold. Peak includes native projections and observer cost;
    // it is process working set, not codec working memory or encoded-file limit.
    std::cout << "discovery_baseline candidates=10000 first_result_us=" << microseconds(*firstResult - started)
              << " total_us=" << microseconds(completed - started)
              << " baseline_working_set_bytes=" << baseline.WorkingSetSize
              << " observed_peak_working_set_bytes=" << observedPeak
              << " process_peak_working_set_bytes=" << processMemory().PeakWorkingSetSize
              << " cancellation_us=" << microseconds(cancellationCompleted - *cancellationRequested) << '\n';
}
