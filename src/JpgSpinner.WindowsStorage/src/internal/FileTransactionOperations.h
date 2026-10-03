#pragma once

#include <jpg_spinner/storage/SourceFileRevisionCalculator.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>

namespace jpg_spinner::storage::internal
{
/// Internal effect seam for deterministic native failures, not a generic filesystem
/// or application phase API. Defaults execute the actual Windows contracts; tests
/// override one unsafe/nondeterministic effect while retaining real files and the
/// transaction's decisive hash/ownership/commit policy. Overrides may throw native
/// HRESULT errors. All streams and buffers are borrowed for the synchronous call.
class FileTransactionOperations
{
  public:
    virtual ~FileTransactionOperations() = default;
    virtual winrt::Windows::Storage::StorageFolder createCorrectedCopyOutputRoot(
        const winrt::Windows::Storage::StorageFolder &selectedRoot) const
    {
        return selectedRoot
            .CreateFolderAsync(L"JPG Spinner Output", winrt::Windows::Storage::CreationCollisionOption::OpenIfExists)
            .get();
    }
    virtual winrt::Windows::Storage::StorageFolder createBatchDestination(
        const winrt::Windows::Storage::StorageFolder &outputRoot, const winrt::hstring &name) const
    {
        return outputRoot.CreateFolderAsync(name, winrt::Windows::Storage::CreationCollisionOption::FailIfExists).get();
    }
    virtual winrt::Windows::Storage::StorageFolder createRelativeDestination(
        const winrt::Windows::Storage::StorageFolder &parent, const winrt::hstring &name) const
    {
        return parent.CreateFolderAsync(name, winrt::Windows::Storage::CreationCollisionOption::OpenIfExists).get();
    }
    virtual winrt::Windows::Storage::StorageFile createStage(const winrt::Windows::Storage::StorageFolder &destination,
                                                             const winrt::hstring &name) const
    {
        return destination.CreateFileAsync(name, winrt::Windows::Storage::CreationCollisionOption::FailIfExists).get();
    }
    virtual winrt::Windows::Storage::Streams::IRandomAccessStream openStagedOutput(
        const winrt::Windows::Storage::StorageFile &stage) const
    {
        return stage
            .OpenAsync(winrt::Windows::Storage::FileAccessMode::ReadWrite,
                       winrt::Windows::Storage::StorageOpenOptions::AllowOnlyReaders)
            .get();
    }
    virtual std::uint32_t writeStagedBytes(const winrt::Windows::Storage::Streams::IRandomAccessStream &stream,
                                           const winrt::Windows::Storage::Streams::IBuffer &buffer) const
    {
        return stream.WriteAsync(buffer).get();
    }
    virtual bool flushStagedBytes(const winrt::Windows::Storage::Streams::IRandomAccessStream &stream) const
    {
        return stream.FlushAsync().get();
    }
    virtual void closeStagedOutput(const winrt::Windows::Storage::Streams::IRandomAccessStream &stream) const
    {
        stream.Close();
    }
    /// Reopen and bounded hash stay together in the real revision calculator;
    /// splitting them here would duplicate its native handle-lifetime protocol.
    virtual domain::ImageProcessingResult<domain::SourceFileRevision> captureStagedRevision(
        const winrt::Windows::Storage::StorageFile &stage, std::stop_token cancellation) const
    {
        return SourceFileRevisionCalculator{}.calculate(stage, cancellation);
    }
    virtual domain::ImageProcessingResult<domain::SourceFileRevision> captureSourceRevision(
        const winrt::Windows::Storage::StorageFile &source, std::stop_token cancellation) const
    {
        return SourceFileRevisionCalculator{}.calculate(source, cancellation);
    }
    virtual void moveStageToCorrectedCopy(const winrt::Windows::Storage::StorageFile &stage,
                                          const winrt::Windows::Storage::StorageFolder &destination,
                                          const winrt::hstring &name) const
    {
        stage.MoveAsync(destination, name, winrt::Windows::Storage::NameCollisionOption::FailIfExists).get();
    }
};
} // namespace jpg_spinner::storage::internal
