#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace enbcore::skyrim {

using Sha256Digest = std::array<std::uint8_t, 32>;

struct RuntimeVersion final {
    std::uint16_t major{0};
    std::uint16_t minor{0};
    std::uint16_t patch{0};
    std::uint16_t build{0};

    [[nodiscard]] constexpr bool operator==(
        const RuntimeVersion&) const noexcept = default;
};

struct InterfaceVersion final {
    std::uint16_t major{0};
    std::uint16_t minor{0};
    std::uint16_t patch{0};

    [[nodiscard]] constexpr bool operator==(
        const InterfaceVersion&) const noexcept = default;
};

struct SkseRuntimeRecord final {
    std::string_view identifier;
    std::wstring_view filename;
    InterfaceVersion version{};
    Sha256Digest sha256{};
    std::uint64_t file_size{0};
};

[[nodiscard]] std::span<const SkseRuntimeRecord>
SupportedSkseRuntimes() noexcept;

enum class ExecutableArchitecture : std::uint8_t {
    Unknown = 0,
    X64 = 1,
};

struct ExecutableIdentity final {
    std::wstring_view module_name;
    std::wstring original_filename;
    RuntimeVersion runtime_version{};
    ExecutableArchitecture architecture{ExecutableArchitecture::Unknown};
    Sha256Digest sha256{};
    std::uint64_t file_size{0};
    std::uintptr_t image_base{0};
    std::size_t image_size{0};
};

struct RuntimeSupportRecord final {
    std::string_view identifier;
    std::wstring_view original_filename;
    RuntimeVersion runtime_version{};
    ExecutableArchitecture architecture{ExecutableArchitecture::Unknown};
    Sha256Digest sha256{};
    std::uint64_t file_size{0};
};

[[nodiscard]] std::span<const RuntimeSupportRecord>
SupportedSkyrimRuntimes() noexcept;

enum class RuntimeStatus : std::uint8_t {
    Rejected = 0,
    Admitted = 1,
};

enum class RuntimeDiagnostic : std::uint8_t {
    None = 0,
    InvalidModuleName = 1,
    UnsupportedArchitecture = 2,
    InvalidModuleRange = 3,
    IncompleteFingerprint = 4,
    UnsupportedFingerprint = 5,
};

inline constexpr std::size_t NoMatchedRuntimeRecord =
    std::numeric_limits<std::size_t>::max();

struct RuntimeEvaluation final {
    RuntimeStatus status{RuntimeStatus::Rejected};
    RuntimeDiagnostic diagnostic{RuntimeDiagnostic::UnsupportedFingerprint};
    std::size_t matched_record{NoMatchedRuntimeRecord};
    std::wstring original_filename;
    RuntimeVersion runtime_version{};
    Sha256Digest sha256{};
    std::uint64_t file_size{0};
    std::uintptr_t image_base{0};
    std::size_t image_size{0};
};

[[nodiscard]] RuntimeEvaluation EvaluateRuntimeIdentity(
    const ExecutableIdentity& identity,
    std::span<const RuntimeSupportRecord> supported_runtimes) noexcept;

[[nodiscard]] bool MatchesSupportedRuntime(
    const ExecutableIdentity& identity,
    const RuntimeEvaluation& runtime) noexcept;

enum class RelocationProviderKind : std::uint8_t {
    None = 0,
    AddressLibrary = 1,
    EmbeddedManifest = 2,
};

enum class RuntimeVariant : std::uint8_t {
    None = 0,
    SpecialEdition = 1,
    AnniversaryEdition = 2,
};

struct RelocationProviderRecord final {
    std::string_view identifier;
    RelocationProviderKind kind{RelocationProviderKind::None};
    RuntimeVersion runtime_version{};
    RuntimeVariant runtime_variant{RuntimeVariant::None};
    Sha256Digest artifact_sha256{};
    std::uint64_t artifact_file_size{0};
};

[[nodiscard]] std::span<const RelocationProviderRecord>
SupportedRelocationProviders() noexcept;

struct SymbolProviderContext final {
    bool relocation_provider_available{false};
    RelocationProviderKind relocation_provider_kind{RelocationProviderKind::None};
    RuntimeVersion relocation_runtime{};
    RuntimeVariant runtime_variant{RuntimeVariant::None};
    InterfaceVersion relocation_provider_version{};
    Sha256Digest relocation_artifact_sha256{};
    std::uint64_t relocation_artifact_file_size{0};
    InterfaceVersion engine_adapter_version{};
    std::wstring_view skse_runtime_filename;
    InterfaceVersion skse_runtime_version{};
    Sha256Digest skse_runtime_sha256{};
    std::uint64_t skse_runtime_file_size{0};
};

struct RuntimeSymbolConstraint final {
    RuntimeVersion runtime_version{};
    RelocationProviderKind relocation_provider_kind{RelocationProviderKind::None};
    RuntimeVariant runtime_variant{RuntimeVariant::None};
    InterfaceVersion minimum_relocation_provider{};
    InterfaceVersion maximum_relocation_provider{};
    InterfaceVersion minimum_engine_adapter{};
    InterfaceVersion maximum_engine_adapter{};
    InterfaceVersion minimum_skse{};
    InterfaceVersion maximum_skse{};
};

enum class Capability : std::uint8_t {
    CameraInverseViewProjection = 0,
    DepthShaderResource = 1,
    WeatherTimeOfDay = 2,
    RenderPhases = 3,
    SkyShaderObservation = 4,
    LightingShaderObservation = 5,
};

inline constexpr std::size_t CapabilityCount = 6;

enum class SymbolContract : std::uint8_t {
    ReadOnlyData = 0,
    CompleteFunctionPrologue = 1,
    DeclaredVtableSlot = 2,
};

struct SymbolDescriptor final {
    std::string_view identifier;
    Capability capability{Capability::CameraInverseViewProjection};
    std::uint64_t relocation_id{0};
    RuntimeSymbolConstraint constraints{};
    SymbolContract contract{SymbolContract::ReadOnlyData};
    std::span<const std::uint8_t> expected_instruction_bytes;
    std::size_t vtable_slot{0};
    std::string_view expected_rtti_name;
};

struct MemoryRegion final {
    std::uintptr_t base{0};
    std::size_t size{0};
    bool readable{false};
    bool writable{false};
    bool executable{false};
};

class MemoryView {
public:
    virtual ~MemoryView() = default;

    [[nodiscard]] virtual std::optional<std::uintptr_t>
    ResolveRelocationId(std::uint64_t id) const noexcept = 0;

    [[nodiscard]] virtual std::optional<MemoryRegion> QueryRegion(
        std::uintptr_t address,
        std::size_t size) const noexcept = 0;

    [[nodiscard]] virtual bool Read(
        std::uintptr_t address,
        std::span<std::uint8_t> destination) const noexcept = 0;

    [[nodiscard]] virtual bool MatchesRtti(
        std::uintptr_t vtable,
        std::string_view expected_name) const noexcept = 0;
};

enum class SymbolStatus : std::uint8_t {
    Rejected = 0,
    Validated = 1,
};

enum class SymbolDiagnostic : std::uint8_t {
    None = 0,
    RuntimeRejected = 1,
    RuntimeIdentityMismatch = 2,
    RelocationProviderUnavailable = 3,
    RelocationRuntimeMismatch = 4,
    ProviderVersionUnsupported = 5,
    InvalidDescriptor = 6,
    AddressUnresolved = 7,
    AddressOutsideModule = 8,
    RegionUnavailable = 9,
    RegionNotReadable = 10,
    RegionNotExecutable = 11,
    InstructionReadFailed = 12,
    InstructionBytesMismatch = 13,
    RttiIdentityMismatch = 14,
    VtableEntryReadFailed = 15,
    MissingDescriptor = 16,
    SkseRuntimeUnsupported = 17,
    RelocationProviderUnsupported = 18,
};

struct SymbolValidation final {
    SymbolStatus status{SymbolStatus::Rejected};
    SymbolDiagnostic diagnostic{SymbolDiagnostic::InvalidDescriptor};
    std::uintptr_t resolved_address{0};
    std::uintptr_t patch_address{0};
    bool hook_arming_permitted{false};
};

[[nodiscard]] SymbolValidation ValidateSymbol(
    const ExecutableIdentity& identity,
    const RuntimeEvaluation& runtime,
    const SymbolProviderContext& provider,
    const SymbolDescriptor& descriptor,
    const MemoryView& memory) noexcept;

enum class CapabilityState : std::uint8_t {
    Unavailable = 0,
    ObserveReady = 1,
    HookReady = 2,
};

struct CapabilityEntry final {
    Capability capability{Capability::CameraInverseViewProjection};
    CapabilityState state{CapabilityState::Unavailable};
    SymbolDiagnostic diagnostic{SymbolDiagnostic::MissingDescriptor};
};

struct CapabilityReport final {
    std::array<CapabilityEntry, CapabilityCount> entries{};
};

[[nodiscard]] CapabilityReport EvaluateObserverCapabilities(
    const ExecutableIdentity& identity,
    const RuntimeEvaluation& runtime,
    const SymbolProviderContext& provider,
    std::span<const SymbolDescriptor> descriptors,
    const MemoryView& memory) noexcept;

} // namespace enbcore::skyrim
