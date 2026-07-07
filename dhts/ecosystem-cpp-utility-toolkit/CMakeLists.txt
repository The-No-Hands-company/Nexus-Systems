cmake_minimum_required(VERSION 3.28)

# ── Project ──────────────────────────────────────────────────────────────────
project(
    nexus_utility_toolkit
    VERSION 1.0.0
    DESCRIPTION "AAA-grade C++23 utility and diagnostic toolkit"
    HOMEPAGE_URL "https://github.com/Nexus-Systems/ecosystem-cpp-utility-toolkit"
    LANGUAGES CXX
)

# ── C++ Standard ─────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Build Options ────────────────────────────────────────────────────────────
option(NEXUS_BUILD_TESTS "Build verification tests" OFF)
option(NEXUS_BUILD_EXAMPLES "Build example programs" OFF)
option(NEXUS_BUILD_BENCHMARKS "Build benchmark suite" OFF)
option(NEXUS_ENABLE_SANITIZERS "Enable Address/Undefined sanitizers" OFF)
option(NEXUS_ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)
option(NEXUS_ENABLE_LTO "Enable link-time optimization" OFF)
option(NEXUS_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(NEXUS_ENABLE_CLANG_TIDY "Enable clang-tidy static analysis" OFF)
option(NEXUS_ENABLE_IWYU "Enable include-what-you-use" OFF)

# ── Interprocedural Optimization ─────────────────────────────────────────────
if(NEXUS_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_output)
    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
        message(STATUS "LTO enabled")
    else()
        message(WARNING "LTO not supported: ${ipo_output}")
    endif()
endif()

# ── Sanitizer Flags ──────────────────────────────────────────────────────────
if(NEXUS_ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address -fsanitize=undefined)
    message(STATUS "Sanitizers enabled: address, undefined")
endif()

# ── Coverage Flags ───────────────────────────────────────────────────────────
if(NEXUS_ENABLE_COVERAGE)
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
    message(STATUS "Coverage instrumentation enabled")
endif()

# ── Warnings as Errors ───────────────────────────────────────────────────────
if(NEXUS_WARNINGS_AS_ERRORS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-Werror)
    elseif(MSVC)
        add_compile_options(/WX)
    endif()
endif()

# ── clang-tidy ───────────────────────────────────────────────────────────────
if(NEXUS_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-config-file=${PROJECT_SOURCE_DIR}/.clang-tidy")
        message(STATUS "clang-tidy enabled: ${CLANG_TIDY_EXE}")
    else()
        message(WARNING "clang-tidy not found")
    endif()
endif()

# ── include-what-you-use ─────────────────────────────────────────────────────
if(NEXUS_ENABLE_IWYU)
    find_program(IWYU_EXE NAMES include-what-you-use iwyu)
    if(IWYU_EXE)
        set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE ${IWYU_EXE})
        message(STATUS "IWYU enabled: ${IWYU_EXE}")
    else()
        message(WARNING "IWYU not found")
    endif()
endif()

# ── Dependencies ─────────────────────────────────────────────────────────────
include(FetchContent)

# GoogleTest for unit testing
if(NEXUS_BUILD_TESTS)
    include(GoogleTest)
    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.15.2
            EXCLUDE_FROM_ALL
        )
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()
    enable_testing()
endif()

# ── Library Target ───────────────────────────────────────────────────────────
add_subdirectory(src/utility)

# ── Tests ────────────────────────────────────────────────────────────────────
if(NEXUS_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# ── Examples ─────────────────────────────────────────────────────────────────
if(NEXUS_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ── Installation ─────────────────────────────────────────────────────────────
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(
    TARGETS nexus_utility_toolkit
    EXPORT nexus_utility_toolkit-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

# ── Package Config ──────────────────────────────────────────────────────────
set(CMAKECONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/nexus_utility_toolkit")

configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/nexus_utility_toolkit-config.cmake.in"
    "${PROJECT_BINARY_DIR}/nexus_utility_toolkit-config.cmake"
    INSTALL_DESTINATION ${CMAKECONFIG_INSTALL_DIR}
)

write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/nexus_utility_toolkit-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        "${PROJECT_BINARY_DIR}/nexus_utility_toolkit-config.cmake"
        "${PROJECT_BINARY_DIR}/nexus_utility_toolkit-config-version.cmake"
    DESTINATION ${CMAKECONFIG_INSTALL_DIR}
)

install(
    EXPORT nexus_utility_toolkit-targets
    FILE nexus_utility_toolkit-targets.cmake
    NAMESPACE nexus::
    DESTINATION ${CMAKECONFIG_INSTALL_DIR}
)

# ── CPack ────────────────────────────────────────────────────────────────────
set(CPACK_PACKAGE_NAME "nexus-utility-toolkit")
set(CPACK_PACKAGE_VENDOR "Nexus Systems")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "AAA-grade C++23 utility toolkit")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
include(CPack)

# ── Print Summary ────────────────────────────────────────────────────────────
message(STATUS "══════════════════════════════════════════════")
message(STATUS "  Nexus Systems Utility Toolkit ${PROJECT_VERSION}")
message(STATUS "  Build type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:   C++23")
message(STATUS "  Tests:          ${NEXUS_BUILD_TESTS}")
message(STATUS "  Examples:       ${NEXUS_BUILD_EXAMPLES}")
message(STATUS "  Benchmarks:     ${NEXUS_BUILD_BENCHMARKS}")
message(STATUS "  Sanitizers:     ${NEXUS_ENABLE_SANITIZERS}")
message(STATUS "  Coverage:       ${NEXUS_ENABLE_COVERAGE}")
message(STATUS "  LTO:            ${NEXUS_ENABLE_LTO}")
message(STATUS "  Werror:         ${NEXUS_WARNINGS_AS_ERRORS}")
message(STATUS "══════════════════════════════════════════════")
