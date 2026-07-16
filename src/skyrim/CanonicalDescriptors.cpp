#include <enbcore/skyrim/CanonicalDescriptors.hpp>

namespace enbcore::skyrim {
namespace {

[[nodiscard]] RuntimeSymbolConstraint SkyConstraint() noexcept
{
    RuntimeSymbolConstraint constraint{};
    constraint.runtime_version = RuntimeVersion{1, 6, 1170, 0};
    constraint.relocation_provider_kind = RelocationProviderKind::AddressLibrary;
    constraint.runtime_variant = RuntimeVariant::AnniversaryEdition;
    return constraint;
}

}  // namespace

SymbolDescriptor WeatherTimeOfDayDescriptor() noexcept
{
    SymbolDescriptor descriptor{};
    descriptor.identifier = "RE::Sky::GetSingleton";
    descriptor.capability = Capability::WeatherTimeOfDay;
    descriptor.relocation_id = kSkyGetSingletonRelocationId;
    descriptor.constraints = SkyConstraint();
    descriptor.contract = SymbolContract::ReadOnlyData;
    return descriptor;
}

}  // namespace enbcore::skyrim
