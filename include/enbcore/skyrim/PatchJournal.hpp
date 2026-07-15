#pragma once

#include <enbcore/skyrim/EngineBridge.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace enbcore::skyrim {

class MutableMemoryView : public MemoryView {
public:
    [[nodiscard]] virtual bool Write(
        std::uintptr_t address,
        std::span<const std::uint8_t> source) noexcept = 0;
};

struct MutationPrerequisites final {
    bool feature_gate_enabled{false};
    bool lifecycle_mutation_permitted{false};
    RuntimeEvaluation runtime_evaluation{};
    CapabilityEntry capability{};
};

struct PropertyPatch final {
    std::string_view identifier;
    std::uintptr_t address{0};
    std::span<const std::uint8_t> expected_bytes;
    std::span<const std::uint8_t> replacement_bytes;
};

enum class PatchState : std::uint8_t {
    Pristine = 0,
    Applied = 1,
    RecoveryRequired = 2,
};

enum class PatchCode : std::uint8_t {
    Applied = 0,
    AlreadyApplied = 1,
    RolledBack = 2,
    AlreadyPristine = 3,
    FeatureGateDisabled = 4,
    LifecycleMutationDenied = 5,
    RuntimeNotAdmitted = 6,
    CapabilityUnavailable = 7,
    InvalidPatch = 8,
    AddressOutsideModule = 9,
    RegionUnavailable = 10,
    RegionNotReadable = 11,
    RegionNotWritable = 12,
    ExecutableRegionRejected = 13,
    BaselineReadFailed = 14,
    BaselineMismatch = 15,
    ApplyFailedRolledBack = 16,
    ApplyFailedRecoveryRequired = 17,
    JournalConflict = 18,
    RollbackBaselineMismatch = 19,
    RollbackFailedStillApplied = 20,
    RollbackFailedRecoveryRequired = 21,
};

struct PatchResult final {
    PatchCode code{PatchCode::InvalidPatch};
    PatchState state{PatchState::Pristine};
    bool compensation_performed{false};
};

class PatchJournal final {
public:
    [[nodiscard]] PatchState state() const noexcept;

    [[nodiscard]] PatchResult Apply(
        const ExecutableIdentity& identity,
        const MutationPrerequisites& prerequisites,
        std::span<const PropertyPatch> patches,
        MutableMemoryView& memory);

    [[nodiscard]] PatchResult Rollback(MutableMemoryView& memory);

private:
    struct Entry final {
        std::string identifier;
        std::uintptr_t address{0};
        std::vector<std::uint8_t> original;
        std::vector<std::uint8_t> replacement;
    };

    PatchState state_{PatchState::Pristine};
    std::uintptr_t module_base_{0};
    std::size_t module_size_{0};
    std::vector<Entry> entries_;
};

} // namespace enbcore::skyrim
