#include <enbcore/skyrim/CanonicalDescriptors.hpp>

namespace enbcore::skyrim {
namespace {

[[nodiscard]] RuntimeSymbolConstraint AeReadConstraint() noexcept
{
    RuntimeSymbolConstraint constraint{};
    constraint.runtime_version = RuntimeVersion{1, 6, 1170, 0};
    constraint.relocation_provider_kind = RelocationProviderKind::AddressLibrary;
    constraint.runtime_variant = RuntimeVariant::AnniversaryEdition;
    return constraint;
}

[[nodiscard]] SymbolDescriptor DataSingletonDescriptor(
    const std::string_view identifier,
    const Capability capability,
    const std::uint64_t relocation_id) noexcept
{
    SymbolDescriptor descriptor{};
    descriptor.identifier = identifier;
    descriptor.capability = capability;
    descriptor.relocation_id = relocation_id;
    descriptor.constraints = AeReadConstraint();
    descriptor.contract = SymbolContract::ReadOnlyData;
    return descriptor;
}

}  // namespace

SymbolDescriptor PlayerCameraSingletonDescriptor() noexcept
{
    return DataSingletonDescriptor(
        "RE::PlayerCamera::Singleton",
        Capability::CameraInverseViewProjection,
        kPlayerCameraSingletonId);
}

SymbolDescriptor CalendarSingletonDescriptor() noexcept
{
    return DataSingletonDescriptor(
        "RE::Calendar::Singleton",
        Capability::WeatherTimeOfDay,
        kCalendarSingletonId);
}

}  // namespace enbcore::skyrim
