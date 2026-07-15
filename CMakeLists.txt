cmake_minimum_required(VERSION 3.30)

project(enb-runtime-core VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

set(
    ENBCORE_ENB_SDK_ROOT
    ""
    CACHE PATH
    "Absolute external cache root containing the locked SDK archive and extracted header"
)

add_library(enb_runtime_core STATIC
    src/enb/LoadedHostResolver.cpp
    src/enb/SdkContract.cpp
    src/platform/windows/WindowsLoadedModulePlatform.cpp
    src/runtime/LifecycleGate.cpp
)

target_compile_features(enb_runtime_core PUBLIC cxx_std_23)
target_include_directories(enb_runtime_core PUBLIC include)

if(WIN32)
    target_link_libraries(enb_runtime_core PRIVATE Psapi)
endif()

if(MSVC)
    target_compile_options(enb_runtime_core PRIVATE /W4 /WX /permissive-)
endif()

if(BUILD_TESTING)
    if(ENBCORE_ENB_SDK_ROOT STREQUAL "")
        message(FATAL_ERROR "ENBCORE_ENB_SDK_ROOT is required when BUILD_TESTING=ON")
    endif()

    cmake_path(IS_ABSOLUTE ENBCORE_ENB_SDK_ROOT enb_sdk_root_is_absolute)
    if(NOT enb_sdk_root_is_absolute)
        message(FATAL_ERROR "ENBCORE_ENB_SDK_ROOT must be an absolute path")
    endif()

    set(enb_sdk_archive "${ENBCORE_ENB_SDK_ROOT}/enbseries-sdk-1002.zip")
    set(enb_sdk_header "${ENBCORE_ENB_SDK_ROOT}/enbseries_sdk/enbseries.h")
    set(enb_sdk_lock "${CMAKE_CURRENT_SOURCE_DIR}/config/enb-sdk-1002.lock.json")
    set(enb_sdk_hash_script "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyEnbSdk1002.cmake")
    set(enb_sdk_hash_stamp "${CMAKE_CURRENT_BINARY_DIR}/enb-sdk-1002.hash-verified")

    add_custom_command(
        OUTPUT "${enb_sdk_hash_stamp}"
        COMMAND
            "${CMAKE_COMMAND}"
            "-DLOCK_FILE=${enb_sdk_lock}"
            "-DSDK_ROOT=${ENBCORE_ENB_SDK_ROOT}"
            -P "${enb_sdk_hash_script}"
        COMMAND "${CMAKE_COMMAND}" -E touch "${enb_sdk_hash_stamp}"
        DEPENDS
            "${enb_sdk_hash_script}"
            "${enb_sdk_lock}"
            "${enb_sdk_archive}"
            "${enb_sdk_header}"
        COMMENT "Verifying locked SDK 1002 archive and header"
        VERBATIM
    )

    add_custom_target(
        enb_sdk_1002_hash_gate
        DEPENDS "${enb_sdk_hash_stamp}"
    )

    add_executable(enb_runtime_core_tests
        tests/LifecycleGateTests.cpp
    )

    target_compile_features(enb_runtime_core_tests PRIVATE cxx_std_23)
    target_link_libraries(enb_runtime_core_tests PRIVATE enb_runtime_core)

    if(MSVC)
        target_compile_options(enb_runtime_core_tests PRIVATE /W4 /WX /permissive-)
    endif()

    add_test(NAME lifecycle_gate COMMAND enb_runtime_core_tests)

    add_executable(enb_sdk_contract_tests
        tests/SdkContractTests.cpp
    )

    target_compile_features(enb_sdk_contract_tests PRIVATE cxx_std_23)
    target_link_libraries(enb_sdk_contract_tests PRIVATE enb_runtime_core)

    if(MSVC)
        target_compile_options(enb_sdk_contract_tests PRIVATE /W4 /WX /permissive-)
    endif()

    add_test(NAME enb_sdk_contract COMMAND enb_sdk_contract_tests)

    add_executable(enb_loaded_host_resolver_tests
        tests/LoadedHostResolverTests.cpp
    )

    target_compile_features(enb_loaded_host_resolver_tests PRIVATE cxx_std_23)
    target_link_libraries(enb_loaded_host_resolver_tests PRIVATE enb_runtime_core)

    if(MSVC)
        target_compile_options(enb_loaded_host_resolver_tests PRIVATE /W4 /WX /permissive-)
    endif()

    add_test(NAME enb_loaded_host_resolver COMMAND enb_loaded_host_resolver_tests)

    add_executable(enb_sdk_1002_abi_probe
        tests/abi/EnbSdk1002AbiProbe.cpp
    )

    target_compile_features(enb_sdk_1002_abi_probe PRIVATE cxx_std_23)
    target_link_libraries(enb_sdk_1002_abi_probe PRIVATE enb_runtime_core)
    target_include_directories(
        enb_sdk_1002_abi_probe
        SYSTEM PRIVATE
        "${ENBCORE_ENB_SDK_ROOT}/enbseries_sdk"
    )
    add_dependencies(enb_sdk_1002_abi_probe enb_sdk_1002_hash_gate)

    if(MSVC)
        target_compile_options(enb_sdk_1002_abi_probe PRIVATE /W4 /WX /permissive-)
    endif()

    add_test(
        NAME enb_sdk_1002_hashes
        COMMAND
            "${CMAKE_COMMAND}"
            "-DLOCK_FILE=${enb_sdk_lock}"
            "-DSDK_ROOT=${ENBCORE_ENB_SDK_ROOT}"
            -P "${enb_sdk_hash_script}"
    )
    add_test(NAME enb_sdk_1002_abi_probe COMMAND enb_sdk_1002_abi_probe)
    set_tests_properties(
        enb_sdk_1002_abi_probe
        PROPERTIES DEPENDS enb_sdk_1002_hashes
    )
endif()
