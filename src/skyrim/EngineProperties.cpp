#include <enbcore/skyrim/EngineProperties.hpp>

#include <algorithm>
#include <cmath>

namespace enbcore::skyrim {
namespace {

constexpr std::array kSchemas{
    EnginePropertySchema{
        EnginePropertyId::CameraWorldFov,
        "camera.world_fov",
        EnginePropertyValueKind::Float32,
        EnginePropertyAccess::ObserveAndReversibleObjectWrite,
        Capability::CameraInverseViewProjection,
        true,
        1.0F,
        179.0F,
    },
    EnginePropertySchema{
        EnginePropertyId::CameraFirstPersonFov,
        "camera.first_person_fov",
        EnginePropertyValueKind::Float32,
        EnginePropertyAccess::ObserveAndReversibleObjectWrite,
        Capability::CameraInverseViewProjection,
        true,
        1.0F,
        179.0F,
    },
    EnginePropertySchema{
        EnginePropertyId::CameraInverseViewProjection,
        "camera.inverse_view_projection",
        EnginePropertyValueKind::Matrix4x4,
        EnginePropertyAccess::ObserveOnly,
        Capability::CameraInverseViewProjection,
        false,
        0.0F,
        0.0F,
    },
    EnginePropertySchema{
        EnginePropertyId::WeatherCurrentFormId,
        "weather.current_form_id",
        EnginePropertyValueKind::UInt32,
        EnginePropertyAccess::ObserveOnly,
        Capability::WeatherTimeOfDay,
        false,
        0.0F,
        0.0F,
    },
    EnginePropertySchema{
        EnginePropertyId::WeatherOutgoingFormId,
        "weather.outgoing_form_id",
        EnginePropertyValueKind::UInt32,
        EnginePropertyAccess::ObserveOnly,
        Capability::WeatherTimeOfDay,
        false,
        0.0F,
        0.0F,
    },
    EnginePropertySchema{
        EnginePropertyId::WeatherTransition,
        "weather.transition",
        EnginePropertyValueKind::Float32,
        EnginePropertyAccess::ObserveOnly,
        Capability::WeatherTimeOfDay,
        true,
        0.0F,
        1.0F,
    },
    EnginePropertySchema{
        EnginePropertyId::CalendarGameHour,
        "calendar.game_hour",
        EnginePropertyValueKind::Float32,
        EnginePropertyAccess::ObserveOnly,
        Capability::WeatherTimeOfDay,
        true,
        0.0F,
        24.0F,
    },
};

[[nodiscard]] bool CapabilityIsReady(
    const CapabilityReport& report,
    const Capability capability) noexcept
{
    const auto index = static_cast<std::size_t>(capability);
    if (index >= report.entries.size()) {
        return false;
    }
    const CapabilityEntry& entry = report.entries[index];
    return entry.capability == capability
        && entry.state != CapabilityState::Unavailable
        && entry.diagnostic == SymbolDiagnostic::None;
}

} // namespace

std::span<const EnginePropertySchema> SupportedEngineProperties() noexcept
{
    return kSchemas;
}

std::optional<EnginePropertySchema> FindEngineProperty(
    const std::string_view name) noexcept
{
    const auto found = std::ranges::find(kSchemas, name, &EnginePropertySchema::name);
    if (found == kSchemas.end()) {
        return std::nullopt;
    }
    return *found;
}

std::optional<EnginePropertySchema> FindEngineProperty(
    const EnginePropertyId id) noexcept
{
    const auto found = std::ranges::find(kSchemas, id, &EnginePropertySchema::id);
    if (found == kSchemas.end()) {
        return std::nullopt;
    }
    return *found;
}

EnginePropertyDiagnostic ValidateEnginePropertyValue(
    const EnginePropertySchema& schema,
    const EnginePropertyValue& value) noexcept
{
    switch (schema.value_kind) {
    case EnginePropertyValueKind::Float32: {
        const float* typed = std::get_if<float>(&value);
        if (typed == nullptr) {
            return EnginePropertyDiagnostic::ValueTypeMismatch;
        }
        if (!std::isfinite(*typed)
            || (schema.has_float_range
                && (*typed < schema.minimum_float || *typed > schema.maximum_float))) {
            return EnginePropertyDiagnostic::ValueOutOfRange;
        }
        return EnginePropertyDiagnostic::None;
    }
    case EnginePropertyValueKind::UInt32:
        return std::holds_alternative<std::uint32_t>(value)
            ? EnginePropertyDiagnostic::None
            : EnginePropertyDiagnostic::ValueTypeMismatch;
    case EnginePropertyValueKind::Matrix4x4: {
        const Matrix4x4* typed = std::get_if<Matrix4x4>(&value);
        if (typed == nullptr) {
            return EnginePropertyDiagnostic::ValueTypeMismatch;
        }
        return std::ranges::all_of(*typed, [](const float component) {
            return std::isfinite(component);
        }) ? EnginePropertyDiagnostic::None
           : EnginePropertyDiagnostic::ValueOutOfRange;
    }
    default:
        return EnginePropertyDiagnostic::ValueTypeMismatch;
    }
}

PropertyObservation ObserveEngineProperty(
    const ExecutableIdentity& identity,
    const RuntimeEvaluation& runtime,
    const CapabilityReport& capabilities,
    const std::string_view name,
    const EnginePropertyAdapter& adapter) noexcept
{
    const std::optional schema = FindEngineProperty(name);
    if (!schema.has_value()) {
        return PropertyObservation{EnginePropertyDiagnostic::UnknownIdentifier, false, 0.0F};
    }
    if (!MatchesSupportedRuntime(identity, runtime)) {
        return PropertyObservation{EnginePropertyDiagnostic::RuntimeNotAdmitted, false, 0.0F};
    }
    if (!CapabilityIsReady(capabilities, schema->capability)) {
        return PropertyObservation{EnginePropertyDiagnostic::CapabilityUnavailable, false, 0.0F};
    }

    EnginePropertyValue value{0.0F};
    if (!adapter.Observe(schema->id, value)) {
        return PropertyObservation{EnginePropertyDiagnostic::AdapterReadFailed, false, 0.0F};
    }
    const EnginePropertyDiagnostic validation =
        ValidateEnginePropertyValue(*schema, value);
    if (validation != EnginePropertyDiagnostic::None) {
        return PropertyObservation{validation, false, 0.0F};
    }
    return PropertyObservation{EnginePropertyDiagnostic::None, true, value};
}

} // namespace enbcore::skyrim
