#pragma once

#include <enbcore/enb/LoadedHostResolver.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>

namespace enbcore::enb {

using Sha256Digest = std::array<std::uint8_t, 32>;

struct WrapperFileVersion final {
    std::uint16_t major{0};
    std::uint16_t minor{0};
    std::uint16_t patch{0};
    std::uint16_t build{0};

    [[nodiscard]] constexpr bool operator==(
        const WrapperFileVersion&) const noexcept = default;
};

enum class WrapperArchitecture : std::uint16_t {
    Unknown = 0,
    X86 = 0x014c,
    X64 = 0x8664,
    Arm64 = 0xaa64,
};

struct WrapperFingerprint final {
    Sha256Digest sha256{};
    std::uint64_t file_size{0};
    WrapperFileVersion file_version{};
    WrapperArchitecture architecture{WrapperArchitecture::Unknown};
    std::wstring original_filename;
};

struct WrapperBuildRecord final {
    std::string_view identifier;
    Sha256Digest sha256{};
    std::uint64_t file_size{0};
    WrapperFileVersion file_version{};
    WrapperArchitecture architecture{WrapperArchitecture::Unknown};
    std::wstring_view original_filename;
};

[[nodiscard]] std::span<const WrapperBuildRecord>
SupportedEnbWrapperBuilds() noexcept;

enum class ModulePathCode : std::uint8_t {
    Complete = 0,
    InvalidModule = 1,
    Unavailable = 2,
    TooLong = 3,
};

struct ModulePathResult final {
    ModulePathCode code{ModulePathCode::Unavailable};
    std::wstring path;
};

class ModulePathPlatform {
public:
    virtual ~ModulePathPlatform() = default;

    [[nodiscard]] virtual ModulePathResult ResolveModulePath(
        LoadedModule module) noexcept = 0;
};

enum class FileVersionCode : std::uint8_t {
    Complete = 0,
    Unavailable = 1,
    Malformed = 2,
    OriginalFilenameUnavailable = 3,
};

struct FileVersionResult final {
    FileVersionCode code{FileVersionCode::Unavailable};
    WrapperFileVersion version{};
    std::wstring original_filename;
};

class FileVersionPlatform {
public:
    virtual ~FileVersionPlatform() = default;

    [[nodiscard]] virtual FileVersionResult ReadFileVersion(
        std::wstring_view path) noexcept = 0;
};

class WindowsWrapperFingerprintPlatform final
    : public ModulePathPlatform,
      public FileVersionPlatform {
public:
    [[nodiscard]] ModulePathResult ResolveModulePath(
        LoadedModule module) noexcept override;
    [[nodiscard]] FileVersionResult ReadFileVersion(
        std::wstring_view path) noexcept override;
};

enum class WrapperEvidenceCode : std::uint8_t {
    Complete = 0,
    InvalidModule = 1,
    ModulePathUnavailable = 2,
    ModulePathTooLong = 3,
    FileOpenFailed = 4,
    FileSizeFailed = 5,
    FileReadFailed = 6,
    HashFailed = 7,
    PortableExecutableTruncated = 8,
    PortableExecutableInvalid = 9,
    FileVersionUnavailable = 10,
    FileVersionMalformed = 11,
    OriginalFilenameUnavailable = 12,
};

struct WrapperEvidence final {
    WrapperEvidenceCode code{WrapperEvidenceCode::ModulePathUnavailable};
    std::wstring path;
    WrapperFingerprint fingerprint{};

    [[nodiscard]] constexpr bool complete() const noexcept
    {
        return code == WrapperEvidenceCode::Complete;
    }
};

[[nodiscard]] WrapperEvidence InspectWrapperFile(
    std::wstring_view path,
    FileVersionPlatform& versions) noexcept;

[[nodiscard]] WrapperEvidence InspectLoadedEnbWrapper(
    LoadedModule module,
    ModulePathPlatform& modules,
    FileVersionPlatform& versions) noexcept;

enum class WrapperMismatch : std::uint8_t {
    None = 0,
    Sha256 = 1U << 0,
    FileSize = 1U << 1,
    FileVersion = 1U << 2,
    OriginalFilename = 1U << 3,
    Architecture = 1U << 4,
};

[[nodiscard]] constexpr WrapperMismatch operator|(
    const WrapperMismatch left,
    const WrapperMismatch right) noexcept
{
    return static_cast<WrapperMismatch>(
        static_cast<std::uint8_t>(left) | static_cast<std::uint8_t>(right));
}

[[nodiscard]] constexpr bool ContainsWrapperMismatch(
    const WrapperMismatch value,
    const WrapperMismatch expected) noexcept
{
    return (static_cast<std::uint8_t>(value) & static_cast<std::uint8_t>(expected))
        == static_cast<std::uint8_t>(expected);
}

enum class WrapperPolicy : std::uint8_t {
    Strict = 0,
    AuditOnly = 1,
};

enum class WrapperBuildCode : std::uint8_t {
    KnownBuild = 0,
    UnknownBuild = 1,
    EvidenceUnavailable = 2,
};

enum class WrapperAdmissionCode : std::uint8_t {
    AdmittedKnownBuild = 0,
    AdmittedAuditOnly = 1,
    RejectedUnknownBuild = 2,
    RejectedEvidenceUnavailable = 3,
};

inline constexpr std::size_t kNoAllowlistMatch =
    (std::numeric_limits<std::size_t>::max)();

struct WrapperCompatibilityResult final {
    WrapperBuildCode build{WrapperBuildCode::EvidenceUnavailable};
    WrapperAdmissionCode admission{
        WrapperAdmissionCode::RejectedEvidenceUnavailable};
    WrapperMismatch mismatches{WrapperMismatch::None};
    std::size_t allowlist_index{kNoAllowlistMatch};

    [[nodiscard]] constexpr bool admitted() const noexcept
    {
        return admission == WrapperAdmissionCode::AdmittedKnownBuild
            || admission == WrapperAdmissionCode::AdmittedAuditOnly;
    }
};

[[nodiscard]] WrapperCompatibilityResult ClassifyWrapperBuild(
    const WrapperEvidence& evidence,
    WrapperPolicy policy,
    std::span<const WrapperBuildRecord> allowlist) noexcept;

[[nodiscard]] WrapperCompatibilityResult ClassifySupportedEnbWrapper(
    const WrapperEvidence& evidence,
    WrapperPolicy policy) noexcept;

struct WrapperGateResult final {
    WrapperEvidence evidence{};
    WrapperCompatibilityResult compatibility{};

    [[nodiscard]] constexpr bool admitted() const noexcept
    {
        return compatibility.admitted();
    }
};

[[nodiscard]] WrapperGateResult EvaluateLoadedEnbWrapper(
    LoadedModule module,
    ModulePathPlatform& modules,
    FileVersionPlatform& versions,
    WrapperPolicy policy) noexcept;

} // namespace enbcore::enb
