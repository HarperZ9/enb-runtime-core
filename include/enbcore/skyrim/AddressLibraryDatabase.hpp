#pragma once

#include <enbcore/skyrim/EngineBridge.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace enbcore::skyrim {

struct RelocationArtifactIdentity final {
    RelocationProviderKind kind{RelocationProviderKind::None};
    RuntimeVersion runtime_version{};
    RuntimeVariant runtime_variant{RuntimeVariant::None};
    Sha256Digest sha256{};
    std::uint64_t file_size{0};
};

class RelocationArtifactDigestVerifier {
public:
    virtual ~RelocationArtifactDigestVerifier() = default;

    // Production implementations must hash bytes directly and compare the
    // computed digest with expected. Caller metadata is not verification.
    [[nodiscard]] virtual bool VerifySha256(
        std::span<const std::uint8_t> bytes,
        const Sha256Digest& expected) const noexcept = 0;
};

struct RelocationArtifactAdmissionResult;

// Opaque proof that a named verifier accepted this exact immutable byte span.
// Ordinary callers cannot construct or retarget a verified receipt.
class RelocationArtifactReceipt final {
public:
    RelocationArtifactReceipt(const RelocationArtifactReceipt&) = default;
    RelocationArtifactReceipt(RelocationArtifactReceipt&&) noexcept = default;
    RelocationArtifactReceipt& operator=(const RelocationArtifactReceipt&) = default;
    RelocationArtifactReceipt& operator=(RelocationArtifactReceipt&&) noexcept = default;

    [[nodiscard]] const RelocationArtifactIdentity& identity() const noexcept;
    [[nodiscard]] bool IsBoundTo(
        std::span<const std::uint8_t> bytes) const noexcept;

private:
    RelocationArtifactReceipt(
        RelocationArtifactIdentity identity,
        const std::uint8_t* bytes,
        std::size_t size) noexcept;

    RelocationArtifactIdentity identity_{};
    const std::uint8_t* bytes_{nullptr};
    std::size_t size_{0};

    friend RelocationArtifactAdmissionResult AdmitRelocationArtifact(
        std::span<const std::uint8_t>,
        const RelocationArtifactIdentity&,
        const RelocationArtifactDigestVerifier&) noexcept;
};

enum class RelocationArtifactAdmissionDiagnostic : std::uint8_t {
    None = 0,
    ArtifactUnsupported = 1,
    ArtifactSizeMismatch = 2,
    DigestVerificationFailed = 3,
};

struct RelocationArtifactAdmissionResult final {
    RelocationArtifactAdmissionDiagnostic diagnostic{
        RelocationArtifactAdmissionDiagnostic::ArtifactUnsupported};
    std::optional<RelocationArtifactReceipt> receipt;
};

[[nodiscard]] RelocationArtifactAdmissionResult AdmitRelocationArtifact(
    std::span<const std::uint8_t> bytes,
    const RelocationArtifactIdentity& identity,
    const RelocationArtifactDigestVerifier& verifier) noexcept;

struct RelocationEntry final {
    std::uint64_t id{0};
    std::uint64_t offset{0};

    [[nodiscard]] constexpr bool operator==(
        const RelocationEntry&) const noexcept = default;
};

enum class AddressLibraryStatus : std::uint8_t {
    Rejected = 0,
    Ready = 1,
};

enum class AddressLibraryDiagnostic : std::uint8_t {
    None = 0,
    ArtifactUnsupported = 1,
    ArtifactSizeMismatch = 2,
    UnsupportedFormat = 3,
    RuntimeMismatch = 4,
    InvalidNameLength = 5,
    InvalidPointerSize = 6,
    InvalidEntryCount = 7,
    Truncated = 8,
    InvalidEncoding = 9,
    ArithmeticOverflow = 10,
    OffsetOutsideModule = 11,
    TrailingData = 12,
    DuplicateId = 13,
    AllocationFailure = 14,
    InvalidModuleSize = 15,
    ArtifactBytesMismatch = 16,
};

struct AddressLibraryParseResult final {
    AddressLibraryStatus status{AddressLibraryStatus::Rejected};
    AddressLibraryDiagnostic diagnostic{AddressLibraryDiagnostic::ArtifactUnsupported};
    RuntimeVersion runtime_version{};
    std::vector<RelocationEntry> entries;

    [[nodiscard]] std::optional<std::uint64_t> ResolveOffset(
        std::uint64_t id) const noexcept;

    [[nodiscard]] std::optional<std::uintptr_t> ResolveAddress(
        std::uint64_t id,
        std::uintptr_t image_base,
        std::size_t image_size) const noexcept;
};

[[nodiscard]] AddressLibraryParseResult ParseAddressLibraryV2(
    std::span<const std::uint8_t> bytes,
    const RelocationArtifactReceipt& artifact,
    std::size_t module_image_size) noexcept;

} // namespace enbcore::skyrim
