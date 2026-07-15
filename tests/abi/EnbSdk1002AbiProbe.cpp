#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <enbseries.h>

#include <enbcore/enb/SdkContract.hpp>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace contract = enbcore::enb;

template <typename Official, typename Mirror>
consteval bool function_pointer_abi_compatible()
{
    return std::is_pointer_v<Official>
        && std::is_pointer_v<Mirror>
        && sizeof(Official) == sizeof(Mirror)
        && alignof(Official) == alignof(Mirror);
}

static_assert(sizeof(void*) == 8);
static_assert(ENBSDKVERSION == contract::kSdkVersion);
static_assert(ENBGAMEID_SKYRIMSE == contract::kGameIdentifier);

static_assert(sizeof(long) == sizeof(contract::SdkInteger));
static_assert(alignof(long) == alignof(contract::SdkInteger));
static_assert(sizeof(BOOL) == sizeof(contract::SdkBoolean));
static_assert(alignof(BOOL) == alignof(contract::SdkBoolean));
static_assert(sizeof(unsigned long) == sizeof(std::uint32_t));
static_assert(alignof(unsigned long) == alignof(std::uint32_t));

static_assert(static_cast<long>(ENBParam_NONE) == static_cast<long>(contract::ParameterKind::None));
static_assert(static_cast<long>(ENBParam_FLOAT) == static_cast<long>(contract::ParameterKind::Float));
static_assert(static_cast<long>(ENBParam_INT) == static_cast<long>(contract::ParameterKind::Integer));
static_assert(static_cast<long>(ENBParam_HEX) == static_cast<long>(contract::ParameterKind::Hex));
static_assert(static_cast<long>(ENBParam_BOOL) == static_cast<long>(contract::ParameterKind::Boolean));
static_assert(static_cast<long>(ENBParam_COLOR3) == static_cast<long>(contract::ParameterKind::Color3));
static_assert(static_cast<long>(ENBParam_COLOR4) == static_cast<long>(contract::ParameterKind::Color4));
static_assert(static_cast<long>(ENBParam_VECTOR3) == static_cast<long>(contract::ParameterKind::Vector3));

static_assert(static_cast<long>(ENBCallback_EndFrame) == static_cast<long>(contract::CallbackId::EndFrame));
static_assert(static_cast<long>(ENBCallback_BeginFrame) == static_cast<long>(contract::CallbackId::BeginFrame));
static_assert(static_cast<long>(ENBCallback_PreSave) == static_cast<long>(contract::CallbackId::PreSave));
static_assert(static_cast<long>(ENBCallback_PostLoad) == static_cast<long>(contract::CallbackId::PostLoad));
static_assert(static_cast<long>(ENBCallback_OnInit) == static_cast<long>(contract::CallbackId::OnInit));
static_assert(static_cast<long>(ENBCallback_OnExit) == static_cast<long>(contract::CallbackId::OnExit));
static_assert(static_cast<long>(ENBCallback_PreReset) == static_cast<long>(contract::CallbackId::PreReset));
static_assert(static_cast<long>(ENBCallback_PostReset) == static_cast<long>(contract::CallbackId::PostReset));

static_assert(static_cast<long>(ENBState_ulWeatherCurrent) == static_cast<long>(contract::StateId::WeatherCurrent));
static_assert(static_cast<long>(ENBState_ulWeatherOutgoing) == static_cast<long>(contract::StateId::WeatherOutgoing));
static_assert(static_cast<long>(ENBState_fWeatherTransition) == static_cast<long>(contract::StateId::WeatherTransition));
static_assert(static_cast<long>(ENBState_fTimeOfDay) == static_cast<long>(contract::StateId::TimeOfDay));
static_assert(static_cast<long>(ENBState_fTODFactorDawn) == static_cast<long>(contract::StateId::DawnFactor));
static_assert(static_cast<long>(ENBState_fTODFactorSunrise) == static_cast<long>(contract::StateId::SunriseFactor));
static_assert(static_cast<long>(ENBState_fTODFactorDay) == static_cast<long>(contract::StateId::DayFactor));
static_assert(static_cast<long>(ENBState_fTODFactorSunset) == static_cast<long>(contract::StateId::SunsetFactor));
static_assert(static_cast<long>(ENBState_fTODFactorDusk) == static_cast<long>(contract::StateId::DuskFactor));
static_assert(static_cast<long>(ENBState_fTODFactorNight) == static_cast<long>(contract::StateId::NightFactor));
static_assert(static_cast<long>(ENBState_fTODFactorInteriorDay) == static_cast<long>(contract::StateId::InteriorDayFactor));
static_assert(static_cast<long>(ENBState_fTODFactorInteriorNight) == static_cast<long>(contract::StateId::InteriorNightFactor));
static_assert(static_cast<long>(ENBState_fNightDayFactor) == static_cast<long>(contract::StateId::NightDayFactor));
static_assert(static_cast<long>(ENBState_fInteriorFactor) == static_cast<long>(contract::StateId::InteriorFactor));
static_assert(static_cast<long>(ENBState_ulWorldSpaceID) == static_cast<long>(contract::StateId::WorldSpaceId));
static_assert(static_cast<long>(ENBState_ulLocationID) == static_cast<long>(contract::StateId::LocationId));

static_assert(sizeof(ENBParameterType) == sizeof(contract::ParameterKind));
static_assert(alignof(ENBParameterType) == alignof(contract::ParameterKind));
static_assert(sizeof(ENBCallbackType) == sizeof(contract::CallbackId));
static_assert(alignof(ENBCallbackType) == alignof(contract::CallbackId));
static_assert(sizeof(ENBStateType) == sizeof(contract::StateId));
static_assert(alignof(ENBStateType) == alignof(contract::StateId));

static_assert(sizeof(ENBParameter) == sizeof(contract::Parameter));
static_assert(alignof(ENBParameter) == alignof(contract::Parameter));
static_assert(offsetof(ENBParameter, Data) == offsetof(contract::Parameter, data));
static_assert(offsetof(ENBParameter, Size) == offsetof(contract::Parameter, size));
static_assert(offsetof(ENBParameter, Type) == offsetof(contract::Parameter, type));

// The SDK 1002 constructor assigns ScreenSizeX twice. The probe deliberately
// avoids constructing the official type and verifies its public ABI layout only.
static_assert(sizeof(ENBRenderInfo) == sizeof(contract::RenderInfo));
static_assert(alignof(ENBRenderInfo) == alignof(contract::RenderInfo));
static_assert(offsetof(ENBRenderInfo, d3d11device) == offsetof(contract::RenderInfo, device));
static_assert(offsetof(ENBRenderInfo, d3d11devicecontext) == offsetof(contract::RenderInfo, device_context));
static_assert(offsetof(ENBRenderInfo, dxgiswapchain) == offsetof(contract::RenderInfo, swap_chain));
static_assert(offsetof(ENBRenderInfo, ScreenSizeX) == offsetof(contract::RenderInfo, screen_size_x));
static_assert(offsetof(ENBRenderInfo, ScreenSizeY) == offsetof(contract::RenderInfo, screen_size_y));

using OfficialCallbackShape = void(WINAPI *)(ENBCallbackType);
using ContractCallbackShape = void(WINAPI *)(contract::CallbackId);
static_assert(std::is_same_v<ENBCallbackFunction, OfficialCallbackShape>);
static_assert(std::is_same_v<contract::CallbackFunction, ContractCallbackShape>);

static_assert(std::is_same_v<_ENBGetSDKVersion, long (*)()>);
static_assert(std::is_same_v<_ENBGetVersion, long (*)()>);
static_assert(std::is_same_v<_ENBGetGameIdentifier, long (*)()>);
static_assert(std::is_same_v<_ENBSetCallbackFunction, void (*)(ENBCallbackFunction)>);
static_assert(std::is_same_v<_ENBGetParameter, BOOL (*)(char*, char*, char*, ENBParameter*)>);
static_assert(std::is_same_v<_ENBSetParameter, BOOL (*)(char*, char*, char*, ENBParameter*)>);
static_assert(std::is_same_v<_ENBGetRenderInfo, ENBRenderInfo* (*)()>);
static_assert(std::is_same_v<_ENBStateType, long (*)(ENBStateType)>);

static_assert(function_pointer_abi_compatible<_ENBGetSDKVersion, contract::GetSdkVersion>());
static_assert(function_pointer_abi_compatible<_ENBGetVersion, contract::GetVersion>());
static_assert(function_pointer_abi_compatible<_ENBGetGameIdentifier, contract::GetGameIdentifier>());
static_assert(function_pointer_abi_compatible<_ENBSetCallbackFunction, contract::SetCallbackFunction>());
static_assert(function_pointer_abi_compatible<_ENBGetParameter, contract::GetParameter>());
static_assert(function_pointer_abi_compatible<_ENBSetParameter, contract::SetParameter>());
static_assert(function_pointer_abi_compatible<_ENBGetRenderInfo, contract::GetRenderInfo>());
static_assert(function_pointer_abi_compatible<_ENBStateType, contract::GetState>());
static_assert(sizeof(_ENBGetSDKVersion) == 8);
static_assert(alignof(_ENBGetSDKVersion) == 8);

static_assert(sizeof(contract::SdkExports) == 64);
static_assert(alignof(contract::SdkExports) == 8);
static_assert(offsetof(contract::SdkExports, get_sdk_version) == 0);
static_assert(offsetof(contract::SdkExports, get_version) == 8);
static_assert(offsetof(contract::SdkExports, get_game_identifier) == 16);
static_assert(offsetof(contract::SdkExports, set_callback_function) == 24);
static_assert(offsetof(contract::SdkExports, get_parameter) == 32);
static_assert(offsetof(contract::SdkExports, set_parameter) == 40);
static_assert(offsetof(contract::SdkExports, get_render_info) == 48);
static_assert(offsetof(contract::SdkExports, get_state) == 56);

int main()
{
    struct ParameterSizeCase final {
        ENBParameterType official;
        contract::ParameterKind mirror;
    };

    constexpr ParameterSizeCase cases[]{
        {ENBParam_NONE, contract::ParameterKind::None},
        {ENBParam_FLOAT, contract::ParameterKind::Float},
        {ENBParam_INT, contract::ParameterKind::Integer},
        {ENBParam_HEX, contract::ParameterKind::Hex},
        {ENBParam_BOOL, contract::ParameterKind::Boolean},
        {ENBParam_COLOR3, contract::ParameterKind::Color3},
        {ENBParam_COLOR4, contract::ParameterKind::Color4},
        {ENBParam_VECTOR3, contract::ParameterKind::Vector3},
    };

    for (const ParameterSizeCase& item : cases) {
        if (ENBParameterTypeToSize(item.official)
            != static_cast<long>(contract::ParameterSize(item.mirror))) {
            return 1;
        }
    }

    return 0;
}
