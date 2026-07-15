#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace enbcore::enb {

inline constexpr std::int32_t kSdkVersion = 1002;
inline constexpr std::int32_t kSdkFamilyBegin = 1000;
inline constexpr std::int32_t kSdkFamilyEnd = 2000;
inline constexpr std::int32_t kGameIdentifier = 0x10000006;
inline constexpr std::size_t kParameterPayloadBytes = 16;

using SdkInteger = std::int32_t;
using SdkBoolean = std::int32_t;

enum class ParameterKind : std::int32_t {
    None = 0,
    Float = 1,
    Integer = 2,
    Hex = 3,
    Boolean = 4,
    Color3 = 5,
    Color4 = 6,
    Vector3 = 7,
};

enum class CallbackId : std::int32_t {
    EndFrame = 1,
    BeginFrame = 2,
    PreSave = 3,
    PostLoad = 4,
    OnInit = 5,
    OnExit = 6,
    PreReset = 7,
    PostReset = 8,
};

enum class StateId : std::int32_t {
    WeatherCurrent = 8,
    WeatherOutgoing = 9,
    WeatherTransition = 10,
    TimeOfDay = 11,
    DawnFactor = 12,
    SunriseFactor = 13,
    DayFactor = 14,
    SunsetFactor = 15,
    DuskFactor = 16,
    NightFactor = 17,
    InteriorDayFactor = 18,
    InteriorNightFactor = 19,
    NightDayFactor = 20,
    InteriorFactor = 21,
    WorldSpaceId = 22,
    LocationId = 23,
};

struct Parameter final {
    std::array<std::uint8_t, kParameterPayloadBytes> data{};
    std::uint32_t size{0};
    ParameterKind type{ParameterKind::None};
};

struct RenderInfo final {
    void* device{nullptr};
    void* device_context{nullptr};
    void* swap_chain{nullptr};
    std::uint32_t screen_size_x{0};
    std::uint32_t screen_size_y{0};
};

#if defined(_MSC_VER)
using CallbackFunction = void(__stdcall *)(CallbackId);
#elif defined(__GNUC__) && defined(__i386__)
using CallbackFunction = void(__attribute__((stdcall)) *)(CallbackId);
#else
using CallbackFunction = void (*)(CallbackId);
#endif

using GetSdkVersion = SdkInteger (*)();
using GetVersion = SdkInteger (*)();
using GetGameIdentifier = SdkInteger (*)();
using SetCallbackFunction = void (*)(CallbackFunction);
using GetParameter = SdkBoolean (*)(char*, char*, char*, Parameter*);
using SetParameter = SdkBoolean (*)(char*, char*, char*, Parameter*);
using GetRenderInfo = RenderInfo* (*)();
using GetState = SdkInteger (*)(StateId);

struct SdkExports final {
    GetSdkVersion get_sdk_version{nullptr};
    GetVersion get_version{nullptr};
    GetGameIdentifier get_game_identifier{nullptr};
    SetCallbackFunction set_callback_function{nullptr};
    GetParameter get_parameter{nullptr};
    SetParameter set_parameter{nullptr};
    GetRenderInfo get_render_info{nullptr};
    GetState get_state{nullptr};
};

enum class ValidationCode : std::uint16_t {
    Accepted = 0,
    MissingGetSdkVersion = 1,
    MissingGetVersion = 2,
    MissingGetGameIdentifier = 3,
    MissingSetCallbackFunction = 4,
    MissingGetParameter = 5,
    MissingSetParameter = 6,
    MissingGetRenderInfo = 7,
    MissingGetState = 8,
    WrongSdkFamily = 9,
    SdkVersionTooOld = 10,
    WrongGameIdentifier = 11,
    ParameterPayloadTooLarge = 12,
    InvalidParameterKind = 13,
    InvalidParameterSize = 14,
    RenderInfoNotReady = 15,
};

struct ValidationResult final {
    ValidationCode code{ValidationCode::MissingGetSdkVersion};

    [[nodiscard]] constexpr bool accepted() const noexcept
    {
        return code == ValidationCode::Accepted;
    }
};

[[nodiscard]] std::uint32_t ParameterSize(ParameterKind kind) noexcept;
[[nodiscard]] ValidationResult ValidateParameter(const Parameter& parameter) noexcept;
[[nodiscard]] ValidationResult ValidateSdkHost(const SdkExports& exports) noexcept;

} // namespace enbcore::enb
