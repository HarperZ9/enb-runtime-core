#include <enbcore/enb/WrapperFingerprint.hpp>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

void print_digest(const enbcore::enb::Sha256Digest& digest)
{
    for (const std::uint8_t byte : digest) {
        std::cout << std::uppercase << std::hex << std::setw(2)
                  << std::setfill('0') << static_cast<unsigned int>(byte);
    }
    std::cout << std::dec;
}

} // namespace

int wmain(const int argc, wchar_t** argv)
{
    using namespace enbcore::enb;

    if (argc != 2 || argv == nullptr || argv[1] == nullptr || argv[1][0] == L'\0') {
        std::cerr << "expected one external wrapper path\n";
        return 2;
    }

    WindowsWrapperFingerprintPlatform platform;
    const WrapperEvidence evidence = InspectWrapperFile(argv[1], platform);
    const WrapperCompatibilityResult compatibility = ClassifySupportedEnbWrapper(
        evidence,
        WrapperPolicy::Strict);

    std::cout << "evidence=" << static_cast<unsigned int>(evidence.code)
              << " build=" << static_cast<unsigned int>(compatibility.build)
              << " admission=" << static_cast<unsigned int>(compatibility.admission)
              << " mismatches=" << static_cast<unsigned int>(compatibility.mismatches)
              << " size=" << evidence.fingerprint.file_size
              << " version=" << evidence.fingerprint.file_version.major << '.'
              << evidence.fingerprint.file_version.minor << '.'
              << evidence.fingerprint.file_version.patch << '.'
              << evidence.fingerprint.file_version.build
              << " machine=" << static_cast<unsigned int>(evidence.fingerprint.architecture)
              << " original_filename=";
    std::wcout << evidence.fingerprint.original_filename;
    std::cout << " sha256=";
    print_digest(evidence.fingerprint.sha256);
    std::cout << '\n';

    if (!evidence.complete()
        || compatibility.build != WrapperBuildCode::KnownBuild
        || compatibility.admission != WrapperAdmissionCode::AdmittedKnownBuild
        || !compatibility.admitted()) {
        return 1;
    }
    return 0;
}
