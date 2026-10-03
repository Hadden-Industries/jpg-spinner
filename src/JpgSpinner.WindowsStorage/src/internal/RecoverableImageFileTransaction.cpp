#define NOMINMAX
#include <windows.h>
#include <fileapifromapp.h>
#include <pathcch.h>
#include "RecoverableImageFileTransaction.h"
#include "StorageItemProof.h"
#include "ImageFileTransactionJournal.h"
#include <winrt/Windows.Storage.Search.h>
#include "CorrectedCopyPathPolicy.h"
#include <jpg_spinner/storage/SourceFileRevisionCalculator.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.Storage.Streams.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <new>
#include <limits>
#include <string>
#include <stdexcept>
#include <utility>

#pragma comment(lib, "windowsapp.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "pathcch.lib")

namespace jpg_spinner::storage::internal
{
namespace
{
using namespace winrt::Windows::Storage;
using domain::ImageProcessingErrorCode;
using domain::ImageProcessingStage;
using TransactionResult = domain::ImageProcessingResult<CommittedImageFile>;

TransactionResult failure(ImageProcessingErrorCode code, ImageProcessingStage stage,
                          domain::NativeErrorProjection nativeError = {})
{
    return TransactionResult::failure({code, stage, nativeError});
}

ImageProcessingErrorCode translateFailure(winrt::hresult error, ImageProcessingStage stage) noexcept
{
    if (error == HRESULT_FROM_WIN32(ERROR_DISK_FULL) || error == HRESULT_FROM_WIN32(ERROR_HANDLE_DISK_FULL))
        return ImageProcessingErrorCode::InsufficientStorageSpace;
    if (error == E_OUTOFMEMORY)
        return ImageProcessingErrorCode::WorkingMemoryAllocationFailed;
    if (error == E_ABORT || error == HRESULT_FROM_WIN32(ERROR_CANCELLED))
        return ImageProcessingErrorCode::Cancelled;
    if (stage == ImageProcessingStage::TransactionRecoverabilityPreflight)
        return ImageProcessingErrorCode::StorageProviderRecoveryContractNotEstablished;
    if (stage == ImageProcessingStage::JournalStoreLeaseAcquisition)
        return error == HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION)
                   ? ImageProcessingErrorCode::JournalStoreBusy
                   : ImageProcessingErrorCode::JournalPersistenceFailed;
    if (stage == ImageProcessingStage::TransactionRecovery)
        return ImageProcessingErrorCode::RecoveryConflict;
    if (stage == ImageProcessingStage::SourceRevisionRevalidation)
    {
        if (error == E_ACCESSDENIED)
            return ImageProcessingErrorCode::SourceAccessDenied;
        if (error == HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION))
            return ImageProcessingErrorCode::SourceSharingViolation;
        if (error == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) || error == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND))
            return ImageProcessingErrorCode::SourceChangedAfterAnalysis;
        return ImageProcessingErrorCode::SourceRevisionCaptureFailed;
    }
    if (error == HRESULT_FROM_WIN32(ERROR_FILE_EXISTS) || error == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS))
        return ImageProcessingErrorCode::OutputRelativePathCollision;
    if (stage == ImageProcessingStage::CorrectedCopyDestinationCreation)
        return error == E_ACCESSDENIED ? ImageProcessingErrorCode::DestinationAccessDenied
                                       : ImageProcessingErrorCode::CorrectedCopyDestinationCreationFailed;
    if (stage == ImageProcessingStage::StagingFileCreation)
        return ImageProcessingErrorCode::StagingFileCreationFailed;
    if (stage == ImageProcessingStage::StagingWrite)
        return ImageProcessingErrorCode::StagingWriteFailed;
    if (stage == ImageProcessingStage::StagingFlush)
        return ImageProcessingErrorCode::StagingFlushFailed;
    if (stage == ImageProcessingStage::StagingClose)
        return ImageProcessingErrorCode::StagingCloseFailed;
    if (stage == ImageProcessingStage::StagedOutputVerification)
        return ImageProcessingErrorCode::StagedOutputVerificationFailed;
    if (stage == ImageProcessingStage::BackupCreation)
        return ImageProcessingErrorCode::BackupCreationFailed;
    if (stage == ImageProcessingStage::BackupVerification)
        return ImageProcessingErrorCode::BackupVerificationFailed;
    if (stage == ImageProcessingStage::OriginalReplacement)
        return ImageProcessingErrorCode::OriginalReplacementFailed;
    if (stage == ImageProcessingStage::JournalPersistence)
        return ImageProcessingErrorCode::JournalPersistenceFailed;
    return ImageProcessingErrorCode::CorrectedCopyCommitFailed;
}

/// Owns exactly one exclusively created stage. Cleanup is idempotent and uses
/// handle identity plus canonical parent containment before deletion. If proof
/// is unavailable, leave the artifact and report conflict rather than deleting
/// a same-named file belonging to somebody else.
class OwnedStage final
{
  public:
    StorageFile file{nullptr};
    std::filesystem::path canonicalParent;
    std::filesystem::path canonicalStagePath;
    std::optional<FILE_ID_INFO> identity;
    bool committed{false};

    void remove(const bool rejectMissingStage = false)
    {
        if (!file || committed)
            return;
        try
        {
            const auto proof = inspectItem(file.Path(), false, FILE_READ_ATTRIBUTES | DELETE);
            if (!identity || !isSameIdentity(proof.identity, *identity) || proof.canonicalPath != canonicalStagePath ||
                proof.canonicalPath.parent_path() != canonicalParent)
                throw winrt::hresult_invalid_argument();
            // Delete by the inspected handle, not by a path that could be
            // replaced between authorization and DeleteAsync. No recursive delete.
            FILE_DISPOSITION_INFO disposition{TRUE};
            winrt::check_bool(
                SetFileInformationByHandle(proof.handle.get(), FileDispositionInfo, &disposition, sizeof(disposition)));
            file = nullptr;
        }
        catch (const winrt::hresult_error &error)
        {
            // Before commit, absence is idempotent cleanup. After a failed move,
            // absence could mean a completed rename whose projection stayed stale.
            // Do not convert that uncertain outcome into a definite commit failure.
            if (error.code() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) && !rejectMissingStage)
                file = nullptr;
            else
                throw;
        }
    }
};
} // namespace

TransactionResult RecoverableImageFileTransaction::execute(ImageFileTransactionBatch &batch,
                                                           const ImageFileTransactionRequest &request,
                                                           const domain::ValidatedJpegOutput &validatedOutput,
                                                           const std::stop_token cancellationToken,
                                                           const FileTransactionOperations &operations)
{
    OwnedStage stage;
    std::unique_ptr<ImageFileTransactionJournal> journal;
    std::optional<ImageFileTransactionJournalRecord> journalRecord;
    // Keep exclusion through exception cleanup and terminal publication too.
    // A lease local to the try block would release before its catch handlers.
    winrt::handle journalStoreLease;
    auto phase = ImageProcessingStage::StagingFileCreation;
    const auto cleanupAndAbandon = [&](const bool rejectMissingStage = false) {
        std::optional<StorageItemProof> originalIdentityProof;
        if (phase == ImageProcessingStage::OriginalReplacement && !stage.committed)
        {
            // A failed native replacement does not specify the original's
            // resulting state. Stage presence alone cannot close that uncertainty.
            // Reopen the recorded source path, verify its full reviewed revision
            // and file ID, and retain the metadata handle through cleanup/closure.
            // If any observation fails, preserve the stage and verified backup
            // for restart reconciliation; do not publish an abandonment guess.
            if (!journalRecord)
                throw winrt::hresult_invalid_argument();
            const auto original = StorageFile::GetFileFromPathAsync(journalRecord->sourcePath).get();
            originalIdentityProof.emplace(inspectItem(original.Path(), false));
            const auto revision = operations.captureSourceRevision(original, {});
            if (!revision.valueIfPresent() || *revision.valueIfPresent() != request.expectedSourceRevision ||
                storageItemIdentityText(originalIdentityProof->identity) != journalRecord->sourceIdentity)
                throw winrt::hresult_invalid_argument();
        }
        stage.remove(rejectMissingStage);
        if (stage.committed || !journal || !journalRecord)
            return;
        const auto persisted = journal->readLatest(operations);
        // An unpublished first generation is not trustworthy transaction proof.
        // Preserve its folder/pending evidence rather than inventing closure.
        if (!persisted || persisted->state == ImageFileTransactionState::TransactionAbandonedBeforeCommit)
            return;
        if (static_cast<unsigned>(persisted->state) >=
            static_cast<unsigned>(ImageFileTransactionState::OutputCommitted))
            throw winrt::hresult_invalid_argument();
        auto abandoned = *persisted;
        ++abandoned.generation;
        abandoned.state = ImageFileTransactionState::TransactionAbandonedBeforeCommit;
        journal->publish(abandoned, operations);
    };
    try
    {
        const auto &selectedSourceRoot = batch.selectedSourceRoot;
        auto &batchFolder = batch.correctedCopyFolder;
        const auto &batchDirectoryName = batch.directoryName;
        // The calculator checks explicit COM lifetime ownership too; reject
        // invalid workers before creating any output directories or stage.
        APTTYPE apartmentType;
        APTTYPEQUALIFIER qualifier;
        const auto apartmentResult = CoGetApartmentType(&apartmentType, &qualifier);
        if (FAILED(apartmentResult) || apartmentType != APTTYPE_MTA)
            return failure(ImageProcessingErrorCode::StagingFileCreationFailed, phase,
                           domain::WindowsHResult{FAILED(apartmentResult) ? apartmentResult : RPC_E_WRONG_THREAD});
        if (qualifier == APTTYPEQUALIFIER_IMPLICIT_MTA)
            return failure(ImageProcessingErrorCode::StagingFileCreationFailed, phase,
                           domain::WindowsHResult{CO_E_NOTINITIALIZED});
        if (cancellationToken.stop_requested())
            return failure(ImageProcessingErrorCode::Cancelled, phase);
        if (validatedOutput.encodedBytes().empty())
            return failure(ImageProcessingErrorCode::OutputValidationFailed, ImageProcessingStage::OutputValidation,
                           domain::WindowsHResult{E_INVALIDARG});
        if (!selectedSourceRoot || !batch.journalStore || !request.sourceFile ||
            (request.outputDisposition != domain::OutputDisposition::CreateCorrectedCopy &&
             request.outputDisposition != domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup))
            return failure(ImageProcessingErrorCode::StagingFileCreationFailed, phase,
                           domain::WindowsHResult{E_INVALIDARG});

        // Refuse unsupported identity/rename contracts before output, stage,
        // backup or journal creation. Query the granted native folder handle;
        // do not infer support from path syntax or a successful generic hash.
        // Reparse/containment diagnostics retain their established source phase.
        phase = ImageProcessingStage::SourceRevisionRevalidation;
        const auto qualifiedSourceFileSystem = hasQualifiedRecoveryFileSystem(selectedSourceRoot.Path());
        phase = ImageProcessingStage::TransactionRecoverabilityPreflight;
        if (!qualifiedSourceFileSystem || !hasQualifiedRecoveryFileSystem(batch.journalStore.Path()))
            return failure(ImageProcessingErrorCode::StorageProviderRecoveryContractNotEstablished, phase);

        // Failure to inspect the reviewed source/root is a source-side failure,
        // not a stage-creation failure. The authoritative content recheck still
        // occurs after stage hashing, immediately before commit.
        phase = ImageProcessingStage::SourceRevisionRevalidation;
        const auto rootProof = inspectItem(selectedSourceRoot.Path(), true);
        if (batch.selectedRootIdentity && !isSameIdentity(rootProof.identity, *batch.selectedRootIdentity))
            return failure(ImageProcessingErrorCode::RecoveryConflict, ImageProcessingStage::TransactionRecovery);
        auto sourceProof = inspectItem(request.sourceFile.Path(), false);
        const auto relativePath = sourceProof.canonicalPath.lexically_relative(rootProof.canonicalPath);
        if (relativePath.empty() || relativePath.is_absolute() || relativePath == ".")
            throw winrt::hresult_invalid_argument();
        for (const auto &component : relativePath)
            if (component == ".." || component == ".")
                throw winrt::hresult_invalid_argument();
        // Resolve every source ancestor below the selected root, rejecting
        // reparse-point traversal instead of just validating the final file.
        auto sourceParent = request.sourceFile.GetParentAsync().get();
        for (auto parentPath = relativePath.parent_path(); !parentPath.empty(); parentPath = parentPath.parent_path())
        {
            const auto parentProof = inspectItem(sourceParent.Path(), true);
            if (parentProof.canonicalPath != rootProof.canonicalPath / parentPath)
                throw winrt::hresult_invalid_argument();
            sourceParent = sourceParent.GetParentAsync().get();
        }
        if (!sourceParent || inspectItem(sourceParent.Path(), true).canonicalPath != rootProof.canonicalPath)
            throw winrt::hresult_invalid_argument();
        batch.selectedRootIdentity = rootProof.identity;

        phase = ImageProcessingStage::JournalPersistence;
        const auto journalStoreProof = inspectItem(batch.journalStore.Path(), true);
        phase = ImageProcessingStage::JournalStoreLeaseAcquisition;
        journalStoreLease = acquireJournalStoreLease(batch.journalStore.Path(), journalStoreProof);

        phase = ImageProcessingStage::CorrectedCopyDestinationCreation;
        if (!batchFolder)
        {
            const auto outputRoot = operations.createCorrectedCopyOutputRoot(selectedSourceRoot);
            const auto outputProof = inspectItem(outputRoot.Path(), true);
            if (!isCorrectedCopyOutputRootPath(outputProof.canonicalPath, rootProof.canonicalPath))
                throw winrt::hresult_invalid_argument();
            batchFolder = operations.createBatchDestination(outputRoot, winrt::hstring{batchDirectoryName});
            // Capture identity when the exclusive creation succeeds. Later
            // same-path folders cannot inherit this batch's ownership.
            batch.correctedCopyFolderIdentity = inspectItem(batchFolder.Path(), true).identity;
        }
        const auto batchProof = inspectItem(batchFolder.Path(), true);
        if (!batch.correctedCopyFolderIdentity ||
            !isSameIdentity(batchProof.identity, *batch.correctedCopyFolderIdentity))
            return failure(ImageProcessingErrorCode::RecoveryConflict, ImageProcessingStage::TransactionRecovery);
        if (batchProof.canonicalPath.parent_path() != rootProof.canonicalPath / L"JPG Spinner Output" ||
            batchProof.canonicalPath.filename() != batchDirectoryName)
            throw winrt::hresult_invalid_argument();
        auto destinationFolder = batchFolder;
        auto canonicalDestination = batchProof.canonicalPath;
        for (const auto &component : relativePath.parent_path())
        {
            destinationFolder =
                operations.createRelativeDestination(destinationFolder, winrt::hstring{component.wstring()});
            canonicalDestination /= component;
            if (inspectItem(destinationFolder.Path(), true).canonicalPath != canonicalDestination)
                throw winrt::hresult_invalid_argument();
        }
        const auto destinationProof = inspectItem(destinationFolder.Path(), true);
        phase = ImageProcessingStage::StagingFileCreation;
        winrt::guid transactionIdentifier;
        winrt::check_hresult(CoCreateGuid(reinterpret_cast<GUID *>(&transactionIdentifier)));
        const auto stageName =
            L".jpg-spinner-staged-" + std::wstring{winrt::to_hstring(transactionIdentifier)} + L".jpg";
        stage.file = operations.createStage(destinationFolder, winrt::hstring{stageName});
        stage.canonicalParent = destinationProof.canonicalPath;
        {
            const auto createdProof = inspectItem(stage.file.Path(), false);
            if (createdProof.canonicalPath.parent_path() != stage.canonicalParent)
                throw winrt::hresult_invalid_argument();
            stage.identity = createdProof.identity;
            stage.canonicalStagePath = createdProof.canonicalPath;
        }
        phase = ImageProcessingStage::JournalPersistence;
        const auto journalFolder =
            operations.createTransactionJournalFolder(batch.journalStore, winrt::to_hstring(transactionIdentifier));
        if (inspectItem(journalFolder.Path(), true).canonicalPath.parent_path() != journalStoreProof.canonicalPath)
            throw winrt::hresult_invalid_argument();
        journal = std::make_unique<ImageFileTransactionJournal>(journalFolder, transactionIdentifier);
        journalRecord.emplace(ImageFileTransactionJournalRecord{
            transactionIdentifier, 1, ImageFileTransactionState::TransactionInitialized, request.outputDisposition,
            std::wstring{selectedSourceRoot.Path()}, storageItemIdentityText(rootProof.identity),
            std::wstring{request.sourceFile.Path()}, storageItemIdentityText(sourceProof.identity),
            request.expectedSourceRevision, std::wstring{stage.file.Path()}, storageItemIdentityText(*stage.identity),
            validatedOutput.encodedBytes().size(), validatedOutput.encodedSha256(), L"", L"",
            request.outputDisposition == domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup
                ? std::wstring{request.sourceFile.Path()}
                : (std::filesystem::path{destinationFolder.Path().c_str()} / relativePath.filename()).wstring()});
        journal->publish(*journalRecord, operations);
        const auto publishState = [&](const ImageFileTransactionState state) {
            phase = ImageProcessingStage::JournalPersistence;
            ++journalRecord->generation;
            journalRecord->state = state;
            journal->publish(*journalRecord, operations);
        };
        const auto finishCommit = [&]() {
            // After native completion no cancellation observation may invent an
            // untouched original. Verify actual committed bytes without a token.
            stage.committed = true;
            const auto committed = operations.captureCommittedRevision(stage.file);
            if (!committed.valueIfPresent() ||
                committed.valueIfPresent()->encodedLengthBytes != validatedOutput.encodedBytes().size() ||
                committed.valueIfPresent()->encodedSha256 != validatedOutput.encodedSha256() || !stage.identity ||
                !isSameIdentity(inspectItem(stage.file.Path(), false).identity, *stage.identity))
                return failure(ImageProcessingErrorCode::RecoveryConflict, ImageProcessingStage::TransactionRecovery,
                               committed.errorIfPresent() ? committed.errorIfPresent()->nativeErrorProjection
                                                          : domain::NativeErrorProjection{});
            publishState(ImageFileTransactionState::OutputCommitted);
            publishState(ImageFileTransactionState::OwnedStagingArtifactsCleaned);
            return TransactionResult::success({stage.file});
        };
        phase = ImageProcessingStage::StagingWrite;
        {
            const auto stream = operations.openStagedOutput(stage.file);
            try
            {
                constexpr std::size_t chunkCapacity = 64 * 1024;
                const auto bytes = validatedOutput.encodedBytes();
                for (std::size_t offset = 0; offset < bytes.size();)
                {
                    if (cancellationToken.stop_requested())
                        throw winrt::hresult_error{HRESULT_FROM_WIN32(ERROR_CANCELLED)};
                    const auto length = std::min(chunkCapacity, bytes.size() - offset);
                    const auto begin = reinterpret_cast<const std::uint8_t *>(bytes.data() + offset);
                    const auto buffer =
                        winrt::Windows::Security::Cryptography::CryptographicBuffer::CreateFromByteArray(
                            winrt::array_view<const std::uint8_t>{begin, begin + length});
                    if (operations.writeStagedBytes(stream, buffer) != length)
                        throw winrt::hresult_error{E_FAIL};
                    offset += length;
                }
                phase = ImageProcessingStage::StagingFlush;
                if (!operations.flushStagedBytes(stream))
                    throw winrt::hresult_error{E_FAIL};
                phase = ImageProcessingStage::StagingClose;
                operations.closeStagedOutput(stream);
            }
            catch (...)
            {
                // Explicitly close before owned cleanup, also on write/flush/
                // cancellation failure. A failed close cannot authorize commit.
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
        publishState(ImageFileTransactionState::StagedOutputWritten);
        phase = ImageProcessingStage::StagedOutputVerification;
        const auto stagedRevision = operations.captureStagedRevision(stage.file, cancellationToken);
        if (!stagedRevision.valueIfPresent())
        {
            cleanupAndAbandon();
            const auto &error = *stagedRevision.errorIfPresent();
            const auto code = error.code == ImageProcessingErrorCode::Cancelled ||
                                      error.code == ImageProcessingErrorCode::WorkingMemoryAllocationFailed
                                  ? error.code
                                  : ImageProcessingErrorCode::StagedOutputVerificationFailed;
            // An unavailable digest is not a mismatching digest. Preserve the
            // native cause without converting all failures to an invented E_FAIL.
            return failure(code, phase, error.nativeErrorProjection);
        }
        if (stagedRevision.valueIfPresent()->encodedLengthBytes != validatedOutput.encodedBytes().size() ||
            stagedRevision.valueIfPresent()->encodedSha256 != validatedOutput.encodedSha256())
        {
            cleanupAndAbandon();
            return failure(ImageProcessingErrorCode::StagedOutputHashMismatch, phase);
        }
        {
            const auto verifiedProof = inspectItem(stage.file.Path(), false);
            if (!isSameIdentity(verifiedProof.identity, *stage.identity) ||
                verifiedProof.canonicalPath.parent_path() != stage.canonicalParent)
                throw winrt::hresult_invalid_argument();
        }
        publishState(ImageFileTransactionState::StagedOutputHashVerified);
        phase = ImageProcessingStage::SourceRevisionRevalidation;
        const auto sourceRevision = operations.captureSourceRevision(request.sourceFile, cancellationToken);
        if (!sourceRevision.valueIfPresent())
        {
            cleanupAndAbandon();
            return failure(sourceRevision.errorIfPresent()->code, phase,
                           sourceRevision.errorIfPresent()->nativeErrorProjection);
        }
        if (*sourceRevision.valueIfPresent() != request.expectedSourceRevision)
        {
            cleanupAndAbandon();
            return failure(ImageProcessingErrorCode::SourceChangedAfterAnalysis, phase);
        }
        if (request.outputDisposition == domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup)
        {
            phase = ImageProcessingStage::BackupCreation;
            if (!batch.backupFolder)
            {
                const auto backupRoot = operations.createBackupRoot(selectedSourceRoot);
                if (inspectItem(backupRoot.Path(), true).canonicalPath !=
                    rootProof.canonicalPath / L"JPG Spinner Backups")
                    throw winrt::hresult_invalid_argument();
                batch.backupFolder = operations.createBatchDestination(backupRoot, winrt::hstring{batchDirectoryName});
                batch.backupFolderIdentity = inspectItem(batch.backupFolder.Path(), true).identity;
            }
            const auto backupBatchProof = inspectItem(batch.backupFolder.Path(), true);
            if (!batch.backupFolderIdentity ||
                !isSameIdentity(backupBatchProof.identity, *batch.backupFolderIdentity) ||
                backupBatchProof.canonicalPath != rootProof.canonicalPath / L"JPG Spinner Backups" / batchDirectoryName)
                throw winrt::hresult_invalid_argument();
            auto backupDestination = batch.backupFolder;
            auto canonicalBackupParent = backupBatchProof.canonicalPath;
            for (const auto &component : relativePath.parent_path())
            {
                backupDestination =
                    operations.createRelativeDestination(backupDestination, winrt::hstring{component.wstring()});
                canonicalBackupParent /= component;
                if (inspectItem(backupDestination.Path(), true).canonicalPath != canonicalBackupParent)
                    throw winrt::hresult_invalid_argument();
            }
            const auto backupParentProof = inspectItem(backupDestination.Path(), true);
            const auto backup = operations.copySourceToBackup(request.sourceFile, backupDestination,
                                                              winrt::hstring{relativePath.filename().wstring()});
            // Even an incomplete/unverified copy is retained on failure. Backup
            // deletion is never rollback, and the original is still untouched.
            const auto backupProof = inspectItem(backup.Path(), false);
            if (backupProof.canonicalPath.parent_path() != backupParentProof.canonicalPath ||
                isSameIdentity(backupProof.identity, sourceProof.identity))
                throw winrt::hresult_invalid_argument();
            phase = ImageProcessingStage::BackupVerification;
            {
                const auto stream =
                    backup.OpenAsync(FileAccessMode::ReadWrite, StorageOpenOptions::AllowOnlyReaders).get();
                try
                {
                    if (!operations.flushBackup(stream))
                        throw winrt::hresult_error{E_FAIL};
                    operations.closeBackup(stream);
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
            const auto backupRevision = operations.captureBackupRevision(backup, cancellationToken);
            if (!backupRevision.valueIfPresent())
            {
                cleanupAndAbandon();
                // A stopped bounded read is not evidence of a corrupt backup.
                // The retained copy remains untouched and replacement has not run.
                return failure(backupRevision.errorIfPresent()->code == ImageProcessingErrorCode::Cancelled
                                   ? ImageProcessingErrorCode::Cancelled
                                   : ImageProcessingErrorCode::BackupVerificationFailed,
                               phase, backupRevision.errorIfPresent()->nativeErrorProjection);
            }
            // Copy timestamps are provider metadata, not evidence of byte
            // preservation. Verify length and SHA-256 after close/reopen only.
            if (backupRevision.valueIfPresent()->encodedLengthBytes !=
                    request.expectedSourceRevision.encodedLengthBytes ||
                backupRevision.valueIfPresent()->encodedSha256 != request.expectedSourceRevision.encodedSha256 ||
                !isSameIdentity(inspectItem(backup.Path(), false).identity, backupProof.identity))
                throw winrt::hresult_error{E_FAIL};
            phase = ImageProcessingStage::SourceRevisionRevalidation;
            const auto afterBackup = operations.captureSourceRevision(request.sourceFile, cancellationToken);
            if (!afterBackup.valueIfPresent() || *afterBackup.valueIfPresent() != request.expectedSourceRevision)
            {
                cleanupAndAbandon();
                return afterBackup.valueIfPresent()
                           ? failure(ImageProcessingErrorCode::SourceChangedAfterAnalysis, phase)
                           : failure(afterBackup.errorIfPresent()->code, phase,
                                     afterBackup.errorIfPresent()->nativeErrorProjection);
            }
            if (cancellationToken.stop_requested())
            {
                cleanupAndAbandon();
                return failure(ImageProcessingErrorCode::Cancelled, phase);
            }
            journalRecord->backupPath = std::wstring{backup.Path()};
            journalRecord->backupIdentity = storageItemIdentityText(backupProof.identity);
            publishState(ImageFileTransactionState::VerifiedBackupCreated);
            const auto persistedBackup = journal->readLatest(operations);
            if (!persistedBackup || *persistedBackup != *journalRecord ||
                persistedBackup->state != ImageFileTransactionState::VerifiedBackupCreated)
                throw winrt::hresult_invalid_argument();
            // Publication is commit intent. Finish this file non-interruptibly,
            // while rechecking source bytes immediately before the native effect.
            // A failed recheck is refusal, never a blind replacement or rollback.
            phase = ImageProcessingStage::SourceRevisionRevalidation;
            const auto beforeReplacement = operations.captureSourceRevision(request.sourceFile, {});
            if (!beforeReplacement.valueIfPresent() ||
                *beforeReplacement.valueIfPresent() != request.expectedSourceRevision)
            {
                cleanupAndAbandon();
                return beforeReplacement.valueIfPresent()
                           ? failure(ImageProcessingErrorCode::SourceChangedAfterAnalysis, phase)
                           : failure(beforeReplacement.errorIfPresent()->code, phase,
                                     beforeReplacement.errorIfPresent()->nativeErrorProjection);
            }
            phase = ImageProcessingStage::OriginalReplacement;
            // The proof excludes rename substitution up to this point. Windows
            // Storage replacement requires the source metadata handle closed;
            // completion/hash observations do not promise immunity to hostile
            // external writes in the ensuing native operation's observation gap.
            sourceProof.handle.close();
            operations.replaceOriginalWithStage(stage.file, request.sourceFile);
            return finishCommit();
        }
        phase = ImageProcessingStage::CorrectedCopyCommit;
        if (cancellationToken.stop_requested())
        {
            cleanupAndAbandon();
            return failure(ImageProcessingErrorCode::Cancelled, phase);
        }
        // All stage read/write handles are closed. The digest proves the observed
        // closed bytes, not immunity to arbitrary external writers after close.
        // FailIfExists is essential: the default overload silently suffixes names.
        operations.moveStageToCorrectedCopy(stage.file, destinationFolder,
                                            winrt::hstring{relativePath.filename().wstring()});
        return finishCommit();
    }
    catch (const winrt::hresult_error &error)
    {
        try
        {
            cleanupAndAbandon(phase == ImageProcessingStage::CorrectedCopyCommit ||
                              phase == ImageProcessingStage::OriginalReplacement);
        }
        catch (const winrt::hresult_error &cleanupError)
        {
            return failure(ImageProcessingErrorCode::RecoveryConflict, ImageProcessingStage::TransactionRecovery,
                           domain::WindowsHResult{cleanupError.code().value});
        }
        catch (const std::bad_alloc &)
        {
            // Losing the cleanup proof under memory pressure is still conflict,
            // not permission to delete by a weaker pathname-only check.
            return failure(ImageProcessingErrorCode::RecoveryConflict, ImageProcessingStage::TransactionRecovery,
                           domain::WindowsHResult{E_OUTOFMEMORY});
        }
        if (stage.committed)
            return failure(ImageProcessingErrorCode::RecoveryConflict, ImageProcessingStage::TransactionRecovery,
                           domain::WindowsHResult{error.code().value});
        return failure(translateFailure(error.code(), phase), phase, domain::WindowsHResult{error.code().value});
    }
    catch (const std::bad_alloc &)
    {
        try
        {
            cleanupAndAbandon(phase == ImageProcessingStage::CorrectedCopyCommit ||
                              phase == ImageProcessingStage::OriginalReplacement);
        }
        catch (...)
        {
            return failure(ImageProcessingErrorCode::RecoveryConflict, ImageProcessingStage::TransactionRecovery);
        }
        // Memory pressure after native completion cannot relabel already changed
        // bytes as an ordinary precommit failure. Restart uses the retained proof.
        if (stage.committed)
            return failure(ImageProcessingErrorCode::RecoveryConflict, ImageProcessingStage::TransactionRecovery,
                           domain::WindowsHResult{E_OUTOFMEMORY});
        return failure(ImageProcessingErrorCode::WorkingMemoryAllocationFailed, phase);
    }
}
domain::ImageProcessingResult<ImageFileTransactionRecoverySummary> RecoverableImageFileTransaction::
    recoverIncompleteTransactions(ImageFileTransactionBatch &batch, std::stop_token cancellationToken,
                                  const FileTransactionOperations &operations)
{
    using RecoveryResult = domain::ImageProcessingResult<ImageFileTransactionRecoverySummary>;
    const auto conflict = [](domain::NativeErrorProjection native = {}) {
        return RecoveryResult::failure(
            {ImageProcessingErrorCode::RecoveryConflict, ImageProcessingStage::TransactionRecovery, std::move(native)});
    };
    const auto readFailure = [&](const domain::ImageProcessingError &error) {
        // Interrupted observation leaves every artifact intact. It does not
        // establish contradictory bytes or an unexplained transaction outcome.
        return error.code == ImageProcessingErrorCode::Cancelled
                   ? RecoveryResult::failure({ImageProcessingErrorCode::Cancelled,
                                              ImageProcessingStage::TransactionRecovery, error.nativeErrorProjection})
                   : conflict(error.nativeErrorProjection);
    };
    try
    {
        APTTYPE apartmentType;
        APTTYPEQUALIFIER qualifier;
        const auto apartment = CoGetApartmentType(&apartmentType, &qualifier);
        if (FAILED(apartment) || apartmentType != APTTYPE_MTA || qualifier == APTTYPEQUALIFIER_IMPLICIT_MTA)
            return conflict(domain::WindowsHResult{FAILED(apartment) ? apartment : CO_E_NOTINITIALIZED});
        const auto rootProof = inspectItem(batch.selectedSourceRoot.Path(), true);
        const auto storeProof = inspectItem(batch.journalStore.Path(), true);
        winrt::handle journalStoreLease;
        try
        {
            journalStoreLease = acquireJournalStoreLease(batch.journalStore.Path(), storeProof);
        }
        catch (const winrt::hresult_error &error)
        {
            // A live native lease is retryable contention, not contradictory
            // recovery evidence. Other acquisition failures are journal I/O.
            return RecoveryResult::failure(
                {translateFailure(error.code(), ImageProcessingStage::JournalStoreLeaseAcquisition),
                 ImageProcessingStage::JournalStoreLeaseAcquisition, domain::WindowsHResult{error.code().value}});
        }
        ImageFileTransactionRecoverySummary summary;
        std::optional<domain::ImageProcessingError> firstConflict;
        // Page the supported native enumeration; no manifest-sized materialized
        // directory list and no recursive traversal of user-picked directories.
        for (std::uint32_t offset = 0;;)
        {
            const auto folders =
                batch.journalStore
                    .GetFoldersAsync(winrt::Windows::Storage::Search::CommonFolderQuery::DefaultQuery, offset, 500)
                    .get();
            for (const auto &folder : folders)
            {
                // One transaction's corruption must not starve independently
                // explained recovery. Keep the first conflict for the aggregate
                // result, but never weaken another transaction's proof gates.
                const auto reconcile = [&]() -> RecoveryResult {
                    try
                    {
                        if (cancellationToken.stop_requested())
                            return RecoveryResult::failure(
                                {ImageProcessingErrorCode::Cancelled, ImageProcessingStage::TransactionRecovery});
                        const auto folderProof = inspectItem(folder.Path(), true);
                        if (folderProof.canonicalPath.parent_path() != storeProof.canonicalPath)
                            return conflict();
                        const winrt::guid identifier{std::wstring_view{folder.Name()}};
                        ImageFileTransactionJournal journal{folder, identifier};
                        const auto persisted = journal.readLatest(operations);
                        if (!persisted)
                            return conflict();
                        const auto &record = *persisted;
                        // A verified terminal record closes this transaction. User
                        // images may subsequently be edited, renamed or replaced by a
                        // later transaction; those changes must not reopen old recovery.
                        if (record.state == ImageFileTransactionState::OwnedStagingArtifactsCleaned ||
                            record.state == ImageFileTransactionState::TransactionAbandonedBeforeCommit)
                            return RecoveryResult::success({});
                        // The shared store is not authority to open another selected
                        // root. Leave its incomplete transaction to that root's engine.
                        // A changed identity at our exact selected path remains conflict.
                        if (record.selectedRootIdentity != storageItemIdentityText(rootProof.identity) &&
                            record.selectedRootPath != std::wstring{batch.selectedSourceRoot.Path()})
                            return RecoveryResult::success({});
                        if (record.selectedRootIdentity != storageItemIdentityText(rootProof.identity) ||
                            inspectItem(winrt::hstring{record.selectedRootPath}, true).canonicalPath !=
                                rootProof.canonicalPath)
                            return conflict();
                        const auto source = StorageFile::GetFileFromPathAsync(winrt::hstring{record.sourcePath}).get();
                        const auto sourceProof = inspectItem(source.Path(), false);
                        const auto relativeSource =
                            sourceProof.canonicalPath.lexically_relative(rootProof.canonicalPath);
                        if (relativeSource.empty() || relativeSource.is_absolute())
                            return conflict();
                        for (const auto &component : relativeSource)
                            if (component == "." || component == "..")
                                return conflict();
                        const std::filesystem::path storedStagePath{record.stagePath};
                        const auto stageParent =
                            StorageFolder::GetFolderFromPathAsync(storedStagePath.parent_path().wstring()).get();
                        const auto stageParentProof = inspectItem(stageParent.Path(), true);
                        const auto relativeStageParent = stageParentProof.canonicalPath.lexically_relative(
                            rootProof.canonicalPath / L"JPG Spinner Output");
                        if (relativeStageParent.empty() || relativeStageParent.is_absolute())
                            return conflict();
                        for (const auto &component : relativeStageParent)
                            if (component == "." || component == "..")
                                return conflict();
                        const auto batchName = *relativeStageParent.begin();
                        // Compose the complete relative file before taking its parent:
                        // appending an empty parent_path adds a trailing path component
                        // and falsely rejects sources located directly in the root.
                        const auto expectedStageParent =
                            (rootProof.canonicalPath / L"JPG Spinner Output" / batchName / relativeSource)
                                .parent_path();
                        if (stageParentProof.canonicalPath != expectedStageParent ||
                            storedStagePath.filename() !=
                                L".jpg-spinner-staged-" + std::wstring{winrt::to_hstring(identifier)} + L".jpg")
                            return conflict();
                        const bool replacement =
                            record.outputDisposition == domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup;
                        const auto destinationParent =
                            StorageFolder::GetFolderFromPathAsync(
                                std::filesystem::path{record.destinationPath}.parent_path().wstring())
                                .get();
                        const auto destinationParentProof = inspectItem(destinationParent.Path(), true);
                        if (std::filesystem::path{record.destinationPath}.filename() != relativeSource.filename() ||
                            destinationParentProof.canonicalPath != (replacement
                                                                         ? sourceProof.canonicalPath.parent_path()
                                                                         : stageParentProof.canonicalPath))
                            return conflict();

                        // Verified backups are independently reopened on recovery too.
                        // Missing, changed or substituted backups never justify cleanup
                        // or a reported successful replacement; none is ever deleted.
                        if (!record.backupPath.empty())
                        {
                            const auto backup =
                                StorageFile::GetFileFromPathAsync(winrt::hstring{record.backupPath}).get();
                            const auto backupProof = inspectItem(backup.Path(), false);
                            if (backupProof.canonicalPath !=
                                    rootProof.canonicalPath / L"JPG Spinner Backups" / batchName / relativeSource ||
                                storageItemIdentityText(backupProof.identity) != record.backupIdentity ||
                                record.backupIdentity == record.sourceIdentity ||
                                record.backupIdentity == record.stageIdentity)
                                return conflict();
                            const auto backupRevision = operations.captureBackupRevision(backup, cancellationToken);
                            if (!backupRevision.valueIfPresent())
                                return readFailure(*backupRevision.errorIfPresent());
                            if (backupRevision.valueIfPresent()->encodedLengthBytes !=
                                    record.sourceRevision.encodedLengthBytes ||
                                backupRevision.valueIfPresent()->encodedSha256 != record.sourceRevision.encodedSha256)
                                return conflict();
                        }
                        const auto observedSource = operations.captureSourceRevision(source, cancellationToken);
                        if (!observedSource.valueIfPresent())
                            return readFailure(*observedSource.errorIfPresent());
                        const bool hasCapturedSourceBytes =
                            observedSource.valueIfPresent()->encodedLengthBytes ==
                                record.sourceRevision.encodedLengthBytes &&
                            observedSource.valueIfPresent()->encodedSha256 == record.sourceRevision.encodedSha256;
                        const bool hasTransformedBytes =
                            observedSource.valueIfPresent()->encodedLengthBytes == record.outputLengthBytes &&
                            observedSource.valueIfPresent()->encodedSha256 == record.outputSha256;
                        if (hasCapturedSourceBytes && hasTransformedBytes)
                            return conflict();
                        const auto stageItem = stageParent.TryGetItemAsync(storedStagePath.filename().wstring()).get();
                        const bool sourcePreserved =
                            hasCapturedSourceBytes &&
                            storageItemIdentityText(sourceProof.identity) == record.sourceIdentity;
                        bool outputCommitted = replacement && hasTransformedBytes &&
                                               storageItemIdentityText(sourceProof.identity) == record.stageIdentity &&
                                               !record.backupPath.empty();
                        if (!replacement)
                        {
                            const auto destinationItem =
                                destinationParent.TryGetItemAsync(relativeSource.filename().wstring()).get();
                            if (destinationItem)
                            {
                                const auto destinationFile = destinationItem.as<StorageFile>();
                                const auto destinationProof = inspectItem(destinationFile.Path(), false);
                                const auto destinationRevision =
                                    operations.captureStagedRevision(destinationFile, cancellationToken);
                                if (!destinationRevision.valueIfPresent())
                                    return readFailure(*destinationRevision.errorIfPresent());
                                if (!sourcePreserved ||
                                    destinationProof.canonicalPath.parent_path() != stageParentProof.canonicalPath ||
                                    storageItemIdentityText(destinationProof.identity) != record.stageIdentity ||
                                    destinationRevision.valueIfPresent()->encodedLengthBytes !=
                                        record.outputLengthBytes ||
                                    destinationRevision.valueIfPresent()->encodedSha256 != record.outputSha256)
                                    return conflict();
                                outputCommitted = true;
                            }
                        }
                        if (outputCommitted)
                        {
                            // A native move transfers this exact file ID to the output
                            // path. Both names remaining, an earlier unverified state,
                            // missing backup, or hash disagreement is not a retry signal.
                            if (stageItem || static_cast<unsigned>(record.state) <
                                                 static_cast<unsigned>(
                                                     replacement ? ImageFileTransactionState::VerifiedBackupCreated
                                                                 : ImageFileTransactionState::StagedOutputHashVerified))
                                return conflict();
                            auto terminal = record;
                            if (terminal.state != ImageFileTransactionState::OutputCommitted &&
                                terminal.state != ImageFileTransactionState::OwnedStagingArtifactsCleaned)
                            {
                                ++terminal.generation;
                                terminal.state = ImageFileTransactionState::OutputCommitted;
                                journal.publish(terminal, operations);
                            }
                            if (terminal.state == ImageFileTransactionState::OutputCommitted)
                            {
                                ++terminal.generation;
                                terminal.state = ImageFileTransactionState::OwnedStagingArtifactsCleaned;
                                journal.publish(terminal, operations);
                            }
                            summary.recoveredTransactions.push_back(
                                {identifier, ImageFileTransactionRecoveryOutcome::OutputCommitted});
                            return RecoveryResult::success({});
                        }
                        if (!sourcePreserved)
                            return conflict();
                        if (record.state == ImageFileTransactionState::OutputCommitted ||
                            record.state == ImageFileTransactionState::OwnedStagingArtifactsCleaned)
                            return conflict();
                        // A copy never changes its source. Once its move was
                        // admitted, unchanged source bytes plus two missing names
                        // cannot distinguish no commit from a later output rename
                        // or deletion. Require the exact owned stage for abandonment.
                        if (!replacement && !stageItem &&
                            record.state == ImageFileTransactionState::StagedOutputHashVerified)
                            return conflict();
                        if (stageItem)
                        {
                            const auto stageFile = stageItem.as<StorageFile>();
                            auto stageProof = inspectItem(stageFile.Path(), false);
                            if (storageItemIdentityText(stageProof.identity) != record.stageIdentity ||
                                stageProof.canonicalPath.parent_path() != stageParentProof.canonicalPath ||
                                record.stageIdentity == record.sourceIdentity ||
                                record.stageIdentity == record.backupIdentity)
                                return conflict();
                            if (record.state != ImageFileTransactionState::TransactionInitialized)
                            {
                                const auto revision = operations.captureStagedRevision(stageFile, cancellationToken);
                                if (!revision.valueIfPresent())
                                    return readFailure(*revision.errorIfPresent());
                                if (revision.valueIfPresent()->encodedLengthBytes != record.outputLengthBytes ||
                                    revision.valueIfPresent()->encodedSha256 != record.outputSha256)
                                    return conflict();
                            }
                            OwnedStage owned;
                            owned.file = stageFile;
                            owned.identity = stageProof.identity;
                            owned.canonicalParent = stageParentProof.canonicalPath;
                            owned.canonicalStagePath = stageProof.canonicalPath;
                            // Close the inspection handle before requesting DELETE.
                            // This removes only the journal-bound, UUID-named native
                            // file ID; source/backup bytes are never rollback targets.
                            stageProof.handle.close();
                            owned.remove();
                        }
                        // Hash/identity reconciliation proved the original preserved and
                        // only the owned stage was removed. Persist closure so later
                        // legitimate edits do not turn this refusal into an open conflict.
                        auto abandoned = record;
                        ++abandoned.generation;
                        abandoned.state = ImageFileTransactionState::TransactionAbandonedBeforeCommit;
                        journal.publish(abandoned, operations);
                        summary.recoveredTransactions.push_back(
                            {identifier, ImageFileTransactionRecoveryOutcome::OriginalPreserved});
                        return RecoveryResult::success({});
                    }
                    catch (const winrt::hresult_error &error)
                    {
                        return conflict(domain::WindowsHResult{error.code().value});
                    }
                    catch (const std::invalid_argument &)
                    {
                        return conflict(domain::WindowsHResult{E_INVALIDARG});
                    }
                };
                const auto result = reconcile();
                if (const auto error = result.errorIfPresent())
                {
                    if (error->code == ImageProcessingErrorCode::Cancelled)
                        return result;
                    if (!firstConflict)
                        firstConflict.emplace(*error);
                }
            }
            if (folders.Size() < 500)
                break;
            if (offset > std::numeric_limits<std::uint32_t>::max() - 500)
                return conflict();
            offset += 500;
        }
        if (firstConflict)
            return RecoveryResult::failure(*firstConflict);
        return RecoveryResult::success(std::move(summary));
    }
    catch (const winrt::hresult_error &error)
    {
        return conflict(domain::WindowsHResult{error.code().value});
    }
    catch (const std::bad_alloc &)
    {
        return conflict(domain::WindowsHResult{E_OUTOFMEMORY});
    }
    catch (const std::invalid_argument &)
    {
        // C++/WinRT's GUID string constructor is a standard C++ consumer, not
        // an HRESULT-producing projection. Invalid folder names are data errors.
        return conflict(domain::WindowsHResult{E_INVALIDARG});
    }
}
} // namespace jpg_spinner::storage::internal
