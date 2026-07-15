#pragma once

#include <cstdint>

namespace enbcore::runtime {

enum class State : std::uint8_t {
    InactiveBootstrap = 0,
    Active = 1,
    QuiesceRequested = 2,
    BaselineRestored = 3,
    SaveInProgress = 4,
    ReapplyPending = 5,
    Shutdown = 6,
    Stopped = 7,
    Failed = 8,
};

enum class Event : std::uint8_t {
    Activate = 0,
    BeginSave = 1,
    SaveSucceeded = 2,
    SaveFailed = 3,
    SaveCancelled = 4,
    ReapplyAtVerifiedBarrier = 5,
    RequestShutdown = 6,
    CompleteShutdown = 7,
    Interrupt = 8,
};

enum class TransitionStatus : std::uint8_t {
    Applied = 0,
    Rejected = 1,
};

enum class DiagnosticCode : std::uint16_t {
    None = 0,
    InvalidEventForState = 1,
    BaselineRestorationNotAcknowledged = 2,
};

struct TransitionContext final {
    bool baseline_restoration_acknowledged{false};
};

struct TransitionResult final {
    TransitionStatus status;
    DiagnosticCode diagnostic;
    State previous_state;
    State current_state;
    std::uint64_t generation;
    bool product_mutation_permitted;
};

class LifecycleGate final {
public:
    [[nodiscard]] State state() const noexcept;
    [[nodiscard]] std::uint64_t generation() const noexcept;
    [[nodiscard]] bool productMutationPermitted() const noexcept;
    [[nodiscard]] TransitionResult dispatch(
        Event event,
        TransitionContext context = {}) noexcept;

private:
    State state_{State::InactiveBootstrap};
    std::uint64_t generation_{0};
    std::uint64_t save_depth_{0};
};

} // namespace enbcore::runtime
