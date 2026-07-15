#pragma once

#include <enbcore/enb/SdkContract.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

namespace enbcore::enb {

using LoadedModule = std::uintptr_t;
using ExportAddress = std::uintptr_t;

enum class ModuleEnumerationStatus : std::uint8_t {
    Complete = 0,
    Truncated = 1,
    Failed = 2,
};

struct LoadedModules final {
    ModuleEnumerationStatus status{ModuleEnumerationStatus::Failed};
    std::vector<LoadedModule> modules;
};

class LoadedModulePlatform {
public:
    virtual ~LoadedModulePlatform() = default;

    [[nodiscard]] virtual LoadedModules EnumerateLoadedModules() noexcept = 0;
    [[nodiscard]] virtual ExportAddress FindExport(
        LoadedModule module,
        std::string_view exact_name) noexcept = 0;
};

class WindowsLoadedModulePlatform final : public LoadedModulePlatform {
public:
    [[nodiscard]] LoadedModules EnumerateLoadedModules() noexcept override;
    [[nodiscard]] ExportAddress FindExport(
        LoadedModule module,
        std::string_view exact_name) noexcept override;
};

enum class HostResolutionCode : std::uint8_t {
    Ready = 0,
    NotReady = 1,
    HostNotFound = 2,
    EnumerationFailed = 3,
    EnumerationTruncated = 4,
    PartialCandidate = 5,
    AmbiguousCompatibleHosts = 6,
    WrongSdkFamily = 7,
    SdkVersionTooOld = 8,
    WrongGameIdentifier = 9,
    CandidateRejected = 10,
};

struct HostResolutionResult final {
    HostResolutionCode code{HostResolutionCode::HostNotFound};
    LoadedModule module{0};
    SdkExports exports{};
    bool exports_resolved{false};

    [[nodiscard]] constexpr bool ready() const noexcept
    {
        return code == HostResolutionCode::Ready;
    }

    [[nodiscard]] constexpr bool retryable() const noexcept
    {
        return code == HostResolutionCode::NotReady;
    }
};

[[nodiscard]] HostResolutionResult ResolveLoadedEnbHost(
    LoadedModulePlatform& platform) noexcept;

} // namespace enbcore::enb
