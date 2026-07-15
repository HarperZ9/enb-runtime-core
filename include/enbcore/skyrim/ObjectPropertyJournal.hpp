#pragma once

#include <enbcore/skyrim/EngineProperties.hpp>
#include <enbcore/skyrim/PatchJournal.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace enbcore::skyrim {

enum class ObjectMutationPhase : std::uint8_t {
    None = 0,
    MainThreadPostUpdate = 1,
    FrameBoundary = 2,
};

class ObjectPropertyMemoryView : public MutableMemoryView {
public:
    [[nodiscard]] virtual bool IsOwnerCurrent(
        ObjectOwnerToken owner) const noexcept = 0;

    [[nodiscard]] virtual bool IsDeclaredField(
        EnginePropertyId property,
        ObjectOwnerToken owner,
        std::uintptr_t address,
        std::size_t size) const noexcept = 0;

    [[nodiscard]] virtual bool IsMutationThreadPermitted() const noexcept = 0;

    [[nodiscard]] virtual ObjectMutationPhase
    CurrentMutationPhase() const noexcept = 0;
};

struct ObjectMutationPrerequisites final {
    bool feature_gate_enabled{false};
    bool lifecycle_mutation_permitted{false};
    RuntimeEvaluation runtime_evaluation{};
    CapabilityEntry capability{};
    ObjectMutationPhase required_phase{ObjectMutationPhase::None};
};

enum class ObjectPropertyState : std::uint8_t {
    Pristine = 0,
    Applied = 1,
    RecoveryRequired = 2,
};

enum class ObjectPropertyCode : std::uint8_t {
    Applied = 0,
    AlreadyApplied = 1,
    NoChange = 2,
    RolledBack = 3,
    AlreadyPristine = 4,
    FeatureGateDisabled = 5,
    LifecycleMutationDenied = 6,
    RuntimeNotAdmitted = 7,
    CapabilityUnavailable = 8,
    MutationNotAllowed = 9,
    InvalidBinding = 10,
    InvalidReplacement = 11,
    ThreadNotPermitted = 12,
    PhaseMismatch = 13,
    OwnerUnavailable = 14,
    FieldSchemaMismatch = 15,
    RegionUnavailable = 16,
    RegionNotReadable = 17,
    RegionNotWritable = 18,
    ExecutableRegionRejected = 19,
    BaselineReadFailed = 20,
    BaselineInvalid = 21,
    JournalConflict = 22,
    ApplyFailedRolledBack = 23,
    ApplyFailedRecoveryRequired = 24,
    RollbackBaselineMismatch = 25,
    RollbackFailedStillApplied = 26,
    RollbackFailedRecoveryRequired = 27,
    OwnerInvalidatedBeforeRollback = 28,
};

struct ObjectPropertyResult final {
    ObjectPropertyCode code{ObjectPropertyCode::InvalidBinding};
    ObjectPropertyState state{ObjectPropertyState::Pristine};
    bool compensation_performed{false};
};

class ObjectPropertyJournal final {
public:
    [[nodiscard]] ObjectPropertyState state() const noexcept;

    [[nodiscard]] ObjectPropertyResult Apply(
        const ExecutableIdentity& identity,
        const ObjectMutationPrerequisites& prerequisites,
        const ObjectPropertyBinding& binding,
        std::span<const std::uint8_t> replacement,
        ObjectPropertyMemoryView& memory) noexcept;

    [[nodiscard]] ObjectPropertyResult Rollback(
        ObjectPropertyMemoryView& memory) noexcept;

private:
    static constexpr std::size_t MaximumPropertySize = sizeof(Matrix4x4);

    ObjectPropertyState state_{ObjectPropertyState::Pristine};
    ObjectPropertyBinding binding_{};
    ObjectMutationPhase phase_{ObjectMutationPhase::None};
    std::size_t size_{0};
    std::array<std::uint8_t, MaximumPropertySize> baseline_{};
    std::array<std::uint8_t, MaximumPropertySize> replacement_{};
};

struct PropertyMutationResult final {
    EnginePropertyDiagnostic diagnostic{EnginePropertyDiagnostic::UnknownIdentifier};
    ObjectPropertyResult transaction{};
};

[[nodiscard]] PropertyMutationResult ApplyEnginePropertyMutation(
    const ExecutableIdentity& identity,
    const ObjectMutationPrerequisites& prerequisites,
    std::string_view name,
    const EnginePropertyValue& value,
    const EnginePropertyAdapter& adapter,
    ObjectPropertyMemoryView& memory,
    ObjectPropertyJournal& journal) noexcept;

} // namespace enbcore::skyrim
