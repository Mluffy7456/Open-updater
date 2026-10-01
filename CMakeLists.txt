cmake_minimum_required(VERSION 3.20)
project(OpenUpdater VERSION 2.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(OPENUPDATER_BUILD_GUI "Build the optional Qt 6 GUI" OFF)

if(NOT WIN32)
    find_package(CURL REQUIRED)
endif()

find_package(OpenSSL 1.1.1 REQUIRED COMPONENTS Crypto)

add_library(openupdater_core
    src/core/version.cpp
    src/core/manifest.cpp
    src/core/sha256.cpp
    src/core/github.cpp
    src/core/backup.cpp
    src/core/updater.cpp
    src/core/platform.cpp
    src/core/signature.cpp
)

target_include_directories(openupdater_core PUBLIC include)
target_link_libraries(openupdater_core PRIVATE OpenSSL::Crypto)

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

add_executable(openupdater_cli src/cli/main.cpp)
target_link_libraries(openupdater_cli PRIVATE openupdater_core)
set_target_properties(openupdater_cli PROPERTIES OUTPUT_NAME "openupdater-cli")

if(OPENUPDATER_BUILD_GUI)
    find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent Svg)

    if(WIN32)
        add_executable(openupdater WIN32
            src/gui/main.cpp
            src/gui/resources.qrc
        )
    else()
        add_executable(openupdater
            src/gui/main.cpp
            src/gui/resources.qrc
        )
    endif()
    target_link_libraries(openupdater PRIVATE
        openupdater_core
        Qt6::Widgets
        Qt6::Concurrent
        Qt6::Svg
    )

    if(WIN32)
        set(OPENUPDATER_ICON_PATH "${CMAKE_SOURCE_DIR}/assets/openupdater.ico")
        if(EXISTS "${OPENUPDATER_ICON_PATH}")
            configure_file(
                "${CMAKE_SOURCE_DIR}/src/windows/openupdater.rc.in"
                "${CMAKE_CURRENT_BINARY_DIR}/openupdater.rc"
                @ONLY
            )
            target_sources(openupdater PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/openupdater.rc")
        endif()
    endif()
endif()

if(WIN32)
    set(OPENUPDATER_ICON_PATH "${CMAKE_SOURCE_DIR}/assets/openupdater.ico")
    set(OPENUPDATER_OPENSSL_DLL "C:/Program Files/OpenSSL/bin/libcrypto-3-x64.dll")
    if(NOT EXISTS "${OPENUPDATER_OPENSSL_DLL}")
        message(FATAL_ERROR "OpenSSL runtime DLL not found: ${OPENUPDATER_OPENSSL_DLL}")
    endif()

    install(TARGETS openupdater_cli RUNTIME DESTINATION .)
    install(FILES "${OPENUPDATER_OPENSSL_DLL}" DESTINATION .)

    if(OPENUPDATER_BUILD_GUI)
        install(TARGETS openupdater RUNTIME DESTINATION .)
        install(DIRECTORY "${CMAKE_BINARY_DIR}/Release/" DESTINATION .
            FILES_MATCHING
            PATTERN "Qt6*.dll"
            PATTERN "D3Dcompiler_47.dll"
            PATTERN "opengl32sw.dll"
        )
        install(DIRECTORY "${CMAKE_BINARY_DIR}/Release/platforms/" DESTINATION platforms OPTIONAL)
        install(DIRECTORY "${CMAKE_BINARY_DIR}/Release/styles/" DESTINATION styles OPTIONAL)
    endif()

    set(CPACK_PACKAGE_NAME "OpenUpdater")
    set(CPACK_PACKAGE_VENDOR "Mluffy7456")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Open-source cross-platform application updater")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "OpenUpdater")
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_PACKAGE_FILE_NAME "OpenUpdater-${PROJECT_VERSION}-windows-x64")
    set(CPACK_NSIS_DISPLAY_NAME "OpenUpdater ${PROJECT_VERSION}")
    set(CPACK_NSIS_PACKAGE_NAME "OpenUpdater")
    set(CPACK_NSIS_CONTACT "https://github.com/Mluffy7456/Open-updater")
    set(CPACK_NSIS_HELP_LINK "https://github.com/Mluffy7456/Open-updater")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/Mluffy7456/Open-updater")
    if(EXISTS "${OPENUPDATER_ICON_PATH}")
        set(CPACK_NSIS_MUI_ICON "${OPENUPDATER_ICON_PATH}")
        set(CPACK_NSIS_MUI_UNIICON "${OPENUPDATER_ICON_PATH}")
    endif()
    if(OPENUPDATER_BUILD_GUI)
        set(CPACK_NSIS_CREATE_ICONS_EXTRA
            "CreateShortCut '$DESKTOP\\\\OpenUpdater.lnk' '$INSTDIR\\\\openupdater.exe'"
            "CreateShortCut '$SMPROGRAMS\\\\OpenUpdater.lnk' '$INSTDIR\\\\openupdater.exe'"
        )
    endif()
    include(CPack)
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

    add_executable(openupdater_signature_tests tests/signature_tests.cpp)
    target_link_libraries(openupdater_signature_tests PRIVATE openupdater_core)
    add_test(NAME openupdater_signature_tests COMMAND openupdater_signature_tests)

    add_executable(openupdater_manager_tests tests/manager_tests.cpp)
    target_link_libraries(openupdater_manager_tests PRIVATE openupdater_core)
    add_test(NAME openupdater_manager_tests COMMAND openupdater_manager_tests)
endif()
