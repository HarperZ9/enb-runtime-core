#pragma once

#include <cstddef>
#include <cstdint>

#include <enbcore/skyrim/EngineBridge.hpp>

namespace enbcore::skyrim {

// Canonical engine-read descriptors recovered from the behavioral research (see
// the protected OFFSET-SPEC). Each descriptor binds a capability to the exact
// Address Library relocation the adapter must resolve for a supported runtime.
// Only descriptors whose literal relocation id is actually known are provided;
// the rest await their CommonLibSSE-NG ids and are intentionally absent rather
// than guessed.

// RE::Sky singleton, recovered locally: REL::ID(302296), with the member
// offsets the adapter reads off the resolved pointer.
inline constexpr std::uint64_t kSkyGetSingletonRelocationId = 302296U;
inline constexpr std::size_t kSkyCurrentWeatherOffset = 0x048U;
inline constexpr std::size_t kSkyCurrentWeatherPctOffset = 0x360U;
inline constexpr std::size_t kSkySize = 0x480U;

// The WeatherTimeOfDay capability's read descriptor: the Sky singleton pointer,
// a read-only data symbol, version-gated to Skyrim SE 1.6.1170 (AE) resolved via
// the Address Library. Fail-closed validation is performed by ValidateSymbol.
[[nodiscard]] SymbolDescriptor WeatherTimeOfDayDescriptor() noexcept;

}  // namespace enbcore::skyrim
