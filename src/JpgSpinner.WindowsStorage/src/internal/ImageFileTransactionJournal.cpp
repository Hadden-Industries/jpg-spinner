#define NOMINMAX
#include <windows.h>
#include "ImageFileTransactionJournal.h"
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.Security.Cryptography.Core.h>
#include <winrt/Windows.Storage.Search.h>
#include <winrt/Windows.Storage.Streams.h>
#include <array>
#include <charconv>
#include <cmath>
#include <format>
#include <string_view>
#include <stdexcept>
#include <utility>

namespace jpg_spinner::storage::internal
{
namespace
{
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Data::Json;
using namespace winrt::Windows::Security::Cryptography;
using namespace winrt::Windows::Security::Cryptography::Core;
using Record = ImageFileTransactionJournalRecord;
using State = ImageFileTransactionState;

constexpr std::uint32_t maximumGeneration = 6;
// Five extended-length Unicode paths plus their escaped native JSON forms fit
// within this bounded allocation. Never read arbitrary journal-sized input first.
constexpr std::uint32_t maximumJournalLengthBytes = 1024 * 1024;
constexpr std::array<std::wstring_view, 7> stateNames{
    L"TransactionInitialized",          L"StagedOutputWritten", L"StagedOutputHashVerified",
    L"VerifiedBackupCreated",           L"OutputCommitted",     L"OwnedStagingArtifactsCleaned",
    L"TransactionAbandonedBeforeCommit"};

[[noreturn]] void rejectRecord()
{
    throw winrt::hresult_invalid_argument();
}

winrt::hstring generationName(const std::uint32_t generation)
{
    return winrt::hstring{std::format(L"generation-{:06}.json", generation)};
}

template <typename Integer> Integer decimalInteger(const winrt::hstring &text)
{
    // from_chars is the standard integer consumer; round-trip equality enforces
    // this schema's canonical decimal form without inventing a number grammar.
    const auto bytes = winrt::to_string(text);
    Integer result{};
    const auto parsed = std::from_chars(bytes.data(), bytes.data() + bytes.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != bytes.data() + bytes.size() || std::to_string(result) != bytes)
        rejectRecord();
    return result;
}

winrt::hstring digestText(const domain::Sha256Digest &digest)
{
    const auto begin = reinterpret_cast<const std::uint8_t *>(digest.data());
    return CryptographicBuffer::EncodeToHexString(
        CryptographicBuffer::CreateFromByteArray(winrt::array_view<const std::uint8_t>{begin, begin + digest.size()}));
}

domain::Sha256Digest digestFromText(const winrt::hstring &text)
{
    if (text.size() != 64)
        rejectRecord();
    winrt::com_array<std::uint8_t> bytes;
    const auto buffer = CryptographicBuffer::DecodeFromHexString(text);
    CryptographicBuffer::CopyToByteArray(buffer, bytes);
    if (bytes.size() != 32 || CryptographicBuffer::EncodeToHexString(buffer) != text)
        rejectRecord();
    domain::Sha256Digest digest;
    for (std::uint32_t index = 0; index < digest.size(); ++index)
        digest[index] = static_cast<std::byte>(bytes[index]);
    return digest;
}

void validateIdentity(const std::wstring &identity)
{
    // FILE_ID_INFO is 8 bytes of volume identity and 16 bytes of file identity.
    // The native cryptographic buffer owns hexadecimal syntax validation.
    if (identity.size() != 48)
        rejectRecord();
    const auto buffer = CryptographicBuffer::DecodeFromHexString(identity);
    if (buffer.Length() != 24 || CryptographicBuffer::EncodeToHexString(buffer) != identity)
        rejectRecord();
}

void validateRecord(const Record &record)
{
    if (record.transactionIdentifier == winrt::guid{} || record.generation == 0 ||
        record.generation > maximumGeneration || static_cast<unsigned>(record.state) >= stateNames.size() ||
        (record.outputDisposition != domain::OutputDisposition::CreateCorrectedCopy &&
         record.outputDisposition != domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup) ||
        record.sourceRevision.encodedLengthBytes == 0 || record.outputLengthBytes == 0)
        rejectRecord();
    for (const auto *path : {&record.selectedRootPath, &record.sourcePath, &record.stagePath, &record.destinationPath})
        if (path->empty() || path->size() >= 32768 || path->find(L'\0') != std::wstring::npos)
            rejectRecord();
    for (const auto *identity : {&record.selectedRootIdentity, &record.sourceIdentity, &record.stageIdentity})
        validateIdentity(*identity);
    const bool hasBackup = !record.backupIdentity.empty();
    if (hasBackup != !record.backupPath.empty() || record.backupPath.size() >= 32768 ||
        record.backupPath.find(L'\0') != std::wstring::npos)
        rejectRecord();
    if (hasBackup)
        validateIdentity(record.backupIdentity);
    const bool replacement = record.outputDisposition == domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup;
    const auto stateIndex = static_cast<unsigned>(record.state);
    if (record.state == State::TransactionAbandonedBeforeCommit)
    {
        // Abandonment branches after one of the precommit observations; it is
        // not the seventh successful-commit milestone. A backup fact survives
        // exactly when abandonment follows the verified-backup generation.
        if (record.generation < 2 || record.generation > (replacement ? 5u : 4u) ||
            hasBackup != (replacement && record.generation == 5))
            rejectRecord();
        return;
    }
    if ((!replacement && (hasBackup || record.state == State::VerifiedBackupCreated)) ||
        (replacement && hasBackup != (stateIndex >= static_cast<unsigned>(State::VerifiedBackupCreated))))
        rejectRecord();
    const auto expectedGeneration = stateIndex + 1 - (!replacement && stateIndex >= 4 ? 1 : 0);
    if (record.generation != expectedGeneration)
        rejectRecord();
}

void validateSuccessor(const Record &prior, const Record &next)
{
    validateRecord(next);
    if (next.generation != prior.generation + 1)
        rejectRecord();
    const auto priorIndex = static_cast<unsigned>(prior.state);
    if (next.state == State::TransactionAbandonedBeforeCommit)
    {
        if (priorIndex >= static_cast<unsigned>(State::OutputCommitted))
            rejectRecord();
    }
    else
    {
        // Exact graph adjacency, including the copy-only skipped backup, is
        // required now that generations are also shared by a terminal branch.
        const auto expectedState = priorIndex + 1 +
                                   (prior.outputDisposition == domain::OutputDisposition::CreateCorrectedCopy &&
                                            prior.state == State::StagedOutputHashVerified
                                        ? 1u
                                        : 0u);
        if (expectedState > static_cast<unsigned>(State::OwnedStagingArtifactsCleaned) ||
            static_cast<unsigned>(next.state) != expectedState)
            rejectRecord();
    }
    auto expected = prior;
    expected.generation = next.generation;
    expected.state = next.state;
    // Only one transition adds backup identity. All other artifact facts remain
    // immutable, including after commit; recovery rechecks them, not path aliases.
    if (next.state == State::VerifiedBackupCreated)
    {
        expected.backupPath = next.backupPath;
        expected.backupIdentity = next.backupIdentity;
    }
    if (expected != next)
        rejectRecord();
}

winrt::hstring serialize(const Record &record, const domain::Sha256Digest &predecessor)
{
    validateRecord(record);
    // Version 1 is an exact 20-element scalar tuple. No objects are accepted at
    // any position: JsonObject's documented last-duplicate-wins behavior cannot
    // erase contradictory evidence. JSON syntax remains Windows.Data.Json's job.
    JsonArray values;
    values.Append(JsonValue::CreateNumberValue(1));
    values.Append(JsonValue::CreateStringValue(winrt::to_hstring(record.transactionIdentifier)));
    values.Append(JsonValue::CreateNumberValue(record.generation));
    const auto append = [&](const std::wstring_view value) {
        values.Append(JsonValue::CreateStringValue(winrt::hstring{value}));
    };
    append(winrt::hstring{stateNames[static_cast<unsigned>(record.state)]});
    append(record.outputDisposition == domain::OutputDisposition::CreateCorrectedCopy
               ? L"CreateCorrectedCopy"
               : L"ReplaceOriginalWithVerifiedBackup");
    append(record.selectedRootPath);
    append(record.selectedRootIdentity);
    append(record.sourcePath);
    append(record.sourceIdentity);
    append(winrt::to_hstring(record.sourceRevision.encodedLengthBytes));
    append(winrt::to_hstring(record.sourceRevision.lastWriteTimeUtc.time_since_epoch().count()));
    append(digestText(record.sourceRevision.encodedSha256));
    append(record.stagePath);
    append(record.stageIdentity);
    append(winrt::to_hstring(record.outputLengthBytes));
    append(digestText(record.outputSha256));
    append(record.backupPath);
    append(record.backupIdentity);
    append(record.destinationPath);
    append(digestText(predecessor));
    return values.Stringify();
}

Record parse(const winrt::hstring &text, domain::Sha256Digest &predecessor)
{
    JsonArray values{nullptr};
    if (!JsonArray::TryParse(text, values) || values.Size() != 20 ||
        values.GetAt(0).ValueType() != JsonValueType::Number || values.GetNumberAt(0) != 1 ||
        values.GetAt(2).ValueType() != JsonValueType::Number)
        rejectRecord();
    const auto generation = values.GetNumberAt(2);
    if (!std::isfinite(generation) || generation < 1 || generation > maximumGeneration ||
        generation != static_cast<std::uint32_t>(generation))
        rejectRecord();
    // GetStringAt rejects null, Boolean, number, object and array values rather
    // than coercing them. All unbounded-width integers are decimal string values.
    winrt::guid identifier;
    try
    {
        // Reuse C++/WinRT's GUID consumer. Its string constructor throws
        // std::invalid_argument, unlike the native JSON projection's errors.
        identifier = winrt::guid{std::wstring_view{values.GetStringAt(1)}};
    }
    catch (const std::invalid_argument &)
    {
        // Normalize malformed record data into the journal's native rejection
        // contract so a valid earlier generation remains usable.
        rejectRecord();
    }
    const auto stateText = values.GetStringAt(3);
    auto state = State::TransactionInitialized;
    bool recognizedState = false;
    for (unsigned index = 0; index < stateNames.size(); ++index)
        if (stateText == stateNames[index])
        {
            state = static_cast<State>(index);
            recognizedState = true;
            break;
        }
    if (!recognizedState)
        rejectRecord();
    const auto dispositionText = values.GetStringAt(4);
    if (dispositionText != L"CreateCorrectedCopy" && dispositionText != L"ReplaceOriginalWithVerifiedBackup")
        rejectRecord();
    const auto disposition = dispositionText == L"CreateCorrectedCopy"
                                 ? domain::OutputDisposition::CreateCorrectedCopy
                                 : domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup;
    using RevisionClock = decltype(domain::SourceFileRevision::lastWriteTimeUtc);
    Record record{identifier,
                  static_cast<std::uint32_t>(generation),
                  state,
                  disposition,
                  std::wstring{values.GetStringAt(5)},
                  std::wstring{values.GetStringAt(6)},
                  std::wstring{values.GetStringAt(7)},
                  std::wstring{values.GetStringAt(8)},
                  {decimalInteger<std::uint64_t>(values.GetStringAt(9)),
                   RevisionClock{RevisionClock::duration{decimalInteger<std::int64_t>(values.GetStringAt(10))}},
                   digestFromText(values.GetStringAt(11))},
                  std::wstring{values.GetStringAt(12)},
                  std::wstring{values.GetStringAt(13)},
                  decimalInteger<std::uint64_t>(values.GetStringAt(14)),
                  digestFromText(values.GetStringAt(15)),
                  std::wstring{values.GetStringAt(16)},
                  std::wstring{values.GetStringAt(17)},
                  std::wstring{values.GetStringAt(18)}};
    predecessor = digestFromText(values.GetStringAt(19));
    validateRecord(record);
    return record;
}

struct PublishedGeneration final
{
    Record record;
    domain::Sha256Digest encodedSha256;
    domain::Sha256Digest predecessorSha256;
    FILE_ID_INFO identity;
};

PublishedGeneration readGeneration(const StorageFile &file, const FileTransactionOperations &operations,
                                   const std::filesystem::path &canonicalParent)
{
    const auto proof = inspectItem(file.Path(), false);
    if (proof.canonicalPath.parent_path() != canonicalParent)
        rejectRecord();
    const auto stream = operations.openJournalGeneration(file, FileAccessMode::Read);
    const auto length = stream.Size();
    if (length == 0 || length > maximumJournalLengthBytes)
        rejectRecord();
    const auto bytes = stream
                           .ReadAsync(Buffer{static_cast<std::uint32_t>(length)}, static_cast<std::uint32_t>(length),
                                      InputStreamOptions::None)
                           .get();
    if (bytes.Length() != length || stream.Size() != length)
        rejectRecord();
    const auto text = CryptographicBuffer::ConvertBinaryToString(BinaryStringEncoding::Utf8, bytes);
    domain::Sha256Digest predecessor;
    const auto record = parse(text, predecessor);
    const auto digest = HashAlgorithmProvider::OpenAlgorithm(HashAlgorithmNames::Sha256()).HashData(bytes);
    operations.closeJournalGeneration(stream);
    return {record, digestFromText(CryptographicBuffer::EncodeToHexString(digest)), predecessor, proof.identity};
}

std::optional<PublishedGeneration> readChain(const StorageFolder &folder, const winrt::guid &identifier,
                                             const FileTransactionOperations &operations,
                                             const StorageItemProof &folderProof)
{
    const auto observedFolder = inspectItem(folder.Path(), true);
    if (!isSameIdentity(observedFolder.identity, folderProof.identity))
        rejectRecord();
    // Bound enumeration too. A transaction has at most six published generations;
    // repeated failures cannot create an unbounded parse/allocation workload.
    const auto files =
        folder.GetFilesAsync(winrt::Windows::Storage::Search::CommonFileQuery::DefaultQuery, 0, 33).get();
    if (files.Size() > 32)
        rejectRecord();
    for (const auto &file : files)
    {
        const auto ownedName = file.Name();
        const std::wstring_view name{ownedName};
        if (name.starts_with(L".pending-"))
            continue;
        bool recognizedName = false;
        for (std::uint32_t generation = 1; generation <= maximumGeneration; ++generation)
            if (file.Name() == generationName(generation))
            {
                recognizedName = true;
                break;
            }
        if (!recognizedName)
            rejectRecord();
    }
    std::optional<PublishedGeneration> latest;
    for (std::uint32_t generation = 1; generation <= maximumGeneration; ++generation)
    {
        const auto item = folder.TryGetItemAsync(generationName(generation)).get();
        if (!item)
        {
            // Missing predecessors cannot legitimize a later published record.
            for (auto later = generation + 1; later <= maximumGeneration; ++later)
                if (folder.TryGetItemAsync(generationName(later)).get())
                    rejectRecord();
            break;
        }
        try
        {
            const auto file = item.as<StorageFile>();
            auto candidate = readGeneration(file, operations, folderProof.canonicalPath);
            if (candidate.record.transactionIdentifier != identifier || candidate.record.generation != generation ||
                candidate.predecessorSha256 != (latest ? latest->encodedSha256 : domain::Sha256Digest{}))
                rejectRecord();
            if (latest)
                validateSuccessor(latest->record, candidate.record);
            latest = std::move(candidate);
        }
        catch (const winrt::hresult_error &)
        {
            if (!latest)
                throw;
            // A damaged newer generation does not rewrite the retained valid
            // prefix. Recovery must still reconcile all unexplained artifacts.
            break;
        }
    }
    return latest;
}
} // namespace

ImageFileTransactionJournal::ImageFileTransactionJournal(StorageFolder transactionFolder,
                                                         winrt::guid transactionIdentifier)
    : folder_(std::move(transactionFolder)), transactionIdentifier_(transactionIdentifier),
      folderProof_(inspectItem(folder_.Path(), true))
{
    if (!folder_ || transactionIdentifier_ == winrt::guid{})
        rejectRecord();
}

void ImageFileTransactionJournal::publish(const Record &record, const FileTransactionOperations &operations)
{
    const auto prior = readChain(folder_, transactionIdentifier_, operations, folderProof_);
    validateRecord(record);
    if (record.transactionIdentifier != transactionIdentifier_)
        rejectRecord();
    if (prior)
        validateSuccessor(prior->record, record);
    else if (record.generation != 1)
        rejectRecord();
    if (folder_.TryGetItemAsync(generationName(record.generation)).get())
        rejectRecord();
    const auto text = serialize(record, prior ? prior->encodedSha256 : domain::Sha256Digest{});
    const auto bytes = CryptographicBuffer::ConvertStringToBinary(text, BinaryStringEncoding::Utf8);
    if (bytes.Length() > maximumJournalLengthBytes)
        rejectRecord();
    const auto expectedEncodedSha256 = digestFromText(CryptographicBuffer::EncodeToHexString(
        HashAlgorithmProvider::OpenAlgorithm(HashAlgorithmNames::Sha256()).HashData(bytes)));
    winrt::guid pendingIdentifier;
    winrt::check_hresult(CoCreateGuid(reinterpret_cast<GUID *>(&pendingIdentifier)));
    const auto pending =
        operations.createPendingJournalGeneration(folder_, L".pending-" + winrt::to_hstring(pendingIdentifier));
    {
        const auto proof = inspectItem(pending.Path(), false);
        if (proof.canonicalPath.parent_path() != folderProof_.canonicalPath)
            rejectRecord();
        const auto stream = operations.openJournalGeneration(pending, FileAccessMode::ReadWrite);
        try
        {
            if (operations.writeJournalBytes(stream, bytes) != bytes.Length() || !operations.flushJournalBytes(stream))
                throw winrt::hresult_error{E_FAIL};
            operations.closeJournalGeneration(stream);
        }
        catch (...)
        {
            try
            {
                stream.Close();
            }
            catch (...)
            {
            }
            throw;
        }
    }
    const auto written = readGeneration(pending, operations, folderProof_.canonicalPath);
    // Parsed facts alone do not bind the predecessor or even the exact bytes we
    // wrote. Detect changes to either before a pending record becomes evidence.
    if (written.record != record || written.encodedSha256 != expectedEncodedSha256 ||
        written.predecessorSha256 != (prior ? prior->encodedSha256 : domain::Sha256Digest{}))
        rejectRecord();
    operations.publishJournalGeneration(pending, generationName(record.generation));
    const auto published = readGeneration(folder_.GetFileAsync(generationName(record.generation)).get(), operations,
                                          folderProof_.canonicalPath);
    if (published.record != record || published.encodedSha256 != written.encodedSha256 ||
        !isSameIdentity(published.identity, written.identity))
        rejectRecord();
}

std::optional<Record> ImageFileTransactionJournal::readLatest(const FileTransactionOperations &operations) const
{
    const auto latest = readChain(folder_, transactionIdentifier_, operations, folderProof_);
    return latest ? std::optional<Record>{latest->record} : std::nullopt;
}
} // namespace jpg_spinner::storage::internal
