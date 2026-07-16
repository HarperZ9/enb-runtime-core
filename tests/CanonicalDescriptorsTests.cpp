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

void data_singletons_carry_the_recovered_ae_ids()
{
    const SymbolDescriptor camera = PlayerCameraSingletonDescriptor();
    EXPECT(camera.relocation_id == 400802U);  // PlayerCamera::Singleton, AE 1.6.1170
    EXPECT(camera.capability == Capability::CameraInverseViewProjection);
    EXPECT(camera.contract == SymbolContract::ReadOnlyData);
    EXPECT(!camera.identifier.empty());

    const SymbolDescriptor calendar = CalendarSingletonDescriptor();
    EXPECT(calendar.relocation_id == 400447U);  // Calendar::Singleton, AE 1.6.1170
    EXPECT(calendar.capability == Capability::WeatherTimeOfDay);
    EXPECT(calendar.contract == SymbolContract::ReadOnlyData);
    EXPECT(!calendar.identifier.empty());
}

void descriptors_are_version_gated_to_1_6_1170_ae_address_library()
{
    const RuntimeVersion expected_runtime{1, 6, 1170, 0};
    for (const SymbolDescriptor& descriptor :
         {PlayerCameraSingletonDescriptor(), CalendarSingletonDescriptor()}) {
        EXPECT(descriptor.constraints.runtime_version == expected_runtime);
        EXPECT(descriptor.constraints.relocation_provider_kind
               == RelocationProviderKind::AddressLibrary);
        EXPECT(descriptor.constraints.runtime_variant == RuntimeVariant::AnniversaryEdition);
    }
}

void function_accessor_ids_are_recorded_but_not_shipped_as_data()
{
    // The Sky and WorldRootCamera accessors are functions; their ids are known
    // but they require captured prologue bytes, so they are constants only,
    // never mis-typed as read-only-data descriptors.
    EXPECT(kSkyGetSingletonFunctionId == 13878U);
    EXPECT(kMainWorldRootCameraFunctionId == 36609U);
}

void recovered_member_layouts_are_exposed()
{
    EXPECT(kPlayerCameraWorldFovOffset == 0x13CU);
    EXPECT(kSkyCurrentWeatherOffset == 0x048U);
    EXPECT(kSkyCurrentWeatherPctOffset == 0x360U);
    EXPECT(kTesWeatherColorDataOffset == 0x698U);
    EXPECT(kSkyCurrentWeatherOffset < kSkySize);
    EXPECT(kSkyCurrentWeatherPctOffset < kSkySize);
}

}  // namespace

int main()
{
    data_singletons_carry_the_recovered_ae_ids();
    descriptors_are_version_gated_to_1_6_1170_ae_address_library();
    function_accessor_ids_are_recorded_but_not_shipped_as_data();
    recovered_member_layouts_are_exposed();

    if (failures != 0) {
        std::cerr << failures << " assertion(s) failed\n";
        return 1;
    }

    std::cout << "CanonicalDescriptors tests passed\n";
    return 0;
}
