#pragma once

#include <cstddef>
#include <cstdint>

#include <enbcore/skyrim/EngineBridge.hpp>

namespace enbcore::skyrim {

// Canonical engine-read descriptors recovered from the behavioral research and
// reconciled against the CommonLibSSE-NG the trackers compiled against
// (C:\vcpkg\buildtrees\commonlibsse-ng ... RE\Offsets.h). The core admits the
// versionlib-1-6-1170-0 (AE) database, so every id here is the Address Library
// AE id. See the protected OFFSET-SPEC for the full table and derivations.
//
// Only DATA-pointer singletons are shipped as descriptors: their read-only
// contract needs just the relocation id. The function accessors
// (Sky::GetSingleton @13878, Main::WorldRootCamera @36609) require captured
// prologue bytes for a CompleteFunctionPrologue descriptor and are
// intentionally absent until that capture exists rather than mis-typed.

// --- Address Library AE ids (1.6.1170) -------------------------------------
inline constexpr std::uint64_t kPlayerCameraSingletonId = 400802U;  // PlayerCamera**
inline constexpr std::uint64_t kCalendarSingletonId = 400447U;      // Calendar**
inline constexpr std::uint64_t kMainSingletonId = 403449U;          // Main**
// Function accessors (need prologue capture before use):
inline constexpr std::uint64_t kSkyGetSingletonFunctionId = 13878U;
inline constexpr std::uint64_t kMainWorldRootCameraFunctionId = 36609U;

// --- Recovered member layouts ----------------------------------------------
inline constexpr std::size_t kPlayerCameraWorldFovOffset = 0x13CU;
inline constexpr std::size_t kSkyCurrentWeatherOffset = 0x048U;
inline constexpr std::size_t kSkyCurrentWeatherPctOffset = 0x360U;
inline constexpr std::size_t kSkySize = 0x480U;
inline constexpr std::size_t kTesWeatherColorDataOffset = 0x698U;
inline constexpr std::size_t kNiCameraWorldToCamOffset = 0x000U;  // within RUNTIME_DATA

// The PlayerCamera singleton pointer read: the CameraInverseViewProjection
// capability's data anchor (worldFOV lives at +0x13C off the resolved object).
[[nodiscard]] SymbolDescriptor PlayerCameraSingletonDescriptor() noexcept;

// The Calendar singleton pointer read: the WeatherTimeOfDay capability's game
// hour data anchor. (The weather colours themselves come off the Sky singleton,
// which NG reaches through a function accessor - see the header note.)
[[nodiscard]] SymbolDescriptor CalendarSingletonDescriptor() noexcept;

}  // namespace enbcore::skyrim
