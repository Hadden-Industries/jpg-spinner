#include "pch.h"
#include "Diagnostics/DiagnosticLogExport.h"
#include <winrt/Windows.Storage.Provider.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <cstdint>
#include <roapi.h>
#include <wil/resource.h>

namespace jpg_spinner::diagnostics
{
winrt::Windows::Foundation::IAsyncAction exportDiagnosticLogAsync(std::shared_ptr<LocalDiagnosticLog> log,
                                                                  winrt::Windows::Storage::StorageFile destination)
{
    if (!log || !destination)
        throw winrt::hresult_invalid_argument();
    const auto extension = destination.FileType();
    if (CompareStringOrdinal(extension.c_str(), -1, L".jsonl", -1, TRUE) != CSTR_EQUAL)
        throw winrt::hresult_invalid_argument();
    const auto cancellation = co_await winrt::get_cancellation_token();
    cancellation.enable_propagation();
    const winrt::apartment_context callerApartment;
    // Schedule bounded local I/O away from the UI. Explicitly own the native
    // MTA initialization while the synchronous logger accesses Storage APIs;
    // unwind that thread-bound scope before any subsequent coroutine await.
    co_await winrt::resume_background();
    std::string snapshot;
    {
        const auto apartment = wil::RoInitialize(RO_INIT_MULTITHREADED);
        snapshot = log->snapshot();
    }
    // Provider completion may present provider-owned UI. Retain the caller's
    // original apartment as the picker workflow did before extraction; only
    // the bounded logger I/O belongs on the background MTA.
    co_await callerApartment;
    if (cancellation())
        throw winrt::hresult_canceled();
    // The bounded snapshot is entirely validated before touching destination
    // bytes. Its owning coroutine frame keeps the byte view alive until the
    // native write finishes. Provider status, not the write alone, is success.
    // https://learn.microsoft.com/windows/uwp/files/quickstart-save-a-file-with-a-picker
    winrt::Windows::Storage::CachedFileManager::DeferUpdates(destination);
    const auto *bytes = reinterpret_cast<const std::uint8_t *>(snapshot.data());
    co_await winrt::Windows::Storage::FileIO::WriteBytesAsync(
        destination, winrt::array_view<const std::uint8_t>{bytes, bytes + snapshot.size()});
    const auto status = co_await winrt::Windows::Storage::CachedFileManager::CompleteUpdatesAsync(destination);
    if (status != winrt::Windows::Storage::Provider::FileUpdateStatus::Complete &&
        status != winrt::Windows::Storage::Provider::FileUpdateStatus::CompleteAndRenamed)
        throw winrt::hresult_error{E_FAIL};
}
} // namespace jpg_spinner::diagnostics
