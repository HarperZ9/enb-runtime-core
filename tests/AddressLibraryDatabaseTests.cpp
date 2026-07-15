#include <enbcore/skyrim/AddressLibraryDatabase.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>

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

static_assert(!std::is_default_constructible_v<RelocationArtifactReceipt>);
static_assert(!std::is_aggregate_v<RelocationArtifactReceipt>);

constexpr RuntimeVersion kRuntime{1, 6, 1170, 0};
constexpr std::size_t kArtifactSize = 795'129U;
constexpr std::size_t kVariantArtifactSize = 795'210U;
constexpr std::size_t kImageSize = 0x20'0000U;
constexpr Sha256Digest kArtifactDigest{
    0xC4U, 0x09U, 0x3CU, 0x56U, 0x9AU, 0x3CU, 0x83U, 0xB2U,
    0x65U, 0x87U, 0xF4U, 0xB9U, 0xEAU, 0x4CU, 0x55U, 0xDEU,
    0x9AU, 0xE6U, 0xE7U, 0x3BU, 0x84U, 0xA2U, 0xAFU, 0x9FU,
    0xB3U, 0xFBU, 0xD3U, 0x0EU, 0x2FU, 0xE0U, 0xD4U, 0x52U,
};
constexpr Sha256Digest kVariantArtifactDigest{
    0xACU, 0x6DU, 0x17U, 0xE8U, 0xA4U, 0xBBU, 0x4DU, 0xA2U,
    0x53U, 0x9EU, 0x7AU, 0x57U, 0x11U, 0x13U, 0xBCU, 0xB2U,
    0x8AU, 0xE5U, 0xADU, 0xF4U, 0x87U, 0x4CU, 0xD9U, 0x77U,
    0x33U, 0x2FU, 0x1AU, 0x52U, 0x15U, 0xF6U, 0x5CU, 0x07U,
};

template <typename Integer>
void AppendLittleEndian(std::vector<std::uint8_t>& bytes, const Integer value)
{
    using Unsigned = std::make_unsigned_t<Integer>;
    const auto encoded = static_cast<Unsigned>(value);
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
        bytes.push_back(static_cast<std::uint8_t>(encoded >> (index * 8U)));
    }
}

std::vector<std::uint8_t> ValidRecords()
{
    std::vector<std::uint8_t> records;
    records.push_back(0x00U);
    AppendLittleEndian(records, std::uint64_t{100U});
    AppendLittleEndian(records, std::uint64_t{0x1000U});
    records.push_back(0x91U);
    records.push_back(0x22U);
    records.push_back(5U);
    records.push_back(0x10U);
    return records;
}

struct HeaderOverrides final {
    std::int32_t format{2};
    RuntimeVersion runtime{kRuntime};
    std::int32_t pointer_size{8};
    std::int32_t address_count{3};
    std::size_t trailing_bytes{0};
};

std::vector<std::uint8_t> BuildDatabase(
    const std::span<const std::uint8_t> records,
    const HeaderOverrides overrides = {})
{
    constexpr std::size_t fixed_header_size = 32U;
    const std::size_t occupied = fixed_header_size + records.size()
        + overrides.trailing_bytes;
    EXPECT(occupied <= kArtifactSize);
    const auto name_length = static_cast<std::int32_t>(kArtifactSize - occupied);

    std::vector<std::uint8_t> bytes;
    bytes.reserve(kArtifactSize);
    AppendLittleEndian(bytes, overrides.format);
    AppendLittleEndian(bytes, static_cast<std::int32_t>(overrides.runtime.major));
    AppendLittleEndian(bytes, static_cast<std::int32_t>(overrides.runtime.minor));
    AppendLittleEndian(bytes, static_cast<std::int32_t>(overrides.runtime.patch));
    AppendLittleEndian(bytes, static_cast<std::int32_t>(overrides.runtime.build));
    AppendLittleEndian(bytes, name_length);
    bytes.insert(bytes.end(), static_cast<std::size_t>(name_length), 0U);
    AppendLittleEndian(bytes, overrides.pointer_size);
    AppendLittleEndian(bytes, overrides.address_count);
    bytes.insert(bytes.end(), records.begin(), records.end());
    bytes.insert(bytes.end(), overrides.trailing_bytes, 0U);
    EXPECT(bytes.size() == kArtifactSize);
    return bytes;
}

RelocationArtifactIdentity MakeArtifactIdentity()
{
    return RelocationArtifactIdentity{
        RelocationProviderKind::AddressLibrary,
        kRuntime,
        RuntimeVariant::AnniversaryEdition,
        kArtifactDigest,
        kArtifactSize,
    };
}

class FakeDigestVerifier final : public RelocationArtifactDigestVerifier {
public:
    bool accepts{true};
    Sha256Digest accepted_digest{kArtifactDigest};
    mutable std::size_t calls{0};

    [[nodiscard]] bool VerifySha256(
        const std::span<const std::uint8_t>,
        const Sha256Digest& expected) const noexcept override
    {
        ++calls;
        return accepts && expected == accepted_digest;
    }
};

RelocationArtifactReceipt AdmitArtifact(
    const std::span<const std::uint8_t> bytes)
{
    const FakeDigestVerifier verifier;
    const RelocationArtifactAdmissionResult admission = AdmitRelocationArtifact(
        bytes, MakeArtifactIdentity(), verifier);
    EXPECT(admission.diagnostic == RelocationArtifactAdmissionDiagnostic::None);
    EXPECT(admission.receipt.has_value());
    EXPECT(verifier.calls == 1U);
    return *admission.receipt;
}

void both_captured_artifact_identities_can_receive_opaque_receipts()
{
    std::vector<std::uint8_t> bytes(kVariantArtifactSize, 0U);
    RelocationArtifactIdentity identity = MakeArtifactIdentity();
    identity.sha256 = kVariantArtifactDigest;
    identity.file_size = kVariantArtifactSize;
    FakeDigestVerifier verifier;
    verifier.accepted_digest = kVariantArtifactDigest;

    const RelocationArtifactAdmissionResult admission =
        AdmitRelocationArtifact(bytes, identity, verifier);

    EXPECT(admission.diagnostic == RelocationArtifactAdmissionDiagnostic::None);
    EXPECT(admission.receipt.has_value());
    EXPECT(admission.receipt->identity().sha256 == kVariantArtifactDigest);
    EXPECT(admission.receipt->IsBoundTo(bytes));
    EXPECT(verifier.calls == 1U);
}

AddressLibraryParseResult ParseAdmitted(
    const std::span<const std::uint8_t> bytes,
    const std::size_t image_size)
{
    const RelocationArtifactReceipt receipt = AdmitArtifact(bytes);
    return ParseAddressLibraryV2(bytes, receipt, image_size);
}

void exact_v2_artifact_decodes_to_a_sorted_read_only_database()
{
    const std::vector records = ValidRecords();
    const std::vector bytes = BuildDatabase(records);

    const AddressLibraryParseResult result = ParseAdmitted(bytes, kImageSize);

    EXPECT(result.status == AddressLibraryStatus::Ready);
    EXPECT(result.diagnostic == AddressLibraryDiagnostic::None);
    EXPECT(result.runtime_version == kRuntime);
    EXPECT(result.entries.size() == 3U);
    EXPECT(result.entries[0] == (RelocationEntry{100U, 0x1000U}));
    EXPECT(result.entries[1] == (RelocationEntry{101U, 0x1008U}));
    EXPECT(result.entries[2] == (RelocationEntry{106U, 0x1018U}));
    EXPECT(result.ResolveOffset(101U) == std::optional<std::uint64_t>{0x1008U});
    EXPECT(!result.ResolveOffset(102U).has_value());
    EXPECT(result.ResolveAddress(101U, 0x140000000ULL, kImageSize)
        == std::optional<std::uintptr_t>{0x140001008ULL});
    EXPECT(!result.ResolveAddress(
        101U,
        (std::numeric_limits<std::uintptr_t>::max)() - 0x1000U,
        kImageSize).has_value());
}

void artifact_identity_is_checked_before_payload_decode()
{
    const std::vector records = ValidRecords();
    const std::vector bytes = BuildDatabase(records);
    FakeDigestVerifier verifier;
    verifier.accepts = false;
    RelocationArtifactAdmissionResult admission = AdmitRelocationArtifact(
        bytes, MakeArtifactIdentity(), verifier);
    EXPECT(admission.diagnostic
        == RelocationArtifactAdmissionDiagnostic::DigestVerificationFailed);
    EXPECT(!admission.receipt.has_value());
    EXPECT(verifier.calls == 1U);

    std::vector truncated_artifact = bytes;
    truncated_artifact.pop_back();
    verifier = FakeDigestVerifier{};
    admission = AdmitRelocationArtifact(
        truncated_artifact, MakeArtifactIdentity(), verifier);
    EXPECT(admission.diagnostic
        == RelocationArtifactAdmissionDiagnostic::ArtifactSizeMismatch);
    EXPECT(!admission.receipt.has_value());
    EXPECT(verifier.calls == 0U);

    RelocationArtifactIdentity wrong = MakeArtifactIdentity();
    wrong.sha256[0] ^= 0xFFU;
    admission = AdmitRelocationArtifact(bytes, wrong, verifier);
    EXPECT(admission.diagnostic
        == RelocationArtifactAdmissionDiagnostic::ArtifactUnsupported);
    EXPECT(!admission.receipt.has_value());
    EXPECT(verifier.calls == 0U);

    wrong = MakeArtifactIdentity();
    --wrong.file_size;
    admission = AdmitRelocationArtifact(bytes, wrong, verifier);
    EXPECT(admission.diagnostic
        == RelocationArtifactAdmissionDiagnostic::ArtifactUnsupported);
    EXPECT(!admission.receipt.has_value());

    const RelocationArtifactReceipt receipt = AdmitArtifact(bytes);
    const std::vector copied_bytes = bytes;
    const AddressLibraryParseResult retargeted = ParseAddressLibraryV2(
        copied_bytes, receipt, kImageSize);
    EXPECT(retargeted.status == AddressLibraryStatus::Rejected);
    EXPECT(retargeted.diagnostic == AddressLibraryDiagnostic::ArtifactBytesMismatch);
    EXPECT(retargeted.entries.empty());
}

void header_contract_is_exact_and_bounded()
{
    const std::vector records = ValidRecords();

    HeaderOverrides overrides;
    overrides.format = 1;
    AddressLibraryParseResult result = ParseAdmitted(
        BuildDatabase(records, overrides), kImageSize);
    EXPECT(result.diagnostic == AddressLibraryDiagnostic::UnsupportedFormat);

    overrides = {};
    overrides.runtime = RuntimeVersion{1, 6, 1130, 0};
    result = ParseAdmitted(BuildDatabase(records, overrides), kImageSize);
    EXPECT(result.diagnostic == AddressLibraryDiagnostic::RuntimeMismatch);

    overrides = {};
    overrides.pointer_size = 4;
    result = ParseAdmitted(BuildDatabase(records, overrides), kImageSize);
    EXPECT(result.diagnostic == AddressLibraryDiagnostic::InvalidPointerSize);

    overrides = {};
    overrides.address_count = 0;
    result = ParseAdmitted(BuildDatabase(records, overrides), kImageSize);
    EXPECT(result.diagnostic == AddressLibraryDiagnostic::InvalidEntryCount);

    std::vector bytes = BuildDatabase(records);
    bytes[20] = 0xFFU;
    bytes[21] = 0xFFU;
    bytes[22] = 0xFFU;
    bytes[23] = 0x7FU;
    result = ParseAdmitted(bytes, kImageSize);
    EXPECT(result.diagnostic == AddressLibraryDiagnostic::InvalidNameLength);
}

void every_record_read_is_bounded_and_trailing_data_is_rejected()
{
    const std::vector records = ValidRecords();
    HeaderOverrides overrides;
    overrides.address_count = 4;
    AddressLibraryParseResult result = ParseAdmitted(
        BuildDatabase(records, overrides), kImageSize);
    EXPECT(result.diagnostic == AddressLibraryDiagnostic::Truncated);

    overrides = {};
    overrides.address_count = 100;
    const std::array<std::uint8_t, 1> tiny_records{0x11U};
    result = ParseAdmitted(BuildDatabase(tiny_records, overrides), kImageSize);
    EXPECT(result.diagnostic == AddressLibraryDiagnostic::InvalidEntryCount);

    overrides = {};
    overrides.trailing_bytes = 1U;
    result = ParseAdmitted(BuildDatabase(records, overrides), kImageSize);
    EXPECT(result.diagnostic == AddressLibraryDiagnostic::TrailingData);
}

void delta_arithmetic_and_offsets_fail_closed()
{
    std::vector<std::uint8_t> records;
    records.push_back(0x00U);
    AppendLittleEndian(records, (std::numeric_limits<std::uint64_t>::max)());
    AppendLittleEndian(records, std::uint64_t{0x1000U});
    records.push_back(0x11U);
    HeaderOverrides overrides;
    overrides.address_count = 2;
    AddressLibraryParseResult result = ParseAdmitted(
        BuildDatabase(records, overrides), kImageSize);
    EXPECT(result.diagnostic == AddressLibraryDiagnostic::ArithmeticOverflow);

    records.clear();
    records.push_back(0x00U);
    AppendLittleEndian(records, std::uint64_t{1U});
    AppendLittleEndian(records, std::uint64_t{0U});
    records.push_back(0x31U);
    records.push_back(1U);
    result = ParseAdmitted(BuildDatabase(records, overrides), kImageSize);
    EXPECT(result.diagnostic == AddressLibraryDiagnostic::ArithmeticOverflow);

    records.clear();
    records.push_back(0x00U);
    AppendLittleEndian(records, std::uint64_t{1U});
    AppendLittleEndian(records, static_cast<std::uint64_t>(kImageSize));
    overrides.address_count = 1;
    result = ParseAdmitted(BuildDatabase(records, overrides), kImageSize);
    EXPECT(result.diagnostic == AddressLibraryDiagnostic::OffsetOutsideModule);

    const std::array<std::uint8_t, 1> invalid_nibble{0x18U};
    result = ParseAdmitted(BuildDatabase(invalid_nibble, overrides), kImageSize);
    EXPECT(result.diagnostic == AddressLibraryDiagnostic::InvalidEncoding);

    records.clear();
    records.push_back(0x00U);
    AppendLittleEndian(records, std::uint64_t{1U});
    AppendLittleEndian(records, std::uint64_t{3U});
    records.push_back(0x91U);
    overrides.address_count = 2;
    result = ParseAdmitted(BuildDatabase(records, overrides), kImageSize);
    EXPECT(result.diagnostic == AddressLibraryDiagnostic::InvalidEncoding);
}

void ids_are_sorted_and_duplicates_are_rejected()
{
    std::vector<std::uint8_t> records;
    records.push_back(0x00U);
    AppendLittleEndian(records, std::uint64_t{100U});
    AppendLittleEndian(records, std::uint64_t{0x1000U});
    records.push_back(0x13U);
    records.push_back(1U);
    HeaderOverrides overrides;
    overrides.address_count = 2;
    AddressLibraryParseResult result = ParseAdmitted(
        BuildDatabase(records, overrides), kImageSize);
    EXPECT(result.status == AddressLibraryStatus::Ready);
    EXPECT(result.entries[0].id == 99U);
    EXPECT(result.entries[1].id == 100U);

    records.clear();
    records.push_back(0x00U);
    AppendLittleEndian(records, std::uint64_t{100U});
    AppendLittleEndian(records, std::uint64_t{0x1000U});
    records.push_back(0x10U);
    AppendLittleEndian(records, std::uint64_t{100U});
    result = ParseAdmitted(BuildDatabase(records, overrides), kImageSize);
    EXPECT(result.status == AddressLibraryStatus::Rejected);
    EXPECT(result.diagnostic == AddressLibraryDiagnostic::DuplicateId);
    EXPECT(result.entries.empty());
}

} // namespace

int main()
{
    exact_v2_artifact_decodes_to_a_sorted_read_only_database();
    both_captured_artifact_identities_can_receive_opaque_receipts();
    artifact_identity_is_checked_before_payload_decode();
    header_contract_is_exact_and_bounded();
    every_record_read_is_bounded_and_trailing_data_is_rejected();
    delta_arithmetic_and_offsets_fail_closed();
    ids_are_sorted_and_duplicates_are_rejected();

    if (failures != 0) {
        std::cerr << failures << " assertion(s) failed\n";
        return 1;
    }
    std::cout << "Address Library database tests passed\n";
    return 0;
}
