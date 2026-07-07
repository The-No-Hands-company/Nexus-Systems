# Nexus Systems - Utility Module

cmake_minimum_required(VERSION 3.28)

# Collect all source files via glob
file(GLOB_RECURSE UTILITY_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*/*.cpp")

# Exclude all graphics .cpp — headers are fully inline
list(FILTER UTILITY_SOURCES EXCLUDE REGEX ".*/graphics/.*\.cpp$")
list(FILTER UTILITY_SOURCES EXCLUDE REGEX ".*/example_usage\\.cpp$")

add_library(nexus_utility_toolkit STATIC ${UTILITY_SOURCES})
add_library(nexus::utility ALIAS nexus_utility_toolkit)

target_compile_features(nexus_utility_toolkit PUBLIC cxx_std_23)

target_include_directories(nexus_utility_toolkit
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_options(nexus_utility_toolkit PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

target_compile_definitions(nexus_utility_toolkit PUBLIC
    $<$<CONFIG:Debug>:NEXUS_ENABLE_ASSERTS>
    $<$<CONFIG:Debug>:NEXUS_ENABLE_CONTRACTS>
    $<$<CONFIG:Debug>:NEXUS_DEBUG_SMART_PTRS>
    $<$<CONFIG:Debug>:NEXUS_DEBUG_ALLOCATIONS>
    $<$<CONFIG:Debug>:NEXUS_DEBUG_MUTEX_CONTENTION>
    $<$<CONFIG:Debug>:NEXUS_ENABLE_DEBUG_BREAKS>
    NEXUS_NO_STACKTRACE
)

message(STATUS "Nexus Utility Module configured")
message(STATUS "  Sources: ${UTILITY_SOURCES}")
