#include <enbcore/skyrim/AddressLibraryDatabase.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>

namespace enbcore::skyrim {
namespace {

class ByteReader final {
public:
    explicit ByteReader(const std::span<const std::uint8_t> bytes) noexcept:
        bytes_(bytes)
    {}

    template <typename Integer>
    [[nodiscard]] bool ReadUnsigned(Integer& value) noexcept
    {
        static_assert(std::is_unsigned_v<Integer>);
        if (Remaining() < sizeof(Integer)) {
            return false;
        }
        value = 0;
        for (std::size_t index = 0; index < sizeof(Integer); ++index) {
            value |= static_cast<Integer>(bytes_[position_ + index])
                << (index * 8U);
        }
        position_ += sizeof(Integer);
        return true;
    }

    [[nodiscard]] bool ReadSigned32(std::int32_t& value) noexcept
    {
        std::uint32_t encoded = 0;
        if (!ReadUnsigned(encoded)) {
            return false;
        }
        value = std::bit_cast<std::int32_t>(encoded);
        return true;
    }

    [[nodiscard]] bool Skip(const std::size_t size) noexcept
    {
        if (Remaining() < size) {
            return false;
        }
        position_ += size;
        return true;
    }

    [[nodiscard]] std::size_t Remaining() const noexcept
    {
        return bytes_.size() - position_;
    }

private:
    std::span<const std::uint8_t> bytes_;
    std::size_t position_{0};
};

[[nodiscard]] AddressLibraryParseResult Reject(
    const AddressLibraryDiagnostic diagnostic) noexcept
{
    AddressLibraryParseResult result;
    result.diagnostic = diagnostic;
    return result;
}

[[nodiscard]] bool AddChecked(
    const std::uint64_t base,
    const std::uint64_t delta,
    std::uint64_t& output) noexcept
{
    if (delta > (std::numeric_limits<std::uint64_t>::max)() - base) {
        return false;
    }
    output = base + delta;
    return true;
}

[[nodiscard]] bool SubtractChecked(
    const std::uint64_t base,
    const std::uint64_t delta,
    std::uint64_t& output) noexcept
{
    if (delta > base) {
        return false;
    }
    output = base - delta;
    return true;
}

enum class DecodeStatus : std::uint8_t {
    Ok = 0,
    Truncated = 1,
    InvalidEncoding = 2,
    ArithmeticOverflow = 3,
};

[[nodiscard]] DecodeStatus DecodeValue(
    ByteReader& reader,
    const std::uint8_t mode,
    const std::uint64_t previous,
    std::uint64_t& output) noexcept
{
    std::uint8_t delta8 = 0;
    std::uint16_t delta16 = 0;
    std::uint32_t absolute32 = 0;
    switch (mode) {
    case 0:
        return reader.ReadUnsigned(output)
            ? DecodeStatus::Ok
            : DecodeStatus::Truncated;
    case 1:
        return AddChecked(previous, 1U, output)
            ? DecodeStatus::Ok
            : DecodeStatus::ArithmeticOverflow;
    case 2:
        if (!reader.ReadUnsigned(delta8)) {
            return DecodeStatus::Truncated;
        }
        return AddChecked(previous, delta8, output)
            ? DecodeStatus::Ok
            : DecodeStatus::ArithmeticOverflow;
    case 3:
        if (!reader.ReadUnsigned(delta8)) {
            return DecodeStatus::Truncated;
        }
        return SubtractChecked(previous, delta8, output)
            ? DecodeStatus::Ok
            : DecodeStatus::ArithmeticOverflow;
    case 4:
        if (!reader.ReadUnsigned(delta16)) {
            return DecodeStatus::Truncated;
        }
        return AddChecked(previous, delta16, output)
            ? DecodeStatus::Ok
            : DecodeStatus::ArithmeticOverflow;
    case 5:
        if (!reader.ReadUnsigned(delta16)) {
            return DecodeStatus::Truncated;
        }
        return SubtractChecked(previous, delta16, output)
            ? DecodeStatus::Ok
            : DecodeStatus::ArithmeticOverflow;
    case 6:
        if (!reader.ReadUnsigned(delta16)) {
            return DecodeStatus::Truncated;
        }
        output = delta16;
        return DecodeStatus::Ok;
    case 7:
        if (!reader.ReadUnsigned(absolute32)) {
            return DecodeStatus::Truncated;
        }
        output = absolute32;
        return DecodeStatus::Ok;
    default:
        return DecodeStatus::InvalidEncoding;
    }
}

[[nodiscard]] AddressLibraryDiagnostic ToDiagnostic(
    const DecodeStatus status) noexcept
{
    switch (status) {
    case DecodeStatus::Truncated:
        return AddressLibraryDiagnostic::Truncated;
    case DecodeStatus::InvalidEncoding:
        return AddressLibraryDiagnostic::InvalidEncoding;
    case DecodeStatus::ArithmeticOverflow:
        return AddressLibraryDiagnostic::ArithmeticOverflow;
    case DecodeStatus::Ok:
    default:
        return AddressLibraryDiagnostic::None;
    }
}

[[nodiscard]] bool IsAdmittedArtifact(
    const RelocationArtifactIdentity& artifact) noexcept
{
    return std::ranges::any_of(
        SupportedRelocationProviders(),
        [&artifact](const RelocationProviderRecord& record) {
            return artifact.kind == RelocationProviderKind::AddressLibrary
                && artifact.kind == record.kind
                && artifact.runtime_version == record.runtime_version
                && artifact.runtime_variant == record.runtime_variant
                && artifact.sha256 == record.artifact_sha256
                && artifact.file_size == record.artifact_file_size;
        });
}

} // namespace

RelocationArtifactReceipt::RelocationArtifactReceipt(
    const RelocationArtifactIdentity identity,
    const std::uint8_t* const bytes,
    const std::size_t size) noexcept:
    identity_(identity),
    bytes_(bytes),
    size_(size)
{}

const RelocationArtifactIdentity& RelocationArtifactReceipt::identity() const noexcept
{
    return identity_;
}

bool RelocationArtifactReceipt::IsBoundTo(
    const std::span<const std::uint8_t> bytes) const noexcept
{
    return bytes.data() == bytes_ && bytes.size() == size_;
}

RelocationArtifactAdmissionResult AdmitRelocationArtifact(
    const std::span<const std::uint8_t> bytes,
    const RelocationArtifactIdentity& identity,
    const RelocationArtifactDigestVerifier& verifier) noexcept
{
    RelocationArtifactAdmissionResult result;
    if (!IsAdmittedArtifact(identity)) {
        return result;
    }
    if (bytes.size() != identity.file_size) {
        result.diagnostic =
            RelocationArtifactAdmissionDiagnostic::ArtifactSizeMismatch;
        return result;
    }
    if (!verifier.VerifySha256(bytes, identity.sha256)) {
        result.diagnostic =
            RelocationArtifactAdmissionDiagnostic::DigestVerificationFailed;
        return result;
    }

    result.diagnostic = RelocationArtifactAdmissionDiagnostic::None;
    result.receipt = RelocationArtifactReceipt{identity, bytes.data(), bytes.size()};
    return result;
}

std::optional<std::uint64_t> AddressLibraryParseResult::ResolveOffset(
    const std::uint64_t id) const noexcept
{
    if (status != AddressLibraryStatus::Ready) {
        return std::nullopt;
    }
    const RelocationEntry sought{id, 0};
    const auto found = std::lower_bound(
        entries.begin(),
        entries.end(),
        sought,
        [](const RelocationEntry& left, const RelocationEntry& right) {
            return left.id < right.id;
        });
    if (found == entries.end() || found->id != id) {
        return std::nullopt;
    }
    return found->offset;
}

std::optional<std::uintptr_t> AddressLibraryParseResult::ResolveAddress(
    const std::uint64_t id,
    const std::uintptr_t image_base,
    const std::size_t image_size) const noexcept
{
    if (image_base == 0U || image_size == 0U
        || image_size > (std::numeric_limits<std::uintptr_t>::max)() - image_base) {
        return std::nullopt;
    }
    const std::optional offset = ResolveOffset(id);
    if (!offset.has_value()
        || *offset >= image_size
        || *offset > (std::numeric_limits<std::uintptr_t>::max)()) {
        return std::nullopt;
    }
    const auto address_offset = static_cast<std::uintptr_t>(*offset);
    if (address_offset > (std::numeric_limits<std::uintptr_t>::max)() - image_base) {
        return std::nullopt;
    }
    return image_base + address_offset;
}

AddressLibraryParseResult ParseAddressLibraryV2(
    const std::span<const std::uint8_t> bytes,
    const RelocationArtifactReceipt& artifact,
    const std::size_t module_image_size) noexcept
{
    if (!artifact.IsBoundTo(bytes)) {
        return Reject(AddressLibraryDiagnostic::ArtifactBytesMismatch);
    }
    const RelocationArtifactIdentity& identity = artifact.identity();
    if (!IsAdmittedArtifact(identity)) {
        return Reject(AddressLibraryDiagnostic::ArtifactUnsupported);
    }
    if (bytes.size() != identity.file_size) {
        return Reject(AddressLibraryDiagnostic::ArtifactSizeMismatch);
    }
    if (module_image_size == 0U) {
        return Reject(AddressLibraryDiagnostic::InvalidModuleSize);
    }

    try {
        ByteReader reader(bytes);
        std::int32_t format = 0;
        if (!reader.ReadSigned32(format)) {
            return Reject(AddressLibraryDiagnostic::Truncated);
        }
        if (format != 2) {
            return Reject(AddressLibraryDiagnostic::UnsupportedFormat);
        }

        std::array<std::int32_t, 4> version{};
        for (std::int32_t& component : version) {
            if (!reader.ReadSigned32(component)) {
                return Reject(AddressLibraryDiagnostic::Truncated);
            }
        }
        if (version[0] != identity.runtime_version.major
            || version[1] != identity.runtime_version.minor
            || version[2] != identity.runtime_version.patch
            || version[3] != identity.runtime_version.build) {
            return Reject(AddressLibraryDiagnostic::RuntimeMismatch);
        }

        std::int32_t name_length = 0;
        if (!reader.ReadSigned32(name_length)) {
            return Reject(AddressLibraryDiagnostic::Truncated);
        }
        if (name_length < 0
            || reader.Remaining() < 2U * sizeof(std::int32_t)
            || static_cast<std::size_t>(name_length)
                > reader.Remaining() - 2U * sizeof(std::int32_t)
            || !reader.Skip(static_cast<std::size_t>(name_length))) {
            return Reject(AddressLibraryDiagnostic::InvalidNameLength);
        }

        std::int32_t pointer_size = 0;
        std::int32_t address_count = 0;
        if (!reader.ReadSigned32(pointer_size)
            || !reader.ReadSigned32(address_count)) {
            return Reject(AddressLibraryDiagnostic::Truncated);
        }
        if (pointer_size != 8) {
            return Reject(AddressLibraryDiagnostic::InvalidPointerSize);
        }
        if (address_count <= 0
            || static_cast<std::uint64_t>(address_count) > reader.Remaining()) {
            return Reject(AddressLibraryDiagnostic::InvalidEntryCount);
        }

        std::vector<RelocationEntry> entries;
        entries.reserve(static_cast<std::size_t>(address_count));
        std::uint64_t previous_id = 0;
        std::uint64_t previous_offset = 0;
        for (std::int32_t index = 0; index < address_count; ++index) {
            std::uint8_t type = 0;
            if (!reader.ReadUnsigned(type)) {
                return Reject(AddressLibraryDiagnostic::Truncated);
            }
            const std::uint8_t id_mode = type & 0x0FU;
            if (id_mode > 7U) {
                return Reject(AddressLibraryDiagnostic::InvalidEncoding);
            }

            std::uint64_t id = 0;
            DecodeStatus decoded = DecodeValue(reader, id_mode, previous_id, id);
            if (decoded != DecodeStatus::Ok) {
                return Reject(ToDiagnostic(decoded));
            }

            const std::uint8_t offset_encoding = type >> 4U;
            const bool scaled = (offset_encoding & 8U) != 0U;
            if (scaled && previous_offset % static_cast<std::uint64_t>(pointer_size) != 0U) {
                return Reject(AddressLibraryDiagnostic::InvalidEncoding);
            }
            const std::uint64_t offset_base = scaled
                ? previous_offset / static_cast<std::uint64_t>(pointer_size)
                : previous_offset;
            std::uint64_t offset = 0;
            decoded = DecodeValue(reader, offset_encoding & 7U, offset_base, offset);
            if (decoded != DecodeStatus::Ok) {
                return Reject(ToDiagnostic(decoded));
            }
            if (scaled) {
                if (offset > (std::numeric_limits<std::uint64_t>::max)()
                        / static_cast<std::uint64_t>(pointer_size)) {
                    return Reject(AddressLibraryDiagnostic::ArithmeticOverflow);
                }
                offset *= static_cast<std::uint64_t>(pointer_size);
            }
            if (offset >= module_image_size) {
                return Reject(AddressLibraryDiagnostic::OffsetOutsideModule);
            }

            entries.push_back(RelocationEntry{id, offset});
            previous_id = id;
            previous_offset = offset;
        }

        if (reader.Remaining() != 0U) {
            return Reject(AddressLibraryDiagnostic::TrailingData);
        }
        std::ranges::sort(entries, {}, &RelocationEntry::id);
        const auto duplicate = std::adjacent_find(
            entries.begin(),
            entries.end(),
            [](const RelocationEntry& left, const RelocationEntry& right) {
                return left.id == right.id;
            });
        if (duplicate != entries.end()) {
            return Reject(AddressLibraryDiagnostic::DuplicateId);
        }

        AddressLibraryParseResult result;
        result.status = AddressLibraryStatus::Ready;
        result.diagnostic = AddressLibraryDiagnostic::None;
        result.runtime_version = identity.runtime_version;
        result.entries = std::move(entries);
        return result;
    } catch (const std::bad_alloc&) {
        return Reject(AddressLibraryDiagnostic::AllocationFailure);
    } catch (...) {
        return Reject(AddressLibraryDiagnostic::Truncated);
    }
}

} // namespace enbcore::skyrim
