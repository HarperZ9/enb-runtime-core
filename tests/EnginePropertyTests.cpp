#include <enbcore/skyrim/EngineProperties.hpp>
#include <enbcore/skyrim/ObjectPropertyJournal.hpp>

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

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

using namespace enbcore::skyrim;

constexpr std::uintptr_t kModuleBase = 0x140000000ULL;
constexpr std::size_t kModuleSize = 0x100000U;
constexpr std::uintptr_t kOwnerAddress = 0x000001E000001000ULL;
constexpr std::uintptr_t kFieldAddress = kOwnerAddress + 0x120U;
constexpr std::uint64_t kOwnerGeneration = 7U;
constexpr RuntimeVersion kRuntime{1, 6, 1170, 0};
constexpr Sha256Digest kExecutableDigest{
    0xC4U, 0x34U, 0x20U, 0x88U, 0x94U, 0xF0U, 0x7FU, 0x60U,
    0x4BU, 0x85U, 0x2FU, 0x29U, 0xB8U, 0xEDU, 0xC3U, 0xA5U,
    0x8CU, 0x4DU, 0xE6U, 0x3DU, 0xE7U, 0x83U, 0x37U, 0x37U,
    0x33U, 0xE7U, 0x2BU, 0x2BU, 0x73U, 0xF3U, 0x3BU, 0xE9U,
};

ExecutableIdentity MakeIdentity()
{
    return ExecutableIdentity{
        L"SkyrimSE.exe",
        L"TESV.exe",
        kRuntime,
        ExecutableArchitecture::X64,
        kExecutableDigest,
        37'157'144U,
        kModuleBase,
        kModuleSize,
    };
}

RuntimeEvaluation AdmitRuntime()
{
    return EvaluateRuntimeIdentity(MakeIdentity(), SupportedSkyrimRuntimes());
}

CapabilityReport ReadyReport(const Capability ready)
{
    CapabilityReport report;
    for (std::size_t index = 0; index < report.entries.size(); ++index) {
        report.entries[index] = CapabilityEntry{
            static_cast<Capability>(index),
            CapabilityState::Unavailable,
            SymbolDiagnostic::MissingDescriptor,
        };
    }
    const auto index = static_cast<std::size_t>(ready);
    report.entries[index] = CapabilityEntry{
        ready,
        CapabilityState::ObserveReady,
        SymbolDiagnostic::None,
    };
    return report;
}

class FakePropertyAdapter final : public EnginePropertyAdapter {
public:
    EnginePropertyId observed_id{EnginePropertyId::CameraWorldFov};
    EnginePropertyValue observed_value{75.0F};
    bool observe_success{true};
    std::optional<ObjectPropertyBinding> binding{ObjectPropertyBinding{
        EnginePropertyId::CameraWorldFov,
        ObjectOwnerToken{kOwnerAddress, kOwnerGeneration},
        kFieldAddress,
    }};
    mutable std::size_t observe_calls{0};
    mutable std::size_t binding_calls{0};

    [[nodiscard]] bool Observe(
        const EnginePropertyId id,
        EnginePropertyValue& value) const noexcept override
    {
        ++observe_calls;
        if (!observe_success || id != observed_id) {
            return false;
        }
        value = observed_value;
        return true;
    }

    [[nodiscard]] std::optional<ObjectPropertyBinding> ResolveWritableBinding(
        const EnginePropertyId id) const noexcept override
    {
        ++binding_calls;
        if (!binding.has_value() || binding->property != id) {
            return std::nullopt;
        }
        return binding;
    }
};

class FakeObjectMemory final : public ObjectPropertyMemoryView {
public:
    MemoryRegion region{kOwnerAddress, 0x1000U, true, true, false};
    ObjectOwnerToken current_owner{kOwnerAddress, kOwnerGeneration};
    EnginePropertyId declared_property{EnginePropertyId::CameraWorldFov};
    std::uintptr_t declared_field{kFieldAddress};
    bool owner_current{true};
    bool field_declared{true};
    bool thread_permitted{true};
    ObjectMutationPhase phase{ObjectMutationPhase::MainThreadPostUpdate};
    std::size_t write_calls{0};
    std::optional<std::size_t> partial_failure_call;

    void Put(const std::uintptr_t address, const std::span<const std::uint8_t> bytes)
    {
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            bytes_[address + index] = bytes[index];
        }
    }

    [[nodiscard]] std::vector<std::uint8_t> Get(
        const std::uintptr_t address,
        const std::size_t size) const
    {
        std::vector<std::uint8_t> result(size);
        for (std::size_t index = 0; index < size; ++index) {
            const auto found = bytes_.find(address + index);
            result[index] = found == bytes_.end() ? 0U : found->second;
        }
        return result;
    }

    [[nodiscard]] std::optional<std::uintptr_t> ResolveRelocationId(
        std::uint64_t) const noexcept override
    {
        return std::nullopt;
    }

    [[nodiscard]] std::optional<MemoryRegion> QueryRegion(
        const std::uintptr_t address,
        const std::size_t size) const noexcept override
    {
        if (address < region.base || size > region.size
            || address - region.base > region.size - size) {
            return std::nullopt;
        }
        return region;
    }

    [[nodiscard]] bool Read(
        const std::uintptr_t address,
        const std::span<std::uint8_t> destination) const noexcept override
    {
        for (std::size_t index = 0; index < destination.size(); ++index) {
            const auto found = bytes_.find(address + index);
            if (found == bytes_.end()) {
                return false;
            }
            destination[index] = found->second;
        }
        return true;
    }

    [[nodiscard]] bool MatchesRtti(
        std::uintptr_t,
        std::string_view) const noexcept override
    {
        return false;
    }

    [[nodiscard]] bool Write(
        const std::uintptr_t address,
        const std::span<const std::uint8_t> source) noexcept override
    {
        ++write_calls;
        if (partial_failure_call.has_value()
            && write_calls == *partial_failure_call) {
            if (!source.empty()) {
                Put(address, source.first(1U));
            }
            return false;
        }
        Put(address, source);
        return true;
    }

    [[nodiscard]] bool IsOwnerCurrent(
        const ObjectOwnerToken owner) const noexcept override
    {
        return owner_current && owner == current_owner;
    }

    [[nodiscard]] bool IsDeclaredField(
        const EnginePropertyId property,
        const ObjectOwnerToken owner,
        const std::uintptr_t address,
        const std::size_t size) const noexcept override
    {
        return field_declared
            && owner == current_owner
            && property == declared_property
            && address == declared_field
            && size == sizeof(float);
    }

    [[nodiscard]] bool IsMutationThreadPermitted() const noexcept override
    {
        return thread_permitted;
    }

    [[nodiscard]] ObjectMutationPhase CurrentMutationPhase() const noexcept override
    {
        return phase;
    }

private:
    std::unordered_map<std::uintptr_t, std::uint8_t> bytes_;
};

ObjectMutationPrerequisites MutationPrerequisites()
{
    return ObjectMutationPrerequisites{
        true,
        true,
        AdmitRuntime(),
        CapabilityEntry{
            Capability::CameraInverseViewProjection,
            CapabilityState::HookReady,
            SymbolDiagnostic::None,
        },
        ObjectMutationPhase::MainThreadPostUpdate,
    };
}

void property_names_and_types_are_explicit_and_unknown_names_reject()
{
    const std::span schemas = SupportedEngineProperties();
    constexpr std::array<std::string_view, 7> expected_names{
        "camera.world_fov",
        "camera.first_person_fov",
        "camera.inverse_view_projection",
        "weather.current_form_id",
        "weather.outgoing_form_id",
        "weather.transition",
        "calendar.game_hour",
    };
    EXPECT(schemas.size() == 7U);
    if (schemas.size() == expected_names.size()) {
        for (std::size_t index = 0; index < expected_names.size(); ++index) {
            EXPECT(schemas[index].name == expected_names[index]);
            EXPECT(FindEngineProperty(expected_names[index]).has_value());
        }
        EXPECT(schemas[0].access
            == EnginePropertyAccess::ObserveAndReversibleObjectWrite);
        EXPECT(schemas[1].access
            == EnginePropertyAccess::ObserveAndReversibleObjectWrite);
        for (std::size_t index = 2; index < schemas.size(); ++index) {
            EXPECT(schemas[index].access == EnginePropertyAccess::ObserveOnly);
        }
    }

    const auto fov = FindEngineProperty("camera.world_fov");
    EXPECT(fov.has_value());
    EXPECT(fov->id == EnginePropertyId::CameraWorldFov);
    EXPECT(fov->value_kind == EnginePropertyValueKind::Float32);
    EXPECT(fov->access == EnginePropertyAccess::ObserveAndReversibleObjectWrite);
    EXPECT(fov->capability == Capability::CameraInverseViewProjection);

    EXPECT(!FindEngineProperty("CameraFOV").has_value());
    EXPECT(!FindEngineProperty("camera.unknown").has_value());
    EXPECT(!FindEngineProperty("").has_value());
}

void observation_requires_exact_runtime_capability_type_and_range()
{
    FakePropertyAdapter adapter;
    const PropertyObservation accepted = ObserveEngineProperty(
        MakeIdentity(),
        AdmitRuntime(),
        ReadyReport(Capability::CameraInverseViewProjection),
        "camera.world_fov",
        adapter);
    EXPECT(accepted.diagnostic == EnginePropertyDiagnostic::None);
    EXPECT(accepted.observed);
    EXPECT(std::get<float>(accepted.value) == 75.0F);
    EXPECT(adapter.observe_calls == 1U);

    const PropertyObservation unknown = ObserveEngineProperty(
        MakeIdentity(),
        AdmitRuntime(),
        ReadyReport(Capability::CameraInverseViewProjection),
        "camera.typo",
        adapter);
    EXPECT(unknown.diagnostic == EnginePropertyDiagnostic::UnknownIdentifier);
    EXPECT(!unknown.observed);
    EXPECT(adapter.observe_calls == 1U);

    RuntimeEvaluation incomplete = AdmitRuntime();
    incomplete.matched_record = NoMatchedRuntimeRecord;
    const PropertyObservation wrong_runtime = ObserveEngineProperty(
        MakeIdentity(),
        incomplete,
        ReadyReport(Capability::CameraInverseViewProjection),
        "camera.world_fov",
        adapter);
    EXPECT(wrong_runtime.diagnostic == EnginePropertyDiagnostic::RuntimeNotAdmitted);
    EXPECT(adapter.observe_calls == 1U);

    const PropertyObservation unavailable = ObserveEngineProperty(
        MakeIdentity(),
        AdmitRuntime(),
        ReadyReport(Capability::WeatherTimeOfDay),
        "camera.world_fov",
        adapter);
    EXPECT(unavailable.diagnostic == EnginePropertyDiagnostic::CapabilityUnavailable);
    EXPECT(adapter.observe_calls == 1U);

    adapter.observed_value = std::uint32_t{75U};
    const PropertyObservation wrong_type = ObserveEngineProperty(
        MakeIdentity(),
        AdmitRuntime(),
        ReadyReport(Capability::CameraInverseViewProjection),
        "camera.world_fov",
        adapter);
    EXPECT(wrong_type.diagnostic == EnginePropertyDiagnostic::ValueTypeMismatch);

    adapter.observed_value = std::numeric_limits<float>::quiet_NaN();
    const PropertyObservation invalid_value = ObserveEngineProperty(
        MakeIdentity(),
        AdmitRuntime(),
        ReadyReport(Capability::CameraInverseViewProjection),
        "camera.world_fov",
        adapter);
    EXPECT(invalid_value.diagnostic == EnginePropertyDiagnostic::ValueOutOfRange);
}

void weather_and_time_are_observer_only_in_the_first_slice()
{
    FakePropertyAdapter adapter;
    adapter.observed_id = EnginePropertyId::WeatherTransition;
    adapter.observed_value = 0.5F;
    const PropertyObservation weather = ObserveEngineProperty(
        MakeIdentity(),
        AdmitRuntime(),
        ReadyReport(Capability::WeatherTimeOfDay),
        "weather.transition",
        adapter);
    EXPECT(weather.diagnostic == EnginePropertyDiagnostic::None);
    EXPECT(weather.observed);

    FakeObjectMemory memory;
    ObjectPropertyJournal journal;
    const PropertyMutationResult mutation = ApplyEnginePropertyMutation(
        MakeIdentity(),
        MutationPrerequisites(),
        "weather.transition",
        EnginePropertyValue{0.75F},
        adapter,
        memory,
        journal);
    EXPECT(mutation.diagnostic == EnginePropertyDiagnostic::MutationNotAllowed);
    EXPECT(journal.state() == ObjectPropertyState::Pristine);
    EXPECT(memory.write_calls == 0U);

    const PropertyMutationResult unknown = ApplyEnginePropertyMutation(
        MakeIdentity(),
        MutationPrerequisites(),
        "camera.typo",
        EnginePropertyValue{75.0F},
        adapter,
        memory,
        journal);
    EXPECT(unknown.diagnostic == EnginePropertyDiagnostic::UnknownIdentifier);
    EXPECT(adapter.binding_calls == 0U);
    EXPECT(memory.write_calls == 0U);

    adapter.binding = ObjectPropertyBinding{
        EnginePropertyId::CameraWorldFov,
        ObjectOwnerToken{kOwnerAddress, kOwnerGeneration},
        kFieldAddress,
    };
    const PropertyMutationResult out_of_range = ApplyEnginePropertyMutation(
        MakeIdentity(),
        MutationPrerequisites(),
        "camera.world_fov",
        EnginePropertyValue{200.0F},
        adapter,
        memory,
        journal);
    EXPECT(out_of_range.diagnostic == EnginePropertyDiagnostic::ValueOutOfRange);
    EXPECT(adapter.binding_calls == 0U);
    EXPECT(memory.write_calls == 0U);
}

void camera_fov_object_mutation_captures_and_restores_the_exact_heap_baseline()
{
    FakePropertyAdapter adapter;
    FakeObjectMemory memory;
    constexpr float baseline = 75.0F;
    constexpr float replacement = 82.5F;
    const auto baseline_bytes = std::bit_cast<std::array<std::uint8_t, 4>>(baseline);
    const auto replacement_bytes = std::bit_cast<std::array<std::uint8_t, 4>>(replacement);
    memory.Put(kFieldAddress, baseline_bytes);

    ObjectPropertyJournal journal;
    const PropertyMutationResult applied = ApplyEnginePropertyMutation(
        MakeIdentity(),
        MutationPrerequisites(),
        "camera.world_fov",
        EnginePropertyValue{replacement},
        adapter,
        memory,
        journal);
    EXPECT(applied.diagnostic == EnginePropertyDiagnostic::None);
    EXPECT(applied.transaction.code == ObjectPropertyCode::Applied);
    EXPECT(applied.transaction.state == ObjectPropertyState::Applied);
    EXPECT(memory.Get(kFieldAddress, replacement_bytes.size())
        == std::vector<std::uint8_t>(replacement_bytes.begin(), replacement_bytes.end()));
    EXPECT(memory.write_calls == 1U);

    const PropertyMutationResult repeated = ApplyEnginePropertyMutation(
        MakeIdentity(),
        MutationPrerequisites(),
        "camera.world_fov",
        EnginePropertyValue{replacement},
        adapter,
        memory,
        journal);
    EXPECT(repeated.diagnostic == EnginePropertyDiagnostic::None);
    EXPECT(repeated.transaction.code == ObjectPropertyCode::AlreadyApplied);
    EXPECT(memory.write_calls == 1U);

    const ObjectPropertyResult restored = journal.Rollback(memory);
    const ObjectPropertyResult repeated_restore = journal.Rollback(memory);
    EXPECT(restored.code == ObjectPropertyCode::RolledBack);
    EXPECT(restored.state == ObjectPropertyState::Pristine);
    EXPECT(repeated_restore.code == ObjectPropertyCode::AlreadyPristine);
    EXPECT(repeated_restore.state == ObjectPropertyState::Pristine);
    EXPECT(memory.Get(kFieldAddress, baseline_bytes.size())
        == std::vector<std::uint8_t>(baseline_bytes.begin(), baseline_bytes.end()));
    EXPECT(memory.write_calls == 2U);
}

void owner_generation_thread_phase_and_field_schema_all_gate_writes()
{
    constexpr float baseline = 75.0F;
    const auto baseline_bytes = std::bit_cast<std::array<std::uint8_t, 4>>(baseline);

    const auto attempt = [&](FakeObjectMemory& memory, FakePropertyAdapter& adapter) {
        memory.Put(kFieldAddress, baseline_bytes);
        ObjectPropertyJournal journal;
        return ApplyEnginePropertyMutation(
            MakeIdentity(),
            MutationPrerequisites(),
            "camera.world_fov",
            EnginePropertyValue{80.0F},
            adapter,
            memory,
            journal);
    };

    FakePropertyAdapter adapter;
    FakeObjectMemory memory;
    memory.Put(kFieldAddress, baseline_bytes);
    ObjectMutationPrerequisites observer_only = MutationPrerequisites();
    observer_only.capability.state = CapabilityState::ObserveReady;
    ObjectPropertyJournal observer_journal;
    PropertyMutationResult result = ApplyEnginePropertyMutation(
        MakeIdentity(),
        observer_only,
        "camera.world_fov",
        EnginePropertyValue{80.0F},
        adapter,
        memory,
        observer_journal);
    EXPECT(result.transaction.code == ObjectPropertyCode::CapabilityUnavailable);
    EXPECT(memory.write_calls == 0U);

    memory = FakeObjectMemory{};
    memory.owner_current = false;
    result = attempt(memory, adapter);
    EXPECT(result.transaction.code == ObjectPropertyCode::OwnerUnavailable);
    EXPECT(memory.write_calls == 0U);

    memory = FakeObjectMemory{};
    memory.thread_permitted = false;
    result = attempt(memory, adapter);
    EXPECT(result.transaction.code == ObjectPropertyCode::ThreadNotPermitted);
    EXPECT(memory.write_calls == 0U);

    memory = FakeObjectMemory{};
    memory.phase = ObjectMutationPhase::FrameBoundary;
    result = attempt(memory, adapter);
    EXPECT(result.transaction.code == ObjectPropertyCode::PhaseMismatch);
    EXPECT(memory.write_calls == 0U);

    memory = FakeObjectMemory{};
    memory.field_declared = false;
    result = attempt(memory, adapter);
    EXPECT(result.transaction.code == ObjectPropertyCode::FieldSchemaMismatch);
    EXPECT(memory.write_calls == 0U);
}

void rollback_never_writes_after_owner_invalidation()
{
    FakePropertyAdapter adapter;
    FakeObjectMemory memory;
    constexpr float baseline = 75.0F;
    const auto baseline_bytes = std::bit_cast<std::array<std::uint8_t, 4>>(baseline);
    memory.Put(kFieldAddress, baseline_bytes);
    ObjectPropertyJournal journal;
    const PropertyMutationResult applied = ApplyEnginePropertyMutation(
        MakeIdentity(),
        MutationPrerequisites(),
        "camera.world_fov",
        EnginePropertyValue{80.0F},
        adapter,
        memory,
        journal);
    EXPECT(applied.transaction.code == ObjectPropertyCode::Applied);

    memory.owner_current = false;
    const ObjectPropertyResult rollback = journal.Rollback(memory);
    EXPECT(rollback.code == ObjectPropertyCode::OwnerInvalidatedBeforeRollback);
    EXPECT(rollback.state == ObjectPropertyState::RecoveryRequired);
    EXPECT(memory.write_calls == 1U);
}

void a_partial_rollback_never_claims_that_the_baseline_was_restored()
{
    FakePropertyAdapter adapter;
    FakeObjectMemory memory;
    constexpr float baseline = 75.1F;
    constexpr float replacement = 80.2F;
    const auto baseline_bytes = std::bit_cast<std::array<std::uint8_t, 4>>(baseline);
    memory.Put(kFieldAddress, baseline_bytes);

    ObjectPropertyJournal journal;
    const PropertyMutationResult applied = ApplyEnginePropertyMutation(
        MakeIdentity(),
        MutationPrerequisites(),
        "camera.world_fov",
        EnginePropertyValue{replacement},
        adapter,
        memory,
        journal);
    EXPECT(applied.transaction.code == ObjectPropertyCode::Applied);

    memory.partial_failure_call = 2U;
    const ObjectPropertyResult rollback = journal.Rollback(memory);
    EXPECT(rollback.code == ObjectPropertyCode::RollbackFailedRecoveryRequired);
    EXPECT(rollback.state == ObjectPropertyState::RecoveryRequired);
    EXPECT(rollback.compensation_performed);
    EXPECT(journal.state() == ObjectPropertyState::RecoveryRequired);
    EXPECT(memory.write_calls == 2U);
}

void a_false_write_with_partial_mutation_is_explicitly_restored_and_verified()
{
    FakePropertyAdapter adapter;
    FakeObjectMemory memory;
    constexpr float baseline = 75.0F;
    const auto baseline_bytes = std::bit_cast<std::array<std::uint8_t, 4>>(baseline);
    memory.Put(kFieldAddress, baseline_bytes);
    memory.partial_failure_call = 1U;

    ObjectPropertyJournal journal;
    const PropertyMutationResult result = ApplyEnginePropertyMutation(
        MakeIdentity(),
        MutationPrerequisites(),
        "camera.world_fov",
        EnginePropertyValue{80.0F},
        adapter,
        memory,
        journal);

    EXPECT(result.diagnostic == EnginePropertyDiagnostic::TransactionRejected);
    EXPECT(result.transaction.code == ObjectPropertyCode::ApplyFailedRolledBack);
    EXPECT(result.transaction.state == ObjectPropertyState::Pristine);
    EXPECT(result.transaction.compensation_performed);
    EXPECT(journal.state() == ObjectPropertyState::Pristine);
    EXPECT(memory.Get(kFieldAddress, baseline_bytes.size())
        == std::vector<std::uint8_t>(baseline_bytes.begin(), baseline_bytes.end()));
    EXPECT(memory.write_calls == 2U);
}

void an_invalid_live_baseline_rejects_before_the_first_write()
{
    FakePropertyAdapter adapter;
    FakeObjectMemory memory;
    const float invalid = std::numeric_limits<float>::quiet_NaN();
    const auto invalid_bytes = std::bit_cast<std::array<std::uint8_t, 4>>(invalid);
    memory.Put(kFieldAddress, invalid_bytes);

    ObjectPropertyJournal journal;
    const PropertyMutationResult result = ApplyEnginePropertyMutation(
        MakeIdentity(),
        MutationPrerequisites(),
        "camera.world_fov",
        EnginePropertyValue{80.0F},
        adapter,
        memory,
        journal);

    EXPECT(result.diagnostic == EnginePropertyDiagnostic::TransactionRejected);
    EXPECT(result.transaction.code == ObjectPropertyCode::BaselineInvalid);
    EXPECT(result.transaction.state == ObjectPropertyState::Pristine);
    EXPECT(memory.write_calls == 0U);
}

} // namespace

int main()
{
    property_names_and_types_are_explicit_and_unknown_names_reject();
    observation_requires_exact_runtime_capability_type_and_range();
    weather_and_time_are_observer_only_in_the_first_slice();
    camera_fov_object_mutation_captures_and_restores_the_exact_heap_baseline();
    owner_generation_thread_phase_and_field_schema_all_gate_writes();
    rollback_never_writes_after_owner_invalidation();
    a_partial_rollback_never_claims_that_the_baseline_was_restored();
    a_false_write_with_partial_mutation_is_explicitly_restored_and_verified();
    an_invalid_live_baseline_rejects_before_the_first_write();

    if (failures != 0) {
        std::cerr << failures << " assertion(s) failed\n";
        return 1;
    }
    std::cout << "Engine property tests passed\n";
    return 0;
}
