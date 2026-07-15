#include <enbcore/skyrim/EngineBridge.hpp>
#include <enbcore/skyrim/PatchJournal.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <string>
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
constexpr std::uintptr_t kDataAddress = kModuleBase + 0x1000U;
constexpr std::uintptr_t kFunctionAddress = kModuleBase + 0x2000U;
constexpr std::uintptr_t kVtableAddress = kModuleBase + 0x3000U;
constexpr std::uintptr_t kVtableTarget = kModuleBase + 0x4000U;
constexpr std::uintptr_t kPropertyAddress = kModuleBase + 0x5000U;

constexpr RuntimeVersion kRuntime{1, 6, 1170, 0};
constexpr InterfaceVersion kRelocationProviderVersion{11, 0, 0};
constexpr InterfaceVersion kEngineAdapterVersion{3, 7, 0};

constexpr Sha256Digest kExecutableDigest{
    0xC4U, 0x34U, 0x20U, 0x88U, 0x94U, 0xF0U, 0x7FU, 0x60U,
    0x4BU, 0x85U, 0x2FU, 0x29U, 0xB8U, 0xEDU, 0xC3U, 0xA5U,
    0x8CU, 0x4DU, 0xE6U, 0x3DU, 0xE7U, 0x83U, 0x37U, 0x37U,
    0x33U, 0xE7U, 0x2BU, 0x2BU, 0x73U, 0xF3U, 0x3BU, 0xE9U,
};
constexpr std::uint64_t kExecutableFileSize = 37'157'144U;
constexpr Sha256Digest kSkseRuntimeDigest{
    0xC9U, 0xA2U, 0xC8U, 0xA8U, 0x0DU, 0xF6U, 0xBFU, 0x23U,
    0x72U, 0xC5U, 0xF4U, 0x94U, 0x68U, 0xBBU, 0x2EU, 0x5AU,
    0xB6U, 0x77U, 0x86U, 0x15U, 0x72U, 0x65U, 0xB6U, 0xF2U,
    0x9EU, 0xCEU, 0x9FU, 0x4EU, 0xACU, 0x07U, 0x5DU, 0x54U,
};
constexpr std::uint64_t kSkseRuntimeFileSize = 1'173'504U;
constexpr Sha256Digest kRelocationArtifactDigest{
    0xC4U, 0x09U, 0x3CU, 0x56U, 0x9AU, 0x3CU, 0x83U, 0xB2U,
    0x65U, 0x87U, 0xF4U, 0xB9U, 0xEAU, 0x4CU, 0x55U, 0xDEU,
    0x9AU, 0xE6U, 0xE7U, 0x3BU, 0x84U, 0xA2U, 0xAFU, 0x9FU,
    0xB3U, 0xFBU, 0xD3U, 0x0EU, 0x2FU, 0xE0U, 0xD4U, 0x52U,
};
constexpr std::uint64_t kRelocationArtifactFileSize = 795'129U;

ExecutableIdentity MakeIdentity()
{
    ExecutableIdentity identity;
    identity.module_name = L"SkyrimSE.exe";
    identity.original_filename = L"TESV.exe";
    identity.runtime_version = kRuntime;
    identity.architecture = ExecutableArchitecture::X64;
    identity.sha256 = kExecutableDigest;
    identity.file_size = kExecutableFileSize;
    identity.image_base = kModuleBase;
    identity.image_size = kModuleSize;
    return identity;
}

RuntimeSupportRecord MakeSupportRecord()
{
    RuntimeSupportRecord record;
    record.identifier = "synthetic-skyrim-1.6.1170";
    record.original_filename = L"TESV.exe";
    record.runtime_version = kRuntime;
    record.architecture = ExecutableArchitecture::X64;
    record.sha256 = kExecutableDigest;
    record.file_size = kExecutableFileSize;
    return record;
}

RuntimeEvaluation AdmitSyntheticRuntime()
{
    const std::array records{MakeSupportRecord()};
    return EvaluateRuntimeIdentity(MakeIdentity(), records);
}

SymbolProviderContext MakeProvider()
{
    SymbolProviderContext provider;
    provider.relocation_provider_available = true;
    provider.relocation_provider_kind = RelocationProviderKind::AddressLibrary;
    provider.relocation_runtime = kRuntime;
    provider.runtime_variant = RuntimeVariant::AnniversaryEdition;
    provider.relocation_provider_version = kRelocationProviderVersion;
    provider.relocation_artifact_sha256 = kRelocationArtifactDigest;
    provider.relocation_artifact_file_size = kRelocationArtifactFileSize;
    provider.engine_adapter_version = kEngineAdapterVersion;
    provider.skse_runtime_filename = L"skse64_1_6_1170.dll";
    provider.skse_runtime_version = InterfaceVersion{2, 2, 6};
    provider.skse_runtime_sha256 = kSkseRuntimeDigest;
    provider.skse_runtime_file_size = kSkseRuntimeFileSize;
    return provider;
}

RuntimeSymbolConstraint MakeConstraint()
{
    RuntimeSymbolConstraint constraint;
    constraint.runtime_version = kRuntime;
    constraint.relocation_provider_kind = RelocationProviderKind::AddressLibrary;
    constraint.runtime_variant = RuntimeVariant::AnniversaryEdition;
    constraint.minimum_relocation_provider = InterfaceVersion{10, 0, 0};
    constraint.maximum_relocation_provider = InterfaceVersion{11, 9, 9};
    constraint.minimum_engine_adapter = InterfaceVersion{3, 7, 0};
    constraint.maximum_engine_adapter = InterfaceVersion{3, 9, 9};
    constraint.minimum_skse = InterfaceVersion{2, 2, 6};
    constraint.maximum_skse = InterfaceVersion{2, 2, 6};
    return constraint;
}

class FakeMemory final : public MutableMemoryView {
public:
    struct Mapping final {
        std::uint64_t id;
        std::uintptr_t address;
    };

    void AddRegion(const MemoryRegion region)
    {
        regions_.push_back(region);
    }

    void MapSymbol(const std::uint64_t id, const std::uintptr_t address)
    {
        symbols_.push_back(Mapping{id, address});
    }

    void SetRtti(const std::uintptr_t vtable, std::string name)
    {
        rtti_[vtable] = std::move(name);
    }

    void Put(const std::uintptr_t address, const std::span<const std::uint8_t> bytes)
    {
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            bytes_[address + i] = bytes[i];
        }
    }

    template <typename T>
    void PutValue(const std::uintptr_t address, const T value)
    {
        std::array<std::uint8_t, sizeof(T)> bytes{};
        std::memcpy(bytes.data(), &value, sizeof(value));
        Put(address, bytes);
    }

    [[nodiscard]] std::vector<std::uint8_t> Get(
        const std::uintptr_t address,
        const std::size_t size) const
    {
        std::vector<std::uint8_t> result(size);
        for (std::size_t i = 0; i < size; ++i) {
            const auto found = bytes_.find(address + i);
            result[i] = found == bytes_.end() ? 0U : found->second;
        }
        return result;
    }

    void FailWriteCall(const std::size_t call)
    {
        fail_write_call_ = call;
    }

    [[nodiscard]] std::size_t writeCalls() const noexcept
    {
        return write_calls_;
    }

    [[nodiscard]] std::optional<std::uintptr_t> ResolveRelocationId(
        const std::uint64_t id) const noexcept override
    {
        const auto found = std::ranges::find(symbols_, id, &Mapping::id);
        if (found == symbols_.end()) {
            return std::nullopt;
        }
        return found->address;
    }

    [[nodiscard]] std::optional<MemoryRegion> QueryRegion(
        const std::uintptr_t address,
        const std::size_t size) const noexcept override
    {
        if (size == 0U || address > std::numeric_limits<std::uintptr_t>::max() - size) {
            return std::nullopt;
        }
        const auto end = address + size;
        for (const auto& region : regions_) {
            if (region.size == 0U
                || region.base > std::numeric_limits<std::uintptr_t>::max() - region.size) {
                continue;
            }
            if (address >= region.base && end <= region.base + region.size) {
                return region;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] bool Read(
        const std::uintptr_t address,
        const std::span<std::uint8_t> destination) const noexcept override
    {
        for (std::size_t i = 0; i < destination.size(); ++i) {
            const auto found = bytes_.find(address + i);
            if (found == bytes_.end()) {
                return false;
            }
            destination[i] = found->second;
        }
        return true;
    }

    [[nodiscard]] bool MatchesRtti(
        const std::uintptr_t vtable,
        const std::string_view expected_name) const noexcept override
    {
        const auto found = rtti_.find(vtable);
        return found != rtti_.end() && found->second == expected_name;
    }

    [[nodiscard]] bool Write(
        const std::uintptr_t address,
        const std::span<const std::uint8_t> source) noexcept override
    {
        ++write_calls_;
        if (fail_write_call_.has_value() && write_calls_ == *fail_write_call_) {
            return false;
        }
        Put(address, source);
        return true;
    }

private:
    std::vector<MemoryRegion> regions_;
    std::vector<Mapping> symbols_;
    std::unordered_map<std::uintptr_t, std::string> rtti_;
    std::unordered_map<std::uintptr_t, std::uint8_t> bytes_;
    std::optional<std::size_t> fail_write_call_;
    std::size_t write_calls_{0};
};

FakeMemory MakeValidMemory()
{
    FakeMemory memory;
    memory.AddRegion(MemoryRegion{kModuleBase, 0x1800U, true, true, false});
    memory.AddRegion(MemoryRegion{kFunctionAddress, 0x1000U, true, false, true});
    memory.AddRegion(MemoryRegion{kVtableAddress, 0x1000U, true, false, false});
    memory.AddRegion(MemoryRegion{kVtableTarget, 0x1000U, true, false, true});
    memory.AddRegion(MemoryRegion{kPropertyAddress, 0x1000U, true, true, false});
    return memory;
}

SymbolDescriptor MakeDataDescriptor(
    const Capability capability = Capability::CameraInverseViewProjection,
    const std::uint64_t id = 100U)
{
    return SymbolDescriptor{
        "synthetic-data",
        capability,
        id,
        MakeConstraint(),
        SymbolContract::ReadOnlyData,
        {},
        0U,
        {},
    };
}

SymbolDescriptor MakePrologueDescriptor(
    const std::span<const std::uint8_t> expected,
    const Capability capability = Capability::RenderPhases,
    const std::uint64_t id = 200U)
{
    return SymbolDescriptor{
        "synthetic-prologue",
        capability,
        id,
        MakeConstraint(),
        SymbolContract::CompleteFunctionPrologue,
        expected,
        0U,
        {},
    };
}

SymbolDescriptor MakeVtableDescriptor(
    const Capability capability = Capability::SkyShaderObservation,
    const std::uint64_t id = 300U)
{
    return SymbolDescriptor{
        "synthetic-vtable",
        capability,
        id,
        MakeConstraint(),
        SymbolContract::DeclaredVtableSlot,
        {},
        4U,
        ".?AVBSSkyShader@@",
    };
}

void runtime_identity_requires_an_exact_executable_fingerprint()
{
    const std::array records{MakeSupportRecord()};
    const RuntimeEvaluation admitted = EvaluateRuntimeIdentity(MakeIdentity(), records);

    EXPECT(admitted.status == RuntimeStatus::Admitted);
    EXPECT(admitted.diagnostic == RuntimeDiagnostic::None);
    EXPECT(admitted.matched_record == 0U);

    ExecutableIdentity unsupported = MakeIdentity();
    unsupported.runtime_version = RuntimeVersion{1, 6, 1130, 0};
    const RuntimeEvaluation rejected = EvaluateRuntimeIdentity(unsupported, records);

    EXPECT(rejected.status == RuntimeStatus::Rejected);
    EXPECT(rejected.diagnostic == RuntimeDiagnostic::UnsupportedFingerprint);
    EXPECT(rejected.matched_record == NoMatchedRuntimeRecord);
}

void captured_skyrim_1_6_1170_is_the_only_built_in_runtime_fingerprint()
{
    const std::span records = SupportedSkyrimRuntimes();

    EXPECT(records.size() == 1U);
    EXPECT(records.front().runtime_version == kRuntime);
    EXPECT(records.front().sha256 == kExecutableDigest);
    EXPECT(records.front().file_size == kExecutableFileSize);
    EXPECT(records.front().original_filename == L"TESV.exe");

    const RuntimeEvaluation admitted = EvaluateRuntimeIdentity(MakeIdentity(), records);
    EXPECT(admitted.status == RuntimeStatus::Admitted);
    EXPECT(admitted.diagnostic == RuntimeDiagnostic::None);
}

void a_wrapping_loaded_image_range_is_rejected_before_fingerprint_admission()
{
    const std::array records{MakeSupportRecord()};
    ExecutableIdentity identity = MakeIdentity();
    identity.image_base = (std::numeric_limits<std::uintptr_t>::max)() - 7U;
    identity.image_size = 16U;

    const RuntimeEvaluation result = EvaluateRuntimeIdentity(identity, records);

    EXPECT(result.status == RuntimeStatus::Rejected);
    EXPECT(result.diagnostic == RuntimeDiagnostic::InvalidModuleRange);
    EXPECT(result.matched_record == NoMatchedRuntimeRecord);
}

void runtime_evaluation_owns_its_original_filename_receipt()
{
    const std::array records{MakeSupportRecord()};
    std::wstring original_filename = L"TESV.exe";
    ExecutableIdentity identity = MakeIdentity();
    identity.original_filename = original_filename;

    const RuntimeEvaluation result = EvaluateRuntimeIdentity(identity, records);
    original_filename.assign(L"NOPE.exe");

    EXPECT(result.status == RuntimeStatus::Admitted);
    EXPECT(result.original_filename == L"TESV.exe");
}

void missing_relocation_provider_rejects_without_touching_memory()
{
    FakeMemory memory = MakeValidMemory();
    memory.MapSymbol(100U, kDataAddress);
    const std::array<std::uint8_t, 1> bytes{0x44U};
    memory.Put(kDataAddress, bytes);

    SymbolProviderContext provider = MakeProvider();
    provider.relocation_provider_available = false;

    const SymbolValidation result = ValidateSymbol(
        MakeIdentity(), AdmitSyntheticRuntime(), provider, MakeDataDescriptor(), memory);

    EXPECT(result.status == SymbolStatus::Rejected);
    EXPECT(result.diagnostic == SymbolDiagnostic::RelocationProviderUnavailable);
    EXPECT(!result.hook_arming_permitted);
    EXPECT(memory.writeCalls() == 0U);
}

void relocation_provider_admission_requires_an_exact_artifact_identity()
{
    const std::span records = SupportedRelocationProviders();
    EXPECT(records.size() == 2U);
    EXPECT(records.front().kind == RelocationProviderKind::AddressLibrary);
    EXPECT(records.front().runtime_version == kRuntime);
    EXPECT(records.front().runtime_variant == RuntimeVariant::AnniversaryEdition);
    EXPECT(records.front().artifact_sha256 == kRelocationArtifactDigest);
    EXPECT(records.front().artifact_file_size == kRelocationArtifactFileSize);

    FakeMemory memory = MakeValidMemory();
    memory.MapSymbol(100U, kDataAddress);
    const std::array<std::uint8_t, 1> bytes{0x44U};
    memory.Put(kDataAddress, bytes);

    SymbolProviderContext provider = MakeProvider();
    provider.relocation_artifact_sha256[0] ^= 0xFFU;
    const SymbolValidation wrong_artifact = ValidateSymbol(
        MakeIdentity(), AdmitSyntheticRuntime(), provider, MakeDataDescriptor(), memory);

    EXPECT(wrong_artifact.status == SymbolStatus::Rejected);
    EXPECT(wrong_artifact.diagnostic == SymbolDiagnostic::RelocationProviderUnsupported);
    EXPECT(!wrong_artifact.hook_arming_permitted);

    provider = MakeProvider();
    provider.relocation_provider_kind = RelocationProviderKind::EmbeddedManifest;
    const SymbolValidation unregistered_native_manifest = ValidateSymbol(
        MakeIdentity(), AdmitSyntheticRuntime(), provider, MakeDataDescriptor(), memory);

    EXPECT(unregistered_native_manifest.status == SymbolStatus::Rejected);
    EXPECT(unregistered_native_manifest.diagnostic
        == SymbolDiagnostic::RelocationProviderUnsupported);
    EXPECT(!unregistered_native_manifest.hook_arming_permitted);
    EXPECT(memory.writeCalls() == 0U);
}

void skse_provider_admission_uses_the_runtime_dll_and_never_loader_bytes()
{
    const std::span records = SupportedSkseRuntimes();
    EXPECT(records.size() == 1U);
    EXPECT(records.front().filename == L"skse64_1_6_1170.dll");
    EXPECT(records.front().version == (InterfaceVersion{2, 2, 6}));
    EXPECT(records.front().sha256 == kSkseRuntimeDigest);
    EXPECT(records.front().file_size == kSkseRuntimeFileSize);

    FakeMemory memory = MakeValidMemory();
    memory.MapSymbol(100U, kDataAddress);
    const std::array<std::uint8_t, 1> bytes{0x44U};
    memory.Put(kDataAddress, bytes);
    SymbolProviderContext provider = MakeProvider();
    provider.skse_runtime_sha256[0] ^= 0xFFU;

    const SymbolValidation result = ValidateSymbol(
        MakeIdentity(), AdmitSyntheticRuntime(), provider, MakeDataDescriptor(), memory);

    EXPECT(result.status == SymbolStatus::Rejected);
    EXPECT(result.diagnostic == SymbolDiagnostic::SkseRuntimeUnsupported);
    EXPECT(!result.hook_arming_permitted);
    EXPECT(memory.writeCalls() == 0U);

    provider = MakeProvider();
    provider.skse_runtime_filename = L"skse64_loader.exe";
    const SymbolValidation loader = ValidateSymbol(
        MakeIdentity(), AdmitSyntheticRuntime(), provider, MakeDataDescriptor(), memory);

    EXPECT(loader.status == SymbolStatus::Rejected);
    EXPECT(loader.diagnostic == SymbolDiagnostic::SkseRuntimeUnsupported);
    EXPECT(!loader.hook_arming_permitted);
    EXPECT(memory.writeCalls() == 0U);
}

void complete_prologue_bytes_are_required_before_hook_arming()
{
    FakeMemory memory = MakeValidMemory();
    memory.MapSymbol(200U, kFunctionAddress);
    const std::array<std::uint8_t, 4> observed{0x48U, 0x89U, 0x5CU, 0x24U};
    const std::array<std::uint8_t, 4> expected{0x48U, 0x89U, 0x5CU, 0x25U};
    memory.Put(kFunctionAddress, observed);

    const SymbolValidation result = ValidateSymbol(
        MakeIdentity(),
        AdmitSyntheticRuntime(),
        MakeProvider(),
        MakePrologueDescriptor(expected),
        memory);

    EXPECT(result.status == SymbolStatus::Rejected);
    EXPECT(result.diagnostic == SymbolDiagnostic::InstructionBytesMismatch);
    EXPECT(!result.hook_arming_permitted);
    EXPECT(memory.writeCalls() == 0U);
}

void module_bounds_and_memory_protection_are_both_mandatory()
{
    const std::array<std::uint8_t, 4> expected{0x48U, 0x89U, 0x5CU, 0x24U};

    FakeMemory outside = MakeValidMemory();
    outside.MapSymbol(200U, kModuleBase + kModuleSize + 0x100U);
    outside.AddRegion(MemoryRegion{
        kModuleBase + kModuleSize + 0x100U,
        0x100U,
        true,
        false,
        true,
    });
    outside.Put(kModuleBase + kModuleSize + 0x100U, expected);
    const SymbolValidation out_of_range = ValidateSymbol(
        MakeIdentity(),
        AdmitSyntheticRuntime(),
        MakeProvider(),
        MakePrologueDescriptor(expected),
        outside);

    EXPECT(out_of_range.status == SymbolStatus::Rejected);
    EXPECT(out_of_range.diagnostic == SymbolDiagnostic::AddressOutsideModule);
    EXPECT(outside.writeCalls() == 0U);

    FakeMemory non_executable;
    non_executable.AddRegion(MemoryRegion{kFunctionAddress, 0x100U, true, false, false});
    non_executable.MapSymbol(200U, kFunctionAddress);
    non_executable.Put(kFunctionAddress, expected);
    const SymbolValidation wrong_protection = ValidateSymbol(
        MakeIdentity(),
        AdmitSyntheticRuntime(),
        MakeProvider(),
        MakePrologueDescriptor(expected),
        non_executable);

    EXPECT(wrong_protection.status == SymbolStatus::Rejected);
    EXPECT(wrong_protection.diagnostic == SymbolDiagnostic::RegionNotExecutable);
    EXPECT(non_executable.writeCalls() == 0U);
}

void vtable_identity_and_slot_target_must_validate_before_arming()
{
    FakeMemory memory = MakeValidMemory();
    memory.MapSymbol(300U, kVtableAddress);
    memory.SetRtti(kVtableAddress, ".?AVBSLightingShader@@");
    memory.PutValue(kVtableAddress + 4U * sizeof(std::uintptr_t), kVtableTarget);

    const SymbolValidation bad_identity = ValidateSymbol(
        MakeIdentity(),
        AdmitSyntheticRuntime(),
        MakeProvider(),
        MakeVtableDescriptor(),
        memory);

    EXPECT(bad_identity.status == SymbolStatus::Rejected);
    EXPECT(bad_identity.diagnostic == SymbolDiagnostic::RttiIdentityMismatch);
    EXPECT(!bad_identity.hook_arming_permitted);
    EXPECT(memory.writeCalls() == 0U);

    memory.SetRtti(kVtableAddress, ".?AVBSSkyShader@@");
    const SymbolValidation accepted = ValidateSymbol(
        MakeIdentity(),
        AdmitSyntheticRuntime(),
        MakeProvider(),
        MakeVtableDescriptor(),
        memory);

    EXPECT(accepted.status == SymbolStatus::Validated);
    EXPECT(accepted.diagnostic == SymbolDiagnostic::None);
    EXPECT(accepted.resolved_address == kVtableTarget);
    EXPECT(accepted.patch_address == kVtableAddress + 4U * sizeof(std::uintptr_t));
    EXPECT(accepted.hook_arming_permitted);
    EXPECT(memory.writeCalls() == 0U);
}

void observer_capabilities_are_absent_until_each_contract_validates()
{
    FakeMemory memory = MakeValidMemory();
    const std::array<std::uint8_t, 1> one_byte{0x7FU};

    std::array<SymbolDescriptor, CapabilityCount> descriptors{};
    for (std::size_t index = 0; index < descriptors.size(); ++index) {
        const auto capability = static_cast<Capability>(index);
        const auto id = 1000U + index;
        descriptors[index] = MakeDataDescriptor(capability, id);
        const auto address = kDataAddress + index * 0x10U;
        memory.MapSymbol(id, address);
        memory.Put(address, one_byte);
    }

    const CapabilityReport complete = EvaluateObserverCapabilities(
        MakeIdentity(),
        AdmitSyntheticRuntime(),
        MakeProvider(),
        descriptors,
        memory);

    for (std::size_t index = 0; index < CapabilityCount; ++index) {
        EXPECT(complete.entries[index].capability == static_cast<Capability>(index));
        EXPECT(complete.entries[index].state == CapabilityState::ObserveReady);
        EXPECT(complete.entries[index].diagnostic == SymbolDiagnostic::None);
    }

    SymbolProviderContext unavailable = MakeProvider();
    unavailable.relocation_provider_available = false;
    const CapabilityReport rejected = EvaluateObserverCapabilities(
        MakeIdentity(),
        AdmitSyntheticRuntime(),
        unavailable,
        descriptors,
        memory);

    for (const auto& entry : rejected.entries) {
        EXPECT(entry.state == CapabilityState::Unavailable);
        EXPECT(entry.diagnostic == SymbolDiagnostic::RelocationProviderUnavailable);
    }
    EXPECT(memory.writeCalls() == 0U);
}

MutationPrerequisites MakeMutationPrerequisites()
{
    MutationPrerequisites prerequisites;
    prerequisites.feature_gate_enabled = true;
    prerequisites.lifecycle_mutation_permitted = true;
    prerequisites.runtime_evaluation = AdmitSyntheticRuntime();
    prerequisites.capability = CapabilityEntry{
        Capability::SkyShaderObservation,
        CapabilityState::ObserveReady,
        SymbolDiagnostic::None,
    };
    return prerequisites;
}

PropertyPatch MakePatch(
    const std::string_view identifier,
    const std::uintptr_t address,
    const std::span<const std::uint8_t> expected,
    const std::span<const std::uint8_t> replacement)
{
    return PropertyPatch{identifier, address, expected, replacement};
}

void mutation_gates_reject_without_writes()
{
    FakeMemory memory = MakeValidMemory();
    const std::array<std::uint8_t, 4> baseline{0x01U, 0x02U, 0x03U, 0x04U};
    const std::array<std::uint8_t, 4> replacement{0x10U, 0x20U, 0x30U, 0x40U};
    memory.Put(kPropertyAddress, baseline);
    const std::array patches{MakePatch("sky-property", kPropertyAddress, baseline, replacement)};

    PatchJournal journal;
    MutationPrerequisites gate = MakeMutationPrerequisites();
    gate.feature_gate_enabled = false;
    const PatchResult disabled = journal.Apply(MakeIdentity(), gate, patches, memory);

    EXPECT(disabled.code == PatchCode::FeatureGateDisabled);
    EXPECT(disabled.state == PatchState::Pristine);
    EXPECT(memory.Get(kPropertyAddress, baseline.size())
        == std::vector<std::uint8_t>(baseline.begin(), baseline.end()));
    EXPECT(memory.writeCalls() == 0U);

    gate = MakeMutationPrerequisites();
    gate.runtime_evaluation.status = RuntimeStatus::Rejected;
    const PatchResult unsupported = journal.Apply(MakeIdentity(), gate, patches, memory);

    EXPECT(unsupported.code == PatchCode::RuntimeNotAdmitted);
    EXPECT(unsupported.state == PatchState::Pristine);
    EXPECT(memory.writeCalls() == 0U);
}

void mutation_rejects_an_admission_evaluated_for_a_different_loaded_image()
{
    FakeMemory memory = MakeValidMemory();
    const std::array<std::uint8_t, 2> baseline{0x51U, 0x52U};
    const std::array<std::uint8_t, 2> replacement{0x61U, 0x62U};
    memory.Put(kPropertyAddress, baseline);
    const std::array patches{
        MakePatch("sky-property", kPropertyAddress, baseline, replacement)};

    MutationPrerequisites gate = MakeMutationPrerequisites();
    ++gate.runtime_evaluation.image_base;
    PatchJournal journal;
    const PatchResult result = journal.Apply(MakeIdentity(), gate, patches, memory);

    EXPECT(result.code == PatchCode::RuntimeNotAdmitted);
    EXPECT(result.state == PatchState::Pristine);
    EXPECT(memory.Get(kPropertyAddress, baseline.size())
        == std::vector<std::uint8_t>(baseline.begin(), baseline.end()));
    EXPECT(memory.writeCalls() == 0U);
}

void an_incomplete_runtime_receipt_cannot_enable_symbols_or_mutation()
{
    FakeMemory memory = MakeValidMemory();
    memory.MapSymbol(100U, kDataAddress);
    const std::array<std::uint8_t, 1> observed{0x44U};
    memory.Put(kDataAddress, observed);

    RuntimeEvaluation forged = AdmitSyntheticRuntime();
    forged.matched_record = NoMatchedRuntimeRecord;
    const SymbolValidation symbol = ValidateSymbol(
        MakeIdentity(), forged, MakeProvider(), MakeDataDescriptor(), memory);

    EXPECT(symbol.status == SymbolStatus::Rejected);
    EXPECT(symbol.diagnostic == SymbolDiagnostic::RuntimeIdentityMismatch);
    EXPECT(memory.writeCalls() == 0U);

    const std::array<std::uint8_t, 2> baseline{0x51U, 0x52U};
    const std::array<std::uint8_t, 2> replacement{0x61U, 0x62U};
    memory.Put(kPropertyAddress, baseline);
    const std::array patches{
        MakePatch("sky-property", kPropertyAddress, baseline, replacement)};
    MutationPrerequisites gate = MakeMutationPrerequisites();
    gate.runtime_evaluation = forged;
    PatchJournal journal;
    const PatchResult patch = journal.Apply(MakeIdentity(), gate, patches, memory);

    EXPECT(patch.code == PatchCode::RuntimeNotAdmitted);
    EXPECT(patch.state == PatchState::Pristine);
    EXPECT(memory.Get(kPropertyAddress, baseline.size())
        == std::vector<std::uint8_t>(baseline.begin(), baseline.end()));
    EXPECT(memory.writeCalls() == 0U);
}

void partial_apply_is_compensated_back_to_the_exact_baseline()
{
    FakeMemory memory = MakeValidMemory();
    const std::array<std::uint8_t, 2> first_before{0x01U, 0x02U};
    const std::array<std::uint8_t, 2> first_after{0x11U, 0x12U};
    const std::array<std::uint8_t, 2> second_before{0x03U, 0x04U};
    const std::array<std::uint8_t, 2> second_after{0x13U, 0x14U};
    memory.Put(kPropertyAddress, first_before);
    memory.Put(kPropertyAddress + 0x10U, second_before);
    memory.FailWriteCall(2U);

    const std::array patches{
        MakePatch("first", kPropertyAddress, first_before, first_after),
        MakePatch("second", kPropertyAddress + 0x10U, second_before, second_after),
    };
    PatchJournal journal;
    const PatchResult result = journal.Apply(
        MakeIdentity(), MakeMutationPrerequisites(), patches, memory);

    EXPECT(result.code == PatchCode::ApplyFailedRolledBack);
    EXPECT(result.state == PatchState::Pristine);
    EXPECT(result.compensation_performed);
    EXPECT(memory.Get(kPropertyAddress, first_before.size())
        == std::vector<std::uint8_t>(first_before.begin(), first_before.end()));
    EXPECT(memory.Get(kPropertyAddress + 0x10U, second_before.size())
        == std::vector<std::uint8_t>(second_before.begin(), second_before.end()));
    EXPECT(memory.writeCalls() == 3U);
}

void overlapping_property_ranges_are_rejected_before_the_first_write()
{
    FakeMemory memory = MakeValidMemory();
    const std::array<std::uint8_t, 4> first_before{0x01U, 0x02U, 0x03U, 0x04U};
    const std::array<std::uint8_t, 4> first_after{0x11U, 0x12U, 0x13U, 0x14U};
    const std::array<std::uint8_t, 2> second_before{0x03U, 0x04U};
    const std::array<std::uint8_t, 2> second_after{0x23U, 0x24U};
    memory.Put(kPropertyAddress, first_before);

    const std::array patches{
        MakePatch("first", kPropertyAddress, first_before, first_after),
        MakePatch("overlap", kPropertyAddress + 2U, second_before, second_after),
    };
    PatchJournal journal;
    const PatchResult result = journal.Apply(
        MakeIdentity(), MakeMutationPrerequisites(), patches, memory);

    EXPECT(result.code == PatchCode::InvalidPatch);
    EXPECT(result.state == PatchState::Pristine);
    EXPECT(memory.Get(kPropertyAddress, first_before.size())
        == std::vector<std::uint8_t>(first_before.begin(), first_before.end()));
    EXPECT(memory.writeCalls() == 0U);
}

void rollback_failure_keeps_the_applied_state_and_bytes_when_no_write_occurs()
{
    FakeMemory memory = MakeValidMemory();
    const std::array<std::uint8_t, 2> baseline{0x21U, 0x22U};
    const std::array<std::uint8_t, 2> replacement{0x31U, 0x32U};
    memory.Put(kPropertyAddress, baseline);
    const std::array patches{MakePatch("sky-property", kPropertyAddress, baseline, replacement)};

    PatchJournal journal;
    const PatchResult applied = journal.Apply(
        MakeIdentity(), MakeMutationPrerequisites(), patches, memory);
    EXPECT(applied.code == PatchCode::Applied);

    memory.FailWriteCall(2U);
    const PatchResult failed = journal.Rollback(memory);

    EXPECT(failed.code == PatchCode::RollbackFailedStillApplied);
    EXPECT(failed.state == PatchState::Applied);
    EXPECT(memory.Get(kPropertyAddress, replacement.size())
        == std::vector<std::uint8_t>(replacement.begin(), replacement.end()));
}

void a_validated_property_patch_applies_and_reverses_idempotently()
{
    FakeMemory memory = MakeValidMemory();
    const std::array<std::uint8_t, 4> baseline{0x40U, 0x00U, 0x00U, 0x00U};
    const std::array<std::uint8_t, 4> replacement{0x44U, 0x00U, 0x00U, 0x00U};
    memory.Put(kPropertyAddress, baseline);
    const std::array patches{MakePatch("sky-flags", kPropertyAddress, baseline, replacement)};

    PatchJournal journal;
    const PatchResult first = journal.Apply(
        MakeIdentity(), MakeMutationPrerequisites(), patches, memory);
    const PatchResult repeated = journal.Apply(
        MakeIdentity(), MakeMutationPrerequisites(), patches, memory);

    EXPECT(first.code == PatchCode::Applied);
    EXPECT(first.state == PatchState::Applied);
    EXPECT(repeated.code == PatchCode::AlreadyApplied);
    EXPECT(repeated.state == PatchState::Applied);
    EXPECT(memory.Get(kPropertyAddress, replacement.size())
        == std::vector<std::uint8_t>(replacement.begin(), replacement.end()));
    EXPECT(memory.writeCalls() == 1U);

    const PatchResult restored = journal.Rollback(memory);
    const PatchResult repeated_restore = journal.Rollback(memory);

    EXPECT(restored.code == PatchCode::RolledBack);
    EXPECT(restored.state == PatchState::Pristine);
    EXPECT(repeated_restore.code == PatchCode::AlreadyPristine);
    EXPECT(repeated_restore.state == PatchState::Pristine);
    EXPECT(memory.Get(kPropertyAddress, baseline.size())
        == std::vector<std::uint8_t>(baseline.begin(), baseline.end()));
    EXPECT(memory.writeCalls() == 2U);
}

} // namespace

int main()
{
    runtime_identity_requires_an_exact_executable_fingerprint();
    captured_skyrim_1_6_1170_is_the_only_built_in_runtime_fingerprint();
    a_wrapping_loaded_image_range_is_rejected_before_fingerprint_admission();
    runtime_evaluation_owns_its_original_filename_receipt();
    missing_relocation_provider_rejects_without_touching_memory();
    relocation_provider_admission_requires_an_exact_artifact_identity();
    skse_provider_admission_uses_the_runtime_dll_and_never_loader_bytes();
    complete_prologue_bytes_are_required_before_hook_arming();
    module_bounds_and_memory_protection_are_both_mandatory();
    vtable_identity_and_slot_target_must_validate_before_arming();
    observer_capabilities_are_absent_until_each_contract_validates();
    mutation_gates_reject_without_writes();
    mutation_rejects_an_admission_evaluated_for_a_different_loaded_image();
    an_incomplete_runtime_receipt_cannot_enable_symbols_or_mutation();
    partial_apply_is_compensated_back_to_the_exact_baseline();
    overlapping_property_ranges_are_rejected_before_the_first_write();
    rollback_failure_keeps_the_applied_state_and_bytes_when_no_write_occurs();
    a_validated_property_patch_applies_and_reverses_idempotently();

    if (failures != 0) {
        std::cerr << failures << " assertion(s) failed\n";
        return 1;
    }

    std::cout << "Skyrim engine bridge tests passed\n";
    return 0;
}
