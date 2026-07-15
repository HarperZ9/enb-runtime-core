#include <enbcore/enb/LoadedHostResolver.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

namespace enbcore::enb {
namespace {

constexpr std::array<std::string_view, 8> kExportNames{
    "ENBGetSDKVersion",
    "ENBGetVersion",
    "ENBGetGameIdentifier",
    "ENBSetCallbackFunction",
    "ENBGetParameter",
    "ENBSetParameter",
    "ENBGetRenderInfo",
    "ENBGetState",
};

struct Candidate final {
    LoadedModule module{0};
    SdkExports exports{};
};

template <typename Function>
[[nodiscard]] Function FunctionAt(const ExportAddress address) noexcept
{
    return address == 0 ? nullptr : reinterpret_cast<Function>(address);
}

[[nodiscard]] bool IsComplete(const SdkExports& exports) noexcept
{
    return exports.get_sdk_version != nullptr
        && exports.get_version != nullptr
        && exports.get_game_identifier != nullptr
        && exports.set_callback_function != nullptr
        && exports.get_parameter != nullptr
        && exports.set_parameter != nullptr
        && exports.get_render_info != nullptr
        && exports.get_state != nullptr;
}

[[nodiscard]] SdkExports ResolveExports(
    LoadedModulePlatform& platform,
    const LoadedModule module,
    const ExportAddress anchor) noexcept
{
    SdkExports exports;
    exports.get_sdk_version = FunctionAt<GetSdkVersion>(anchor);
    exports.get_version = FunctionAt<GetVersion>(
        platform.FindExport(module, kExportNames[1]));
    exports.get_game_identifier = FunctionAt<GetGameIdentifier>(
        platform.FindExport(module, kExportNames[2]));
    exports.set_callback_function = FunctionAt<SetCallbackFunction>(
        platform.FindExport(module, kExportNames[3]));
    exports.get_parameter = FunctionAt<GetParameter>(
        platform.FindExport(module, kExportNames[4]));
    exports.set_parameter = FunctionAt<SetParameter>(
        platform.FindExport(module, kExportNames[5]));
    exports.get_render_info = FunctionAt<GetRenderInfo>(
        platform.FindExport(module, kExportNames[6]));
    exports.get_state = FunctionAt<GetState>(
        platform.FindExport(module, kExportNames[7]));
    return exports;
}

[[nodiscard]] HostResolutionCode MapValidationFailure(
    const ValidationCode code) noexcept
{
    switch (code) {
    case ValidationCode::WrongSdkFamily:
        return HostResolutionCode::WrongSdkFamily;
    case ValidationCode::SdkVersionTooOld:
        return HostResolutionCode::SdkVersionTooOld;
    case ValidationCode::WrongGameIdentifier:
        return HostResolutionCode::WrongGameIdentifier;
    default:
        return HostResolutionCode::CandidateRejected;
    }
}

} // namespace

HostResolutionResult ResolveLoadedEnbHost(LoadedModulePlatform& platform) noexcept
{
    LoadedModules loaded;
    try {
        loaded = platform.EnumerateLoadedModules();
    } catch (...) {
        return HostResolutionResult{HostResolutionCode::EnumerationFailed};
    }

    switch (loaded.status) {
    case ModuleEnumerationStatus::Failed:
        return HostResolutionResult{HostResolutionCode::EnumerationFailed};
    case ModuleEnumerationStatus::Truncated:
        return HostResolutionResult{HostResolutionCode::EnumerationTruncated};
    case ModuleEnumerationStatus::Complete:
        break;
    default:
        return HostResolutionResult{HostResolutionCode::EnumerationFailed};
    }

    std::vector<Candidate> candidates;
    bool saw_partial_candidate = false;

    try {
        candidates.reserve(loaded.modules.size());
        for (const LoadedModule module : loaded.modules) {
            const ExportAddress anchor = platform.FindExport(module, kExportNames[0]);
            if (anchor == 0) {
                continue;
            }

            SdkExports exports = ResolveExports(platform, module, anchor);
            if (!IsComplete(exports)) {
                saw_partial_candidate = true;
                continue;
            }

            candidates.push_back(Candidate{module, exports});
        }
    } catch (...) {
        return HostResolutionResult{HostResolutionCode::EnumerationFailed};
    }

    if (saw_partial_candidate) {
        return HostResolutionResult{HostResolutionCode::PartialCandidate};
    }
    if (candidates.empty()) {
        return HostResolutionResult{HostResolutionCode::HostNotFound};
    }

    std::optional<HostResolutionCode> incompatibility;
    std::optional<Candidate> compatible_candidate;
    bool compatible_candidate_ready = false;
    std::size_t compatible_count = 0;

    for (const Candidate& candidate : candidates) {
        const ValidationResult validation = ValidateSdkHost(candidate.exports);
        if (validation.code != ValidationCode::Accepted
            && validation.code != ValidationCode::RenderInfoNotReady) {
            if (!incompatibility.has_value()) {
                incompatibility = MapValidationFailure(validation.code);
            }
            continue;
        }

        ++compatible_count;
        if (!compatible_candidate.has_value()) {
            compatible_candidate = candidate;
            compatible_candidate_ready = validation.code == ValidationCode::Accepted;
        }
    }

    if (incompatibility.has_value()) {
        return HostResolutionResult{*incompatibility};
    }
    if (compatible_count > 1) {
        return HostResolutionResult{HostResolutionCode::AmbiguousCompatibleHosts};
    }
    if (!compatible_candidate.has_value()) {
        return HostResolutionResult{HostResolutionCode::CandidateRejected};
    }

    HostResolutionResult result;
    result.code = compatible_candidate_ready
        ? HostResolutionCode::Ready
        : HostResolutionCode::NotReady;
    result.module = compatible_candidate->module;
    result.exports = compatible_candidate->exports;
    result.exports_resolved = true;
    return result;
}

} // namespace enbcore::enb
