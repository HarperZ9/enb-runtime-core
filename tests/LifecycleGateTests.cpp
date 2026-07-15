#include <enbcore/runtime/LifecycleGate.hpp>

#include <array>
#include <iostream>

namespace {

int failures = 0;

void expect(const bool condition, const char* expression, const char* file, const int line)
{
    if (condition) {
        return;
    }

    std::cerr << file << ':' << line << ": expectation failed: " << expression << '\n';
    ++failures;
}

#define EXPECT(expression) expect((expression), #expression, __FILE__, __LINE__)

void bootstrap_requires_explicit_activation()
{
    using namespace enbcore::runtime;

    LifecycleGate gate;

    EXPECT(gate.state() == State::InactiveBootstrap);
    EXPECT(gate.generation() == 0U);
    EXPECT(!gate.productMutationPermitted());

    const TransitionResult result = gate.dispatch(Event::Activate);

    EXPECT(result.status == TransitionStatus::Applied);
    EXPECT(result.diagnostic == DiagnosticCode::None);
    EXPECT(result.previous_state == State::InactiveBootstrap);
    EXPECT(result.current_state == State::Active);
    EXPECT(result.generation == 1U);
    EXPECT(result.product_mutation_permitted);
    EXPECT(gate.state() == State::Active);
    EXPECT(gate.generation() == 1U);
    EXPECT(gate.productMutationPermitted());
}

void begin_save_requires_baseline_restoration_acknowledgement()
{
    using namespace enbcore::runtime;

    LifecycleGate gate;
    static_cast<void>(gate.dispatch(Event::Activate));

    const TransitionResult rejected = gate.dispatch(Event::BeginSave, TransitionContext{});

    EXPECT(rejected.status == TransitionStatus::Rejected);
    EXPECT(rejected.diagnostic == DiagnosticCode::BaselineRestorationNotAcknowledged);
    EXPECT(rejected.previous_state == State::Active);
    EXPECT(rejected.current_state == State::Active);
    EXPECT(rejected.generation == 1U);
    EXPECT(rejected.product_mutation_permitted);
    EXPECT(gate.state() == State::Active);
    EXPECT(gate.generation() == 1U);

    TransitionContext acknowledged;
    acknowledged.baseline_restoration_acknowledged = true;
    const TransitionResult accepted = gate.dispatch(Event::BeginSave, acknowledged);

    EXPECT(accepted.status == TransitionStatus::Applied);
    EXPECT(accepted.diagnostic == DiagnosticCode::None);
    EXPECT(accepted.previous_state == State::Active);
    EXPECT(accepted.current_state == State::SaveInProgress);
    EXPECT(accepted.generation == 1U);
    EXPECT(!accepted.product_mutation_permitted);
    EXPECT(gate.state() == State::SaveInProgress);
    EXPECT(!gate.productMutationPermitted());
}

void every_save_outcome_requires_reapplication()
{
    using namespace enbcore::runtime;

    constexpr std::array outcomes{
        Event::SaveSucceeded,
        Event::SaveFailed,
        Event::SaveCancelled,
    };

    for (const Event outcome : outcomes) {
        LifecycleGate gate;
        static_cast<void>(gate.dispatch(Event::Activate));

        TransitionContext acknowledged;
        acknowledged.baseline_restoration_acknowledged = true;
        static_cast<void>(gate.dispatch(Event::BeginSave, acknowledged));

        const TransitionResult result = gate.dispatch(outcome);

        EXPECT(result.status == TransitionStatus::Applied);
        EXPECT(result.diagnostic == DiagnosticCode::None);
        EXPECT(result.previous_state == State::SaveInProgress);
        EXPECT(result.current_state == State::ReapplyPending);
        EXPECT(result.generation == 1U);
        EXPECT(!result.product_mutation_permitted);
        EXPECT(gate.state() == State::ReapplyPending);
        EXPECT(!gate.productMutationPermitted());
    }
}

void nested_saves_drain_before_reapplication()
{
    using namespace enbcore::runtime;

    LifecycleGate gate;
    static_cast<void>(gate.dispatch(Event::Activate));

    TransitionContext acknowledged;
    acknowledged.baseline_restoration_acknowledged = true;
    static_cast<void>(gate.dispatch(Event::BeginSave, acknowledged));

    const TransitionResult nested = gate.dispatch(Event::BeginSave);

    EXPECT(nested.status == TransitionStatus::Applied);
    EXPECT(nested.previous_state == State::SaveInProgress);
    EXPECT(nested.current_state == State::SaveInProgress);
    EXPECT(nested.generation == 1U);
    EXPECT(!nested.product_mutation_permitted);

    const TransitionResult inner_outcome = gate.dispatch(Event::SaveFailed);

    EXPECT(inner_outcome.status == TransitionStatus::Applied);
    EXPECT(inner_outcome.previous_state == State::SaveInProgress);
    EXPECT(inner_outcome.current_state == State::SaveInProgress);
    EXPECT(inner_outcome.generation == 1U);
    EXPECT(!inner_outcome.product_mutation_permitted);

    const TransitionResult outer_outcome = gate.dispatch(Event::SaveCancelled);

    EXPECT(outer_outcome.status == TransitionStatus::Applied);
    EXPECT(outer_outcome.previous_state == State::SaveInProgress);
    EXPECT(outer_outcome.current_state == State::ReapplyPending);
    EXPECT(outer_outcome.generation == 1U);
    EXPECT(!outer_outcome.product_mutation_permitted);
}

void only_a_verified_barrier_reactivates_after_save()
{
    using namespace enbcore::runtime;

    LifecycleGate gate;
    static_cast<void>(gate.dispatch(Event::Activate));

    TransitionContext acknowledged;
    acknowledged.baseline_restoration_acknowledged = true;
    static_cast<void>(gate.dispatch(Event::BeginSave, acknowledged));
    static_cast<void>(gate.dispatch(Event::SaveSucceeded));

    const TransitionResult early = gate.dispatch(Event::Activate);

    EXPECT(early.status == TransitionStatus::Rejected);
    EXPECT(early.diagnostic == DiagnosticCode::InvalidEventForState);
    EXPECT(early.previous_state == State::ReapplyPending);
    EXPECT(early.current_state == State::ReapplyPending);
    EXPECT(early.generation == 1U);
    EXPECT(!early.product_mutation_permitted);

    const TransitionResult verified = gate.dispatch(Event::ReapplyAtVerifiedBarrier);

    EXPECT(verified.status == TransitionStatus::Applied);
    EXPECT(verified.diagnostic == DiagnosticCode::None);
    EXPECT(verified.previous_state == State::ReapplyPending);
    EXPECT(verified.current_state == State::Active);
    EXPECT(verified.generation == 2U);
    EXPECT(verified.product_mutation_permitted);

    const TransitionResult repeated = gate.dispatch(Event::ReapplyAtVerifiedBarrier);

    EXPECT(repeated.status == TransitionStatus::Rejected);
    EXPECT(repeated.diagnostic == DiagnosticCode::InvalidEventForState);
    EXPECT(repeated.previous_state == State::Active);
    EXPECT(repeated.current_state == State::Active);
    EXPECT(repeated.generation == 2U);
    EXPECT(repeated.product_mutation_permitted);
}

void active_shutdown_requires_a_restored_baseline()
{
    using namespace enbcore::runtime;

    LifecycleGate gate;
    static_cast<void>(gate.dispatch(Event::Activate));

    const TransitionResult rejected = gate.dispatch(Event::RequestShutdown);

    EXPECT(rejected.status == TransitionStatus::Rejected);
    EXPECT(rejected.diagnostic == DiagnosticCode::BaselineRestorationNotAcknowledged);
    EXPECT(rejected.previous_state == State::Active);
    EXPECT(rejected.current_state == State::Active);
    EXPECT(rejected.generation == 1U);
    EXPECT(rejected.product_mutation_permitted);

    TransitionContext acknowledged;
    acknowledged.baseline_restoration_acknowledged = true;
    const TransitionResult shutdown = gate.dispatch(Event::RequestShutdown, acknowledged);

    EXPECT(shutdown.status == TransitionStatus::Applied);
    EXPECT(shutdown.previous_state == State::Active);
    EXPECT(shutdown.current_state == State::Shutdown);
    EXPECT(shutdown.generation == 1U);
    EXPECT(!shutdown.product_mutation_permitted);

    const TransitionResult stopped = gate.dispatch(Event::CompleteShutdown);

    EXPECT(stopped.status == TransitionStatus::Applied);
    EXPECT(stopped.previous_state == State::Shutdown);
    EXPECT(stopped.current_state == State::Stopped);
    EXPECT(stopped.generation == 1U);
    EXPECT(!stopped.product_mutation_permitted);
}

void interruption_during_save_fails_without_reapplication()
{
    using namespace enbcore::runtime;

    LifecycleGate gate;
    static_cast<void>(gate.dispatch(Event::Activate));

    TransitionContext acknowledged;
    acknowledged.baseline_restoration_acknowledged = true;
    static_cast<void>(gate.dispatch(Event::BeginSave, acknowledged));

    const TransitionResult interrupted = gate.dispatch(Event::Interrupt);

    EXPECT(interrupted.status == TransitionStatus::Applied);
    EXPECT(interrupted.previous_state == State::SaveInProgress);
    EXPECT(interrupted.current_state == State::Failed);
    EXPECT(interrupted.generation == 1U);
    EXPECT(!interrupted.product_mutation_permitted);

    const TransitionResult reapply = gate.dispatch(Event::ReapplyAtVerifiedBarrier);

    EXPECT(reapply.status == TransitionStatus::Rejected);
    EXPECT(reapply.previous_state == State::Failed);
    EXPECT(reapply.current_state == State::Failed);
    EXPECT(reapply.generation == 1U);
    EXPECT(!reapply.product_mutation_permitted);
}

void shutdown_while_reapply_is_pending_keeps_mutations_disabled()
{
    using namespace enbcore::runtime;

    LifecycleGate gate;
    static_cast<void>(gate.dispatch(Event::Activate));

    TransitionContext acknowledged;
    acknowledged.baseline_restoration_acknowledged = true;
    static_cast<void>(gate.dispatch(Event::BeginSave, acknowledged));
    static_cast<void>(gate.dispatch(Event::SaveSucceeded));

    const TransitionResult shutdown = gate.dispatch(Event::RequestShutdown);

    EXPECT(shutdown.status == TransitionStatus::Applied);
    EXPECT(shutdown.previous_state == State::ReapplyPending);
    EXPECT(shutdown.current_state == State::Shutdown);
    EXPECT(shutdown.generation == 1U);
    EXPECT(!shutdown.product_mutation_permitted);

    const TransitionResult reapply = gate.dispatch(Event::ReapplyAtVerifiedBarrier);

    EXPECT(reapply.status == TransitionStatus::Rejected);
    EXPECT(reapply.current_state == State::Shutdown);
    EXPECT(reapply.generation == 1U);
    EXPECT(!reapply.product_mutation_permitted);
}

void invalid_events_fail_closed_without_state_change()
{
    using namespace enbcore::runtime;

    LifecycleGate gate;

    const TransitionResult invalid = gate.dispatch(Event::SaveCancelled);

    EXPECT(invalid.status == TransitionStatus::Rejected);
    EXPECT(invalid.diagnostic == DiagnosticCode::InvalidEventForState);
    EXPECT(invalid.previous_state == State::InactiveBootstrap);
    EXPECT(invalid.current_state == State::InactiveBootstrap);
    EXPECT(invalid.generation == 0U);
    EXPECT(!invalid.product_mutation_permitted);
    EXPECT(gate.state() == State::InactiveBootstrap);
    EXPECT(gate.generation() == 0U);
}

} // namespace

int main()
{
    bootstrap_requires_explicit_activation();
    begin_save_requires_baseline_restoration_acknowledgement();
    every_save_outcome_requires_reapplication();
    nested_saves_drain_before_reapplication();
    only_a_verified_barrier_reactivates_after_save();
    active_shutdown_requires_a_restored_baseline();
    interruption_during_save_fails_without_reapplication();
    shutdown_while_reapply_is_pending_keeps_mutations_disabled();
    invalid_events_fail_closed_without_state_change();

    if (failures != 0) {
        std::cerr << failures << " assertion(s) failed\n";
        return 1;
    }

    std::cout << "LifecycleGate tests passed\n";
    return 0;
}
