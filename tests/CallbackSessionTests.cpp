#include <enbcore/runtime/CallbackSession.hpp>
#include <enbcore/runtime/LifecycleGate.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>

namespace {

int failures = 0;

#define EXPECT(expression)                                                        \
    do {                                                                          \
        if (!(expression)) {                                                      \
            std::cerr << __FILE__ << ':' << __LINE__                              \
                      << ": expectation failed: " #expression "\n";             \
            ++failures;                                                           \
        }                                                                         \
    } while (false)

struct FakeHost final {
    enbcore::enb::RenderInfo render_info{};
    enbcore::enb::CallbackFunction registered_callback{nullptr};
    enbcore::enb::CallbackFunction last_registered_callback{nullptr};
    bool render_ready{true};
    bool invoke_during_registration{false};
    bool invoke_during_unregistration{false};
    std::uint32_t render_info_calls{0};
    std::uint32_t register_calls{0};
    std::uint32_t unregister_calls{0};
};

FakeHost* active_host = nullptr;

enbcore::enb::SdkInteger fake_get_sdk_version()
{
    return enbcore::enb::kSdkVersion;
}

enbcore::enb::SdkInteger fake_get_version()
{
    return 504;
}

enbcore::enb::SdkInteger fake_get_game_identifier()
{
    return enbcore::enb::kGameIdentifier;
}

void fake_set_callback(const enbcore::enb::CallbackFunction callback)
{
    if (callback != nullptr) {
        ++active_host->register_calls;
        active_host->registered_callback = callback;
        active_host->last_registered_callback = callback;
        if (active_host->invoke_during_registration) {
            callback(enbcore::enb::CallbackId::OnInit);
        }
        return;
    }

    ++active_host->unregister_calls;
    const enbcore::enb::CallbackFunction previous =
        active_host->registered_callback;
    active_host->registered_callback = nullptr;
    if (active_host->invoke_during_unregistration && previous != nullptr) {
        previous(enbcore::enb::CallbackId::OnExit);
    }
}

enbcore::enb::SdkBoolean fake_get_parameter(
    char*,
    char*,
    char*,
    enbcore::enb::Parameter*)
{
    return 1;
}

enbcore::enb::SdkBoolean fake_set_parameter(
    char*,
    char*,
    char*,
    enbcore::enb::Parameter*)
{
    return 1;
}

enbcore::enb::RenderInfo* fake_get_render_info()
{
    ++active_host->render_info_calls;
    return active_host->render_ready ? &active_host->render_info : nullptr;
}

enbcore::enb::SdkInteger fake_get_state(enbcore::enb::StateId)
{
    return 0;
}

[[nodiscard]] enbcore::enb::SdkExports make_exports()
{
    return enbcore::enb::SdkExports{
        &fake_get_sdk_version,
        &fake_get_version,
        &fake_get_game_identifier,
        &fake_set_callback,
        &fake_get_parameter,
        &fake_set_parameter,
        &fake_get_render_info,
        &fake_get_state,
    };
}

[[nodiscard]] enbcore::enb::HostResolutionResult make_resolution(
    const enbcore::enb::HostResolutionCode code)
{
    enbcore::enb::HostResolutionResult result;
    result.code = code;
    result.module = 0x504U;
    result.exports = make_exports();
    result.exports_resolved = true;
    return result;
}

void registration_publishes_the_target_before_the_exact_callback_is_set()
{
    using namespace enbcore::runtime;

    FakeHost host;
    host.invoke_during_registration = true;
    active_host = &host;

    CallbackSession session;
    const CallbackSessionResult started =
        session.bootstrap(make_resolution(enbcore::enb::HostResolutionCode::Ready));

    EXPECT(started.code == CallbackSessionCode::Active);
    EXPECT(started.state == CallbackSessionState::Active);
    EXPECT(session.active());
    EXPECT(host.register_calls == 1U);
    EXPECT(host.registered_callback != nullptr);

    std::array<CallbackEventRecord, 1> events{};
    EXPECT(session.drain(events) == 1U);
    EXPECT(events[0].kind == CallbackEventKind::OnInit);
    EXPECT(events[0].lifecycle_handoff == LifecycleHandoffKind::Activate);
    EXPECT(events[0].sequence == 1U);

    static_cast<void>(session.stop());
}

void retained_not_ready_exports_are_retried_once_per_explicit_attempt()
{
    using namespace enbcore::runtime;

    FakeHost host;
    host.render_ready = false;
    active_host = &host;

    CallbackSession session;
    const CallbackSessionResult waiting = session.bootstrap(
        make_resolution(enbcore::enb::HostResolutionCode::NotReady));

    EXPECT(waiting.code == CallbackSessionCode::RenderInfoNotReady);
    EXPECT(waiting.state == CallbackSessionState::WaitingForRenderInfo);
    EXPECT(host.render_info_calls == 1U);
    EXPECT(host.register_calls == 0U);

    const CallbackSessionResult still_waiting = session.retryRenderReadiness();

    EXPECT(still_waiting.code == CallbackSessionCode::RenderInfoNotReady);
    EXPECT(host.render_info_calls == 2U);
    EXPECT(host.register_calls == 0U);

    host.render_ready = true;
    const CallbackSessionResult started = session.retryRenderReadiness();

    EXPECT(started.code == CallbackSessionCode::Active);
    EXPECT(host.render_info_calls == 3U);
    EXPECT(host.register_calls == 1U);

    const CallbackSessionResult repeated = session.retryRenderReadiness();

    EXPECT(repeated.code == CallbackSessionCode::AlreadyActive);
    EXPECT(host.render_info_calls == 3U);
    EXPECT(host.register_calls == 1U);

    static_cast<void>(session.stop());
}

void a_second_process_session_is_rejected_without_registering()
{
    using namespace enbcore::runtime;

    FakeHost host;
    active_host = &host;

    CallbackSession first;
    CallbackSession second;

    EXPECT(
        first.bootstrap(make_resolution(enbcore::enb::HostResolutionCode::Ready)).code
        == CallbackSessionCode::Active);
    EXPECT(
        second.bootstrap(make_resolution(enbcore::enb::HostResolutionCode::Ready)).code
        == CallbackSessionCode::DuplicateActiveSession);
    EXPECT(second.state() == CallbackSessionState::WaitingForProcessSlot);
    EXPECT(host.register_calls == 1U);

    static_cast<void>(first.stop());

    EXPECT(second.retryRenderReadiness().code == CallbackSessionCode::Active);
    EXPECT(host.register_calls == 2U);

    static_cast<void>(second.stop());
}

void every_sdk_callback_becomes_one_ordered_neutral_event()
{
    using namespace enbcore::runtime;

    FakeHost host;
    active_host = &host;
    CallbackSession session;
    static_cast<void>(
        session.bootstrap(make_resolution(enbcore::enb::HostResolutionCode::Ready)));

    constexpr std::array callback_ids{
        enbcore::enb::CallbackId::EndFrame,
        enbcore::enb::CallbackId::BeginFrame,
        enbcore::enb::CallbackId::PreSave,
        enbcore::enb::CallbackId::PostLoad,
        enbcore::enb::CallbackId::OnInit,
        enbcore::enb::CallbackId::OnExit,
        enbcore::enb::CallbackId::PreReset,
        enbcore::enb::CallbackId::PostReset,
    };
    constexpr std::array expected_kinds{
        CallbackEventKind::EndFrame,
        CallbackEventKind::BeginFrame,
        CallbackEventKind::PreSave,
        CallbackEventKind::PostLoad,
        CallbackEventKind::OnInit,
        CallbackEventKind::OnExit,
        CallbackEventKind::PreReset,
        CallbackEventKind::PostReset,
    };
    constexpr std::array expected_handoffs{
        LifecycleHandoffKind::None,
        LifecycleHandoffKind::VerifiedFrameBarrierObserved,
        LifecycleHandoffKind::BeginSave,
        LifecycleHandoffKind::None,
        LifecycleHandoffKind::Activate,
        LifecycleHandoffKind::RequestShutdown,
        LifecycleHandoffKind::None,
        LifecycleHandoffKind::None,
    };

    for (const enbcore::enb::CallbackId id : callback_ids) {
        host.registered_callback(id);
    }

    std::array<CallbackEventRecord, callback_ids.size()> events{};
    EXPECT(session.drain(events) == callback_ids.size());
    for (std::size_t index = 0; index < events.size(); ++index) {
        EXPECT(events[index].kind == expected_kinds[index]);
        EXPECT(events[index].lifecycle_handoff == expected_handoffs[index]);
        EXPECT(events[index].sequence == index + 1U);
    }

    const CallbackDiagnostics diagnostics = session.diagnostics();
    EXPECT(diagnostics.callbacks_received == callback_ids.size());
    EXPECT(diagnostics.events_enqueued == callback_ids.size());
    EXPECT(diagnostics.events_drained == callback_ids.size());
    EXPECT(diagnostics.unknown_callback_ids == 0U);
    EXPECT(diagnostics.overflow_events == 0U);

    static_cast<void>(session.stop());
}

void unknown_callback_ids_are_counted_and_not_forwarded()
{
    using namespace enbcore::runtime;

    FakeHost host;
    active_host = &host;
    CallbackSession session;
    static_cast<void>(
        session.bootstrap(make_resolution(enbcore::enb::HostResolutionCode::Ready)));

    host.registered_callback(static_cast<enbcore::enb::CallbackId>(99));
    host.registered_callback(enbcore::enb::CallbackId::PostReset);

    std::array<CallbackEventRecord, 2> events{};
    EXPECT(session.drain(events) == 1U);
    EXPECT(events[0].kind == CallbackEventKind::PostReset);
    EXPECT(events[0].sequence == 2U);

    const CallbackDiagnostics diagnostics = session.diagnostics();
    EXPECT(diagnostics.callbacks_received == 2U);
    EXPECT(diagnostics.events_enqueued == 1U);
    EXPECT(diagnostics.unknown_callback_ids == 1U);
    EXPECT(diagnostics.overflow_events == 0U);

    static_cast<void>(session.stop());
}

void queue_overflow_is_bounded_and_reported()
{
    using namespace enbcore::runtime;

    FakeHost host;
    active_host = &host;
    CallbackSession session;
    static_cast<void>(
        session.bootstrap(make_resolution(enbcore::enb::HostResolutionCode::Ready)));

    constexpr std::size_t extra_callbacks = 3U;
    for (std::size_t index = 0;
         index < CallbackSession::kEventCapacity + extra_callbacks;
         ++index) {
        host.registered_callback(enbcore::enb::CallbackId::EndFrame);
    }

    std::array<CallbackEventRecord, CallbackSession::kEventCapacity + 4U> events{};
    EXPECT(session.drain(events) == CallbackSession::kEventCapacity);

    const CallbackDiagnostics diagnostics = session.diagnostics();
    EXPECT(
        diagnostics.callbacks_received
        == CallbackSession::kEventCapacity + extra_callbacks);
    EXPECT(diagnostics.events_enqueued == CallbackSession::kEventCapacity);
    EXPECT(diagnostics.events_drained == CallbackSession::kEventCapacity);
    EXPECT(diagnostics.overflow_events == extra_callbacks);

    static_cast<void>(session.stop());
}

void stop_unregisters_once_and_late_callbacks_are_safe()
{
    using namespace enbcore::runtime;

    FakeHost host;
    host.invoke_during_unregistration = true;
    active_host = &host;
    CallbackSession session;
    static_cast<void>(
        session.bootstrap(make_resolution(enbcore::enb::HostResolutionCode::Ready)));

    const enbcore::enb::CallbackFunction late_callback =
        host.last_registered_callback;
    const CallbackSessionResult stopped = session.stop();
    const CallbackDiagnostics stopped_diagnostics = session.diagnostics();

    EXPECT(stopped.code == CallbackSessionCode::Stopped);
    EXPECT(stopped.state == CallbackSessionState::Stopped);
    EXPECT(host.unregister_calls == 1U);
    EXPECT(host.registered_callback == nullptr);

    EXPECT(session.stop().code == CallbackSessionCode::AlreadyStopped);
    EXPECT(host.unregister_calls == 1U);

    late_callback(enbcore::enb::CallbackId::OnInit);
    EXPECT(session.diagnostics().callbacks_received ==
           stopped_diagnostics.callbacks_received);

    std::array<CallbackEventRecord, 2> events{};
    EXPECT(session.drain(events) == 0U);
}

void failure_unregisters_and_unpublishes_the_active_session()
{
    using namespace enbcore::runtime;

    FakeHost host;
    active_host = &host;
    CallbackSession failed;
    static_cast<void>(
        failed.bootstrap(make_resolution(enbcore::enb::HostResolutionCode::Ready)));
    const enbcore::enb::CallbackFunction late_callback =
        host.last_registered_callback;

    const CallbackSessionResult result = failed.fail();

    EXPECT(result.code == CallbackSessionCode::Failed);
    EXPECT(result.state == CallbackSessionState::Failed);
    EXPECT(host.unregister_calls == 1U);

    EXPECT(failed.fail().code == CallbackSessionCode::Failed);
    EXPECT(host.unregister_calls == 1U);

    late_callback(enbcore::enb::CallbackId::BeginFrame);
    EXPECT(failed.diagnostics().callbacks_received == 0U);

    CallbackSession replacement;
    EXPECT(
        replacement.bootstrap(
            make_resolution(enbcore::enb::HostResolutionCode::Ready)).code
        == CallbackSessionCode::Active);
    static_cast<void>(replacement.stop());
}

void invalid_resolver_states_fail_without_host_registration()
{
    using namespace enbcore::runtime;

    FakeHost host;
    active_host = &host;

    CallbackSession unresolved;
    enbcore::enb::HostResolutionResult missing;
    missing.code = enbcore::enb::HostResolutionCode::HostNotFound;
    EXPECT(unresolved.bootstrap(missing).code ==
           CallbackSessionCode::InvalidResolution);
    EXPECT(unresolved.state() == CallbackSessionState::Failed);
    EXPECT(host.register_calls == 0U);

    CallbackSession invalid_exports;
    enbcore::enb::HostResolutionResult invalid =
        make_resolution(enbcore::enb::HostResolutionCode::Ready);
    invalid.exports.get_state = nullptr;
    EXPECT(invalid_exports.bootstrap(invalid).code ==
           CallbackSessionCode::InvalidHost);
    EXPECT(invalid_exports.state() == CallbackSessionState::Failed);
    EXPECT(host.register_calls == 0U);
}

void lifecycle_handoffs_are_explicit_and_never_run_in_callback_context()
{
    using namespace enbcore::runtime;

    FakeHost host;
    active_host = &host;
    CallbackSession session;
    LifecycleGate gate;
    static_cast<void>(
        session.bootstrap(make_resolution(enbcore::enb::HostResolutionCode::Ready)));

    host.registered_callback(enbcore::enb::CallbackId::OnInit);
    host.registered_callback(enbcore::enb::CallbackId::PreSave);
    host.registered_callback(enbcore::enb::CallbackId::PostLoad);
    host.registered_callback(enbcore::enb::CallbackId::BeginFrame);
    host.registered_callback(enbcore::enb::CallbackId::OnExit);

    EXPECT(gate.state() == State::InactiveBootstrap);
    EXPECT(gate.generation() == 0U);

    std::array<CallbackEventRecord, 5> events{};
    EXPECT(session.drain(events) == events.size());
    EXPECT(gate.state() == State::InactiveBootstrap);
    EXPECT(events[0].lifecycle_handoff == LifecycleHandoffKind::Activate);
    EXPECT(events[1].lifecycle_handoff == LifecycleHandoffKind::BeginSave);
    EXPECT(events[2].lifecycle_handoff == LifecycleHandoffKind::None);
    EXPECT(
        events[3].lifecycle_handoff
        == LifecycleHandoffKind::VerifiedFrameBarrierObserved);
    EXPECT(events[4].lifecycle_handoff == LifecycleHandoffKind::RequestShutdown);

    EXPECT(gate.dispatch(Event::Activate).status == TransitionStatus::Applied);
    EXPECT(gate.dispatch(Event::BeginSave).diagnostic ==
           DiagnosticCode::BaselineRestorationNotAcknowledged);

    TransitionContext restored;
    restored.baseline_restoration_acknowledged = true;
    EXPECT(gate.dispatch(Event::BeginSave, restored).status ==
           TransitionStatus::Applied);

    // PostLoad is notification-only. The caller must supply an explicit save
    // outcome; there is intentionally no invented PostSave callback.
    EXPECT(gate.state() == State::SaveInProgress);
    EXPECT(gate.dispatch(Event::SaveSucceeded).status ==
           TransitionStatus::Applied);
    EXPECT(gate.state() == State::ReapplyPending);
    EXPECT(gate.dispatch(Event::ReapplyAtVerifiedBarrier).status ==
           TransitionStatus::Applied);

    EXPECT(gate.dispatch(Event::RequestShutdown, restored).status ==
           TransitionStatus::Applied);

    static_cast<void>(session.stop());
}

} // namespace

int main()
{
    registration_publishes_the_target_before_the_exact_callback_is_set();
    retained_not_ready_exports_are_retried_once_per_explicit_attempt();
    a_second_process_session_is_rejected_without_registering();
    every_sdk_callback_becomes_one_ordered_neutral_event();
    unknown_callback_ids_are_counted_and_not_forwarded();
    queue_overflow_is_bounded_and_reported();
    stop_unregisters_once_and_late_callbacks_are_safe();
    failure_unregisters_and_unpublishes_the_active_session();
    invalid_resolver_states_fail_without_host_registration();
    lifecycle_handoffs_are_explicit_and_never_run_in_callback_context();

    if (failures != 0) {
        std::cerr << failures << " assertion(s) failed\n";
        return 1;
    }

    std::cout << "CallbackSession tests passed\n";
    return 0;
}
