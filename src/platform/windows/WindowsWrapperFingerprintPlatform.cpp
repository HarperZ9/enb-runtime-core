#include <enbcore/enb/WrapperFingerprint.hpp>

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <bcrypt.h>
#include <winver.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace enbcore::enb {
namespace {

constexpr std::size_t kInitialModulePathCapacity = 260;
constexpr std::size_t kMaximumModulePathCapacity = 32'768;
constexpr DWORD kHashReadCapacity = 64U * 1024U;

class UniqueFile final {
public:
    explicit UniqueFile(const HANDLE handle) noexcept
        : handle_(handle)
    {
    }

    ~UniqueFile()
    {
        if (handle_ != INVALID_HANDLE_VALUE) {
            ::CloseHandle(handle_);
        }
    }

    UniqueFile(const UniqueFile&) = delete;
    UniqueFile& operator=(const UniqueFile&) = delete;

    [[nodiscard]] HANDLE get() const noexcept
    {
        return handle_;
    }

private:
    HANDLE handle_{INVALID_HANDLE_VALUE};
};

class UniqueAlgorithm final {
public:
    UniqueAlgorithm() = default;

    ~UniqueAlgorithm()
    {
        if (handle_ != nullptr) {
            ::BCryptCloseAlgorithmProvider(handle_, 0);
        }
    }

    UniqueAlgorithm(const UniqueAlgorithm&) = delete;
    UniqueAlgorithm& operator=(const UniqueAlgorithm&) = delete;

    [[nodiscard]] BCRYPT_ALG_HANDLE* address() noexcept
    {
        return &handle_;
    }

    [[nodiscard]] BCRYPT_ALG_HANDLE get() const noexcept
    {
        return handle_;
    }

private:
    BCRYPT_ALG_HANDLE handle_{nullptr};
};

class UniqueHash final {
public:
    UniqueHash() = default;

    ~UniqueHash()
    {
        if (handle_ != nullptr) {
            ::BCryptDestroyHash(handle_);
        }
    }

    UniqueHash(const UniqueHash&) = delete;
    UniqueHash& operator=(const UniqueHash&) = delete;

    [[nodiscard]] BCRYPT_HASH_HANDLE* address() noexcept
    {
        return &handle_;
    }

    [[nodiscard]] BCRYPT_HASH_HANDLE get() const noexcept
    {
        return handle_;
    }

private:
    BCRYPT_HASH_HANDLE handle_{nullptr};
};

[[nodiscard]] std::wstring ApiPath(const std::wstring_view path)
{
    constexpr std::wstring_view extended_prefix = LR"(\\?\)";
    constexpr std::wstring_view unc_prefix = LR"(\\)";

    if (path.starts_with(extended_prefix) || path.size() < MAX_PATH) {
        return std::wstring(path);
    }
    if (path.starts_with(unc_prefix)) {
        return std::wstring(LR"(\\?\UNC\)") + std::wstring(path.substr(2));
    }
    return std::wstring(extended_prefix) + std::wstring(path);
}

enum class ExactReadCode : std::uint8_t {
    Complete,
    Truncated,
    Failed,
};

[[nodiscard]] ExactReadCode ReadExactAt(
    const HANDLE file,
    const std::uint64_t offset,
    const std::span<std::byte> destination) noexcept
{
    if (offset > static_cast<std::uint64_t>((std::numeric_limits<LONGLONG>::max)())) {
        return ExactReadCode::Failed;
    }

    LARGE_INTEGER position{};
    position.QuadPart = static_cast<LONGLONG>(offset);
    if (::SetFilePointerEx(file, position, nullptr, FILE_BEGIN) == FALSE) {
        return ExactReadCode::Failed;
    }

    std::size_t completed = 0;
    while (completed < destination.size()) {
        const std::size_t remaining = destination.size() - completed;
        const DWORD requested = static_cast<DWORD>((std::min)(
            remaining,
            static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
        DWORD read = 0;
        if (::ReadFile(
                file,
                destination.data() + completed,
                requested,
                &read,
                nullptr)
            == FALSE) {
            return ExactReadCode::Failed;
        }
        if (read == 0) {
            return ExactReadCode::Truncated;
        }
        completed += read;
    }
    return ExactReadCode::Complete;
}

struct ArchitectureResult final {
    WrapperEvidenceCode code{WrapperEvidenceCode::PortableExecutableInvalid};
    WrapperArchitecture architecture{WrapperArchitecture::Unknown};
};

[[nodiscard]] ArchitectureResult ReadArchitecture(
    const HANDLE file,
    const std::uint64_t file_size) noexcept
{
    IMAGE_DOS_HEADER dos{};
    if (file_size < sizeof(dos)) {
        return ArchitectureResult{WrapperEvidenceCode::PortableExecutableTruncated};
    }

    const ExactReadCode dos_read = ReadExactAt(
        file,
        0,
        std::as_writable_bytes(std::span{&dos, 1}));
    if (dos_read == ExactReadCode::Truncated) {
        return ArchitectureResult{WrapperEvidenceCode::PortableExecutableTruncated};
    }
    if (dos_read != ExactReadCode::Complete) {
        return ArchitectureResult{WrapperEvidenceCode::FileReadFailed};
    }
    if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0) {
        return ArchitectureResult{WrapperEvidenceCode::PortableExecutableInvalid};
    }

    const std::uint64_t nt_offset = static_cast<std::uint64_t>(dos.e_lfanew);
    constexpr std::uint64_t nt_prefix_size = sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER);
    if (nt_offset > file_size || file_size - nt_offset < nt_prefix_size) {
        return ArchitectureResult{WrapperEvidenceCode::PortableExecutableTruncated};
    }

    struct NtPrefix final {
        DWORD signature{0};
        IMAGE_FILE_HEADER file_header{};
    };
    static_assert(offsetof(NtPrefix, file_header) == sizeof(DWORD));

    NtPrefix prefix{};
    const ExactReadCode nt_read = ReadExactAt(
        file,
        nt_offset,
        std::as_writable_bytes(std::span{&prefix, 1}));
    if (nt_read == ExactReadCode::Truncated) {
        return ArchitectureResult{WrapperEvidenceCode::PortableExecutableTruncated};
    }
    if (nt_read != ExactReadCode::Complete) {
        return ArchitectureResult{WrapperEvidenceCode::FileReadFailed};
    }
    if (prefix.signature != IMAGE_NT_SIGNATURE) {
        return ArchitectureResult{WrapperEvidenceCode::PortableExecutableInvalid};
    }

    WrapperArchitecture architecture = WrapperArchitecture::Unknown;
    switch (prefix.file_header.Machine) {
    case IMAGE_FILE_MACHINE_I386:
        architecture = WrapperArchitecture::X86;
        break;
    case IMAGE_FILE_MACHINE_AMD64:
        architecture = WrapperArchitecture::X64;
        break;
    case IMAGE_FILE_MACHINE_ARM64:
        architecture = WrapperArchitecture::Arm64;
        break;
    default:
        break;
    }
    return ArchitectureResult{WrapperEvidenceCode::Complete, architecture};
}

[[nodiscard]] WrapperEvidenceCode HashFile(
    const HANDLE file,
    const std::uint64_t expected_size,
    Sha256Digest& digest) noexcept
{
    UniqueAlgorithm algorithm;
    if (!BCRYPT_SUCCESS(::BCryptOpenAlgorithmProvider(
            algorithm.address(),
            BCRYPT_SHA256_ALGORITHM,
            nullptr,
            0))) {
        return WrapperEvidenceCode::HashFailed;
    }

    DWORD object_length = 0;
    DWORD returned = 0;
    if (!BCRYPT_SUCCESS(::BCryptGetProperty(
            algorithm.get(),
            BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&object_length),
            sizeof(object_length),
            &returned,
            0))
        || returned != sizeof(object_length)
        || object_length == 0) {
        return WrapperEvidenceCode::HashFailed;
    }

    DWORD digest_length = 0;
    returned = 0;
    if (!BCRYPT_SUCCESS(::BCryptGetProperty(
            algorithm.get(),
            BCRYPT_HASH_LENGTH,
            reinterpret_cast<PUCHAR>(&digest_length),
            sizeof(digest_length),
            &returned,
            0))
        || returned != sizeof(digest_length)
        || digest_length != digest.size()) {
        return WrapperEvidenceCode::HashFailed;
    }

    std::vector<UCHAR> hash_object(object_length);
    UniqueHash hash;
    if (!BCRYPT_SUCCESS(::BCryptCreateHash(
            algorithm.get(),
            hash.address(),
            hash_object.data(),
            static_cast<ULONG>(hash_object.size()),
            nullptr,
            0,
            0))) {
        return WrapperEvidenceCode::HashFailed;
    }

    LARGE_INTEGER beginning{};
    if (::SetFilePointerEx(file, beginning, nullptr, FILE_BEGIN) == FALSE) {
        return WrapperEvidenceCode::FileReadFailed;
    }

    std::array<UCHAR, kHashReadCapacity> buffer{};
    std::uint64_t total = 0;
    while (true) {
        DWORD read = 0;
        if (::ReadFile(
                file,
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                &read,
                nullptr)
            == FALSE) {
            return WrapperEvidenceCode::FileReadFailed;
        }
        if (read == 0) {
            break;
        }
        if (total > (std::numeric_limits<std::uint64_t>::max)() - read) {
            return WrapperEvidenceCode::FileReadFailed;
        }
        if (!BCRYPT_SUCCESS(::BCryptHashData(hash.get(), buffer.data(), read, 0))) {
            return WrapperEvidenceCode::HashFailed;
        }
        total += read;
    }

    if (total != expected_size) {
        return WrapperEvidenceCode::FileReadFailed;
    }
    if (!BCRYPT_SUCCESS(::BCryptFinishHash(
            hash.get(),
            digest.data(),
            static_cast<ULONG>(digest.size()),
            0))) {
        return WrapperEvidenceCode::HashFailed;
    }
    return WrapperEvidenceCode::Complete;
}

[[nodiscard]] std::wstring ReadVersionString(
    const std::span<const std::byte> block,
    const WORD language,
    const WORD code_page,
    const wchar_t* const field)
{
    std::array<wchar_t, 64> query{};
    const int query_length = _snwprintf_s(
        query.data(),
        query.size(),
        _TRUNCATE,
        L"\\StringFileInfo\\%04x%04x\\%ls",
        static_cast<unsigned int>(language),
        static_cast<unsigned int>(code_page),
        field);
    if (query_length <= 0) {
        return {};
    }

    void* value = nullptr;
    UINT character_count = 0;
    if (::VerQueryValueW(
            const_cast<std::byte*>(block.data()),
            query.data(),
            &value,
            &character_count)
            == FALSE
        || value == nullptr
        || character_count == 0) {
        return {};
    }

    const auto* characters = static_cast<const wchar_t*>(value);
    std::size_t length = character_count;
    if (length != 0 && characters[length - 1] == L'\0') {
        --length;
    }
    return std::wstring(characters, length);
}

[[nodiscard]] bool IsVersionWhitespace(const wchar_t value) noexcept
{
    return value == L' ' || value == L'\t' || value == L'\r' || value == L'\n';
}

[[nodiscard]] bool ParseFourPartVersion(
    const std::wstring_view text,
    WrapperFileVersion& parsed) noexcept
{
    std::array<std::uint16_t, 4> parts{};
    std::size_t position = 0;

    for (std::size_t index = 0; index < parts.size(); ++index) {
        while (position < text.size() && IsVersionWhitespace(text[position])) {
            ++position;
        }
        if (position == text.size()
            || text[position] < L'0'
            || text[position] > L'9') {
            return false;
        }

        std::uint32_t value = 0;
        while (position < text.size()
               && text[position] >= L'0'
               && text[position] <= L'9') {
            value = value * 10U
                + static_cast<std::uint32_t>(text[position] - L'0');
            if (value > (std::numeric_limits<std::uint16_t>::max)()) {
                return false;
            }
            ++position;
        }
        parts[index] = static_cast<std::uint16_t>(value);

        while (position < text.size() && IsVersionWhitespace(text[position])) {
            ++position;
        }
        if (index + 1 < parts.size()) {
            if (position == text.size()
                || (text[position] != L',' && text[position] != L'.')) {
                return false;
            }
            ++position;
        }
    }

    while (position < text.size() && IsVersionWhitespace(text[position])) {
        ++position;
    }
    if (position != text.size()) {
        return false;
    }

    parsed = WrapperFileVersion{parts[0], parts[1], parts[2], parts[3]};
    return true;
}

} // namespace

ModulePathResult WindowsWrapperFingerprintPlatform::ResolveModulePath(
    const LoadedModule module) noexcept
{
    if (module == 0) {
        return ModulePathResult{ModulePathCode::InvalidModule};
    }

    try {
        std::vector<wchar_t> buffer(kInitialModulePathCapacity);
        while (true) {
            ::SetLastError(ERROR_SUCCESS);
            const DWORD length = ::GetModuleFileNameW(
                reinterpret_cast<HMODULE>(module),
                buffer.data(),
                static_cast<DWORD>(buffer.size()));
            if (length == 0) {
                return ModulePathResult{ModulePathCode::Unavailable};
            }
            if (length < buffer.size()) {
                return ModulePathResult{
                    ModulePathCode::Complete,
                    std::wstring(buffer.data(), length)};
            }
            if (buffer.size() >= kMaximumModulePathCapacity) {
                return ModulePathResult{ModulePathCode::TooLong};
            }

            const std::size_t next_capacity = (std::min)(
                buffer.size() * 2,
                kMaximumModulePathCapacity);
            if (next_capacity <= buffer.size()) {
                return ModulePathResult{ModulePathCode::TooLong};
            }
            buffer.resize(next_capacity);
        }
    } catch (...) {
        return ModulePathResult{ModulePathCode::Unavailable};
    }
}

FileVersionResult WindowsWrapperFingerprintPlatform::ReadFileVersion(
    const std::wstring_view path) noexcept
{
    if (path.empty()) {
        return FileVersionResult{FileVersionCode::Unavailable};
    }

    try {
        const std::wstring api_path = ApiPath(path);
        DWORD unused = 0;
        const DWORD byte_count = ::GetFileVersionInfoSizeW(api_path.c_str(), &unused);
        if (byte_count == 0) {
            return FileVersionResult{FileVersionCode::Unavailable};
        }

        std::vector<std::byte> block(byte_count);
        if (::GetFileVersionInfoW(
                api_path.c_str(),
                0,
                byte_count,
                block.data())
            == FALSE) {
            return FileVersionResult{FileVersionCode::Unavailable};
        }

        void* fixed_value = nullptr;
        UINT fixed_size = 0;
        if (::VerQueryValueW(
                block.data(),
                L"\\",
                &fixed_value,
                &fixed_size)
                == FALSE
            || fixed_value == nullptr
            || fixed_size < sizeof(VS_FIXEDFILEINFO)) {
            return FileVersionResult{FileVersionCode::Malformed};
        }

        const auto* fixed = static_cast<const VS_FIXEDFILEINFO*>(fixed_value);
        if (fixed->dwSignature != VS_FFI_SIGNATURE) {
            return FileVersionResult{FileVersionCode::Malformed};
        }

        void* translations = nullptr;
        UINT translations_size = 0;
        WrapperFileVersion version{};
        bool version_found = false;
        std::wstring original_filename;
        if (::VerQueryValueW(
                block.data(),
                L"\\VarFileInfo\\Translation",
                &translations,
                &translations_size)
                != FALSE
            && translations != nullptr) {
            const auto* bytes = static_cast<const std::byte*>(translations);
            for (std::size_t offset = 0;
                 offset + sizeof(WORD) * 2 <= translations_size;
                 offset += sizeof(WORD) * 2) {
                WORD language = 0;
                WORD code_page = 0;
                std::memcpy(&language, bytes + offset, sizeof(language));
                std::memcpy(
                    &code_page,
                    bytes + offset + sizeof(language),
                    sizeof(code_page));
                const std::wstring version_text = ReadVersionString(
                    block,
                    language,
                    code_page,
                    L"FileVersion");
                WrapperFileVersion translated_version{};
                if (!version_text.empty()
                    && ParseFourPartVersion(version_text, translated_version)) {
                    version = translated_version;
                    version_found = true;
                }
                original_filename = ReadVersionString(
                    block,
                    language,
                    code_page,
                    L"OriginalFilename");
                if (version_found && !original_filename.empty()) {
                    break;
                }
            }
        }

        if (!version_found || original_filename.empty()) {
            constexpr std::array fallback_translations{
                std::pair<WORD, WORD>{
                    static_cast<WORD>(0x0000),
                    static_cast<WORD>(0x04b0)},
                std::pair<WORD, WORD>{
                    static_cast<WORD>(0x0409),
                    static_cast<WORD>(0x04b0)},
                std::pair<WORD, WORD>{
                    static_cast<WORD>(0x0409),
                    static_cast<WORD>(0x04e4)},
            };
            for (const auto [language, code_page] : fallback_translations) {
                if (!version_found) {
                    const std::wstring version_text = ReadVersionString(
                        block,
                        language,
                        code_page,
                        L"FileVersion");
                    WrapperFileVersion translated_version{};
                    if (!version_text.empty()
                        && ParseFourPartVersion(version_text, translated_version)) {
                        version = translated_version;
                        version_found = true;
                    }
                }
                if (original_filename.empty()) {
                    original_filename = ReadVersionString(
                        block,
                        language,
                        code_page,
                        L"OriginalFilename");
                }
                if (version_found && !original_filename.empty()) {
                    break;
                }
            }
        }

        if (!version_found) {
            return FileVersionResult{FileVersionCode::Malformed};
        }
        if (original_filename.empty()) {
            return FileVersionResult{
                FileVersionCode::OriginalFilenameUnavailable,
                version};
        }
        return FileVersionResult{
            FileVersionCode::Complete,
            version,
            std::move(original_filename)};
    } catch (...) {
        return FileVersionResult{FileVersionCode::Unavailable};
    }
}

WrapperEvidence InspectWrapperFile(
    const std::wstring_view path,
    FileVersionPlatform& versions) noexcept
{
    WrapperEvidence evidence;
    try {
        evidence.path.assign(path);
        if (path.empty()) {
            evidence.code = WrapperEvidenceCode::FileOpenFailed;
            return evidence;
        }

        const std::wstring api_path = ApiPath(path);
        const UniqueFile file(::CreateFileW(
            api_path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
            nullptr));
        if (file.get() == INVALID_HANDLE_VALUE) {
            evidence.code = WrapperEvidenceCode::FileOpenFailed;
            return evidence;
        }

        LARGE_INTEGER size{};
        if (::GetFileSizeEx(file.get(), &size) == FALSE || size.QuadPart < 0) {
            evidence.code = WrapperEvidenceCode::FileSizeFailed;
            return evidence;
        }
        evidence.fingerprint.file_size = static_cast<std::uint64_t>(size.QuadPart);

        const ArchitectureResult architecture = ReadArchitecture(
            file.get(),
            evidence.fingerprint.file_size);
        if (architecture.code != WrapperEvidenceCode::Complete) {
            evidence.code = architecture.code;
            return evidence;
        }
        evidence.fingerprint.architecture = architecture.architecture;

        evidence.code = HashFile(
            file.get(),
            evidence.fingerprint.file_size,
            evidence.fingerprint.sha256);
        if (evidence.code != WrapperEvidenceCode::Complete) {
            return evidence;
        }

        FileVersionResult version;
        try {
            version = versions.ReadFileVersion(path);
        } catch (...) {
            evidence.code = WrapperEvidenceCode::FileVersionUnavailable;
            return evidence;
        }
        switch (version.code) {
        case FileVersionCode::Complete:
            if (version.original_filename.empty()) {
                evidence.code = WrapperEvidenceCode::OriginalFilenameUnavailable;
                return evidence;
            }
            evidence.fingerprint.file_version = version.version;
            evidence.fingerprint.original_filename = std::move(version.original_filename);
            evidence.code = WrapperEvidenceCode::Complete;
            return evidence;
        case FileVersionCode::Malformed:
            evidence.code = WrapperEvidenceCode::FileVersionMalformed;
            return evidence;
        case FileVersionCode::OriginalFilenameUnavailable:
            evidence.code = WrapperEvidenceCode::OriginalFilenameUnavailable;
            return evidence;
        case FileVersionCode::Unavailable:
        default:
            evidence.code = WrapperEvidenceCode::FileVersionUnavailable;
            return evidence;
        }
    } catch (...) {
        evidence.code = WrapperEvidenceCode::FileReadFailed;
        return evidence;
    }
}

} // namespace enbcore::enb

#else

namespace enbcore::enb {

ModulePathResult WindowsWrapperFingerprintPlatform::ResolveModulePath(
    LoadedModule) noexcept
{
    return ModulePathResult{ModulePathCode::Unavailable};
}

FileVersionResult WindowsWrapperFingerprintPlatform::ReadFileVersion(
    std::wstring_view) noexcept
{
    return FileVersionResult{FileVersionCode::Unavailable};
}

WrapperEvidence InspectWrapperFile(
    std::wstring_view path,
    FileVersionPlatform&) noexcept
{
    WrapperEvidence evidence{WrapperEvidenceCode::FileOpenFailed};
    try {
        evidence.path.assign(path);
    } catch (...) {
    }
    return evidence;
}

} // namespace enbcore::enb

#endif
