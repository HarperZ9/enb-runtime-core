if(NOT DEFINED LOCK_FILE OR LOCK_FILE STREQUAL "")
    message(FATAL_ERROR "LOCK_FILE is required")
endif()

if(NOT DEFINED SDK_ROOT OR SDK_ROOT STREQUAL "")
    message(FATAL_ERROR "SDK_ROOT is required")
endif()

cmake_path(IS_ABSOLUTE SDK_ROOT sdk_root_is_absolute)
if(NOT sdk_root_is_absolute)
    message(FATAL_ERROR "SDK_ROOT must be an absolute path")
endif()

set(sdk_archive "${SDK_ROOT}/enbseries-sdk-1002.zip")
set(sdk_header "${SDK_ROOT}/enbseries_sdk/enbseries.h")

if(NOT EXISTS "${LOCK_FILE}")
    message(FATAL_ERROR "SDK lock file does not exist")
endif()

if(NOT EXISTS "${sdk_archive}")
    message(FATAL_ERROR "Locked SDK archive does not exist under SDK_ROOT")
endif()

if(NOT EXISTS "${sdk_header}")
    message(FATAL_ERROR "Locked SDK header does not exist under SDK_ROOT")
endif()

file(READ "${LOCK_FILE}" lock_json)
string(JSON lock_schema GET "${lock_json}" schema_version)
string(JSON lock_sdk_version GET "${lock_json}" sdk_version)
string(JSON lock_game_id GET "${lock_json}" game_identifier hex)
string(JSON expected_archive_hash GET "${lock_json}" hashes archive_sha256)
string(JSON expected_header_hash GET "${lock_json}" hashes header_sha256)

if(NOT lock_schema STREQUAL "1")
    message(FATAL_ERROR "Unsupported SDK lock schema")
endif()

if(NOT lock_sdk_version STREQUAL "1002")
    message(FATAL_ERROR "SDK lock version is not 1002")
endif()

if(NOT lock_game_id STREQUAL "0x10000006")
    message(FATAL_ERROR "SDK lock game identifier is not 0x10000006")
endif()

file(SHA256 "${sdk_archive}" actual_archive_hash)
file(SHA256 "${sdk_header}" actual_header_hash)
string(TOUPPER "${actual_archive_hash}" actual_archive_hash)
string(TOUPPER "${actual_header_hash}" actual_header_hash)
string(TOUPPER "${expected_archive_hash}" expected_archive_hash)
string(TOUPPER "${expected_header_hash}" expected_header_hash)

if(NOT actual_archive_hash STREQUAL expected_archive_hash)
    message(FATAL_ERROR "SDK archive SHA-256 does not match the lock")
endif()

if(NOT actual_header_hash STREQUAL expected_header_hash)
    message(FATAL_ERROR "SDK header SHA-256 does not match the lock")
endif()

message(STATUS "Verified SDK 1002 archive and header SHA-256 locks")
