#include <enbcore/runtime/CallbackSession.hpp>

#include <array>
#include <atomic>
#include <thread>

namespace enbcore::runtime {
namespace {

struct ProcessCallbackTarget final {
    std::array<CallbackEventRecord, CallbackSession::kEventCapacity> events{};
    std::atomic<std::uint64_t> read_position{0};
    std::atomic<std::uint64_t> write_position{0};
    std::atomic<std::uint64_t> callbacks_received{0};
    std::atomic<std::uint64_t> events_enqueued{0};
    std::atomic<std::uint64_t> events_drained{0};
    std::atomic<std::uint64_t> unknown_callback_ids{0};
    std::atomic<std::uint64_t> overflow_events{0};
    std::atomic<std::uint32_t> callbacks_in_flight{0};

    void reset() noexcept
    {
        read_position.store(0, std::memory_order_relaxed);
        write_position.store(0, std::memory_order_relaxed);
        callbacks_received.store(0, std::memory_order_relaxed);
        events_enqueued.store(0, std::memory_order_relaxed);
        events_drained.store(0, std::memory_order_relaxed);
        unknown_callback_ids.store(0, std::memory_order_relaxed);
        overflow_events.store(0, std::memory_order_relaxed);
        callbacks_in_flight.store(0, std::memory_order_relaxed);
    }

    void enqueue(
        const CallbackEventKind kind,
        const LifecycleHandoffKind handoff,
        const std::uint64_t sequence) noexcept
    {
        const std::uint64_t write =
            write_position.load(std::memory_order_relaxed);
        const std::uint64_t read = read_position.load(std::memory_order_acquire);
        if (write - read >= CallbackSession::kEventCapacity) {
            overflow_events.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        events[write % CallbackSession::kEventCapacity] =
            CallbackEventRecord{kind, handoff, sequence};
        write_position.store(write + 1U, std::memory_order_release);
        events_enqueued.fetch_add(1, std::memory_order_relaxed);
    }

    [[nodiscard]] std::size_t drain(
        const std::span<CallbackEventRecord> destination) noexcept
    {
        std::uint64_t read = read_position.load(std::memory_order_relaxed);
        const std::uint64_t write =
            write_position.load(std::memory_order_acquire);
        std::size_t count = 0;

        while (read != write && count < destination.size()) {
            destination[count] = events[read % CallbackSession::kEventCapacity];
            ++read;
            ++count;
        }

        if (count != 0U) {
            read_position.store(read, std::memory_order_release);
            events_drained.fetch_add(count, std::memory_order_relaxed);
        }
        return count;
    }

    [[nodiscard]] CallbackDiagnostics diagnostics() const noexcept
    {
        return CallbackDiagnostics{
            callbacks_received.load(std::memory_order_relaxed),
            events_enqueued.load(std::memory_order_relaxed),
            events_drained.load(std::memory_order_relaxed),
            unknown_callback_ids.load(std::memory_order_relaxed),
            overflow_events.load(std::memory_order_relaxed),
        };
    }
};

static_assert(std::atomic<ProcessCallbackTarget*>::is_always_lock_free);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
static_assert(std::atomic<bool>::is_always_lock_free);

std::array<ProcessCallbackTarget, 2> process_targets{};
std::atomic<ProcessCallbackTarget*> published_target{nullptr};
std::atomic<bool> process_slot_reserved{false};
std::uint8_t next_target_index = 0;

[[nodiscard]] bool translate_callback(
    const enb::CallbackId id,
    CallbackEventKind& kind,
    LifecycleHandoffKind& handoff) noexcept
{
    switch (id) {
    case enb::CallbackId::EndFrame:
        kind = CallbackEventKind::EndFrame;
        handoff = LifecycleHandoffKind::None;
        return true;
    case enb::CallbackId::BeginFrame:
        kind = CallbackEventKind::BeginFrame;
        handoff = LifecycleHandoffKind::VerifiedFrameBarrierObserved;
        return true;
    case enb::CallbackId::PreSave:
        kind = CallbackEventKind::PreSave;
        handoff = LifecycleHandoffKind::BeginSave;
        return true;
    case enb::CallbackId::PostLoad:
        kind = CallbackEventKind::PostLoad;
        handoff = LifecycleHandoffKind::None;
        return true;
    case enb::CallbackId::OnInit:
        kind = CallbackEventKind::OnInit;
        handoff = LifecycleHandoffKind::Activate;
        return true;
    case enb::CallbackId::OnExit:
        kind = CallbackEventKind::OnExit;
        handoff = LifecycleHandoffKind::RequestShutdown;
        return true;
    case enb::CallbackId::PreReset:
        kind = CallbackEventKind::PreReset;
        handoff = LifecycleHandoffKind::None;
        return true;
    case enb::CallbackId::PostReset:
        kind = CallbackEventKind::PostReset;
        handoff = LifecycleHandoffKind::None;
        return true;
    }

    return false;
}

#if defined(_MSC_VER)
void __stdcall enb_callback_thunk(const enb::CallbackId id) noexcept
#elif defined(__GNUC__) && defined(__i386__)
void __attribute__((stdcall)) enb_callback_thunk(const enb::CallbackId id) noexcept
#else
void enb_callback_thunk(const enb::CallbackId id) noexcept
#endif
{
    ProcessCallbackTarget* const target =
        published_target.load(std::memory_order_acquire);
    if (target == nullptr) {
        return;
    }

    target->callbacks_in_flight.fetch_add(1, std::memory_order_acquire);
    if (published_target.load(std::memory_order_acquire) != target) {
        target->callbacks_in_flight.fetch_sub(1, std::memory_order_release);
        return;
    }

    const std::uint64_t sequence =
        target->callbacks_received.fetch_add(1, std::memory_order_relaxed) + 1U;
    CallbackEventKind kind = CallbackEventKind::EndFrame;
    LifecycleHandoffKind handoff = LifecycleHandoffKind::None;
    if (!translate_callback(id, kind, handoff)) {
        target->unknown_callback_ids.fetch_add(1, std::memory_order_relaxed);
    } else {
        target->enqueue(kind, handoff, sequence);
    }

    target->callbacks_in_flight.fetch_sub(1, std::memory_order_release);
}

[[nodiscard]] ProcessCallbackTarget& target_at(const std::uint8_t index) noexcept
{
    return process_targets[index];
}

} // namespace

CallbackSession::~CallbackSession() noexcept
{
    static_cast<void>(stop());
}

CallbackSessionResult CallbackSession::bootstrap(
    const enb::HostResolutionResult& resolution) noexcept
{
    if (state_ != CallbackSessionState::Idle) {
        if (state_ == CallbackSessionState::Active) {
            return result(CallbackSessionCode::AlreadyActive);
        }
        if (state_ == CallbackSessionState::Stopped) {
            return result(CallbackSessionCode::AlreadyStopped);
        }
        return result(CallbackSessionCode::InvalidState);
    }

    if (!resolution.exports_resolved
        || (resolution.code != enb::HostResolutionCode::Ready
            && resolution.code != enb::HostResolutionCode::NotReady)) {
        state_ = CallbackSessionState::Failed;
        return result(CallbackSessionCode::InvalidResolution);
    }

    exports_ = resolution.exports;
    const enb::ValidationResult validation = enb::ValidateSdkHost(exports_);
    if (validation.code == enb::ValidationCode::RenderInfoNotReady) {
        state_ = CallbackSessionState::WaitingForRenderInfo;
        return result(CallbackSessionCode::RenderInfoNotReady);
    }
    if (!validation.accepted()) {
        state_ = CallbackSessionState::Failed;
        return result(CallbackSessionCode::InvalidHost);
    }

    return activate();
}

CallbackSessionResult CallbackSession::retryRenderReadiness() noexcept
{
    if (state_ == CallbackSessionState::Active) {
        return result(CallbackSessionCode::AlreadyActive);
    }
    if (state_ != CallbackSessionState::WaitingForRenderInfo
        && state_ != CallbackSessionState::WaitingForProcessSlot) {
        return result(CallbackSessionCode::InvalidState);
    }

    if (exports_.get_render_info() == nullptr) {
        state_ = CallbackSessionState::WaitingForRenderInfo;
        return result(CallbackSessionCode::RenderInfoNotReady);
    }

    return activate();
}

CallbackSessionResult CallbackSession::stop() noexcept
{
    if (state_ == CallbackSessionState::Stopped) {
        return result(CallbackSessionCode::AlreadyStopped);
    }
    return terminate(
        CallbackSessionState::Stopped,
        CallbackSessionCode::Stopped);
}

CallbackSessionResult CallbackSession::fail() noexcept
{
    if (state_ == CallbackSessionState::Stopped) {
        return result(CallbackSessionCode::AlreadyStopped);
    }
    return terminate(
        CallbackSessionState::Failed,
        CallbackSessionCode::Failed);
}

std::size_t CallbackSession::drain(
    const std::span<CallbackEventRecord> destination) noexcept
{
    if (!owns_process_slot_ || state_ != CallbackSessionState::Active) {
        return 0;
    }
    return target_at(target_index_).drain(destination);
}

CallbackDiagnostics CallbackSession::diagnostics() const noexcept
{
    if (owns_process_slot_) {
        return target_at(target_index_).diagnostics();
    }
    return terminal_diagnostics_;
}

CallbackSessionState CallbackSession::state() const noexcept
{
    return state_;
}

bool CallbackSession::active() const noexcept
{
    return state_ == CallbackSessionState::Active;
}

CallbackSessionResult CallbackSession::activate() noexcept
{
    bool expected = false;
    if (!process_slot_reserved.compare_exchange_strong(
            expected,
            true,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
        state_ = CallbackSessionState::WaitingForProcessSlot;
        return result(CallbackSessionCode::DuplicateActiveSession);
    }

    target_index_ = next_target_index;
    next_target_index = static_cast<std::uint8_t>(
        (next_target_index + 1U) % process_targets.size());
    ProcessCallbackTarget& target = target_at(target_index_);
    target.reset();
    terminal_diagnostics_ = {};
    owns_process_slot_ = true;

    published_target.store(&target, std::memory_order_release);
    exports_.set_callback_function(&enb_callback_thunk);
    callback_registered_ = true;
    state_ = CallbackSessionState::Active;
    return result(CallbackSessionCode::Active);
}

CallbackSessionResult CallbackSession::terminate(
    const CallbackSessionState final_state,
    const CallbackSessionCode result_code) noexcept
{
    if (owns_process_slot_) {
        ProcessCallbackTarget& target = target_at(target_index_);
        if (callback_registered_) {
            exports_.set_callback_function(nullptr);
            callback_registered_ = false;
        }

        ProcessCallbackTarget* expected = &target;
        static_cast<void>(published_target.compare_exchange_strong(
            expected,
            nullptr,
            std::memory_order_acq_rel,
            std::memory_order_acquire));
        while (target.callbacks_in_flight.load(std::memory_order_acquire) != 0U) {
            std::this_thread::yield();
        }

        terminal_diagnostics_ = target.diagnostics();
        owns_process_slot_ = false;
        process_slot_reserved.store(false, std::memory_order_release);
    }

    state_ = final_state;
    return result(result_code);
}

CallbackSessionResult CallbackSession::result(
    const CallbackSessionCode code) const noexcept
{
    return CallbackSessionResult{code, state_};
}

} // namespace enbcore::runtime
