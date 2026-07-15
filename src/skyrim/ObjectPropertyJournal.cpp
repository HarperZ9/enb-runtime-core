#include <enbcore/skyrim/ObjectPropertyJournal.hpp>

#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>

namespace enbcore::skyrim {
namespace {

[[nodiscard]] constexpr bool Contains(
    const std::uintptr_t base,
    const std::size_t extent,
    const std::uintptr_t address,
    const std::size_t size) noexcept
{
    if (base == 0U || extent == 0U || size == 0U || address < base) {
        return false;
    }
    const auto offset = address - base;
    return offset < extent && size <= extent - offset;
}

[[nodiscard]] ObjectPropertyResult Result(
    const ObjectPropertyCode code,
    const ObjectPropertyState state,
    const bool compensation = false) noexcept
{
    return ObjectPropertyResult{code, state, compensation};
}

[[nodiscard]] std::size_t ValueSize(
    const EnginePropertyValueKind kind) noexcept
{
    switch (kind) {
    case EnginePropertyValueKind::Float32:
        return sizeof(float);
    case EnginePropertyValueKind::UInt32:
        return sizeof(std::uint32_t);
    case EnginePropertyValueKind::Matrix4x4:
        return sizeof(Matrix4x4);
    default:
        return 0U;
    }
}

[[nodiscard]] bool ReadEquals(
    const MemoryView& memory,
    const std::uintptr_t address,
    const std::span<const std::uint8_t> expected,
    const std::span<std::uint8_t> scratch) noexcept
{
    if (scratch.size() < expected.size()) {
        return false;
    }
    const auto observed = scratch.first(expected.size());
    return memory.Read(address, observed)
        && std::ranges::equal(observed, expected);
}

[[nodiscard]] bool CapabilityMatches(
    const CapabilityEntry& capability,
    const EnginePropertySchema& schema) noexcept
{
    return capability.capability == schema.capability
        && capability.state == CapabilityState::HookReady
        && capability.diagnostic == SymbolDiagnostic::None;
}

[[nodiscard]] bool EncodeValue(
    const EnginePropertySchema& schema,
    const EnginePropertyValue& value,
    const std::span<std::uint8_t> output) noexcept
{
    const std::size_t expected_size = ValueSize(schema.value_kind);
    if (expected_size == 0U || output.size() != expected_size) {
        return false;
    }
    switch (schema.value_kind) {
    case EnginePropertyValueKind::Float32: {
        const float* typed = std::get_if<float>(&value);
        if (typed == nullptr) {
            return false;
        }
        const auto bytes = std::bit_cast<std::array<std::uint8_t, sizeof(float)>>(*typed);
        std::ranges::copy(bytes, output.begin());
        return true;
    }
    case EnginePropertyValueKind::UInt32: {
        const std::uint32_t* typed = std::get_if<std::uint32_t>(&value);
        if (typed == nullptr) {
            return false;
        }
        const auto bytes =
            std::bit_cast<std::array<std::uint8_t, sizeof(std::uint32_t)>>(*typed);
        std::ranges::copy(bytes, output.begin());
        return true;
    }
    case EnginePropertyValueKind::Matrix4x4: {
        const Matrix4x4* typed = std::get_if<Matrix4x4>(&value);
        if (typed == nullptr) {
            return false;
        }
        std::memcpy(output.data(), typed->data(), sizeof(Matrix4x4));
        return true;
    }
    default:
        return false;
    }
}

[[nodiscard]] bool BaselineIsValid(
    const EnginePropertySchema& schema,
    const std::span<const std::uint8_t> bytes) noexcept
{
    if (bytes.size() != ValueSize(schema.value_kind)) {
        return false;
    }

    EnginePropertyValue value{0.0F};
    switch (schema.value_kind) {
    case EnginePropertyValueKind::Float32: {
        float typed = 0.0F;
        std::memcpy(&typed, bytes.data(), sizeof(typed));
        value = typed;
        break;
    }
    case EnginePropertyValueKind::UInt32: {
        std::uint32_t typed = 0U;
        std::memcpy(&typed, bytes.data(), sizeof(typed));
        value = typed;
        break;
    }
    case EnginePropertyValueKind::Matrix4x4: {
        Matrix4x4 typed{};
        std::memcpy(typed.data(), bytes.data(), sizeof(typed));
        value = typed;
        break;
    }
    default:
        return false;
    }
    return ValidateEnginePropertyValue(schema, value)
        == EnginePropertyDiagnostic::None;
}

} // namespace

ObjectPropertyState ObjectPropertyJournal::state() const noexcept
{
    return state_;
}

ObjectPropertyResult ObjectPropertyJournal::Apply(
    const ExecutableIdentity& identity,
    const ObjectMutationPrerequisites& prerequisites,
    const ObjectPropertyBinding& binding,
    const std::span<const std::uint8_t> replacement,
    ObjectPropertyMemoryView& memory) noexcept
{
    if (!prerequisites.feature_gate_enabled) {
        return Result(ObjectPropertyCode::FeatureGateDisabled, state_);
    }
    if (!prerequisites.lifecycle_mutation_permitted) {
        return Result(ObjectPropertyCode::LifecycleMutationDenied, state_);
    }
    if (!MatchesSupportedRuntime(identity, prerequisites.runtime_evaluation)) {
        return Result(ObjectPropertyCode::RuntimeNotAdmitted, state_);
    }
    if (state_ == ObjectPropertyState::RecoveryRequired) {
        return Result(ObjectPropertyCode::JournalConflict, state_);
    }

    const std::optional schema = FindEngineProperty(binding.property);
    if (!schema.has_value()
        || schema->access != EnginePropertyAccess::ObserveAndReversibleObjectWrite) {
        return Result(ObjectPropertyCode::MutationNotAllowed, state_);
    }
    if (!CapabilityMatches(prerequisites.capability, *schema)) {
        return Result(ObjectPropertyCode::CapabilityUnavailable, state_);
    }
    const std::size_t expected_size = ValueSize(schema->value_kind);
    if (binding.owner.address == 0U
        || binding.owner.generation == 0U
        || binding.field_address == 0U) {
        return Result(ObjectPropertyCode::InvalidBinding, state_);
    }
    if (replacement.size() != expected_size
        || replacement.empty()
        || replacement.size() > MaximumPropertySize) {
        return Result(ObjectPropertyCode::InvalidReplacement, state_);
    }
    if (!memory.IsMutationThreadPermitted()) {
        return Result(ObjectPropertyCode::ThreadNotPermitted, state_);
    }
    if (prerequisites.required_phase == ObjectMutationPhase::None
        || memory.CurrentMutationPhase() != prerequisites.required_phase) {
        return Result(ObjectPropertyCode::PhaseMismatch, state_);
    }
    if (!memory.IsOwnerCurrent(binding.owner)) {
        return Result(ObjectPropertyCode::OwnerUnavailable, state_);
    }
    if (!memory.IsDeclaredField(
            binding.property,
            binding.owner,
            binding.field_address,
            replacement.size())) {
        return Result(ObjectPropertyCode::FieldSchemaMismatch, state_);
    }

    const auto region = memory.QueryRegion(binding.field_address, replacement.size());
    if (!region.has_value()
        || !Contains(
            region->base,
            region->size,
            binding.field_address,
            replacement.size())) {
        return Result(ObjectPropertyCode::RegionUnavailable, state_);
    }
    if (!region->readable) {
        return Result(ObjectPropertyCode::RegionNotReadable, state_);
    }
    if (!region->writable) {
        return Result(ObjectPropertyCode::RegionNotWritable, state_);
    }
    if (region->executable) {
        return Result(ObjectPropertyCode::ExecutableRegionRejected, state_);
    }

    std::array<std::uint8_t, MaximumPropertySize> scratch{};
    if (state_ == ObjectPropertyState::Applied) {
        if (!(binding.owner == binding_.owner)
            || binding.property != binding_.property
            || binding.field_address != binding_.field_address
            || replacement.size() != size_
            || !std::ranges::equal(
                replacement,
                std::span{replacement_}.first(size_))
            || prerequisites.required_phase != phase_
            || !ReadEquals(
                memory,
                binding_.field_address,
                std::span{replacement_}.first(size_),
                scratch)) {
            return Result(ObjectPropertyCode::JournalConflict, state_);
        }
        return Result(ObjectPropertyCode::AlreadyApplied, state_);
    }

    const auto baseline = std::span{baseline_}.first(replacement.size());
    if (!memory.Read(binding.field_address, baseline)) {
        return Result(ObjectPropertyCode::BaselineReadFailed, state_);
    }
    if (!BaselineIsValid(*schema, baseline)) {
        baseline_.fill(0U);
        return Result(ObjectPropertyCode::BaselineInvalid, state_);
    }
    if (std::ranges::equal(baseline, replacement)) {
        baseline_.fill(0U);
        return Result(ObjectPropertyCode::NoChange, state_);
    }
    if (!ReadEquals(memory, binding.field_address, baseline, scratch)) {
        baseline_.fill(0U);
        return Result(ObjectPropertyCode::BaselineReadFailed, state_);
    }

    binding_ = binding;
    phase_ = prerequisites.required_phase;
    size_ = replacement.size();
    std::ranges::copy(replacement, replacement_.begin());

    const bool write_reported_success =
        memory.Write(binding.field_address, replacement);
    const bool replacement_present =
        ReadEquals(memory, binding.field_address, replacement, scratch);
    if (!write_reported_success || !replacement_present) {
        static_cast<void>(memory.Write(binding.field_address, baseline));
        const bool restored =
            ReadEquals(memory, binding.field_address, baseline, scratch);
        if (restored) {
            binding_ = {};
            phase_ = ObjectMutationPhase::None;
            size_ = 0U;
            baseline_.fill(0U);
            replacement_.fill(0U);
            return Result(
                ObjectPropertyCode::ApplyFailedRolledBack,
                ObjectPropertyState::Pristine,
                true);
        }
        state_ = ObjectPropertyState::RecoveryRequired;
        return Result(
            ObjectPropertyCode::ApplyFailedRecoveryRequired,
            state_,
            true);
    }

    state_ = ObjectPropertyState::Applied;
    return Result(ObjectPropertyCode::Applied, state_);
}

ObjectPropertyResult ObjectPropertyJournal::Rollback(
    ObjectPropertyMemoryView& memory) noexcept
{
    if (state_ == ObjectPropertyState::Pristine) {
        return Result(ObjectPropertyCode::AlreadyPristine, state_);
    }
    if (state_ == ObjectPropertyState::RecoveryRequired || size_ == 0U) {
        return Result(ObjectPropertyCode::RollbackFailedRecoveryRequired, state_);
    }
    if (!memory.IsMutationThreadPermitted()) {
        return Result(ObjectPropertyCode::ThreadNotPermitted, state_);
    }
    if (memory.CurrentMutationPhase() != phase_) {
        return Result(ObjectPropertyCode::PhaseMismatch, state_);
    }
    if (!memory.IsOwnerCurrent(binding_.owner)) {
        state_ = ObjectPropertyState::RecoveryRequired;
        return Result(ObjectPropertyCode::OwnerInvalidatedBeforeRollback, state_);
    }
    if (!memory.IsDeclaredField(
            binding_.property,
            binding_.owner,
            binding_.field_address,
            size_)) {
        state_ = ObjectPropertyState::RecoveryRequired;
        return Result(ObjectPropertyCode::FieldSchemaMismatch, state_);
    }

    const auto region = memory.QueryRegion(binding_.field_address, size_);
    if (!region.has_value()
        || !Contains(region->base, region->size, binding_.field_address, size_)
        || !region->readable
        || !region->writable
        || region->executable) {
        return Result(ObjectPropertyCode::RollbackBaselineMismatch, state_);
    }

    std::array<std::uint8_t, MaximumPropertySize> scratch{};
    const auto replacement = std::span{replacement_}.first(size_);
    if (!ReadEquals(memory, binding_.field_address, replacement, scratch)) {
        return Result(ObjectPropertyCode::RollbackBaselineMismatch, state_);
    }
    const auto baseline = std::span{baseline_}.first(size_);
    static_cast<void>(memory.Write(binding_.field_address, baseline));
    if (!ReadEquals(memory, binding_.field_address, baseline, scratch)) {
        if (ReadEquals(memory, binding_.field_address, replacement, scratch)) {
            return Result(ObjectPropertyCode::RollbackFailedStillApplied, state_);
        }
        state_ = ObjectPropertyState::RecoveryRequired;
        return Result(
            ObjectPropertyCode::RollbackFailedRecoveryRequired,
            state_,
            true);
    }

    state_ = ObjectPropertyState::Pristine;
    binding_ = {};
    phase_ = ObjectMutationPhase::None;
    size_ = 0U;
    baseline_.fill(0U);
    replacement_.fill(0U);
    return Result(ObjectPropertyCode::RolledBack, state_);
}

PropertyMutationResult ApplyEnginePropertyMutation(
    const ExecutableIdentity& identity,
    const ObjectMutationPrerequisites& prerequisites,
    const std::string_view name,
    const EnginePropertyValue& value,
    const EnginePropertyAdapter& adapter,
    ObjectPropertyMemoryView& memory,
    ObjectPropertyJournal& journal) noexcept
{
    const std::optional schema = FindEngineProperty(name);
    if (!schema.has_value()) {
        return PropertyMutationResult{
            EnginePropertyDiagnostic::UnknownIdentifier,
            Result(ObjectPropertyCode::InvalidBinding, journal.state()),
        };
    }
    if (schema->access != EnginePropertyAccess::ObserveAndReversibleObjectWrite) {
        return PropertyMutationResult{
            EnginePropertyDiagnostic::MutationNotAllowed,
            Result(ObjectPropertyCode::MutationNotAllowed, journal.state()),
        };
    }
    if (!prerequisites.feature_gate_enabled) {
        return PropertyMutationResult{
            EnginePropertyDiagnostic::TransactionRejected,
            Result(ObjectPropertyCode::FeatureGateDisabled, journal.state()),
        };
    }
    if (!prerequisites.lifecycle_mutation_permitted) {
        return PropertyMutationResult{
            EnginePropertyDiagnostic::TransactionRejected,
            Result(ObjectPropertyCode::LifecycleMutationDenied, journal.state()),
        };
    }
    if (!MatchesSupportedRuntime(identity, prerequisites.runtime_evaluation)) {
        return PropertyMutationResult{
            EnginePropertyDiagnostic::RuntimeNotAdmitted,
            Result(ObjectPropertyCode::RuntimeNotAdmitted, journal.state()),
        };
    }
    if (!CapabilityMatches(prerequisites.capability, *schema)) {
        return PropertyMutationResult{
            EnginePropertyDiagnostic::CapabilityUnavailable,
            Result(ObjectPropertyCode::CapabilityUnavailable, journal.state()),
        };
    }
    if (!memory.IsMutationThreadPermitted()) {
        return PropertyMutationResult{
            EnginePropertyDiagnostic::TransactionRejected,
            Result(ObjectPropertyCode::ThreadNotPermitted, journal.state()),
        };
    }
    if (prerequisites.required_phase == ObjectMutationPhase::None
        || memory.CurrentMutationPhase() != prerequisites.required_phase) {
        return PropertyMutationResult{
            EnginePropertyDiagnostic::TransactionRejected,
            Result(ObjectPropertyCode::PhaseMismatch, journal.state()),
        };
    }
    const EnginePropertyDiagnostic validation =
        ValidateEnginePropertyValue(*schema, value);
    if (validation != EnginePropertyDiagnostic::None) {
        return PropertyMutationResult{
            validation,
            Result(ObjectPropertyCode::InvalidReplacement, journal.state()),
        };
    }
    const std::optional binding = adapter.ResolveWritableBinding(schema->id);
    if (!binding.has_value() || binding->property != schema->id) {
        return PropertyMutationResult{
            EnginePropertyDiagnostic::BindingUnavailable,
            Result(ObjectPropertyCode::InvalidBinding, journal.state()),
        };
    }

    const std::size_t size = ValueSize(schema->value_kind);
    std::array<std::uint8_t, sizeof(Matrix4x4)> encoded{};
    if (size == 0U || !EncodeValue(*schema, value, std::span{encoded}.first(size))) {
        return PropertyMutationResult{
            EnginePropertyDiagnostic::ValueTypeMismatch,
            Result(ObjectPropertyCode::InvalidReplacement, journal.state()),
        };
    }
    const ObjectPropertyResult transaction = journal.Apply(
        identity,
        prerequisites,
        *binding,
        std::span{encoded}.first(size),
        memory);
    const bool accepted = transaction.code == ObjectPropertyCode::Applied
        || transaction.code == ObjectPropertyCode::AlreadyApplied
        || transaction.code == ObjectPropertyCode::NoChange;
    return PropertyMutationResult{
        accepted
            ? EnginePropertyDiagnostic::None
            : EnginePropertyDiagnostic::TransactionRejected,
        transaction,
    };
}

} // namespace enbcore::skyrim
