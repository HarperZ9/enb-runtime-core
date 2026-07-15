#include <enbcore/enb/SdkContract.hpp>

namespace enbcore::enb {

std::uint32_t ParameterSize(const ParameterKind kind) noexcept
{
    switch (kind) {
    case ParameterKind::Float:
    case ParameterKind::Integer:
    case ParameterKind::Hex:
    case ParameterKind::Boolean:
        return 4;

    case ParameterKind::Color3:
    case ParameterKind::Vector3:
        return 12;

    case ParameterKind::Color4:
        return 16;

    case ParameterKind::None:
        return 0;
    }

    return 0;
}

ValidationResult ValidateParameter(const Parameter& parameter) noexcept
{
    if (parameter.size > kParameterPayloadBytes) {
        return ValidationResult{ValidationCode::ParameterPayloadTooLarge};
    }

    switch (parameter.type) {
    case ParameterKind::Float:
    case ParameterKind::Integer:
    case ParameterKind::Hex:
    case ParameterKind::Boolean:
    case ParameterKind::Color3:
    case ParameterKind::Color4:
    case ParameterKind::Vector3:
        break;

    case ParameterKind::None:
    default:
        return ValidationResult{ValidationCode::InvalidParameterKind};
    }

    if (parameter.size != ParameterSize(parameter.type)) {
        return ValidationResult{ValidationCode::InvalidParameterSize};
    }

    return ValidationResult{ValidationCode::Accepted};
}

ValidationResult ValidateSdkHost(const SdkExports& exports) noexcept
{
    if (exports.get_sdk_version == nullptr) {
        return ValidationResult{ValidationCode::MissingGetSdkVersion};
    }
    if (exports.get_version == nullptr) {
        return ValidationResult{ValidationCode::MissingGetVersion};
    }
    if (exports.get_game_identifier == nullptr) {
        return ValidationResult{ValidationCode::MissingGetGameIdentifier};
    }
    if (exports.set_callback_function == nullptr) {
        return ValidationResult{ValidationCode::MissingSetCallbackFunction};
    }
    if (exports.get_parameter == nullptr) {
        return ValidationResult{ValidationCode::MissingGetParameter};
    }
    if (exports.set_parameter == nullptr) {
        return ValidationResult{ValidationCode::MissingSetParameter};
    }
    if (exports.get_render_info == nullptr) {
        return ValidationResult{ValidationCode::MissingGetRenderInfo};
    }
    if (exports.get_state == nullptr) {
        return ValidationResult{ValidationCode::MissingGetState};
    }

    const SdkInteger reported_version = exports.get_sdk_version();
    if (reported_version < kSdkFamilyBegin || reported_version >= kSdkFamilyEnd) {
        return ValidationResult{ValidationCode::WrongSdkFamily};
    }
    if (reported_version < kSdkVersion) {
        return ValidationResult{ValidationCode::SdkVersionTooOld};
    }
    if (exports.get_game_identifier() != kGameIdentifier) {
        return ValidationResult{ValidationCode::WrongGameIdentifier};
    }
    if (exports.get_render_info() == nullptr) {
        return ValidationResult{ValidationCode::RenderInfoNotReady};
    }

    return ValidationResult{ValidationCode::Accepted};
}

} // namespace enbcore::enb
