#pragma once

namespace jpg_spinner::presentation
{
/// UI-thread close admission for an owned window. A close request is sticky:
/// new work must not start afterwards. Active native operations request stop
/// and settle before ordinary window closure; this is not a Windows shutdown
/// deferral, forced-termination guarantee or physical-durability promise.
class CooperativeWindowClose final
{
  public:
    /// Returns whether the closing event may proceed immediately. Otherwise
    /// the host cancels that event and requests cooperative work cancellation.
    [[nodiscard]] bool requestClose(bool hasActiveWork) noexcept
    {
        closeRequested_ = true;
        return !hasActiveWork;
    }
    [[nodiscard]] bool closeRequested() const noexcept
    {
        return closeRequested_;
    }
    /// Work completion, not merely an issued stop request, admits deferred close.
    [[nodiscard]] bool shouldCloseAfterWorkSettles(bool hasActiveWork) const noexcept
    {
        return closeRequested_ && !hasActiveWork;
    }

  private:
    bool closeRequested_{};
};
} // namespace jpg_spinner::presentation
