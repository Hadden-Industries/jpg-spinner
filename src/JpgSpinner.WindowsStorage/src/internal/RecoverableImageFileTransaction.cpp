#define NOMINMAX
#include <windows.h>
#include <fileapifromapp.h>
#include <pathcch.h>
#include "RecoverableImageFileTransaction.h"
#include "CorrectedCopyPathPolicy.h"
#include <jpg_spinner/storage/SourceFileRevisionCalculator.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.Storage.Streams.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <new>
#include <string>
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

/// A handle-derived proof, not StorageFile::IsEqual's possibly path-only comparison.
struct StorageItemProof final
{
    winrt::handle handle;
    std::filesystem::path canonicalPath;
    FILE_ID_INFO identity;
};

StorageItemProof inspectItem(const winrt::hstring &path, const bool directory,
                             const DWORD access = FILE_READ_ATTRIBUTES)
{
    if (path.empty())
        throw winrt::hresult_invalid_argument();
    // Windows Storage can create a stage beyond MAX_PATH even when this process
    // has no longPathAware manifest/registry opt-in. Let the supported App-family
    // PathCch consumer produce extended-length drive/UNC syntax; never hand-parse
    // prefixes or change machine policy. This is only an open-path representation,
    // not identity or containment proof. ENSURE implies preserving trailing dots
    // and spaces and must not be combined with ALLOW_LONG_PATHS (SDK contract).
    std::wstring extendedLengthOpenPath(PATHCCH_MAX_CCH, L'\0');
    winrt::check_hresult(PathCchCanonicalizeEx(extendedLengthOpenPath.data(), extendedLengthOpenPath.size(),
                                               path.c_str(), PATHCCH_ENSURE_IS_EXTENDED_LENGTH_PATH));
    // FromApp honors native security; never retry denied metadata access through
    // an unrestricted desktop open. Open the reparse point itself to reject it.
    const auto rawHandle = CreateFileFromAppW(
        extendedLengthOpenPath.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT | (directory ? FILE_FLAG_BACKUP_SEMANTICS : 0), nullptr);
    if (rawHandle == INVALID_HANDLE_VALUE)
        winrt::throw_last_error();
    winrt::handle handle{rawHandle};
    FILE_ATTRIBUTE_TAG_INFO attributes{};
    winrt::check_bool(
        GetFileInformationByHandleEx(handle.get(), FileAttributeTagInfo, &attributes, sizeof(attributes)));
    if ((attributes.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ||
        ((attributes.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) != directory)
        throw winrt::hresult_invalid_argument();
    FILE_ID_INFO identity{};
    winrt::check_bool(GetFileInformationByHandleEx(handle.get(), FileIdInfo, &identity, sizeof(identity)));
    // NT volume names avoid translating through the Mount Manager. The native
    // result is only compared; it is never passed back as an I/O path.
    std::wstring finalPath(32768, L'\0');
    const auto length =
        GetFinalPathNameByHandleW(handle.get(), finalPath.data(), static_cast<DWORD>(finalPath.size()), VOLUME_NAME_NT);
    if (length == 0)
        winrt::throw_last_error();
    if (length >= finalPath.size())
        throw winrt::hresult_invalid_argument();
    finalPath.resize(length);
    return {std::move(handle), std::filesystem::path{finalPath}, identity};
}

bool isSameIdentity(const FILE_ID_INFO &left, const FILE_ID_INFO &right) noexcept
{
    return left.VolumeSerialNumber == right.VolumeSerialNumber &&
           std::memcmp(left.FileId.Identifier, right.FileId.Identifier, sizeof(left.FileId.Identifier)) == 0;
}

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

TransactionResult RecoverableImageFileTransaction::executeCorrectedCopy(
    CorrectedCopyBatch &batch, const ImageFileTransactionRequest &request,
    const domain::ValidatedJpegOutput &validatedOutput, const std::stop_token cancellationToken,
    const FileTransactionOperations &operations)
{
    OwnedStage stage;
    auto phase = ImageProcessingStage::StagingFileCreation;
    try
    {
        const auto &selectedSourceRoot = batch.selectedSourceRoot;
        auto &batchFolder = batch.folder;
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
        // Task 10 supplies backup verification and journaled replacement. Never
        // reinterpret this valid domain choice as permission for an unbacked write.
        if (request.outputDisposition == domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup)
            return failure(ImageProcessingErrorCode::OriginalReplacementFailed,
                           ImageProcessingStage::OriginalReplacement, domain::WindowsHResult{E_NOTIMPL});
        if (!selectedSourceRoot || !request.sourceFile ||
            request.outputDisposition != domain::OutputDisposition::CreateCorrectedCopy)
            return failure(ImageProcessingErrorCode::StagingFileCreationFailed, phase,
                           domain::WindowsHResult{E_INVALIDARG});

        // Failure to inspect the reviewed source/root is a source-side failure,
        // not a stage-creation failure. The authoritative content recheck still
        // occurs after stage hashing, immediately before commit.
        phase = ImageProcessingStage::SourceRevisionRevalidation;
        const auto rootProof = inspectItem(selectedSourceRoot.Path(), true);
        if (batch.selectedRootIdentity && !isSameIdentity(rootProof.identity, *batch.selectedRootIdentity))
            return failure(ImageProcessingErrorCode::RecoveryConflict, ImageProcessingStage::TransactionRecovery);
        const auto sourceProof = inspectItem(request.sourceFile.Path(), false);
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
            batch.folderIdentity = inspectItem(batchFolder.Path(), true).identity;
        }
        const auto batchProof = inspectItem(batchFolder.Path(), true);
        if (!batch.folderIdentity || !isSameIdentity(batchProof.identity, *batch.folderIdentity))
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
        phase = ImageProcessingStage::StagedOutputVerification;
        const auto stagedRevision = operations.captureStagedRevision(stage.file, cancellationToken);
        if (!stagedRevision.valueIfPresent())
        {
            stage.remove();
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
            stage.remove();
            return failure(ImageProcessingErrorCode::StagedOutputHashMismatch, phase);
        }
        {
            const auto verifiedProof = inspectItem(stage.file.Path(), false);
            if (!isSameIdentity(verifiedProof.identity, *stage.identity) ||
                verifiedProof.canonicalPath.parent_path() != stage.canonicalParent)
                throw winrt::hresult_invalid_argument();
        }
        phase = ImageProcessingStage::SourceRevisionRevalidation;
        const auto sourceRevision = operations.captureSourceRevision(request.sourceFile, cancellationToken);
        if (!sourceRevision.valueIfPresent())
        {
            stage.remove();
            return failure(sourceRevision.errorIfPresent()->code, phase,
                           sourceRevision.errorIfPresent()->nativeErrorProjection);
        }
        if (*sourceRevision.valueIfPresent() != request.expectedSourceRevision)
        {
            stage.remove();
            return failure(ImageProcessingErrorCode::SourceChangedAfterAnalysis, phase);
        }
        phase = ImageProcessingStage::CorrectedCopyCommit;
        if (cancellationToken.stop_requested())
        {
            stage.remove();
            return failure(ImageProcessingErrorCode::Cancelled, phase);
        }
        // All stage read/write handles are closed. The digest proves the observed
        // closed bytes, not immunity to arbitrary external writers after close.
        // FailIfExists is essential: the default overload silently suffixes names.
        operations.moveStageToCorrectedCopy(stage.file, destinationFolder,
                                            winrt::hstring{relativePath.filename().wstring()});
        stage.committed = true;
        return TransactionResult::success({stage.file});
    }
    catch (const winrt::hresult_error &error)
    {
        try
        {
            stage.remove(phase == ImageProcessingStage::CorrectedCopyCommit);
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
        return failure(translateFailure(error.code(), phase), phase, domain::WindowsHResult{error.code().value});
    }
    catch (const std::bad_alloc &)
    {
        try
        {
            stage.remove(phase == ImageProcessingStage::CorrectedCopyCommit);
        }
        catch (...)
        {
            return failure(ImageProcessingErrorCode::RecoveryConflict, ImageProcessingStage::TransactionRecovery);
        }
        return failure(ImageProcessingErrorCode::WorkingMemoryAllocationFailed, phase);
    }
}
} // namespace jpg_spinner::storage::internal
