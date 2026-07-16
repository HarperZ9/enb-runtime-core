cmake_minimum_required(VERSION 3.30)

project(enb-runtime-core VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

set(
    ENBCORE_ENB_SDK_ROOT
    ""
    CACHE PATH
    "Absolute external cache root containing the locked SDK archive and extracted header"
)

set(
    ENBCORE_ENB_WRAPPER_PATH
    ""
    CACHE FILEPATH
    "Optional absolute read-only path to an ENB wrapper build for fingerprint integration testing"
)

add_library(enb_runtime_core STATIC
    src/enb/LoadedHostResolver.cpp
    src/enb/SdkContract.cpp
    src/enb/WrapperFingerprint.cpp
    src/platform/windows/WindowsLoadedModulePlatform.cpp
    src/platform/windows/WindowsWrapperFingerprintPlatform.cpp
    src/runtime/CallbackSession.cpp
    src/runtime/CameraPublication.cpp
    src/runtime/FramePulse.cpp
    src/runtime/LifecycleGate.cpp
    src/skyrim/AddressLibraryDatabase.cpp
    src/skyrim/CanonicalDescriptors.cpp
    src/skyrim/EngineBridge.cpp
    src/skyrim/EngineProperties.cpp
    src/skyrim/ObjectPropertyJournal.cpp
    src/skyrim/PatchJournal.cpp
)

target_compile_features(enb_runtime_core PUBLIC cxx_std_23)
target_include_directories(enb_runtime_core PUBLIC include)

if(WIN32)
    target_link_libraries(enb_runtime_core PRIVATE Bcrypt Psapi Version)
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

    add_executable(enb_callback_session_tests
        tests/CallbackSessionTests.cpp
    )

    target_compile_features(enb_callback_session_tests PRIVATE cxx_std_23)
    target_link_libraries(enb_callback_session_tests PRIVATE enb_runtime_core)

    if(MSVC)
        target_compile_options(enb_callback_session_tests PRIVATE /W4 /WX /permissive-)
    endif()

    add_test(NAME enb_callback_session COMMAND enb_callback_session_tests)

    add_executable(enb_wrapper_fingerprint_tests
        tests/WrapperFingerprintTests.cpp
    )

    target_compile_features(enb_wrapper_fingerprint_tests PRIVATE cxx_std_23)
    target_link_libraries(enb_wrapper_fingerprint_tests PRIVATE enb_runtime_core)

    if(MSVC)
        target_compile_options(enb_wrapper_fingerprint_tests PRIVATE /W4 /WX /permissive-)
    endif()

    add_test(NAME enb_wrapper_fingerprint COMMAND enb_wrapper_fingerprint_tests)

    add_executable(enb_skyrim_engine_bridge_tests
        tests/SkyrimEngineBridgeTests.cpp
    )

    target_compile_features(enb_skyrim_engine_bridge_tests PRIVATE cxx_std_23)
    target_link_libraries(enb_skyrim_engine_bridge_tests PRIVATE enb_runtime_core)

    if(MSVC)
        target_compile_options(
            enb_skyrim_engine_bridge_tests
            PRIVATE /W4 /WX /permissive-
        )
    endif()

    add_test(NAME skyrim_engine_bridge COMMAND enb_skyrim_engine_bridge_tests)

    add_executable(enb_address_library_database_tests
        tests/AddressLibraryDatabaseTests.cpp
    )

    target_compile_features(enb_address_library_database_tests PRIVATE cxx_std_23)
    target_link_libraries(enb_address_library_database_tests PRIVATE enb_runtime_core)

    if(MSVC)
        target_compile_options(
            enb_address_library_database_tests
            PRIVATE /W4 /WX /permissive-
        )
    endif()

    add_test(NAME address_library_database COMMAND enb_address_library_database_tests)

    add_executable(enb_engine_property_tests
        tests/EnginePropertyTests.cpp
    )

    target_compile_features(enb_engine_property_tests PRIVATE cxx_std_23)
    target_link_libraries(enb_engine_property_tests PRIVATE enb_runtime_core)

    if(MSVC)
        target_compile_options(
            enb_engine_property_tests
            PRIVATE /W4 /WX /permissive-
        )
    endif()

    add_test(NAME engine_properties COMMAND enb_engine_property_tests)

    add_executable(enb_frame_pulse_tests
        tests/FramePulseTests.cpp
    )

    target_compile_features(enb_frame_pulse_tests PRIVATE cxx_std_23)
    target_link_libraries(enb_frame_pulse_tests PRIVATE enb_runtime_core)

    if(MSVC)
        target_compile_options(enb_frame_pulse_tests PRIVATE /W4 /WX /permissive-)
    endif()

    add_test(NAME frame_pulse COMMAND enb_frame_pulse_tests)

    add_executable(enb_camera_publication_tests
        tests/CameraPublicationTests.cpp
    )

    target_compile_features(enb_camera_publication_tests PRIVATE cxx_std_23)
    target_link_libraries(enb_camera_publication_tests PRIVATE enb_runtime_core)

    if(MSVC)
        target_compile_options(enb_camera_publication_tests PRIVATE /W4 /WX /permissive-)
    endif()

    add_test(NAME camera_publication COMMAND enb_camera_publication_tests)

    add_executable(enb_canonical_descriptors_tests
        tests/CanonicalDescriptorsTests.cpp
    )

    target_compile_features(enb_canonical_descriptors_tests PRIVATE cxx_std_23)
    target_link_libraries(enb_canonical_descriptors_tests PRIVATE enb_runtime_core)

    if(MSVC)
        target_compile_options(enb_canonical_descriptors_tests PRIVATE /W4 /WX /permissive-)
    endif()

    add_test(NAME canonical_descriptors COMMAND enb_canonical_descriptors_tests)

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

    if(NOT ENBCORE_ENB_WRAPPER_PATH STREQUAL "")
        cmake_path(IS_ABSOLUTE ENBCORE_ENB_WRAPPER_PATH enb_wrapper_path_is_absolute)
        if(NOT enb_wrapper_path_is_absolute)
            message(FATAL_ERROR "ENBCORE_ENB_WRAPPER_PATH must be an absolute path")
        endif()

        add_executable(enb_wrapper_fingerprint_integration_tests
            tests/integration/EnbWrapperFingerprintIntegration.cpp
        )
        target_compile_features(
            enb_wrapper_fingerprint_integration_tests
            PRIVATE cxx_std_23
        )
        target_link_libraries(
            enb_wrapper_fingerprint_integration_tests
            PRIVATE enb_runtime_core
        )

        if(MSVC)
            target_compile_options(
                enb_wrapper_fingerprint_integration_tests
                PRIVATE /W4 /WX /permissive-
            )
        endif()

        add_test(
            NAME enb_wrapper_fingerprint_integration
            COMMAND
                enb_wrapper_fingerprint_integration_tests
                "${ENBCORE_ENB_WRAPPER_PATH}"
        )
    endif()
endif()
