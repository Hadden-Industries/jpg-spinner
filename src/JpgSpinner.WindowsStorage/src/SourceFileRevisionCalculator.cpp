#define NOMINMAX
#include <windows.h>

#include <jpg_spinner/storage/SourceFileRevisionCalculator.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.Security.Cryptography.Core.h>
#include <winrt/Windows.Storage.FileProperties.h>
#include <winrt/Windows.Storage.Streams.h>

#include <limits>
#include <new>
#include <utility>

#pragma comment(lib, "windowsapp.lib")

namespace jpg_spinner::storage
{
namespace
{
using domain::ImageProcessingErrorCode;
using domain::ImageProcessingStage;
using RevisionResult = domain::ImageProcessingResult<domain::SourceFileRevision>;

RevisionResult failure(const ImageProcessingErrorCode code, const domain::NativeErrorProjection nativeError = {})
{
    return RevisionResult::failure({code, ImageProcessingStage::SourceRevisionCapture, nativeError});
}

ImageProcessingErrorCode errorCodeFor(const winrt::hresult nativeError) noexcept
{
    if (nativeError == E_ACCESSDENIED)
        return ImageProcessingErrorCode::SourceAccessDenied;
    if (nativeError == HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION))
        return ImageProcessingErrorCode::SourceSharingViolation;
    if (nativeError == E_ABORT || nativeError == HRESULT_FROM_WIN32(ERROR_CANCELLED))
        return ImageProcessingErrorCode::Cancelled;
    if (nativeError == E_OUTOFMEMORY)
        return ImageProcessingErrorCode::WorkingMemoryAllocationFailed;
    return ImageProcessingErrorCode::SourceRevisionCaptureFailed;
}

} // namespace

RevisionResult SourceFileRevisionCalculator::calculate(const winrt::Windows::Storage::StorageFile &sourceFile,
                                                       const std::stop_token cancellationToken,
                                                       const SourceFileRevisionProgressObserver &progressObserver) const
{
    using namespace winrt::Windows::Storage;
    using namespace winrt::Windows::Storage::Streams;
    using namespace winrt::Windows::Security::Cryptography;
    using namespace winrt::Windows::Security::Cryptography::Core;

    try
    {
        if (cancellationToken.stop_requested())
            return failure(ImageProcessingErrorCode::Cancelled);
        APTTYPE apartmentType;
        APTTYPEQUALIFIER apartmentQualifier;
        const HRESULT apartmentResult = CoGetApartmentType(&apartmentType, &apartmentQualifier);
        if (FAILED(apartmentResult) || apartmentType != APTTYPE_MTA)
            return failure(ImageProcessingErrorCode::SourceRevisionCaptureFailed,
                           domain::WindowsHResult{FAILED(apartmentResult) ? apartmentResult : RPC_E_WRONG_THREAD});
        // COM can report MTA on an uninitialized thread when another thread
        // initialized the process MTA. Require this caller's explicit lifetime
        // ownership rather than relying on another thread to keep COM alive.
        if (apartmentQualifier == APTTYPEQUALIFIER_IMPLICIT_MTA)
            return failure(ImageProcessingErrorCode::SourceRevisionCaptureFailed,
                           domain::WindowsHResult{CO_E_NOTINITIALIZED});
        if (!sourceFile)
            return failure(ImageProcessingErrorCode::SourceRevisionCaptureFailed, domain::WindowsHResult{E_INVALIDARG});

        // Hold the actual stream throughout metadata/hash capture. A StorageFile
        // alone is not a locked handle; incompatible writers must fail closed.
        auto stream = sourceFile.OpenAsync(FileAccessMode::Read, StorageOpenOptions::AllowOnlyReaders).get();
        const auto before = sourceFile.GetBasicPropertiesAsync().get();
        const auto initialLength = stream.Size();
        if (before.Size() != initialLength)
            return failure(ImageProcessingErrorCode::SourceChangedAfterAnalysis);
        if (before.DateModified().time_since_epoch().count() <= 0)
            return failure(ImageProcessingErrorCode::SourceRevisionCaptureFailed);

        constexpr std::uint32_t readBufferCapacity = 64 * 1024;
        const Buffer readBuffer{readBufferCapacity};
        const auto hash = HashAlgorithmProvider::OpenAlgorithm(HashAlgorithmNames::Sha256()).CreateHash();
        std::uint64_t hashedLength = 0;
        if (progressObserver)
            progressObserver({hashedLength, initialLength});

        while (true)
        {
            if (cancellationToken.stop_requested())
                return failure(ImageProcessingErrorCode::Cancelled);
            // ReadAsync may return a different buffer; hash its result, not an
            // assumed mutation of readBuffer (the documented IInputStream contract).
            const auto bytes = stream.ReadAsync(readBuffer, readBufferCapacity, InputStreamOptions::Partial).get();
            if (cancellationToken.stop_requested())
                return failure(ImageProcessingErrorCode::Cancelled);
            const auto readLength = bytes.Length();
            if (readLength == 0)
                break;
            if (readLength > readBufferCapacity ||
                readLength > std::numeric_limits<std::uint64_t>::max() - hashedLength || hashedLength > initialLength ||
                readLength > initialLength - hashedLength)
                return failure(ImageProcessingErrorCode::SourceChangedAfterAnalysis);
            hash.Append(bytes);
            hashedLength += readLength;
            if (progressObserver)
                progressObserver({hashedLength, initialLength});
        }

        const auto after = sourceFile.GetBasicPropertiesAsync().get();
        if (cancellationToken.stop_requested())
            return failure(ImageProcessingErrorCode::Cancelled);
        if (hashedLength != initialLength || stream.Size() != initialLength || after.Size() != before.Size() ||
            after.DateModified() != before.DateModified())
            return failure(ImageProcessingErrorCode::SourceChangedAfterAnalysis);

        winrt::com_array<std::uint8_t> nativeDigest;
        CryptographicBuffer::CopyToByteArray(hash.GetValueAndReset(), nativeDigest);
        domain::Sha256Digest digest{};
        if (nativeDigest.size() != digest.size())
            return failure(ImageProcessingErrorCode::SourceRevisionCaptureFailed);
        for (std::uint32_t index = 0; index < nativeDigest.size(); ++index)
            digest[index] = static_cast<std::byte>(nativeDigest[index]);

        // A close failure is still capture failure. Close before publishing the
        // result so the next transaction phase cannot race an outstanding reader.
        stream.Close();
        if (cancellationToken.stop_requested())
            return failure(ImageProcessingErrorCode::Cancelled);
        return RevisionResult::success({hashedLength, winrt::clock::to_sys(before.DateModified()), digest});
    }
    catch (const winrt::hresult_error &error)
    {
        return failure(errorCodeFor(error.code()), domain::WindowsHResult{error.code().value});
    }
    catch (const std::bad_alloc &)
    {
        return failure(ImageProcessingErrorCode::WorkingMemoryAllocationFailed);
    }
}
} // namespace jpg_spinner::storage
