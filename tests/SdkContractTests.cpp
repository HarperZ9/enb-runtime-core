#include <enbcore/enb/SdkContract.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <type_traits>

namespace {

int failures = 0;

enbcore::enb::SdkInteger reported_sdk_version = enbcore::enb::kSdkVersion;
enbcore::enb::SdkInteger reported_game_identifier = enbcore::enb::kGameIdentifier;
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

enbcore::enb::SdkInteger fake_get_sdk_version()
{
    return reported_sdk_version;
}

enbcore::enb::SdkInteger fake_get_version()
{
    return 0;
}

enbcore::enb::SdkInteger fake_get_game_identifier()
{
    return reported_game_identifier;
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
    return render_info_ready ? &render_info : nullptr;
}

enbcore::enb::SdkInteger fake_get_state(enbcore::enb::StateId)
{
    return 0;
}

enbcore::enb::SdkExports valid_exports()
{
    using namespace enbcore::enb;

    return SdkExports{
        &fake_get_sdk_version,
        &fake_get_version,
        &fake_get_game_identifier,
        &fake_set_callback,
        &fake_get_parameter,
        &fake_set_parameter,
        &fake_get_render_info,
        &fake_get_state,
    };
}

void reset_fake_host()
{
    reported_sdk_version = enbcore::enb::kSdkVersion;
    reported_game_identifier = enbcore::enb::kGameIdentifier;
    render_info_ready = true;
}

void sdk_constants_are_fixed()
{
    using namespace enbcore::enb;

    EXPECT(kSdkVersion == 1002);
    EXPECT(kSdkFamilyBegin == 1000);
    EXPECT(kSdkFamilyEnd == 2000);
    EXPECT(kGameIdentifier == 0x10000006);
    EXPECT(kParameterPayloadBytes == 16U);
}

void parameter_kind_values_match_the_contract()
{
    using enum enbcore::enb::ParameterKind;

    constexpr std::array values{
        static_cast<std::int32_t>(None),
        static_cast<std::int32_t>(Float),
        static_cast<std::int32_t>(Integer),
        static_cast<std::int32_t>(Hex),
        static_cast<std::int32_t>(Boolean),
        static_cast<std::int32_t>(Color3),
        static_cast<std::int32_t>(Color4),
        static_cast<std::int32_t>(Vector3),
    };

    EXPECT((values == std::array<std::int32_t, 8>{0, 1, 2, 3, 4, 5, 6, 7}));
}

void callback_ids_one_through_eight_are_fixed()
{
    using enum enbcore::enb::CallbackId;

    constexpr std::array values{
        static_cast<std::int32_t>(EndFrame),
        static_cast<std::int32_t>(BeginFrame),
        static_cast<std::int32_t>(PreSave),
        static_cast<std::int32_t>(PostLoad),
        static_cast<std::int32_t>(OnInit),
        static_cast<std::int32_t>(OnExit),
        static_cast<std::int32_t>(PreReset),
        static_cast<std::int32_t>(PostReset),
    };

    EXPECT((values == std::array<std::int32_t, 8>{1, 2, 3, 4, 5, 6, 7, 8}));
}

void state_ids_eight_through_twenty_three_are_fixed()
{
    using enum enbcore::enb::StateId;

    constexpr std::array values{
        static_cast<std::int32_t>(WeatherCurrent),
        static_cast<std::int32_t>(WeatherOutgoing),
        static_cast<std::int32_t>(WeatherTransition),
        static_cast<std::int32_t>(TimeOfDay),
        static_cast<std::int32_t>(DawnFactor),
        static_cast<std::int32_t>(SunriseFactor),
        static_cast<std::int32_t>(DayFactor),
        static_cast<std::int32_t>(SunsetFactor),
        static_cast<std::int32_t>(DuskFactor),
        static_cast<std::int32_t>(NightFactor),
        static_cast<std::int32_t>(InteriorDayFactor),
        static_cast<std::int32_t>(InteriorNightFactor),
        static_cast<std::int32_t>(NightDayFactor),
        static_cast<std::int32_t>(InteriorFactor),
        static_cast<std::int32_t>(WorldSpaceId),
        static_cast<std::int32_t>(LocationId),
    };

    EXPECT((values == std::array<std::int32_t, 16>{
                          8, 9, 10, 11, 12, 13, 14, 15,
                          16, 17, 18, 19, 20, 21, 22, 23}));
}

void parameter_sizes_are_deterministic()
{
    using namespace enbcore::enb;

    EXPECT(ParameterSize(ParameterKind::None) == 0U);
    EXPECT(ParameterSize(ParameterKind::Float) == 4U);
    EXPECT(ParameterSize(ParameterKind::Integer) == 4U);
    EXPECT(ParameterSize(ParameterKind::Hex) == 4U);
    EXPECT(ParameterSize(ParameterKind::Boolean) == 4U);
    EXPECT(ParameterSize(ParameterKind::Color3) == 12U);
    EXPECT(ParameterSize(ParameterKind::Color4) == 16U);
    EXPECT(ParameterSize(ParameterKind::Vector3) == 12U);
    EXPECT(ParameterSize(static_cast<ParameterKind>(99)) == 0U);
}

void payload_and_render_info_layouts_are_fixed()
{
    using namespace enbcore::enb;

    EXPECT(sizeof(ParameterKind) == 4U);
    EXPECT(sizeof(CallbackId) == 4U);
    EXPECT(sizeof(StateId) == 4U);
    EXPECT(sizeof(Parameter) == 24U);
    EXPECT(alignof(Parameter) == 4U);
    EXPECT(offsetof(Parameter, data) == 0U);
    EXPECT(offsetof(Parameter, size) == 16U);
    EXPECT(offsetof(Parameter, type) == 20U);

    EXPECT(sizeof(RenderInfo) == 32U);
    EXPECT(alignof(RenderInfo) == 8U);
    EXPECT(offsetof(RenderInfo, device) == 0U);
    EXPECT(offsetof(RenderInfo, device_context) == 8U);
    EXPECT(offsetof(RenderInfo, swap_chain) == 16U);
    EXPECT(offsetof(RenderInfo, screen_size_x) == 24U);
    EXPECT(offsetof(RenderInfo, screen_size_y) == 28U);
}

void export_function_pointer_types_are_exact()
{
    using namespace enbcore::enb;

    EXPECT((std::is_same_v<GetSdkVersion, SdkInteger (*)()>));
    EXPECT((std::is_same_v<GetVersion, SdkInteger (*)()>));
    EXPECT((std::is_same_v<GetGameIdentifier, SdkInteger (*)()>));
    EXPECT((std::is_same_v<SetCallbackFunction, void (*)(CallbackFunction)>));
    EXPECT((std::is_same_v<GetParameter, SdkBoolean (*)(char*, char*, char*, Parameter*)>));
    EXPECT((std::is_same_v<SetParameter, SdkBoolean (*)(char*, char*, char*, Parameter*)>));
    EXPECT((std::is_same_v<GetRenderInfo, RenderInfo* (*)()>));
    EXPECT((std::is_same_v<GetState, SdkInteger (*)(StateId)>));
}

void valid_parameter_shapes_are_accepted()
{
    using namespace enbcore::enb;

    constexpr std::array kinds{
        ParameterKind::Float,
        ParameterKind::Integer,
        ParameterKind::Hex,
        ParameterKind::Boolean,
        ParameterKind::Color3,
        ParameterKind::Color4,
        ParameterKind::Vector3,
    };

    for (const ParameterKind kind : kinds) {
        Parameter parameter;
        parameter.type = kind;
        parameter.size = ParameterSize(kind);

        const ValidationResult result = ValidateParameter(parameter);

        EXPECT(result.code == ValidationCode::Accepted);
        EXPECT(result.accepted());
    }
}

void oversized_parameter_payload_is_rejected()
{
    using namespace enbcore::enb;

    Parameter parameter;
    parameter.type = ParameterKind::Color4;
    parameter.size = 17;

    const ValidationResult result = ValidateParameter(parameter);

    EXPECT(result.code == ValidationCode::ParameterPayloadTooLarge);
    EXPECT(!result.accepted());
}

void invalid_parameter_kind_is_rejected()
{
    using namespace enbcore::enb;

    Parameter parameter;
    parameter.type = static_cast<ParameterKind>(99);
    parameter.size = 4;

    const ValidationResult result = ValidateParameter(parameter);

    EXPECT(result.code == ValidationCode::InvalidParameterKind);
    EXPECT(!result.accepted());
}

void mismatched_parameter_size_is_rejected()
{
    using namespace enbcore::enb;

    Parameter parameter;
    parameter.type = ParameterKind::Float;
    parameter.size = 12;

    const ValidationResult result = ValidateParameter(parameter);

    EXPECT(result.code == ValidationCode::InvalidParameterSize);
    EXPECT(!result.accepted());
}

void validation_result_defaults_to_fail_closed()
{
    using namespace enbcore::enb;

    const ValidationResult result;

    EXPECT(result.code == ValidationCode::MissingGetSdkVersion);
    EXPECT(!result.accepted());
}

void required_export_failures_have_stable_codes()
{
    using namespace enbcore::enb;

    reset_fake_host();
    SdkExports exports = valid_exports();

    exports.get_sdk_version = nullptr;
    EXPECT(ValidateSdkHost(exports).code == ValidationCode::MissingGetSdkVersion);
    exports = valid_exports();
    exports.get_version = nullptr;
    EXPECT(ValidateSdkHost(exports).code == ValidationCode::MissingGetVersion);
    exports = valid_exports();
    exports.get_game_identifier = nullptr;
    EXPECT(ValidateSdkHost(exports).code == ValidationCode::MissingGetGameIdentifier);
    exports = valid_exports();
    exports.set_callback_function = nullptr;
    EXPECT(ValidateSdkHost(exports).code == ValidationCode::MissingSetCallbackFunction);
    exports = valid_exports();
    exports.get_parameter = nullptr;
    EXPECT(ValidateSdkHost(exports).code == ValidationCode::MissingGetParameter);
    exports = valid_exports();
    exports.set_parameter = nullptr;
    EXPECT(ValidateSdkHost(exports).code == ValidationCode::MissingSetParameter);
    exports = valid_exports();
    exports.get_render_info = nullptr;
    EXPECT(ValidateSdkHost(exports).code == ValidationCode::MissingGetRenderInfo);
    exports = valid_exports();
    exports.get_state = nullptr;
    EXPECT(ValidateSdkHost(exports).code == ValidationCode::MissingGetState);

    EXPECT(static_cast<std::uint16_t>(ValidationCode::MissingGetSdkVersion) == 1U);
    EXPECT(static_cast<std::uint16_t>(ValidationCode::MissingGetState) == 8U);
}

void sdk_version_admission_is_the_1000_family_from_1002()
{
    using namespace enbcore::enb;

    reset_fake_host();
    const SdkExports exports = valid_exports();

    for (const SdkInteger version : std::array<SdkInteger, 2>{1002, 1999}) {
        reported_sdk_version = version;
        EXPECT(ValidateSdkHost(exports).code == ValidationCode::Accepted);
    }

    for (const SdkInteger version : std::array<SdkInteger, 2>{999, 2000}) {
        reported_sdk_version = version;
        EXPECT(ValidateSdkHost(exports).code == ValidationCode::WrongSdkFamily);
    }

    reported_sdk_version = 1001;
    EXPECT(ValidateSdkHost(exports).code == ValidationCode::SdkVersionTooOld);
}

void wrong_game_identifier_is_rejected()
{
    using namespace enbcore::enb;

    reset_fake_host();
    reported_game_identifier = kGameIdentifier + 1;

    const ValidationResult result = ValidateSdkHost(valid_exports());

    EXPECT(result.code == ValidationCode::WrongGameIdentifier);
    EXPECT(!result.accepted());
}

void unavailable_render_info_is_not_ready()
{
    using namespace enbcore::enb;

    reset_fake_host();
    render_info_ready = false;

    const ValidationResult result = ValidateSdkHost(valid_exports());

    EXPECT(result.code == ValidationCode::RenderInfoNotReady);
    EXPECT(!result.accepted());
}

void valid_sdk_host_is_accepted()
{
    using namespace enbcore::enb;

    reset_fake_host();

    const ValidationResult result = ValidateSdkHost(valid_exports());

    EXPECT(result.code == ValidationCode::Accepted);
    EXPECT(result.accepted());
}

} // namespace

int main()
{
    sdk_constants_are_fixed();
    parameter_kind_values_match_the_contract();
    callback_ids_one_through_eight_are_fixed();
    state_ids_eight_through_twenty_three_are_fixed();
    parameter_sizes_are_deterministic();
    payload_and_render_info_layouts_are_fixed();
    export_function_pointer_types_are_exact();
    valid_parameter_shapes_are_accepted();
    oversized_parameter_payload_is_rejected();
    invalid_parameter_kind_is_rejected();
    mismatched_parameter_size_is_rejected();
    validation_result_defaults_to_fail_closed();
    required_export_failures_have_stable_codes();
    sdk_version_admission_is_the_1000_family_from_1002();
    wrong_game_identifier_is_rejected();
    unavailable_render_info_is_not_ready();
    valid_sdk_host_is_accepted();

    if (failures != 0) {
        std::cerr << failures << " assertion(s) failed\n";
        return 1;
    }

    std::cout << "SdkContract tests passed\n";
    return 0;
}
