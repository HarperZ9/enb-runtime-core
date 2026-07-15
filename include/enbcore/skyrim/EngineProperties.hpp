#pragma once

#include <enbcore/skyrim/EngineBridge.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <variant>

namespace enbcore::skyrim {

using Matrix4x4 = std::array<float, 16>;

enum class EnginePropertyId : std::uint8_t {
    CameraWorldFov = 0,
    CameraFirstPersonFov = 1,
    CameraInverseViewProjection = 2,
    WeatherCurrentFormId = 3,
    WeatherOutgoingFormId = 4,
    WeatherTransition = 5,
    CalendarGameHour = 6,
};

inline constexpr std::size_t EnginePropertyCount = 7;

enum class EnginePropertyValueKind : std::uint8_t {
    Float32 = 0,
    UInt32 = 1,
    Matrix4x4 = 2,
};

enum class EnginePropertyAccess : std::uint8_t {
    ObserveOnly = 0,
    ObserveAndReversibleObjectWrite = 1,
};

using EnginePropertyValue = std::variant<float, std::uint32_t, Matrix4x4>;

struct EnginePropertySchema final {
    EnginePropertyId id{EnginePropertyId::CameraWorldFov};
    std::string_view name;
    EnginePropertyValueKind value_kind{EnginePropertyValueKind::Float32};
    EnginePropertyAccess access{EnginePropertyAccess::ObserveOnly};
    Capability capability{Capability::CameraInverseViewProjection};
    bool has_float_range{false};
    float minimum_float{0.0F};
    float maximum_float{0.0F};
};

[[nodiscard]] std::span<const EnginePropertySchema>
SupportedEngineProperties() noexcept;

[[nodiscard]] std::optional<EnginePropertySchema> FindEngineProperty(
    std::string_view name) noexcept;

[[nodiscard]] std::optional<EnginePropertySchema> FindEngineProperty(
    EnginePropertyId id) noexcept;

enum class EnginePropertyDiagnostic : std::uint8_t {
    None = 0,
    UnknownIdentifier = 1,
    RuntimeNotAdmitted = 2,
    CapabilityUnavailable = 3,
    AdapterReadFailed = 4,
    ValueTypeMismatch = 5,
    ValueOutOfRange = 6,
    MutationNotAllowed = 7,
    BindingUnavailable = 8,
    TransactionRejected = 9,
};

[[nodiscard]] EnginePropertyDiagnostic ValidateEnginePropertyValue(
    const EnginePropertySchema& schema,
    const EnginePropertyValue& value) noexcept;

struct ObjectOwnerToken final {
    std::uintptr_t address{0};
    std::uint64_t generation{0};

    [[nodiscard]] constexpr bool operator==(
        const ObjectOwnerToken&) const noexcept = default;
};

struct ObjectPropertyBinding final {
    EnginePropertyId property{EnginePropertyId::CameraWorldFov};
    ObjectOwnerToken owner{};
    std::uintptr_t field_address{0};
};

class EnginePropertyAdapter {
public:
    virtual ~EnginePropertyAdapter() = default;

    [[nodiscard]] virtual bool Observe(
        EnginePropertyId id,
        EnginePropertyValue& value) const noexcept = 0;

    [[nodiscard]] virtual std::optional<ObjectPropertyBinding>
    ResolveWritableBinding(EnginePropertyId id) const noexcept = 0;
};

struct PropertyObservation final {
    EnginePropertyDiagnostic diagnostic{EnginePropertyDiagnostic::UnknownIdentifier};
    bool observed{false};
    EnginePropertyValue value{0.0F};
};

[[nodiscard]] PropertyObservation ObserveEngineProperty(
    const ExecutableIdentity& identity,
    const RuntimeEvaluation& runtime,
    const CapabilityReport& capabilities,
    std::string_view name,
    const EnginePropertyAdapter& adapter) noexcept;

} // namespace enbcore::skyrim
