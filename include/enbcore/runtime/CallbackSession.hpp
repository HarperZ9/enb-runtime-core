#pragma once

#include <enbcore/enb/LoadedHostResolver.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace enbcore::runtime {

enum class CallbackEventKind : std::uint8_t {
    EndFrame = 1,
    BeginFrame = 2,
    PreSave = 3,
    PostLoad = 4,
    OnInit = 5,
    OnExit = 6,
    PreReset = 7,
    PostReset = 8,
};

enum class LifecycleHandoffKind : std::uint8_t {
    None = 0,
    Activate = 1,
    BeginSave = 2,
    RequestShutdown = 3,
    VerifiedFrameBarrierObserved = 4,
};

struct CallbackEventRecord final {
    CallbackEventKind kind{CallbackEventKind::EndFrame};
    LifecycleHandoffKind lifecycle_handoff{LifecycleHandoffKind::None};
    std::uint64_t sequence{0};
};

struct CallbackDiagnostics final {
    std::uint64_t callbacks_received{0};
    std::uint64_t events_enqueued{0};
    std::uint64_t events_drained{0};
    std::uint64_t unknown_callback_ids{0};
    std::uint64_t overflow_events{0};
};

enum class CallbackSessionState : std::uint8_t {
    Idle = 0,
    WaitingForRenderInfo = 1,
    WaitingForProcessSlot = 2,
    Active = 3,
    Stopped = 4,
    Failed = 5,
};

enum class CallbackSessionCode : std::uint8_t {
    Active = 0,
    RenderInfoNotReady = 1,
    DuplicateActiveSession = 2,
    AlreadyActive = 3,
    Stopped = 4,
    AlreadyStopped = 5,
    Failed = 6,
    InvalidResolution = 7,
    InvalidHost = 8,
    InvalidState = 9,
};

struct CallbackSessionResult final {
    CallbackSessionCode code{CallbackSessionCode::InvalidState};
    CallbackSessionState state{CallbackSessionState::Idle};

    [[nodiscard]] constexpr bool active() const noexcept
    {
        return code == CallbackSessionCode::Active;
    }

    [[nodiscard]] constexpr bool retryable() const noexcept
    {
        return code == CallbackSessionCode::RenderInfoNotReady
            || code == CallbackSessionCode::DuplicateActiveSession;
    }
};

class CallbackSession final {
public:
    static constexpr std::size_t kEventCapacity = 64;

    CallbackSession() noexcept = default;
    ~CallbackSession() noexcept;

    CallbackSession(const CallbackSession&) = delete;
    CallbackSession& operator=(const CallbackSession&) = delete;
    CallbackSession(CallbackSession&&) = delete;
    CallbackSession& operator=(CallbackSession&&) = delete;

    [[nodiscard]] CallbackSessionResult bootstrap(
        const enb::HostResolutionResult& resolution) noexcept;
    [[nodiscard]] CallbackSessionResult retryRenderReadiness() noexcept;
    [[nodiscard]] CallbackSessionResult stop() noexcept;
    [[nodiscard]] CallbackSessionResult fail() noexcept;

    [[nodiscard]] std::size_t drain(
        std::span<CallbackEventRecord> destination) noexcept;
    [[nodiscard]] CallbackDiagnostics diagnostics() const noexcept;
    [[nodiscard]] CallbackSessionState state() const noexcept;
    [[nodiscard]] bool active() const noexcept;

private:
    [[nodiscard]] CallbackSessionResult activate() noexcept;
    [[nodiscard]] CallbackSessionResult terminate(
        CallbackSessionState final_state,
        CallbackSessionCode result_code) noexcept;
    [[nodiscard]] CallbackSessionResult result(
        CallbackSessionCode code) const noexcept;

    enb::SdkExports exports_{};
    CallbackDiagnostics terminal_diagnostics_{};
    CallbackSessionState state_{CallbackSessionState::Idle};
    std::uint8_t target_index_{0};
    bool owns_process_slot_{false};
    bool callback_registered_{false};
};

} // namespace enbcore::runtime
