#include <enbcore/runtime/LifecycleGate.hpp>

namespace enbcore::runtime {

State LifecycleGate::state() const noexcept
{
    return state_;
}

std::uint64_t LifecycleGate::generation() const noexcept
{
    return generation_;
}

bool LifecycleGate::productMutationPermitted() const noexcept
{
    return state_ == State::Active;
}

TransitionResult LifecycleGate::dispatch(
    const Event event,
    const TransitionContext context) noexcept
{
    const State previous = state_;
    const auto result = [this, previous](
                            const TransitionStatus status,
                            const DiagnosticCode diagnostic) noexcept {
        return TransitionResult{
            status,
            diagnostic,
            previous,
            state_,
            generation_,
            productMutationPermitted(),
        };
    };

    switch (event) {
    case Event::Activate:
        if (state_ != State::InactiveBootstrap) {
            return result(
                TransitionStatus::Rejected,
                DiagnosticCode::InvalidEventForState);
        }

        state_ = State::Active;
        ++generation_;
        return result(TransitionStatus::Applied, DiagnosticCode::None);

    case Event::BeginSave:
        if (state_ == State::SaveInProgress) {
            ++save_depth_;
            return result(TransitionStatus::Applied, DiagnosticCode::None);
        }
        if (state_ != State::Active) {
            return result(
                TransitionStatus::Rejected,
                DiagnosticCode::InvalidEventForState);
        }
        if (!context.baseline_restoration_acknowledged) {
            return result(
                TransitionStatus::Rejected,
                DiagnosticCode::BaselineRestorationNotAcknowledged);
        }

        state_ = State::QuiesceRequested;
        state_ = State::BaselineRestored;
        state_ = State::SaveInProgress;
        save_depth_ = 1;
        return result(TransitionStatus::Applied, DiagnosticCode::None);

    case Event::SaveSucceeded:
    case Event::SaveFailed:
    case Event::SaveCancelled:
        if (state_ != State::SaveInProgress) {
            return result(
                TransitionStatus::Rejected,
                DiagnosticCode::InvalidEventForState);
        }

        --save_depth_;
        if (save_depth_ == 0) {
            state_ = State::ReapplyPending;
        }
        return result(TransitionStatus::Applied, DiagnosticCode::None);

    case Event::ReapplyAtVerifiedBarrier:
        if (state_ != State::ReapplyPending) {
            return result(
                TransitionStatus::Rejected,
                DiagnosticCode::InvalidEventForState);
        }

        state_ = State::Active;
        ++generation_;
        return result(TransitionStatus::Applied, DiagnosticCode::None);

    case Event::RequestShutdown:
        if (state_ == State::Active) {
            if (!context.baseline_restoration_acknowledged) {
                return result(
                    TransitionStatus::Rejected,
                    DiagnosticCode::BaselineRestorationNotAcknowledged);
            }

            state_ = State::QuiesceRequested;
            state_ = State::BaselineRestored;
        } else if (
            state_ != State::InactiveBootstrap
            && state_ != State::QuiesceRequested
            && state_ != State::BaselineRestored
            && state_ != State::SaveInProgress
            && state_ != State::ReapplyPending) {
            return result(
                TransitionStatus::Rejected,
                DiagnosticCode::InvalidEventForState);
        }

        save_depth_ = 0;
        state_ = State::Shutdown;
        return result(TransitionStatus::Applied, DiagnosticCode::None);

    case Event::CompleteShutdown:
        if (state_ != State::Shutdown) {
            return result(
                TransitionStatus::Rejected,
                DiagnosticCode::InvalidEventForState);
        }

        state_ = State::Stopped;
        return result(TransitionStatus::Applied, DiagnosticCode::None);

    case Event::Interrupt:
        if (state_ == State::Active) {
            if (!context.baseline_restoration_acknowledged) {
                return result(
                    TransitionStatus::Rejected,
                    DiagnosticCode::BaselineRestorationNotAcknowledged);
            }

            state_ = State::QuiesceRequested;
            state_ = State::BaselineRestored;
        } else if (
            state_ != State::InactiveBootstrap
            && state_ != State::QuiesceRequested
            && state_ != State::BaselineRestored
            && state_ != State::SaveInProgress
            && state_ != State::ReapplyPending) {
            return result(
                TransitionStatus::Rejected,
                DiagnosticCode::InvalidEventForState);
        }

        save_depth_ = 0;
        state_ = State::Failed;
        return result(TransitionStatus::Applied, DiagnosticCode::None);
    }

    return result(
        TransitionStatus::Rejected,
        DiagnosticCode::InvalidEventForState);
}

} // namespace enbcore::runtime
