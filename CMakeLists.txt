cmake_minimum_required(VERSION 3.30)

project(enb-runtime-core VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

add_library(enb_runtime_core STATIC
    src/runtime/LifecycleGate.cpp
)

target_compile_features(enb_runtime_core PUBLIC cxx_std_23)
target_include_directories(enb_runtime_core PUBLIC include)

if(MSVC)
    target_compile_options(enb_runtime_core PRIVATE /W4 /WX /permissive-)
endif()

if(BUILD_TESTING)
    add_executable(enb_runtime_core_tests
        tests/LifecycleGateTests.cpp
    )

    target_compile_features(enb_runtime_core_tests PRIVATE cxx_std_23)
    target_link_libraries(enb_runtime_core_tests PRIVATE enb_runtime_core)

    if(MSVC)
        target_compile_options(enb_runtime_core_tests PRIVATE /W4 /WX /permissive-)
    endif()

    add_test(NAME lifecycle_gate COMMAND enb_runtime_core_tests)
endif()
