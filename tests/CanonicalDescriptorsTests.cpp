#include <enbcore/skyrim/CanonicalDescriptors.hpp>

#include <enbcore/skyrim/EngineBridge.hpp>

#include <iostream>

namespace {

int failures = 0;

void expect(const bool condition, const char* expression, const char* file, const int line)
{
    if (condition) {
        return;
    }
    std::cerr << file << ':' << line << ": expectation failed: " << expression << '\n';
    ++failures;
}

#define EXPECT(expression) expect((expression), #expression, __FILE__, __LINE__)

using namespace enbcore::skyrim;

void weather_descriptor_carries_the_recovered_sky_relocation()
{
    const SymbolDescriptor descriptor = WeatherTimeOfDayDescriptor();

    // Recovered from the research: RE::Sky::GetSingleton -> REL::ID(302296).
    EXPECT(descriptor.relocation_id == 302296U);
    EXPECT(descriptor.capability == Capability::WeatherTimeOfDay);
    // The Sky singleton is a data pointer, not a hooked function.
    EXPECT(descriptor.contract == SymbolContract::ReadOnlyData);
    EXPECT(!descriptor.identifier.empty());
}

void weather_descriptor_is_version_gated_to_1_6_1170_ae_address_library()
{
    const SymbolDescriptor descriptor = WeatherTimeOfDayDescriptor();
    const RuntimeSymbolConstraint& constraint = descriptor.constraints;

    const RuntimeVersion expected_runtime{1, 6, 1170, 0};
    EXPECT(constraint.runtime_version == expected_runtime);
    EXPECT(constraint.relocation_provider_kind == RelocationProviderKind::AddressLibrary);
    EXPECT(constraint.runtime_variant == RuntimeVariant::AnniversaryEdition);
}

void recovered_sky_member_layout_is_exposed()
{
    // Recovered member offsets for the read the adapter performs off the
    // resolved Sky pointer.
    EXPECT(kSkyCurrentWeatherOffset == 0x048U);
    EXPECT(kSkyCurrentWeatherPctOffset == 0x360U);
    EXPECT(kSkySize == 0x480U);
    EXPECT(kSkyCurrentWeatherOffset < kSkySize);
    EXPECT(kSkyCurrentWeatherPctOffset < kSkySize);
}

}  // namespace

int main()
{
    weather_descriptor_carries_the_recovered_sky_relocation();
    weather_descriptor_is_version_gated_to_1_6_1170_ae_address_library();
    recovered_sky_member_layout_is_exposed();

    if (failures != 0) {
        std::cerr << failures << " assertion(s) failed\n";
        return 1;
    }

    std::cout << "CanonicalDescriptors tests passed\n";
    return 0;
}
