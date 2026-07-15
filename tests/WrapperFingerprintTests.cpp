#include <enbcore/enb/WrapperFingerprint.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

int failures = 0;
std::uint64_t assertions = 0;

void expect(const bool condition, const char* expression, const char* file, const int line)
{
    ++assertions;
    if (condition) {
        return;
    }

    std::cerr << file << ':' << line << ": expectation failed: " << expression << '\n';
    ++failures;
}

#define EXPECT(expression) expect((expression), #expression, __FILE__, __LINE__)

[[nodiscard]] std::wstring extended_path(const std::wstring_view path)
{
    constexpr std::wstring_view prefix = LR"(\\?\)";
    constexpr std::wstring_view unc_prefix = LR"(\\)";
    if (path.starts_with(prefix)) {
        return std::wstring(path);
    }
    if (path.starts_with(unc_prefix)) {
        return std::wstring(LR"(\\?\UNC\)") + std::wstring(path.substr(2));
    }
    return std::wstring(prefix) + std::wstring(path);
}

class TempTree final {
public:
    TempTree()
    {
        std::array<wchar_t, 32'768> buffer{};
        const DWORD length = ::GetTempPathW(
            static_cast<DWORD>(buffer.size()),
            buffer.data());
        if (length == 0 || length >= buffer.size()) {
            return;
        }

        root_ = std::wstring(buffer.data(), length)
            + L"enbcore-fingerprint-"
            + std::to_wstring(::GetCurrentProcessId())
            + L"-"
            + std::to_wstring(::GetTickCount64());
        create_directory(root_);
    }

    ~TempTree()
    {
        for (auto it = files_.rbegin(); it != files_.rend(); ++it) {
            ::DeleteFileW(extended_path(*it).c_str());
        }
        for (auto it = directories_.rbegin(); it != directories_.rend(); ++it) {
            ::RemoveDirectoryW(extended_path(*it).c_str());
        }
    }

    TempTree(const TempTree&) = delete;
    TempTree& operator=(const TempTree&) = delete;

    [[nodiscard]] bool ready() const noexcept
    {
        return !root_.empty() && !directories_.empty();
    }

    [[nodiscard]] const std::wstring& root() const noexcept
    {
        return root_;
    }

    [[nodiscard]] std::wstring make_directory(
        const std::wstring_view parent,
        const std::wstring_view name)
    {
        std::wstring path(parent);
        if (!path.empty() && path.back() != L'\\') {
            path.push_back(L'\\');
        }
        path.append(name);
        create_directory(path);
        return path;
    }

    [[nodiscard]] std::wstring file_path(
        const std::wstring_view parent,
        const std::wstring_view name) const
    {
        std::wstring path(parent);
        if (!path.empty() && path.back() != L'\\') {
            path.push_back(L'\\');
        }
        path.append(name);
        return path;
    }

    [[nodiscard]] bool write(
        const std::wstring& path,
        const std::span<const std::byte> bytes)
    {
        const HANDLE file = ::CreateFileW(
            extended_path(path).c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (file == INVALID_HANDLE_VALUE) {
            return false;
        }

        bool complete = true;
        std::size_t written_total = 0;
        while (written_total < bytes.size()) {
            const std::size_t remaining = bytes.size() - written_total;
            const DWORD requested = static_cast<DWORD>((std::min)(
                remaining,
                static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
            DWORD written = 0;
            if (::WriteFile(
                    file,
                    bytes.data() + written_total,
                    requested,
                    &written,
                    nullptr)
                    == FALSE
                || written == 0) {
                complete = false;
                break;
            }
            written_total += written;
        }

        if (::CloseHandle(file) == FALSE) {
            complete = false;
        }
        if (complete) {
            if (std::ranges::find(files_, path) == files_.end()) {
                files_.push_back(path);
            }
        }
        return complete;
    }

private:
    void create_directory(const std::wstring& path)
    {
        if (::CreateDirectoryW(extended_path(path).c_str(), nullptr) != FALSE) {
            directories_.push_back(path);
        }
    }

    std::wstring root_;
    std::vector<std::wstring> directories_;
    std::vector<std::wstring> files_;
};

[[nodiscard]] std::vector<std::byte> minimal_pe(const std::uint16_t machine)
{
    std::vector<std::byte> bytes(512, std::byte{0});
    bytes[0] = std::byte{'M'};
    bytes[1] = std::byte{'Z'};

    constexpr std::int32_t pe_offset = 128;
    std::memcpy(bytes.data() + 0x3c, &pe_offset, sizeof(pe_offset));
    constexpr std::array signature{
        std::byte{'P'}, std::byte{'E'}, std::byte{0}, std::byte{0}};
    std::memcpy(bytes.data() + pe_offset, signature.data(), signature.size());
    std::memcpy(bytes.data() + pe_offset + signature.size(), &machine, sizeof(machine));
    bytes.back() = std::byte{0x5a};
    return bytes;
}

class FakeVersionPlatform final : public enbcore::enb::FileVersionPlatform {
public:
    enbcore::enb::FileVersionResult next{
        enbcore::enb::FileVersionCode::Complete,
        {9, 8, 7, 6},
        L"synthetic.dll"};
    std::wstring requested_path;
    std::size_t calls{0};

    [[nodiscard]] enbcore::enb::FileVersionResult ReadFileVersion(
        const std::wstring_view path) noexcept override
    {
        ++calls;
        try {
            requested_path.assign(path);
            return next;
        } catch (...) {
            return enbcore::enb::FileVersionResult{
                enbcore::enb::FileVersionCode::Unavailable};
        }
    }
};

class FakeModulePathPlatform final : public enbcore::enb::ModulePathPlatform {
public:
    enbcore::enb::ModulePathResult next;
    enbcore::enb::LoadedModule requested_module{0};
    std::size_t calls{0};

    [[nodiscard]] enbcore::enb::ModulePathResult ResolveModulePath(
        const enbcore::enb::LoadedModule module) noexcept override
    {
        ++calls;
        requested_module = module;
        try {
            return next;
        } catch (...) {
            return enbcore::enb::ModulePathResult{
                enbcore::enb::ModulePathCode::Unavailable};
        }
    }
};

[[nodiscard]] enbcore::enb::WrapperBuildRecord record_for(
    const enbcore::enb::WrapperFingerprint& fingerprint)
{
    return enbcore::enb::WrapperBuildRecord{
        "synthetic-build",
        fingerprint.sha256,
        fingerprint.file_size,
        fingerprint.file_version,
        fingerprint.architecture,
        fingerprint.original_filename};
}

void the_production_allowlist_names_the_exact_current_build()
{
    using namespace enbcore::enb;

    const std::span builds = SupportedEnbWrapperBuilds();
    EXPECT(builds.size() == 1U);
    EXPECT(builds[0].identifier == "enbseries-0.504-sha256-87583e85");
    EXPECT(builds[0].file_size == 4'665'344U);
    EXPECT(builds[0].file_version == (WrapperFileVersion{0, 5, 0, 4}));
    EXPECT(builds[0].architecture == WrapperArchitecture::X64);
    EXPECT(builds[0].original_filename == L"d3d11.dll");

    constexpr Sha256Digest expected{
        0x87, 0x58, 0x3e, 0x85, 0xce, 0x99, 0x3e, 0x63,
        0x38, 0x48, 0x6f, 0x6d, 0x5d, 0xe0, 0x9f, 0x9a,
        0x74, 0x66, 0x1b, 0xa2, 0x45, 0xe8, 0x20, 0xdc,
        0x75, 0xe3, 0xa7, 0xa8, 0x2c, 0x98, 0x88, 0xc7};
    EXPECT(builds[0].sha256 == expected);
}

void a_synthetic_exact_match_is_admitted()
{
    using namespace enbcore::enb;

    TempTree tree;
    EXPECT(tree.ready());
    const std::wstring path = tree.file_path(tree.root(), L"exact.dll");
    const std::vector bytes = minimal_pe(IMAGE_FILE_MACHINE_AMD64);
    EXPECT(tree.write(path, bytes));

    FakeVersionPlatform versions;
    const WrapperEvidence evidence = InspectWrapperFile(path, versions);
    EXPECT(evidence.code == WrapperEvidenceCode::Complete);
    EXPECT(evidence.path == path);
    EXPECT(evidence.fingerprint.file_size == bytes.size());
    EXPECT(evidence.fingerprint.file_version == versions.next.version);
    EXPECT(evidence.fingerprint.architecture == WrapperArchitecture::X64);
    EXPECT(evidence.fingerprint.original_filename == L"synthetic.dll");
    EXPECT(versions.calls == 1U);

    const WrapperBuildRecord record = record_for(evidence.fingerprint);
    const std::array allowlist{record};
    const WrapperCompatibilityResult result = ClassifyWrapperBuild(
        evidence,
        WrapperPolicy::Strict,
        allowlist);

    EXPECT(result.build == WrapperBuildCode::KnownBuild);
    EXPECT(result.admission == WrapperAdmissionCode::AdmittedKnownBuild);
    EXPECT(result.admitted());
    EXPECT(result.mismatches == WrapperMismatch::None);
    EXPECT(result.allowlist_index == 0U);
}

void one_byte_mutation_is_an_explicit_unknown_hash()
{
    using namespace enbcore::enb;

    TempTree tree;
    const std::wstring path = tree.file_path(tree.root(), L"mutation.dll");
    std::vector bytes = minimal_pe(IMAGE_FILE_MACHINE_AMD64);
    EXPECT(tree.write(path, bytes));

    FakeVersionPlatform versions;
    const WrapperEvidence baseline = InspectWrapperFile(path, versions);
    EXPECT(baseline.complete());
    const WrapperBuildRecord record = record_for(baseline.fingerprint);
    const std::array allowlist{record};

    bytes.back() ^= std::byte{1};
    EXPECT(tree.write(path, bytes));
    const WrapperEvidence mutated = InspectWrapperFile(path, versions);
    EXPECT(mutated.complete());
    EXPECT(mutated.fingerprint.sha256 != baseline.fingerprint.sha256);
    EXPECT(mutated.fingerprint.file_size == baseline.fingerprint.file_size);

    const WrapperCompatibilityResult result = ClassifyWrapperBuild(
        mutated,
        WrapperPolicy::Strict,
        allowlist);
    EXPECT(result.build == WrapperBuildCode::UnknownBuild);
    EXPECT(result.admission == WrapperAdmissionCode::RejectedUnknownBuild);
    EXPECT(!result.admitted());
    EXPECT(result.mismatches == WrapperMismatch::Sha256);
}

void metadata_and_architecture_mismatches_are_individually_diagnosed()
{
    using namespace enbcore::enb;

    TempTree tree;
    const std::wstring path = tree.file_path(tree.root(), L"metadata.dll");
    const std::vector bytes = minimal_pe(IMAGE_FILE_MACHINE_AMD64);
    EXPECT(tree.write(path, bytes));

    FakeVersionPlatform versions;
    const WrapperEvidence evidence = InspectWrapperFile(path, versions);
    EXPECT(evidence.complete());

    const WrapperBuildRecord baseline = record_for(evidence.fingerprint);
    auto classify = [&](const WrapperBuildRecord& expected) {
        const std::array allowlist{expected};
        return ClassifyWrapperBuild(evidence, WrapperPolicy::Strict, allowlist);
    };

    WrapperBuildRecord wrong_size = baseline;
    ++wrong_size.file_size;
    EXPECT(classify(wrong_size).mismatches == WrapperMismatch::FileSize);

    WrapperBuildRecord wrong_version = baseline;
    ++wrong_version.file_version.build;
    EXPECT(classify(wrong_version).mismatches == WrapperMismatch::FileVersion);

    WrapperBuildRecord wrong_name = baseline;
    wrong_name.original_filename = L"not-d3d11.dll";
    EXPECT(classify(wrong_name).mismatches == WrapperMismatch::OriginalFilename);

    WrapperBuildRecord wrong_architecture = baseline;
    wrong_architecture.architecture = WrapperArchitecture::X86;
    EXPECT(classify(wrong_architecture).mismatches == WrapperMismatch::Architecture);
}

void x86_pe_metadata_is_observed_without_being_mislabelled()
{
    using namespace enbcore::enb;

    TempTree tree;
    const std::wstring path = tree.file_path(tree.root(), L"x86.dll");
    const std::vector bytes = minimal_pe(IMAGE_FILE_MACHINE_I386);
    EXPECT(tree.write(path, bytes));

    FakeVersionPlatform versions;
    const WrapperEvidence evidence = InspectWrapperFile(path, versions);
    EXPECT(evidence.complete());
    EXPECT(evidence.fingerprint.architecture == WrapperArchitecture::X86);
}

void missing_unreadable_and_truncated_inputs_fail_as_evidence()
{
    using namespace enbcore::enb;

    TempTree tree;
    FakeVersionPlatform versions;

    const std::wstring missing = tree.file_path(tree.root(), L"missing.dll");
    const WrapperEvidence absent = InspectWrapperFile(missing, versions);
    EXPECT(absent.code == WrapperEvidenceCode::FileOpenFailed);
    EXPECT(versions.calls == 0U);

    const WrapperEvidence directory = InspectWrapperFile(tree.root(), versions);
    EXPECT(directory.code == WrapperEvidenceCode::FileOpenFailed);
    EXPECT(versions.calls == 0U);

    const std::wstring truncated_path = tree.file_path(tree.root(), L"truncated.dll");
    const std::array truncated_bytes{std::byte{'M'}, std::byte{'Z'}};
    EXPECT(tree.write(truncated_path, truncated_bytes));
    const WrapperEvidence truncated = InspectWrapperFile(truncated_path, versions);
    EXPECT(truncated.code == WrapperEvidenceCode::PortableExecutableTruncated);
    EXPECT(versions.calls == 0U);

    const WrapperCompatibilityResult strict = ClassifyWrapperBuild(
        truncated,
        WrapperPolicy::Strict,
        SupportedEnbWrapperBuilds());
    EXPECT(strict.build == WrapperBuildCode::EvidenceUnavailable);
    EXPECT(strict.admission == WrapperAdmissionCode::RejectedEvidenceUnavailable);
    EXPECT(!strict.admitted());

    const WrapperCompatibilityResult audit = ClassifyWrapperBuild(
        truncated,
        WrapperPolicy::AuditOnly,
        SupportedEnbWrapperBuilds());
    EXPECT(audit.build == WrapperBuildCode::EvidenceUnavailable);
    EXPECT(audit.admission == WrapperAdmissionCode::AdmittedAuditOnly);
    EXPECT(audit.admitted());
}

void hashing_is_deterministic()
{
    using namespace enbcore::enb;

    TempTree tree;
    const std::wstring path = tree.file_path(tree.root(), L"repeat.dll");
    const std::vector bytes = minimal_pe(IMAGE_FILE_MACHINE_AMD64);
    EXPECT(tree.write(path, bytes));

    FakeVersionPlatform versions;
    const WrapperEvidence first = InspectWrapperFile(path, versions);
    const WrapperEvidence second = InspectWrapperFile(path, versions);
    EXPECT(first.complete());
    EXPECT(second.complete());
    EXPECT(first.fingerprint.sha256 == second.fingerprint.sha256);
    EXPECT(first.fingerprint.file_size == second.fingerprint.file_size);
    EXPECT(versions.calls == 2U);
}

void strict_and_audit_only_policies_diverge_only_for_unknown_builds()
{
    using namespace enbcore::enb;

    TempTree tree;
    const std::wstring path = tree.file_path(tree.root(), L"policy.dll");
    const std::vector bytes = minimal_pe(IMAGE_FILE_MACHINE_AMD64);
    EXPECT(tree.write(path, bytes));

    FakeVersionPlatform versions;
    const WrapperEvidence evidence = InspectWrapperFile(path, versions);
    EXPECT(evidence.complete());
    WrapperBuildRecord unknown_record = record_for(evidence.fingerprint);
    unknown_record.sha256[0] ^= 0xffU;
    const std::array allowlist{unknown_record};

    const WrapperCompatibilityResult strict = ClassifyWrapperBuild(
        evidence,
        WrapperPolicy::Strict,
        allowlist);
    const WrapperCompatibilityResult audit = ClassifyWrapperBuild(
        evidence,
        WrapperPolicy::AuditOnly,
        allowlist);

    EXPECT(strict.build == WrapperBuildCode::UnknownBuild);
    EXPECT(strict.admission == WrapperAdmissionCode::RejectedUnknownBuild);
    EXPECT(!strict.admitted());
    EXPECT(audit.build == WrapperBuildCode::UnknownBuild);
    EXPECT(audit.admission == WrapperAdmissionCode::AdmittedAuditOnly);
    EXPECT(audit.admitted());
    EXPECT(strict.mismatches == audit.mismatches);
}

void long_unicode_paths_survive_the_complete_file_pipeline()
{
    using namespace enbcore::enb;

    TempTree tree;
    std::wstring directory = tree.root();
    constexpr std::wstring_view segment =
        L"clouds-雲-аврора-abcdefghijklmnopqrstuvwxyz0123456789";
    for (int index = 0; index < 5; ++index) {
        directory = tree.make_directory(directory, segment);
    }
    const std::wstring path = tree.file_path(directory, L"真実-天空-نور.dll");
    EXPECT(path.size() > 260U);

    const std::vector bytes = minimal_pe(IMAGE_FILE_MACHINE_AMD64);
    EXPECT(tree.write(path, bytes));
    FakeVersionPlatform versions;
    const WrapperEvidence evidence = InspectWrapperFile(path, versions);

    EXPECT(evidence.complete());
    EXPECT(evidence.path == path);
    EXPECT(versions.requested_path == path);
}

void loaded_module_evaluation_uses_the_injected_canonical_path()
{
    using namespace enbcore::enb;

    TempTree tree;
    const std::wstring path = tree.file_path(tree.root(), L"loaded.dll");
    const std::vector bytes = minimal_pe(IMAGE_FILE_MACHINE_AMD64);
    EXPECT(tree.write(path, bytes));

    FakeModulePathPlatform modules;
    modules.next = ModulePathResult{ModulePathCode::Complete, path};
    FakeVersionPlatform versions;

    const WrapperGateResult gate = EvaluateLoadedEnbWrapper(
        0x1234U,
        modules,
        versions,
        WrapperPolicy::AuditOnly);

    EXPECT(modules.calls == 1U);
    EXPECT(modules.requested_module == 0x1234U);
    EXPECT(gate.evidence.complete());
    EXPECT(gate.evidence.path == path);
    EXPECT(gate.compatibility.build == WrapperBuildCode::UnknownBuild);
    EXPECT(gate.compatibility.admission == WrapperAdmissionCode::AdmittedAuditOnly);
    EXPECT(gate.compatibility.admitted());

    modules.next = ModulePathResult{ModulePathCode::InvalidModule};
    const WrapperGateResult invalid = EvaluateLoadedEnbWrapper(
        0,
        modules,
        versions,
        WrapperPolicy::Strict);
    EXPECT(invalid.evidence.code == WrapperEvidenceCode::InvalidModule);
    EXPECT(invalid.compatibility.admission
        == WrapperAdmissionCode::RejectedEvidenceUnavailable);
}

void version_resource_failures_remain_explicit()
{
    using namespace enbcore::enb;

    TempTree tree;
    const std::wstring path = tree.file_path(tree.root(), L"version.dll");
    const std::vector bytes = minimal_pe(IMAGE_FILE_MACHINE_AMD64);
    EXPECT(tree.write(path, bytes));

    FakeVersionPlatform versions;
    versions.next.code = FileVersionCode::Unavailable;
    EXPECT(InspectWrapperFile(path, versions).code
        == WrapperEvidenceCode::FileVersionUnavailable);

    versions.next.code = FileVersionCode::Malformed;
    EXPECT(InspectWrapperFile(path, versions).code
        == WrapperEvidenceCode::FileVersionMalformed);

    versions.next.code = FileVersionCode::OriginalFilenameUnavailable;
    EXPECT(InspectWrapperFile(path, versions).code
        == WrapperEvidenceCode::OriginalFilenameUnavailable);
}

void the_windows_module_path_adapter_resolves_the_current_image()
{
    using namespace enbcore::enb;

    WindowsWrapperFingerprintPlatform platform;
    const HMODULE current = ::GetModuleHandleW(nullptr);
    EXPECT(current != nullptr);
    const ModulePathResult result = platform.ResolveModulePath(
        reinterpret_cast<LoadedModule>(current));
    EXPECT(result.code == ModulePathCode::Complete);
    EXPECT(!result.path.empty());
    EXPECT(result.path.find(L"enb_wrapper_fingerprint_tests") != std::wstring::npos);
}

} // namespace

int main()
{
    the_production_allowlist_names_the_exact_current_build();
    a_synthetic_exact_match_is_admitted();
    one_byte_mutation_is_an_explicit_unknown_hash();
    metadata_and_architecture_mismatches_are_individually_diagnosed();
    x86_pe_metadata_is_observed_without_being_mislabelled();
    missing_unreadable_and_truncated_inputs_fail_as_evidence();
    hashing_is_deterministic();
    strict_and_audit_only_policies_diverge_only_for_unknown_builds();
    long_unicode_paths_survive_the_complete_file_pipeline();
    loaded_module_evaluation_uses_the_injected_canonical_path();
    version_resource_failures_remain_explicit();
    the_windows_module_path_adapter_resolves_the_current_image();

    if (failures != 0) {
        std::cerr << failures << " failure(s) across " << assertions << " assertions\n";
        return 1;
    }

    std::cout << "Wrapper fingerprint tests passed: " << assertions << " assertions\n";
    return 0;
}
