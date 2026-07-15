#include <enbcore/skyrim/PatchJournal.hpp>

#include <algorithm>
#include <limits>
#include <utility>

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

[[nodiscard]] constexpr bool RegionContains(
    const MemoryRegion& region,
    const std::uintptr_t address,
    const std::size_t size) noexcept
{
    return Contains(region.base, region.size, address, size);
}

[[nodiscard]] constexpr bool RangesOverlap(
    const std::uintptr_t left_address,
    const std::size_t left_size,
    const std::uintptr_t right_address,
    const std::size_t right_size) noexcept
{
    if (left_address <= right_address) {
        return right_address - left_address < left_size;
    }
    return left_address - right_address < right_size;
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

[[nodiscard]] PatchResult Result(
    const PatchCode code,
    const PatchState state,
    const bool compensation = false) noexcept
{
    return PatchResult{code, state, compensation};
}

} // namespace

PatchState PatchJournal::state() const noexcept
{
    return state_;
}

PatchResult PatchJournal::Apply(
    const ExecutableIdentity& identity,
    const MutationPrerequisites& prerequisites,
    const std::span<const PropertyPatch> patches,
    MutableMemoryView& memory)
{
    if (!prerequisites.feature_gate_enabled) {
        return Result(PatchCode::FeatureGateDisabled, state_);
    }
    if (!prerequisites.lifecycle_mutation_permitted) {
        return Result(PatchCode::LifecycleMutationDenied, state_);
    }
    if (!MatchesSupportedRuntime(identity, prerequisites.runtime_evaluation)) {
        return Result(PatchCode::RuntimeNotAdmitted, state_);
    }
    if (prerequisites.capability.state == CapabilityState::Unavailable
        || prerequisites.capability.diagnostic != SymbolDiagnostic::None) {
        return Result(PatchCode::CapabilityUnavailable, state_);
    }
    if (state_ == PatchState::RecoveryRequired) {
        return Result(PatchCode::JournalConflict, state_);
    }

    if (state_ == PatchState::Applied) {
        if (patches.size() != entries_.size()) {
            return Result(PatchCode::JournalConflict, state_);
        }
        std::size_t maximum_size = 0;
        for (const Entry& entry : entries_) {
            maximum_size = (std::max)(maximum_size, entry.replacement.size());
        }
        std::vector<std::uint8_t> scratch(maximum_size);
        for (std::size_t index = 0; index < entries_.size(); ++index) {
            const Entry& entry = entries_[index];
            const PropertyPatch& patch = patches[index];
            if (patch.identifier != entry.identifier
                || patch.address != entry.address
                || !std::ranges::equal(patch.expected_bytes, entry.original)
                || !std::ranges::equal(patch.replacement_bytes, entry.replacement)
                || !ReadEquals(memory, entry.address, entry.replacement, scratch)) {
                return Result(PatchCode::JournalConflict, state_);
            }
        }
        return Result(PatchCode::AlreadyApplied, state_);
    }

    if (patches.empty() || identity.image_base == 0U || identity.image_size == 0U) {
        return Result(PatchCode::InvalidPatch, state_);
    }

    std::vector<Entry> staged;
    staged.reserve(patches.size());
    for (std::size_t index = 0; index < patches.size(); ++index) {
        const PropertyPatch& patch = patches[index];
        if (patch.identifier.empty()
            || patch.expected_bytes.empty()
            || patch.expected_bytes.size() != patch.replacement_bytes.size()
            || std::ranges::equal(patch.expected_bytes, patch.replacement_bytes)) {
            return Result(PatchCode::InvalidPatch, state_);
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (patch.identifier == patches[prior].identifier
                || RangesOverlap(
                    patch.address,
                    patch.expected_bytes.size(),
                    patches[prior].address,
                    patches[prior].expected_bytes.size())) {
                return Result(PatchCode::InvalidPatch, state_);
            }
        }
        if (!Contains(
                identity.image_base,
                identity.image_size,
                patch.address,
                patch.expected_bytes.size())) {
            return Result(PatchCode::AddressOutsideModule, state_);
        }
        const auto region = memory.QueryRegion(
            patch.address,
            patch.expected_bytes.size());
        if (!region.has_value()
            || !RegionContains(*region, patch.address, patch.expected_bytes.size())) {
            return Result(PatchCode::RegionUnavailable, state_);
        }
        if (!region->readable) {
            return Result(PatchCode::RegionNotReadable, state_);
        }
        if (!region->writable) {
            return Result(PatchCode::RegionNotWritable, state_);
        }
        if (region->executable) {
            return Result(PatchCode::ExecutableRegionRejected, state_);
        }

        std::vector<std::uint8_t> observed(patch.expected_bytes.size());
        if (!memory.Read(patch.address, observed)) {
            return Result(PatchCode::BaselineReadFailed, state_);
        }
        if (!std::ranges::equal(observed, patch.expected_bytes)) {
            return Result(PatchCode::BaselineMismatch, state_);
        }
        staged.push_back(Entry{
            std::string{patch.identifier},
            patch.address,
            std::move(observed),
            std::vector<std::uint8_t>{
                patch.replacement_bytes.begin(),
                patch.replacement_bytes.end()},
        });
    }

    std::size_t maximum_size = 0;
    for (const Entry& entry : staged) {
        maximum_size = (std::max)(maximum_size, entry.replacement.size());
    }
    std::vector<std::uint8_t> scratch(maximum_size);

    std::size_t applied_count = 0;
    for (std::size_t index = 0; index < staged.size(); ++index) {
        Entry& entry = staged[index];
        const bool wrote = memory.Write(entry.address, entry.replacement);
        if (!wrote
            || !ReadEquals(memory, entry.address, entry.replacement, scratch)) {
            const std::size_t compensation_count = wrote ? index + 1U : applied_count;
            bool restored = true;
            for (std::size_t count = compensation_count; count > 0U; --count) {
                Entry& applied = staged[count - 1U];
                if (!memory.Write(applied.address, applied.original)
                    || !ReadEquals(
                        memory,
                        applied.address,
                        applied.original,
                        scratch)) {
                    restored = false;
                    break;
                }
            }
            if (restored) {
                return Result(
                    PatchCode::ApplyFailedRolledBack,
                    PatchState::Pristine,
                    compensation_count != 0U);
            }

            entries_ = std::move(staged);
            module_base_ = identity.image_base;
            module_size_ = identity.image_size;
            state_ = PatchState::RecoveryRequired;
            return Result(
                PatchCode::ApplyFailedRecoveryRequired,
                state_,
                true);
        }
        ++applied_count;
    }

    entries_ = std::move(staged);
    module_base_ = identity.image_base;
    module_size_ = identity.image_size;
    state_ = PatchState::Applied;
    return Result(PatchCode::Applied, state_);
}

PatchResult PatchJournal::Rollback(MutableMemoryView& memory)
{
    if (state_ == PatchState::Pristine) {
        return Result(PatchCode::AlreadyPristine, state_);
    }
    if (state_ == PatchState::RecoveryRequired || entries_.empty()) {
        return Result(PatchCode::RollbackFailedRecoveryRequired, state_);
    }

    std::size_t maximum_size = 0;
    for (const Entry& entry : entries_) {
        maximum_size = (std::max)(maximum_size, entry.replacement.size());
    }
    std::vector<std::uint8_t> scratch(maximum_size);

    for (const Entry& entry : entries_) {
        if (!Contains(module_base_, module_size_, entry.address, entry.replacement.size())) {
            return Result(PatchCode::RollbackBaselineMismatch, state_);
        }
        const auto region = memory.QueryRegion(entry.address, entry.replacement.size());
        if (!region.has_value()
            || !RegionContains(*region, entry.address, entry.replacement.size())
            || !region->readable
            || !region->writable
            || region->executable
            || !ReadEquals(memory, entry.address, entry.replacement, scratch)) {
            return Result(PatchCode::RollbackBaselineMismatch, state_);
        }
    }

    std::vector<std::size_t> restored_indices;
    restored_indices.reserve(entries_.size());
    for (std::size_t count = entries_.size(); count > 0U; --count) {
        const std::size_t index = count - 1U;
        Entry& entry = entries_[index];
        const bool wrote = memory.Write(entry.address, entry.original);
        if (wrote) {
            restored_indices.push_back(index);
        }
        if (!wrote || !ReadEquals(memory, entry.address, entry.original, scratch)) {
            bool reapplied = true;
            for (const std::size_t restored : restored_indices) {
                Entry& prior = entries_[restored];
                if (!memory.Write(prior.address, prior.replacement)
                    || !ReadEquals(
                        memory,
                        prior.address,
                        prior.replacement,
                        scratch)) {
                    reapplied = false;
                    break;
                }
            }
            if (reapplied) {
                state_ = PatchState::Applied;
                return Result(PatchCode::RollbackFailedStillApplied, state_);
            }
            state_ = PatchState::RecoveryRequired;
            return Result(
                PatchCode::RollbackFailedRecoveryRequired,
                state_,
                true);
        }
    }

    entries_.clear();
    module_base_ = 0;
    module_size_ = 0;
    state_ = PatchState::Pristine;
    return Result(PatchCode::RolledBack, state_);
}

} // namespace enbcore::skyrim
