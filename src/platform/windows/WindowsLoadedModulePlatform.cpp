#include <enbcore/enb/LoadedHostResolver.hpp>

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define PSAPI_VERSION 1
#include <Windows.h>
#include <Psapi.h>

#include <limits>
#include <string>
#include <vector>

namespace enbcore::enb {
namespace {

constexpr std::size_t kInitialModuleCapacity = 256;
constexpr std::size_t kEnumerationGrowthAllowance = 16;

[[nodiscard]] bool ByteCapacityFitsDword(const std::size_t module_count) noexcept
{
    return module_count
        <= static_cast<std::size_t>((std::numeric_limits<DWORD>::max)()) / sizeof(HMODULE);
}

} // namespace

LoadedModules WindowsLoadedModulePlatform::EnumerateLoadedModules() noexcept
{
    LoadedModules result;

    try {
        std::vector<HMODULE> native_modules(kInitialModuleCapacity);
        DWORD bytes_needed = 0;
        DWORD byte_capacity = static_cast<DWORD>(native_modules.size() * sizeof(HMODULE));

        if (::EnumProcessModules(
                ::GetCurrentProcess(),
                native_modules.data(),
                byte_capacity,
                &bytes_needed)
            == FALSE) {
            return result;
        }

        if (bytes_needed > byte_capacity) {
            const std::size_t required_count =
                (static_cast<std::size_t>(bytes_needed) + sizeof(HMODULE) - 1)
                / sizeof(HMODULE);
            const std::size_t retry_capacity =
                required_count + kEnumerationGrowthAllowance;
            if (retry_capacity < required_count || !ByteCapacityFitsDword(retry_capacity)) {
                result.status = ModuleEnumerationStatus::Truncated;
                return result;
            }

            native_modules.resize(retry_capacity);
            byte_capacity = static_cast<DWORD>(native_modules.size() * sizeof(HMODULE));
            bytes_needed = 0;
            if (::EnumProcessModules(
                    ::GetCurrentProcess(),
                    native_modules.data(),
                    byte_capacity,
                    &bytes_needed)
                == FALSE) {
                return result;
            }
            if (bytes_needed > byte_capacity) {
                result.status = ModuleEnumerationStatus::Truncated;
                return result;
            }
        }

        if (bytes_needed % sizeof(HMODULE) != 0) {
            return result;
        }

        const std::size_t module_count = bytes_needed / sizeof(HMODULE);
        native_modules.resize(module_count);
        result.modules.reserve(module_count);
        for (const HMODULE module : native_modules) {
            result.modules.push_back(reinterpret_cast<LoadedModule>(module));
        }
        result.status = ModuleEnumerationStatus::Complete;
        return result;
    } catch (...) {
        return result;
    }
}

ExportAddress WindowsLoadedModulePlatform::FindExport(
    const LoadedModule module,
    const std::string_view exact_name) noexcept
{
    if (module == 0 || exact_name.empty()
        || exact_name.find('\0') != std::string_view::npos) {
        return 0;
    }

    try {
        const std::string terminated_name(exact_name);
        const FARPROC export_address = ::GetProcAddress(
            reinterpret_cast<HMODULE>(module),
            terminated_name.c_str());
        return reinterpret_cast<ExportAddress>(export_address);
    } catch (...) {
        return 0;
    }
}

} // namespace enbcore::enb

#else

namespace enbcore::enb {

LoadedModules WindowsLoadedModulePlatform::EnumerateLoadedModules() noexcept
{
    return LoadedModules{};
}

ExportAddress WindowsLoadedModulePlatform::FindExport(
    LoadedModule,
    std::string_view) noexcept
{
    return 0;
}

} // namespace enbcore::enb

#endif
