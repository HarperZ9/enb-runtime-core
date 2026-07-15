#include <enbcore/enb/WrapperFingerprint.hpp>

#include <array>
#include <bit>
#include <cstdint>

namespace enbcore::enb {
namespace {

constexpr std::array<WrapperBuildRecord, 1> kSupportedBuilds{{
    WrapperBuildRecord{
        "enbseries-0.504-sha256-87583e85",
        Sha256Digest{
            0x87, 0x58, 0x3e, 0x85, 0xce, 0x99, 0x3e, 0x63,
            0x38, 0x48, 0x6f, 0x6d, 0x5d, 0xe0, 0x9f, 0x9a,
            0x74, 0x66, 0x1b, 0xa2, 0x45, 0xe8, 0x20, 0xdc,
            0x75, 0xe3, 0xa7, 0xa8, 0x2c, 0x98, 0x88, 0xc7},
        4'665'344,
        WrapperFileVersion{0, 5, 0, 4},
        WrapperArchitecture::X64,
        L"d3d11.dll"},
}};

[[nodiscard]] WrapperMismatch CompareBuild(
    const WrapperFingerprint& actual,
    const WrapperBuildRecord& expected) noexcept
{
    WrapperMismatch mismatches = WrapperMismatch::None;
    if (actual.sha256 != expected.sha256) {
        mismatches = mismatches | WrapperMismatch::Sha256;
    }
    if (actual.file_size != expected.file_size) {
        mismatches = mismatches | WrapperMismatch::FileSize;
    }
    if (actual.file_version != expected.file_version) {
        mismatches = mismatches | WrapperMismatch::FileVersion;
    }
    if (actual.original_filename != expected.original_filename) {
        mismatches = mismatches | WrapperMismatch::OriginalFilename;
    }
    if (actual.architecture != expected.architecture) {
        mismatches = mismatches | WrapperMismatch::Architecture;
    }
    return mismatches;
}

[[nodiscard]] WrapperAdmissionCode UnknownAdmission(
    const WrapperPolicy policy) noexcept
{
    return policy == WrapperPolicy::AuditOnly
        ? WrapperAdmissionCode::AdmittedAuditOnly
        : WrapperAdmissionCode::RejectedUnknownBuild;
}

[[nodiscard]] WrapperAdmissionCode EvidenceAdmission(
    const WrapperPolicy policy) noexcept
{
    return policy == WrapperPolicy::AuditOnly
        ? WrapperAdmissionCode::AdmittedAuditOnly
        : WrapperAdmissionCode::RejectedEvidenceUnavailable;
}

} // namespace

std::span<const WrapperBuildRecord> SupportedEnbWrapperBuilds() noexcept
{
    return kSupportedBuilds;
}

WrapperEvidence InspectLoadedEnbWrapper(
    const LoadedModule module,
    ModulePathPlatform& modules,
    FileVersionPlatform& versions) noexcept
{
    ModulePathResult module_path;
    try {
        module_path = modules.ResolveModulePath(module);
    } catch (...) {
        return WrapperEvidence{WrapperEvidenceCode::ModulePathUnavailable};
    }

    switch (module_path.code) {
    case ModulePathCode::Complete:
        if (module_path.path.empty()) {
            return WrapperEvidence{WrapperEvidenceCode::ModulePathUnavailable};
        }
        return InspectWrapperFile(module_path.path, versions);
    case ModulePathCode::InvalidModule:
        return WrapperEvidence{WrapperEvidenceCode::InvalidModule};
    case ModulePathCode::TooLong:
        return WrapperEvidence{WrapperEvidenceCode::ModulePathTooLong};
    case ModulePathCode::Unavailable:
    default:
        return WrapperEvidence{WrapperEvidenceCode::ModulePathUnavailable};
    }
}

WrapperCompatibilityResult ClassifyWrapperBuild(
    const WrapperEvidence& evidence,
    const WrapperPolicy policy,
    const std::span<const WrapperBuildRecord> allowlist) noexcept
{
    if (!evidence.complete()) {
        return WrapperCompatibilityResult{
            WrapperBuildCode::EvidenceUnavailable,
            EvidenceAdmission(policy),
            WrapperMismatch::None,
            kNoAllowlistMatch};
    }

    WrapperMismatch closest = WrapperMismatch::None;
    std::size_t closest_index = kNoAllowlistMatch;
    int closest_count = (std::numeric_limits<int>::max)();

    for (std::size_t index = 0; index < allowlist.size(); ++index) {
        const WrapperMismatch mismatches = CompareBuild(
            evidence.fingerprint,
            allowlist[index]);
        if (mismatches == WrapperMismatch::None) {
            return WrapperCompatibilityResult{
                WrapperBuildCode::KnownBuild,
                WrapperAdmissionCode::AdmittedKnownBuild,
                WrapperMismatch::None,
                index};
        }

        const int mismatch_count = std::popcount(
            static_cast<unsigned int>(static_cast<std::uint8_t>(mismatches)));
        if (mismatch_count < closest_count) {
            closest = mismatches;
            closest_index = index;
            closest_count = mismatch_count;
        }
    }

    return WrapperCompatibilityResult{
        WrapperBuildCode::UnknownBuild,
        UnknownAdmission(policy),
        closest,
        closest_index};
}

WrapperCompatibilityResult ClassifySupportedEnbWrapper(
    const WrapperEvidence& evidence,
    const WrapperPolicy policy) noexcept
{
    return ClassifyWrapperBuild(evidence, policy, SupportedEnbWrapperBuilds());
}

WrapperGateResult EvaluateLoadedEnbWrapper(
    const LoadedModule module,
    ModulePathPlatform& modules,
    FileVersionPlatform& versions,
    const WrapperPolicy policy) noexcept
{
    WrapperGateResult result;
    result.evidence = InspectLoadedEnbWrapper(module, modules, versions);
    result.compatibility = ClassifySupportedEnbWrapper(result.evidence, policy);
    return result;
}

} // namespace enbcore::enb
