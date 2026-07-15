#include <enbcore/enb/LoadedHostResolver.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <map>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using enbcore::enb::ExportAddress;
using enbcore::enb::LoadedModule;

int failures = 0;
int sdk_version_calls = 0;
int game_identifier_calls = 0;
int render_info_calls = 0;
enbcore::enb::RenderInfo render_info;
bool render_info_ready = true;

void expect(const bool condition, const char* expression, const char* file, const int line)
{
    if (condition) {
        return;
    }

    std::cerr << file << ':' << line << ": expectation failed: " << expression << '\n';
    ++failures;
}

#define EXPECT(expression) expect((expression), #expression, __FILE__, __LINE__)

template <typename Function>
ExportAddress address_of(Function function) noexcept
{
    return reinterpret_cast<ExportAddress>(function);
}

enbcore::enb::SdkInteger fake_get_sdk_version()
{
    ++sdk_version_calls;
    return enbcore::enb::kSdkVersion;
}

enbcore::enb::SdkInteger fake_get_old_sdk_version()
{
    ++sdk_version_calls;
    return enbcore::enb::kSdkVersion - 1;
}

enbcore::enb::SdkInteger fake_get_foreign_sdk_version()
{
    ++sdk_version_calls;
    return enbcore::enb::kSdkFamilyEnd;
}

enbcore::enb::SdkInteger fake_get_version()
{
    return 504;
}

enbcore::enb::SdkInteger fake_get_game_identifier()
{
    ++game_identifier_calls;
    return enbcore::enb::kGameIdentifier;
}

enbcore::enb::SdkInteger fake_get_wrong_game_identifier()
{
    ++game_identifier_calls;
    return enbcore::enb::kGameIdentifier + 1;
}

void fake_set_callback(enbcore::enb::CallbackFunction)
{
}

enbcore::enb::SdkBoolean fake_get_parameter(
    char*,
    char*,
    char*,
    enbcore::enb::Parameter*)
{
    return 1;
}

enbcore::enb::SdkBoolean fake_set_parameter(
    char*,
    char*,
    char*,
    enbcore::enb::Parameter*)
{
    return 1;
}

enbcore::enb::RenderInfo* fake_get_render_info()
{
    ++render_info_calls;
    return render_info_ready ? &render_info : nullptr;
}

enbcore::enb::SdkInteger fake_get_state(enbcore::enb::StateId)
{
    return 0;
}

struct FakeModule final {
    LoadedModule handle{0};
    std::map<std::string, ExportAddress, std::less<>> exports;
};

class FakePlatform final : public enbcore::enb::LoadedModulePlatform {
public:
    enbcore::enb::ModuleEnumerationStatus enumeration_status{
        enbcore::enb::ModuleEnumerationStatus::Complete};
    std::vector<FakeModule> modules;
    std::vector<std::pair<LoadedModule, std::string>> lookups;

    [[nodiscard]] enbcore::enb::LoadedModules EnumerateLoadedModules() noexcept override
    {
        enbcore::enb::LoadedModules result;
        result.status = enumeration_status;
        for (const FakeModule& module : modules) {
            result.modules.push_back(module.handle);
        }
        return result;
    }

    [[nodiscard]] ExportAddress FindExport(
        const LoadedModule module,
        const std::string_view exact_name) noexcept override
    {
        lookups.emplace_back(module, exact_name);
        const auto found_module = std::ranges::find(
            modules,
            module,
            &FakeModule::handle);
        if (found_module == modules.end()) {
            return 0;
        }

        const auto found_export = found_module->exports.find(exact_name);
        return found_export == found_module->exports.end() ? 0 : found_export->second;
    }
};

std::map<std::string, ExportAddress, std::less<>> complete_exports()
{
    return {
        {"ENBGetSDKVersion", address_of(&fake_get_sdk_version)},
        {"ENBGetVersion", address_of(&fake_get_version)},
        {"ENBGetGameIdentifier", address_of(&fake_get_game_identifier)},
        {"ENBSetCallbackFunction", address_of(&fake_set_callback)},
        {"ENBGetParameter", address_of(&fake_get_parameter)},
        {"ENBSetParameter", address_of(&fake_set_parameter)},
        {"ENBGetRenderInfo", address_of(&fake_get_render_info)},
        {"ENBGetState", address_of(&fake_get_state)},
    };
}

void reset_host()
{
    sdk_version_calls = 0;
    game_identifier_calls = 0;
    render_info_calls = 0;
    render_info_ready = true;
}

void enumeration_failures_do_not_probe_modules()
{
    using namespace enbcore::enb;

    for (const auto [enumeration, expected] : std::array{
             std::pair{ModuleEnumerationStatus::Failed, HostResolutionCode::EnumerationFailed},
             std::pair{ModuleEnumerationStatus::Truncated, HostResolutionCode::EnumerationTruncated},
             std::pair{static_cast<ModuleEnumerationStatus>(99), HostResolutionCode::EnumerationFailed}}) {
        FakePlatform platform;
        platform.enumeration_status = enumeration;
        platform.modules.push_back(FakeModule{1, complete_exports()});

        const HostResolutionResult result = ResolveLoadedEnbHost(platform);

        EXPECT(result.code == expected);
        EXPECT(!result.exports_resolved);
        EXPECT(platform.lookups.empty());
    }
}

void modules_without_the_anchor_are_ignored_even_with_decoys()
{
    using namespace enbcore::enb;

    FakePlatform platform;
    auto decoys = complete_exports();
    decoys.erase("ENBGetSDKVersion");
    decoys.emplace("ENBGetSDKVersioN", address_of(&fake_get_sdk_version));
    decoys.emplace("ENBGetSDKVersion.debug", address_of(&fake_get_sdk_version));
    platform.modules.push_back(FakeModule{10, std::move(decoys)});
    platform.modules.push_back(FakeModule{11, {{"ENBGetState", address_of(&fake_get_state)}}});

    const HostResolutionResult result = ResolveLoadedEnbHost(platform);

    EXPECT(result.code == HostResolutionCode::HostNotFound);
    EXPECT(!result.exports_resolved);
    EXPECT(platform.lookups.size() == 2U);
    EXPECT(std::ranges::all_of(platform.lookups, [](const auto& lookup) {
        return lookup.second == "ENBGetSDKVersion";
    }));
}

void a_single_complete_host_resolves_exactly_the_eight_contract_names()
{
    using namespace enbcore::enb;

    reset_host();
    FakePlatform platform;
    auto exports = complete_exports();
    exports.emplace("ENBGetVersionDebug", address_of(&fake_get_version));
    exports.emplace("UnrelatedExport", address_of(&fake_get_version));
    platform.modules.push_back(FakeModule{20, std::move(exports)});

    const HostResolutionResult result = ResolveLoadedEnbHost(platform);

    EXPECT(result.code == HostResolutionCode::Ready);
    EXPECT(result.ready());
    EXPECT(!result.retryable());
    EXPECT(result.exports_resolved);
    EXPECT(result.module == 20U);
    EXPECT(result.exports.get_sdk_version == &fake_get_sdk_version);
    EXPECT(result.exports.get_version == &fake_get_version);
    EXPECT(result.exports.get_game_identifier == &fake_get_game_identifier);
    EXPECT(result.exports.set_callback_function == &fake_set_callback);
    EXPECT(result.exports.get_parameter == &fake_get_parameter);
    EXPECT(result.exports.set_parameter == &fake_set_parameter);
    EXPECT(result.exports.get_render_info == &fake_get_render_info);
    EXPECT(result.exports.get_state == &fake_get_state);
    EXPECT(platform.lookups.size() == 8U);

    constexpr std::array<std::string_view, 8> expected_names{
        "ENBGetSDKVersion",
        "ENBGetVersion",
        "ENBGetGameIdentifier",
        "ENBSetCallbackFunction",
        "ENBGetParameter",
        "ENBSetParameter",
        "ENBGetRenderInfo",
        "ENBGetState",
    };
    for (std::size_t index = 0; index < expected_names.size(); ++index) {
        EXPECT(platform.lookups[index].second == expected_names[index]);
    }
}

void a_partial_anchored_candidate_fails_before_calling_foreign_code()
{
    using namespace enbcore::enb;

    reset_host();
    FakePlatform platform;
    auto exports = complete_exports();
    exports.erase("ENBGetState");
    platform.modules.push_back(FakeModule{30, std::move(exports)});

    const HostResolutionResult result = ResolveLoadedEnbHost(platform);

    EXPECT(result.code == HostResolutionCode::PartialCandidate);
    EXPECT(!result.exports_resolved);
    EXPECT(sdk_version_calls == 0);
    EXPECT(game_identifier_calls == 0);
    EXPECT(render_info_calls == 0);
}

void a_partial_candidate_poisoning_a_complete_candidate_fails_closed()
{
    using namespace enbcore::enb;

    reset_host();
    FakePlatform platform;
    auto partial = complete_exports();
    partial.erase("ENBSetParameter");
    platform.modules.push_back(FakeModule{31, std::move(partial)});
    platform.modules.push_back(FakeModule{32, complete_exports()});

    const HostResolutionResult result = ResolveLoadedEnbHost(platform);

    EXPECT(result.code == HostResolutionCode::PartialCandidate);
    EXPECT(!result.exports_resolved);
    EXPECT(sdk_version_calls == 0);
}

void wrong_sdk_and_game_reports_fail_closed()
{
    using namespace enbcore::enb;

    struct Case final {
        ExportAddress sdk;
        ExportAddress game;
        HostResolutionCode expected;
    };

    const std::array cases{
        Case{address_of(&fake_get_foreign_sdk_version), address_of(&fake_get_game_identifier), HostResolutionCode::WrongSdkFamily},
        Case{address_of(&fake_get_old_sdk_version), address_of(&fake_get_game_identifier), HostResolutionCode::SdkVersionTooOld},
        Case{address_of(&fake_get_sdk_version), address_of(&fake_get_wrong_game_identifier), HostResolutionCode::WrongGameIdentifier},
    };

    LoadedModule handle = 40;
    for (const Case& test_case : cases) {
        reset_host();
        FakePlatform platform;
        auto exports = complete_exports();
        exports["ENBGetSDKVersion"] = test_case.sdk;
        exports["ENBGetGameIdentifier"] = test_case.game;
        platform.modules.push_back(FakeModule{handle++, std::move(exports)});

        const HostResolutionResult result = ResolveLoadedEnbHost(platform);

        EXPECT(result.code == test_case.expected);
        EXPECT(!result.exports_resolved);
        EXPECT(!result.ready());
        EXPECT(!result.retryable());
    }
}

void any_incompatible_complete_candidate_poisoning_a_valid_one_fails_closed()
{
    using namespace enbcore::enb;

    reset_host();
    FakePlatform platform;
    auto wrong = complete_exports();
    wrong["ENBGetGameIdentifier"] = address_of(&fake_get_wrong_game_identifier);
    platform.modules.push_back(FakeModule{50, complete_exports()});
    platform.modules.push_back(FakeModule{51, std::move(wrong)});

    const HostResolutionResult result = ResolveLoadedEnbHost(platform);

    EXPECT(result.code == HostResolutionCode::WrongGameIdentifier);
    EXPECT(!result.exports_resolved);
}

void multiple_compatible_complete_hosts_are_ambiguous()
{
    using namespace enbcore::enb;

    reset_host();
    FakePlatform platform;
    platform.modules.push_back(FakeModule{60, complete_exports()});
    platform.modules.push_back(FakeModule{61, complete_exports()});

    const HostResolutionResult result = ResolveLoadedEnbHost(platform);

    EXPECT(result.code == HostResolutionCode::AmbiguousCompatibleHosts);
    EXPECT(!result.exports_resolved);
    EXPECT(!result.ready());
}

void a_single_compatible_host_without_render_info_is_retryable_and_retained()
{
    using namespace enbcore::enb;

    reset_host();
    render_info_ready = false;
    FakePlatform platform;
    platform.modules.push_back(FakeModule{70, complete_exports()});

    const HostResolutionResult waiting = ResolveLoadedEnbHost(platform);

    EXPECT(waiting.code == HostResolutionCode::NotReady);
    EXPECT(!waiting.ready());
    EXPECT(waiting.retryable());
    EXPECT(waiting.exports_resolved);
    EXPECT(waiting.module == 70U);
    EXPECT(waiting.exports.get_render_info == &fake_get_render_info);
    EXPECT(waiting.exports.get_sdk_version == &fake_get_sdk_version);

    render_info_ready = true;
    EXPECT(waiting.exports.get_render_info() == &render_info);
    platform.lookups.clear();
    const HostResolutionResult ready = ResolveLoadedEnbHost(platform);

    EXPECT(ready.code == HostResolutionCode::Ready);
    EXPECT(ready.ready());
    EXPECT(!ready.retryable());
    EXPECT(ready.exports_resolved);
    EXPECT(ready.module == waiting.module);
}

void an_empty_complete_snapshot_reports_no_host()
{
    using namespace enbcore::enb;

    FakePlatform platform;

    const HostResolutionResult result = ResolveLoadedEnbHost(platform);

    EXPECT(result.code == HostResolutionCode::HostNotFound);
    EXPECT(!result.exports_resolved);
    EXPECT(platform.lookups.empty());
}

#if defined(_WIN32)
void the_windows_adapter_enumerates_the_current_process_and_resolves_a_known_export()
{
    using namespace enbcore::enb;

    WindowsLoadedModulePlatform platform;
    const LoadedModules loaded = platform.EnumerateLoadedModules();

    EXPECT(loaded.status == ModuleEnumerationStatus::Complete);
    EXPECT(!loaded.modules.empty());

    bool found_known_export = false;
    for (const LoadedModule module : loaded.modules) {
        if (platform.FindExport(module, "GetCurrentProcessId") != 0) {
            found_known_export = true;
            break;
        }
    }
    EXPECT(found_known_export);
}
#endif

} // namespace

int main()
{
    enumeration_failures_do_not_probe_modules();
    modules_without_the_anchor_are_ignored_even_with_decoys();
    a_single_complete_host_resolves_exactly_the_eight_contract_names();
    a_partial_anchored_candidate_fails_before_calling_foreign_code();
    a_partial_candidate_poisoning_a_complete_candidate_fails_closed();
    wrong_sdk_and_game_reports_fail_closed();
    any_incompatible_complete_candidate_poisoning_a_valid_one_fails_closed();
    multiple_compatible_complete_hosts_are_ambiguous();
    a_single_compatible_host_without_render_info_is_retryable_and_retained();
    an_empty_complete_snapshot_reports_no_host();
#if defined(_WIN32)
    the_windows_adapter_enumerates_the_current_process_and_resolves_a_known_export();
#endif

    if (failures != 0) {
        std::cerr << failures << " assertion(s) failed\n";
        return 1;
    }

    std::cout << "Loaded host resolver tests passed\n";
    return 0;
}
