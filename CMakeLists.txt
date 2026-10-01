cmake_minimum_required(VERSION 3.20)
project(OpenUpdater VERSION 0.2.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    find_package(CURL REQUIRED)
endif()

add_library(openupdater_core
    src/core/version.cpp
    src/core/manifest.cpp
    src/core/sha256.cpp
    src/core/updater.cpp
)

target_include_directories(openupdater_core PUBLIC include)

if(WIN32)
    target_link_libraries(openupdater_core PRIVATE winhttp)
else()
    target_link_libraries(openupdater_core PRIVATE CURL::libcurl)
endif()

if(MSVC)
    target_compile_options(openupdater_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(openupdater_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(openupdater src/cli/main.cpp)
target_link_libraries(openupdater PRIVATE openupdater_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(openupdater_tests tests/core_tests.cpp)
    target_link_libraries(openupdater_tests PRIVATE openupdater_core)
    add_test(NAME openupdater_tests COMMAND openupdater_tests)

    add_executable(openupdater_sha256_tests tests/sha256_tests.cpp)
    target_link_libraries(openupdater_sha256_tests PRIVATE openupdater_core)
    add_test(NAME openupdater_sha256_tests COMMAND openupdater_sha256_tests)
endif()
