#include <enbcore/skyrim/EngineBridge.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

namespace enbcore::skyrim {
namespace {

constexpr Sha256Digest kSkyrim1170Sha256{
    0xC4U, 0x34U, 0x20U, 0x88U, 0x94U, 0xF0U, 0x7FU, 0x60U,
    0x4BU, 0x85U, 0x2FU, 0x29U, 0xB8U, 0xEDU, 0xC3U, 0xA5U,
    0x8CU, 0x4DU, 0xE6U, 0x3DU, 0xE7U, 0x83U, 0x37U, 0x37U,
    0x33U, 0xE7U, 0x2BU, 0x2BU, 0x73U, 0xF3U, 0x3BU, 0xE9U,
};

constexpr std::array kSupportedSkyrimRuntimes{
    RuntimeSupportRecord{
        "skyrim-se-1.6.1170.0-c4342088",
        L"TESV.exe",
        RuntimeVersion{1, 6, 1170, 0},
        ExecutableArchitecture::X64,
        kSkyrim1170Sha256,
        37'157'144U,
    },
};

constexpr Sha256Digest kSkse226Sha256{
    0xC9U, 0xA2U, 0xC8U, 0xA8U, 0x0DU, 0xF6U, 0xBFU, 0x23U,
    0x72U, 0xC5U, 0xF4U, 0x94U, 0x68U, 0xBBU, 0x2EU, 0x5AU,
    0xB6U, 0x77U, 0x86U, 0x15U, 0x72U, 0x65U, 0xB6U, 0xF2U,
    0x9EU, 0xCEU, 0x9FU, 0x4EU, 0xACU, 0x07U, 0x5DU, 0x54U,
};

constexpr std::array kSupportedSkseRuntimes{
    SkseRuntimeRecord{
        "skse-2.2.6-skyrim-1.6.1170",
        L"skse64_1_6_1170.dll",
        InterfaceVersion{2, 2, 6},
        kSkse226Sha256,
        1'173'504U,
    },
};

constexpr Sha256Digest kAddressLibrary1170Sha256{
    0xC4U, 0x09U, 0x3CU, 0x56U, 0x9AU, 0x3CU, 0x83U, 0xB2U,
    0x65U, 0x87U, 0xF4U, 0xB9U, 0xEAU, 0x4CU, 0x55U, 0xDEU,
    0x9AU, 0xE6U, 0xE7U, 0x3BU, 0x84U, 0xA2U, 0xAFU, 0x9FU,
    0xB3U, 0xFBU, 0xD3U, 0x0EU, 0x2FU, 0xE0U, 0xD4U, 0x52U,
};

constexpr Sha256Digest kAddressLibrary1170VariantSha256{
    0xACU, 0x6DU, 0x17U, 0xE8U, 0xA4U, 0xBBU, 0x4DU, 0xA2U,
    0x53U, 0x9EU, 0x7AU, 0x57U, 0x11U, 0x13U, 0xBCU, 0xB2U,
    0x8AU, 0xE5U, 0xADU, 0xF4U, 0x87U, 0x4CU, 0xD9U, 0x77U,
    0x33U, 0x2FU, 0x1AU, 0x52U, 0x15U, 0xF6U, 0x5CU, 0x07U,
};

constexpr std::array kSupportedRelocationProviders{
    RelocationProviderRecord{
        "address-library-1.6.1170.0",
        RelocationProviderKind::AddressLibrary,
        RuntimeVersion{1, 6, 1170, 0},
        RuntimeVariant::AnniversaryEdition,
        kAddressLibrary1170Sha256,
        795'129U,
    },
    RelocationProviderRecord{
        "address-library-1.6.1170.0-variant-1",
        RelocationProviderKind::AddressLibrary,
        RuntimeVersion{1, 6, 1170, 0},
        RuntimeVariant::AnniversaryEdition,
        kAddressLibrary1170VariantSha256,
        795'210U,
    },
};

[[nodiscard]] constexpr bool IsZeroDigest(const Sha256Digest& digest) noexcept
{
    return std::ranges::all_of(digest, [](const std::uint8_t value) {
        return value == 0U;
    });
}

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

[[nodiscard]] constexpr int Compare(
    const InterfaceVersion left,
    const InterfaceVersion right) noexcept
{
    if (left.major != right.major) {
        return left.major < right.major ? -1 : 1;
    }
    if (left.minor != right.minor) {
        return left.minor < right.minor ? -1 : 1;
    }
    if (left.patch != right.patch) {
        return left.patch < right.patch ? -1 : 1;
    }
    return 0;
}

[[nodiscard]] constexpr bool InRange(
    const InterfaceVersion value,
    const InterfaceVersion minimum,
    const InterfaceVersion maximum) noexcept
{
    return Compare(value, minimum) >= 0 && Compare(value, maximum) <= 0;
}

[[nodiscard]] SymbolValidation Reject(
    const SymbolDiagnostic diagnostic) noexcept
{
    return SymbolValidation{
        SymbolStatus::Rejected,
        diagnostic,
        0,
        0,
        false,
    };
}

[[nodiscard]] SymbolDiagnostic ValidateProvider(
    const ExecutableIdentity& identity,
    const RuntimeEvaluation& runtime,
    const SymbolProviderContext& provider,
    const SymbolDescriptor& descriptor) noexcept
{
    if (runtime.status != RuntimeStatus::Admitted) {
        return SymbolDiagnostic::RuntimeRejected;
    }
    if (!MatchesSupportedRuntime(identity, runtime)) {
        return SymbolDiagnostic::RuntimeIdentityMismatch;
    }
    if (!provider.relocation_provider_available) {
        return SymbolDiagnostic::RelocationProviderUnavailable;
    }
    const bool supported_relocation_provider = std::ranges::any_of(
        kSupportedRelocationProviders,
        [&provider](const RelocationProviderRecord& record) {
            return provider.relocation_provider_kind == record.kind
                && provider.relocation_runtime == record.runtime_version
                && provider.runtime_variant == record.runtime_variant
                && provider.relocation_artifact_sha256 == record.artifact_sha256
                && provider.relocation_artifact_file_size == record.artifact_file_size;
        });
    if (!supported_relocation_provider) {
        return SymbolDiagnostic::RelocationProviderUnsupported;
    }
    const bool supported_skse = std::ranges::any_of(
        kSupportedSkseRuntimes,
        [&provider](const SkseRuntimeRecord& record) {
            return provider.skse_runtime_filename == record.filename
                && provider.skse_runtime_version == record.version
                && provider.skse_runtime_sha256 == record.sha256
                && provider.skse_runtime_file_size == record.file_size;
        });
    if (!supported_skse) {
        return SymbolDiagnostic::SkseRuntimeUnsupported;
    }
    if (provider.relocation_runtime != identity.runtime_version
        || descriptor.constraints.runtime_version != identity.runtime_version
        || provider.relocation_provider_kind
            != descriptor.constraints.relocation_provider_kind
        || provider.runtime_variant != descriptor.constraints.runtime_variant) {
        return SymbolDiagnostic::RelocationRuntimeMismatch;
    }
    if (!InRange(
            provider.relocation_provider_version,
            descriptor.constraints.minimum_relocation_provider,
            descriptor.constraints.maximum_relocation_provider)
        || !InRange(
            provider.engine_adapter_version,
            descriptor.constraints.minimum_engine_adapter,
            descriptor.constraints.maximum_engine_adapter)
        || !InRange(
            provider.skse_runtime_version,
            descriptor.constraints.minimum_skse,
            descriptor.constraints.maximum_skse)) {
        return SymbolDiagnostic::ProviderVersionUnsupported;
    }
    return SymbolDiagnostic::None;
}

} // namespace

std::span<const RuntimeSupportRecord> SupportedSkyrimRuntimes() noexcept
{
    return kSupportedSkyrimRuntimes;
}

std::span<const SkseRuntimeRecord> SupportedSkseRuntimes() noexcept
{
    return kSupportedSkseRuntimes;
}

std::span<const RelocationProviderRecord> SupportedRelocationProviders() noexcept
{
    return kSupportedRelocationProviders;
}

RuntimeEvaluation EvaluateRuntimeIdentity(
    const ExecutableIdentity& identity,
    const std::span<const RuntimeSupportRecord> supported_runtimes) noexcept
{
    RuntimeEvaluation result;
    try {
        result.original_filename.assign(identity.original_filename);
    } catch (...) {
        result.diagnostic = RuntimeDiagnostic::IncompleteFingerprint;
        return result;
    }
    result.runtime_version = identity.runtime_version;
    result.sha256 = identity.sha256;
    result.file_size = identity.file_size;
    result.image_base = identity.image_base;
    result.image_size = identity.image_size;

    if (identity.module_name != L"SkyrimSE.exe") {
        result.diagnostic = RuntimeDiagnostic::InvalidModuleName;
        return result;
    }
    if (identity.architecture != ExecutableArchitecture::X64) {
        result.diagnostic = RuntimeDiagnostic::UnsupportedArchitecture;
        return result;
    }
    if (identity.image_base == 0U
        || identity.image_size == 0U
        || identity.image_base
            > (std::numeric_limits<std::uintptr_t>::max)() - identity.image_size) {
        result.diagnostic = RuntimeDiagnostic::InvalidModuleRange;
        return result;
    }
    if (identity.original_filename.empty()
        || identity.file_size == 0U
        || IsZeroDigest(identity.sha256)) {
        result.diagnostic = RuntimeDiagnostic::IncompleteFingerprint;
        return result;
    }

    for (std::size_t index = 0; index < supported_runtimes.size(); ++index) {
        const RuntimeSupportRecord& record = supported_runtimes[index];
        if (record.identifier.empty()
            || record.original_filename.empty()
            || record.file_size == 0U
            || IsZeroDigest(record.sha256)) {
            continue;
        }
        if (record.runtime_version == identity.runtime_version
            && record.architecture == identity.architecture
            && record.sha256 == identity.sha256
            && record.file_size == identity.file_size
            && record.original_filename == identity.original_filename) {
            result.status = RuntimeStatus::Admitted;
            result.diagnostic = RuntimeDiagnostic::None;
            result.matched_record = index;
            return result;
        }
    }

    result.diagnostic = RuntimeDiagnostic::UnsupportedFingerprint;
    return result;
}

bool MatchesSupportedRuntime(
    const ExecutableIdentity& identity,
    const RuntimeEvaluation& runtime) noexcept
{
    if (runtime.status != RuntimeStatus::Admitted
        || runtime.diagnostic != RuntimeDiagnostic::None
        || runtime.matched_record >= kSupportedSkyrimRuntimes.size()
        || identity.module_name != L"SkyrimSE.exe"
        || identity.architecture != ExecutableArchitecture::X64
        || identity.image_base == 0U
        || identity.image_size == 0U
        || identity.image_base
            > (std::numeric_limits<std::uintptr_t>::max)() - identity.image_size
        || runtime.original_filename != identity.original_filename
        || runtime.runtime_version != identity.runtime_version
        || runtime.sha256 != identity.sha256
        || runtime.file_size != identity.file_size
        || runtime.image_base != identity.image_base
        || runtime.image_size != identity.image_size) {
        return false;
    }

    const RuntimeSupportRecord& record =
        kSupportedSkyrimRuntimes[runtime.matched_record];
    return !record.identifier.empty()
        && record.original_filename == identity.original_filename
        && record.runtime_version == identity.runtime_version
        && record.architecture == identity.architecture
        && record.sha256 == identity.sha256
        && record.file_size == identity.file_size;
}

SymbolValidation ValidateSymbol(
    const ExecutableIdentity& identity,
    const RuntimeEvaluation& runtime,
    const SymbolProviderContext& provider,
    const SymbolDescriptor& descriptor,
    const MemoryView& memory) noexcept
{
    const SymbolDiagnostic provider_diagnostic =
        ValidateProvider(identity, runtime, provider, descriptor);
    if (provider_diagnostic != SymbolDiagnostic::None) {
        return Reject(provider_diagnostic);
    }
    if (descriptor.identifier.empty() || descriptor.relocation_id == 0U) {
        return Reject(SymbolDiagnostic::InvalidDescriptor);
    }
    if (descriptor.contract == SymbolContract::CompleteFunctionPrologue
        && descriptor.expected_instruction_bytes.empty()) {
        return Reject(SymbolDiagnostic::InvalidDescriptor);
    }
    if (descriptor.contract == SymbolContract::DeclaredVtableSlot
        && descriptor.expected_rtti_name.empty()) {
        return Reject(SymbolDiagnostic::InvalidDescriptor);
    }

    const auto resolved =
        memory.ResolveRelocationId(descriptor.relocation_id);
    if (!resolved.has_value()) {
        return Reject(SymbolDiagnostic::AddressUnresolved);
    }
    if (!Contains(identity.image_base, identity.image_size, *resolved, 1U)) {
        return Reject(SymbolDiagnostic::AddressOutsideModule);
    }

    if (descriptor.contract == SymbolContract::ReadOnlyData) {
        const auto region = memory.QueryRegion(*resolved, 1U);
        if (!region.has_value() || !RegionContains(*region, *resolved, 1U)) {
            return Reject(SymbolDiagnostic::RegionUnavailable);
        }
        if (!region->readable) {
            return Reject(SymbolDiagnostic::RegionNotReadable);
        }
        return SymbolValidation{
            SymbolStatus::Validated,
            SymbolDiagnostic::None,
            *resolved,
            *resolved,
            false,
        };
    }

    if (descriptor.contract == SymbolContract::CompleteFunctionPrologue) {
        const std::size_t size = descriptor.expected_instruction_bytes.size();
        if (!Contains(identity.image_base, identity.image_size, *resolved, size)) {
            return Reject(SymbolDiagnostic::AddressOutsideModule);
        }
        const auto region = memory.QueryRegion(*resolved, size);
        if (!region.has_value() || !RegionContains(*region, *resolved, size)) {
            return Reject(SymbolDiagnostic::RegionUnavailable);
        }
        if (!region->readable) {
            return Reject(SymbolDiagnostic::RegionNotReadable);
        }
        if (!region->executable) {
            return Reject(SymbolDiagnostic::RegionNotExecutable);
        }

        std::array<std::uint8_t, 128> observed{};
        if (size > observed.size()) {
            return Reject(SymbolDiagnostic::InvalidDescriptor);
        }
        const std::span observed_span{observed.data(), size};
        if (!memory.Read(*resolved, observed_span)) {
            return Reject(SymbolDiagnostic::InstructionReadFailed);
        }
        if (!std::ranges::equal(
                observed_span,
                descriptor.expected_instruction_bytes)) {
            return Reject(SymbolDiagnostic::InstructionBytesMismatch);
        }
        return SymbolValidation{
            SymbolStatus::Validated,
            SymbolDiagnostic::None,
            *resolved,
            *resolved,
            true,
        };
    }

    if (descriptor.contract != SymbolContract::DeclaredVtableSlot
        || descriptor.vtable_slot
            > (std::numeric_limits<std::size_t>::max() / sizeof(std::uintptr_t))) {
        return Reject(SymbolDiagnostic::InvalidDescriptor);
    }
    const std::size_t slot_offset = descriptor.vtable_slot * sizeof(std::uintptr_t);
    if (*resolved > std::numeric_limits<std::uintptr_t>::max() - slot_offset) {
        return Reject(SymbolDiagnostic::AddressOutsideModule);
    }
    const std::uintptr_t slot_address = *resolved + slot_offset;
    if (!Contains(
            identity.image_base,
            identity.image_size,
            slot_address,
            sizeof(std::uintptr_t))) {
        return Reject(SymbolDiagnostic::AddressOutsideModule);
    }
    const auto vtable_region = memory.QueryRegion(
        slot_address,
        sizeof(std::uintptr_t));
    if (!vtable_region.has_value()
        || !RegionContains(*vtable_region, slot_address, sizeof(std::uintptr_t))) {
        return Reject(SymbolDiagnostic::RegionUnavailable);
    }
    if (!vtable_region->readable) {
        return Reject(SymbolDiagnostic::RegionNotReadable);
    }
    if (!memory.MatchesRtti(*resolved, descriptor.expected_rtti_name)) {
        return Reject(SymbolDiagnostic::RttiIdentityMismatch);
    }

    std::array<std::uint8_t, sizeof(std::uintptr_t)> pointer_bytes{};
    if (!memory.Read(slot_address, pointer_bytes)) {
        return Reject(SymbolDiagnostic::VtableEntryReadFailed);
    }
    std::uintptr_t target = 0;
    std::memcpy(&target, pointer_bytes.data(), sizeof(target));
    if (!Contains(identity.image_base, identity.image_size, target, 1U)) {
        return Reject(SymbolDiagnostic::AddressOutsideModule);
    }
    const auto target_region = memory.QueryRegion(target, 1U);
    if (!target_region.has_value() || !RegionContains(*target_region, target, 1U)) {
        return Reject(SymbolDiagnostic::RegionUnavailable);
    }
    if (!target_region->readable) {
        return Reject(SymbolDiagnostic::RegionNotReadable);
    }
    if (!target_region->executable) {
        return Reject(SymbolDiagnostic::RegionNotExecutable);
    }
    return SymbolValidation{
        SymbolStatus::Validated,
        SymbolDiagnostic::None,
        target,
        slot_address,
        true,
    };
}

CapabilityReport EvaluateObserverCapabilities(
    const ExecutableIdentity& identity,
    const RuntimeEvaluation& runtime,
    const SymbolProviderContext& provider,
    const std::span<const SymbolDescriptor> descriptors,
    const MemoryView& memory) noexcept
{
    CapabilityReport report;
    std::array<bool, CapabilityCount> seen{};
    std::array<bool, CapabilityCount> rejected{};
    std::array<bool, CapabilityCount> hook_ready{};

    for (std::size_t index = 0; index < CapabilityCount; ++index) {
        report.entries[index].capability = static_cast<Capability>(index);
        report.entries[index].state = CapabilityState::Unavailable;
        report.entries[index].diagnostic = SymbolDiagnostic::MissingDescriptor;
    }

    for (const SymbolDescriptor& descriptor : descriptors) {
        const auto index = static_cast<std::size_t>(descriptor.capability);
        if (index >= CapabilityCount) {
            continue;
        }
        seen[index] = true;
        const SymbolValidation validation = ValidateSymbol(
            identity,
            runtime,
            provider,
            descriptor,
            memory);
        if (validation.status != SymbolStatus::Validated) {
            if (!rejected[index]) {
                report.entries[index].diagnostic = validation.diagnostic;
            }
            rejected[index] = true;
            continue;
        }
        hook_ready[index] = hook_ready[index] || validation.hook_arming_permitted;
    }

    for (std::size_t index = 0; index < CapabilityCount; ++index) {
        if (!seen[index] || rejected[index]) {
            continue;
        }
        report.entries[index].state = hook_ready[index]
            ? CapabilityState::HookReady
            : CapabilityState::ObserveReady;
        report.entries[index].diagnostic = SymbolDiagnostic::None;
    }
    return report;
}

} // namespace enbcore::skyrim
