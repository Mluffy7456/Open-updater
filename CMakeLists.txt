cmake_minimum_required(VERSION 3.20)
project(OpenUpdater VERSION 1.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OPENUPDATER_BUILD_GUI "Build the optional Qt 6 GUI" OFF)

if(NOT WIN32)
    find_package(CURL REQUIRED)
endif()

add_library(openupdater_core
    src/core/version.cpp
    src/core/manifest.cpp
    src/core/sha256.cpp
    src/core/github.cpp
    src/core/backup.cpp
    src/core/updater.cpp
    src/core/platform.cpp
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

if(OPENUPDATER_BUILD_GUI)
    find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)

    add_executable(openupdater_gui src/gui/main.cpp)
    target_link_libraries(openupdater_gui PRIVATE
        openupdater_core
        Qt6::Widgets
        Qt6::Concurrent
    )
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(openupdater_tests tests/core_tests.cpp)
    target_link_libraries(openupdater_tests PRIVATE openupdater_core)
    add_test(NAME openupdater_tests COMMAND openupdater_tests)

    add_executable(openupdater_sha256_tests tests/sha256_tests.cpp)
    target_link_libraries(openupdater_sha256_tests PRIVATE openupdater_core)
    add_test(NAME openupdater_sha256_tests COMMAND openupdater_sha256_tests)

    add_executable(openupdater_backup_tests tests/backup_tests.cpp)
    target_link_libraries(openupdater_backup_tests PRIVATE openupdater_core)
    add_test(NAME openupdater_backup_tests COMMAND openupdater_backup_tests)

    add_executable(openupdater_update_tests tests/update_tests.cpp)
    target_link_libraries(openupdater_update_tests PRIVATE openupdater_core)
    add_test(NAME openupdater_update_tests COMMAND openupdater_update_tests)
endif()
