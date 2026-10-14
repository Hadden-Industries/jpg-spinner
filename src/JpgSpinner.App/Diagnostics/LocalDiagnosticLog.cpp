#include "pch.h"
#include "Diagnostics/LocalDiagnosticLog.h"
#include <jpg_spinner/domain/ImageProcessingSymbolicNames.h>
#include <jpg_spinner/storage/DirectChildStorageFileLease.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <format>
#include <winrt/Windows.Storage.Search.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Storage.FileProperties.h>
#include <windowsstoragecom.h>
#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <sstream>
#include <limits>

namespace jpg_spinner::diagnostics
{
ImageProcessingDiagnosticRecord captureImageProcessingDiagnosticRecord(const winrt::guid correlationIdentifier,
                                                                       const domain::ImageProcessingStage observedStage,
                                                                       const std::chrono::milliseconds elapsedTime,
                                                                       const domain::ImageProcessingError *failure)
{
#if defined(_M_ARM64)
    constexpr auto architecture = ApplicationArchitecture::Arm64;
#elif defined(_M_X64)
    constexpr auto architecture = ApplicationArchitecture::X64;
#else
    constexpr auto architecture = ApplicationArchitecture::X86;
#endif
    const auto violation =
        failure ? std::get_if<domain::JpegResourceLimitViolation>(&failure->diagnosticContext) : nullptr;
    return {std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now()),
            winrt::Windows::ApplicationModel::Package::Current().Id().Version(),
            architecture,
            correlationIdentifier,
            failure ? failure->stage : observedStage,
            failure ? std::optional{failure->code} : std::nullopt,
            elapsedTime,
            violation ? std::optional{violation->resourceLimit} : std::nullopt};
}
std::string serializeImageProcessingDiagnosticRecord(const ImageProcessingDiagnosticRecord &record)
{
    using namespace std::chrono;
    using winrt::Windows::Data::Json::JsonValue;
    constexpr auto maximumExactJsonInteger = 9'007'199'254'740'991LL;
    if (record.correlationIdentifier == winrt::guid{} || record.elapsedTime.count() < 0 ||
        record.elapsedTime.count() > maximumExactJsonInteger ||
        record.recordedAtUtc < sys_days{year{1} / January / 1} ||
        record.recordedAtUtc >= sys_days{year{9999} / December / 31} + days{1})
        throw winrt::hresult_invalid_argument();
    const auto architecture = [&] {
        switch (record.applicationArchitecture)
        {
        case ApplicationArchitecture::X86:
            return L"x86";
        case ApplicationArchitecture::X64:
            return L"x64";
        case ApplicationArchitecture::Arm64:
            return L"arm64";
        default:
            return L"Unknown";
        }
    }();
    winrt::Windows::Data::Json::JsonObject json;
    json.Insert(L"recordedAtUtc",
                JsonValue::CreateStringValue(winrt::to_hstring(std::format("{:%FT%TZ}", record.recordedAtUtc))));
    const auto &version = record.applicationVersion;
    json.Insert(L"applicationVersion",
                JsonValue::CreateStringValue(winrt::to_hstring(
                    std::format("{}.{}.{}.{}", version.Major, version.Minor, version.Build, version.Revision))));
    json.Insert(L"applicationArchitecture", JsonValue::CreateStringValue(architecture));
    json.Insert(L"correlationIdentifier",
                JsonValue::CreateStringValue(winrt::to_hstring(record.correlationIdentifier)));
    json.Insert(L"stage", JsonValue::CreateStringValue(winrt::hstring{domain::symbolicName(record.stage)}));
    json.Insert(L"errorCode",
                record.errorCode ? JsonValue::CreateStringValue(winrt::hstring{domain::symbolicName(*record.errorCode)})
                                 : JsonValue::CreateNullValue());
    // WinRT JSON numbers are doubles. The exact-integer bound above prevents
    // silently rounding a duration into a different diagnostic observation.
    json.Insert(L"elapsedMilliseconds", JsonValue::CreateNumberValue(static_cast<double>(record.elapsedTime.count())));
    json.Insert(L"resourceLimit",
                record.resourceLimit
                    ? JsonValue::CreateStringValue(winrt::hstring{domain::symbolicName(*record.resourceLimit)})
                    : JsonValue::CreateNullValue());
    return winrt::to_string(json.Stringify()) + '\n';
}
} // namespace jpg_spinner::diagnostics

namespace jpg_spinner::diagnostics
{
namespace
{
constexpr std::uint32_t maximumRecordLengthBytes = 4096;
constexpr auto leaseName = L".diagnostic-log-writer-lease";
constexpr auto pendingRecordName = L".pending-diagnostic-record.jsonl";
constexpr std::wstring_view recordPrefix = L"diagnostic-";
constexpr std::wstring_view recordSuffix = L".jsonl";

void requireNonReparseStorageItem(const winrt::Windows::Storage::IStorageItem &item)
{
    // Observe the original item's documented UInt32 attributes before opening
    // a stream/handle: brokered opens can follow a reparse point. A valid JSON
    // target does not confer diagnostic-store ownership on an escaping link.
    const auto properties =
        item.GetBasicPropertiesAsync().get().RetrievePropertiesAsync({L"System.FileAttributes"}).get();
    if ((winrt::unbox_value<std::uint32_t>(properties.Lookup(L"System.FileAttributes")) &
         FILE_ATTRIBUTE_REPARSE_POINT) != 0)
        throw winrt::hresult_invalid_argument();
}

winrt::handle acquireWriterLease(const winrt::Windows::Storage::StorageFolder &folder)
{
    requireNonReparseStorageItem(folder);
    const auto file =
        folder.CreateFileAsync(leaseName, winrt::Windows::Storage::CreationCollisionOption::OpenIfExists).get();
    const storage::DirectChildStorageFileLease containment{folder, file};
    winrt::handle handle;
    winrt::check_hresult(file.as<IStorageItemHandleAccess>()->Create(
        static_cast<HANDLE_ACCESS_OPTIONS>(HAO_READ | HAO_WRITE), HSO_SHARE_NONE, HO_NONE, nullptr, handle.put()));
    FILE_STANDARD_INFO standard{};
    FILE_ATTRIBUTE_TAG_INFO attributes{};
    winrt::check_bool(GetFileInformationByHandleEx(handle.get(), FileStandardInfo, &standard, sizeof(standard)));
    winrt::check_bool(
        GetFileInformationByHandleEx(handle.get(), FileAttributeTagInfo, &attributes, sizeof(attributes)));
    if (standard.EndOfFile.QuadPart != 0 || standard.Directory ||
        (attributes.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
        throw winrt::hresult_invalid_argument();
    return handle; // Native sharing releases on process death; no stale marker deletion.
}

/// Export rehydrates only the closed typed record, not raw persisted JSON.
/// Windows owns JSON syntax; standard chrono/from_chars own scalar parsing.
/// Foreign members/types, invalid symbols and unsupported numeric precision
/// refuse export rather than leaking externally substituted log contents.
ImageProcessingDiagnosticRecord readSafeRecord(const std::string &line)
{
    using namespace std::chrono;
    using winrt::Windows::Data::Json::JsonValueType;
    if (line.empty() || line.back() != '\n' || line.find('\n') != line.size() - 1)
        throw winrt::hresult_invalid_argument();
    const auto json = winrt::Windows::Data::Json::JsonObject::Parse(winrt::to_hstring(line));
    if (json.Size() != 8)
        throw winrt::hresult_invalid_argument();
    ImageProcessingDiagnosticRecord record{};
    const auto timeText = winrt::to_string(json.GetNamedString(L"recordedAtUtc"));
    std::istringstream timeInput{timeText};
    timeInput.imbue(std::locale::classic());
    from_stream(timeInput, "%FT%TZ", record.recordedAtUtc);
    if (timeInput.fail() || timeInput.peek() != std::char_traits<char>::eof() ||
        std::format("{:%FT%TZ}", record.recordedAtUtc) != timeText)
        throw winrt::hresult_invalid_argument();
    const auto versionText = winrt::to_string(json.GetNamedString(L"applicationVersion"));
    const std::array versionFields{&record.applicationVersion.Major, &record.applicationVersion.Minor,
                                   &record.applicationVersion.Build, &record.applicationVersion.Revision};
    const auto *position = versionText.data();
    const auto *end = position + versionText.size();
    for (std::size_t index = 0; index < versionFields.size(); ++index)
    {
        const auto parsed = std::from_chars(position, end, *versionFields[index]);
        if (parsed.ec != std::errc{} ||
            (index + 1 == versionFields.size() ? parsed.ptr != end : parsed.ptr == end || *parsed.ptr != '.'))
            throw winrt::hresult_invalid_argument();
        position = parsed.ptr + (index + 1 == versionFields.size() ? 0 : 1);
    }
    const auto architecture = json.GetNamedString(L"applicationArchitecture");
    if (architecture == L"x86")
        record.applicationArchitecture = ApplicationArchitecture::X86;
    else if (architecture == L"x64")
        record.applicationArchitecture = ApplicationArchitecture::X64;
    else if (architecture == L"arm64")
        record.applicationArchitecture = ApplicationArchitecture::Arm64;
    else if (architecture == L"Unknown")
        record.applicationArchitecture = static_cast<ApplicationArchitecture>(-1);
    else
        throw winrt::hresult_invalid_argument();
    record.correlationIdentifier = winrt::guid{std::wstring_view{json.GetNamedString(L"correlationIdentifier")}};
    const auto symbol = []<typename Enum>(const winrt::hstring &text, Enum last) {
        if (text == L"Unknown")
            return static_cast<Enum>(-1);
        for (int ordinal = 0; ordinal <= static_cast<int>(last); ++ordinal)
            if (text == domain::symbolicName(static_cast<Enum>(ordinal)))
                return static_cast<Enum>(ordinal);
        throw winrt::hresult_invalid_argument();
    };
    record.stage = symbol(json.GetNamedString(L"stage"), domain::ImageProcessingStage::JournalStoreLeaseAcquisition);
    const auto error = json.GetNamedValue(L"errorCode");
    if (error.ValueType() != JsonValueType::Null)
        record.errorCode = symbol(error.GetString(), domain::ImageProcessingErrorCode::Cancelled);
    const auto limit = json.GetNamedValue(L"resourceLimit");
    if (limit.ValueType() != JsonValueType::Null)
        record.resourceLimit = symbol(limit.GetString(), domain::JpegResourceLimit::ProgressiveScanCount);
    const auto elapsed = json.GetNamedNumber(L"elapsedMilliseconds");
    if (!std::isfinite(elapsed) || elapsed < 0 || elapsed > 9'007'199'254'740'991.0 || std::floor(elapsed) != elapsed)
        throw winrt::hresult_invalid_argument();
    record.elapsedTime = milliseconds{static_cast<std::int64_t>(elapsed)};
    return record;
}

struct RetainedDiagnosticRecord final
{
    winrt::Windows::Storage::StorageFile file;
    std::uint64_t publicationSequence;
    std::string sanitizedLine;
    std::uint64_t persistedLengthBytes;
};

std::vector<RetainedDiagnosticRecord> inspectRecords(const winrt::Windows::Storage::StorageFolder &folder)
{
    using namespace winrt::Windows::Storage;
    // Bound native enumeration and each read independently, before allocating
    // payload buffers. Unknown objects remain untouched, not guessed ownership.
    if (folder.GetFoldersAsync(Search::CommonFolderQuery::DefaultQuery, 0, 1).get().Size() != 0)
        throw winrt::hresult_invalid_argument();
    const auto files = folder.GetFilesAsync(Search::CommonFileQuery::DefaultQuery, 0, 130).get();
    if (files.Size() > 129)
        throw winrt::hresult_invalid_argument();
    std::vector<RetainedDiagnosticRecord> records;
    for (const auto &file : files)
    {
        const auto retainedName = file.Name();
        const std::wstring_view name{retainedName};
        if (name == leaseName)
            continue;
        const storage::DirectChildStorageFileLease containment{folder, file};
        if (name == pendingRecordName)
        {
            // A reserved app-owned scratch slot is never a published record.
            // Keep partial crash bytes out of export; an exclusive writer can
            // replace this one slot, not arbitrary unknown filenames.
            if (file.GetBasicPropertiesAsync().get().Size() > maximumRecordLengthBytes)
                throw winrt::hresult_invalid_argument();
            continue;
        }
        if (!name.starts_with(recordPrefix) || !name.ends_with(recordSuffix))
            throw winrt::hresult_invalid_argument();
        constexpr std::size_t sequenceWidth = 20;
        if (name.size() < recordPrefix.size() + sequenceWidth + 1 + recordSuffix.size() ||
            name[recordPrefix.size() + sequenceWidth] != L'-')
            throw winrt::hresult_invalid_argument();
        const auto sequenceText = winrt::to_string(winrt::hstring{name.substr(recordPrefix.size(), sequenceWidth)});
        std::uint64_t sequence{};
        const auto parsedSequence =
            std::from_chars(sequenceText.data(), sequenceText.data() + sequenceText.size(), sequence);
        if (parsedSequence.ec != std::errc{} || parsedSequence.ptr != sequenceText.data() + sequenceText.size() ||
            sequence == 0)
            throw winrt::hresult_invalid_argument();
        const auto identifierOffset = recordPrefix.size() + sequenceWidth + 1;
        const winrt::guid identifier{
            name.substr(identifierOffset, name.size() - identifierOffset - recordSuffix.size())};
        if (identifier == winrt::guid{})
            throw winrt::hresult_invalid_argument();
        const auto stream = file.OpenReadAsync().get();
        Streams::DataReader reader{stream};
        reader.UnicodeEncoding(Streams::UnicodeEncoding::Utf8);
        const auto size = reader.LoadAsync(maximumRecordLengthBytes + 1).get();
        if (size > maximumRecordLengthBytes)
            throw winrt::hresult_invalid_argument();
        const auto line = winrt::to_string(reader.ReadString(size));
        const auto safe = serializeImageProcessingDiagnosticRecord(readSafeRecord(line));
        records.push_back({file, sequence, safe, size});
    }
    std::ranges::sort(records, [](const auto &left, const auto &right) {
        return left.publicationSequence < right.publicationSequence;
    });
    for (std::size_t index = 1; index < records.size(); ++index)
        if (records[index - 1].publicationSequence == records[index].publicationSequence)
            throw winrt::hresult_invalid_argument();
    return records;
}
} // namespace
LocalDiagnosticLog::LocalDiagnosticLog(winrt::Windows::Storage::StorageFolder privateLogFolder,
                                       DiagnosticLogRetentionLimits limits)
    : privateLogFolder_(std::move(privateLogFolder)), limits_(limits)
{
    if (!privateLogFolder_ || limits_.maximumRetainedFileCount == 0 || limits_.maximumRetainedFileCount > 128 ||
        limits_.maximumRetainedLengthBytes == 0 || limits_.maximumRetainedLengthBytes > 1024 * 1024)
        throw winrt::hresult_invalid_argument();
}
bool LocalDiagnosticLog::append(const ImageProcessingDiagnosticRecord &record) noexcept
{
    try
    {
        const auto line = serializeImageProcessingDiagnosticRecord(record);
        if (line.size() > maximumRecordLengthBytes || line.size() > limits_.maximumRetainedLengthBytes)
            return false;
        const std::scoped_lock lock{mutex_};
        const auto lease = acquireWriterLease(privateLogFolder_);
        const auto records = inspectRecords(privateLogFolder_);
        const auto lastSequence = records.empty() ? 0 : records.back().publicationSequence;
        if (lastSequence == (std::numeric_limits<std::uint64_t>::max)())
            return false;
        std::uint64_t total{};
        for (const auto &existing : records)
            total += existing.persistedLengthBytes;
        std::size_t oldest{};
        // Prune before publication, so even the new record cannot transiently
        // exceed the declared retained-byte/file limits. Loss of old diagnostics
        // on a later write failure is acceptable; images/journals are not logs.
        while (records.size() - oldest >= limits_.maximumRetainedFileCount ||
               total + line.size() > limits_.maximumRetainedLengthBytes)
        {
            records.at(oldest).file.DeleteAsync().get();
            total -= records.at(oldest++).persistedLengthBytes;
        }
        GUID nativeIdentifier{};
        winrt::check_hresult(CoCreateGuid(&nativeIdentifier));
        // Storage DateCreated can be coarser than consecutive appends. A
        // lease-serialized persisted sequence, not wall time or random UUID
        // sorting, supplies deterministic publication/retention order.
        const auto name = std::wstring{recordPrefix} + std::format(L"{:020}-", lastSequence + 1) +
                          std::wstring{winrt::to_hstring(std::bit_cast<winrt::guid>(nativeIdentifier))} +
                          std::wstring{recordSuffix};
        const auto file =
            privateLogFolder_
                .CreateFileAsync(pendingRecordName, winrt::Windows::Storage::CreationCollisionOption::OpenIfExists)
                .get();
        const auto *bytes = reinterpret_cast<const std::uint8_t *>(line.data());
        {
            const storage::DirectChildStorageFileLease containment{privateLogFolder_, file};
            winrt::Windows::Storage::FileIO::WriteBytesAsync(
                file, winrt::array_view<const std::uint8_t>{bytes, bytes + line.size()})
                .get();
        }
        // Publish complete bytes by a native collision-refusing rename. The
        // stage proof closes before this authorized move. This is namespace
        // publication, not a promise of survival after power/media failure.
        file.MoveAsync(privateLogFolder_, name, winrt::Windows::Storage::NameCollisionOption::FailIfExists).get();
        return true;
    }
    catch (...)
    {
        return false; // Never recursively log a logger failure or expose its text.
    }
}
std::string LocalDiagnosticLog::snapshot() const
{
    const std::scoped_lock lock{mutex_};
    const auto lease = acquireWriterLease(privateLogFolder_);
    const auto records = inspectRecords(privateLogFolder_);
    if (records.size() > limits_.maximumRetainedFileCount)
        throw winrt::hresult_invalid_argument();
    std::string result;
    std::uint64_t persistedTotal{};
    for (const auto &record : records)
    {
        persistedTotal += record.persistedLengthBytes;
        if (persistedTotal > limits_.maximumRetainedLengthBytes ||
            result.size() + record.sanitizedLine.size() > limits_.maximumRetainedLengthBytes)
            throw winrt::hresult_invalid_argument();
        result += record.sanitizedLine;
    }
    return result;
}
} // namespace jpg_spinner::diagnostics
