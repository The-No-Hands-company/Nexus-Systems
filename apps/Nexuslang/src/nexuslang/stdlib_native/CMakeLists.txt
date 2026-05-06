cmake_minimum_required(VERSION 3.15)

project(NLPLRuntime
    VERSION 1.0.0
    DESCRIPTION "NLPL native runtime library -- C implementations of core stdlib functions"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------

set(NLPL_SOURCES
    src/nxl_runtime.c
    src/nxl_string.c
    src/nxl_math.c
    src/nxl_collections.c
    src/nxl_io.c
)

# ---------------------------------------------------------------------------
# Static library  (libNLPL.a)
# ---------------------------------------------------------------------------

add_library(NLPL STATIC ${NLPL_SOURCES})

target_include_directories(NLPL
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# All platforms require libm for math functions.
target_link_libraries(NLPL PUBLIC m)

# Enable strict warnings in Debug builds.
target_compile_options(NLPL PRIVATE
    $<$<CONFIG:Debug>:-Wall -Wextra -Wno-unused-parameter -fsanitize=address,undefined>
    $<$<CONFIG:Release>:-O2>
)
target_link_options(NLPL PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

# ---------------------------------------------------------------------------
# Shared library  (libNLPL.so / libNLPL.dylib)  -- optional
# ---------------------------------------------------------------------------

option(NLPL_BUILD_SHARED "Build libNLPL as a shared library in addition to the static one" OFF)

if(NLPL_BUILD_SHARED)
    add_library(NLPL_shared SHARED ${NLPL_SOURCES})
    set_target_properties(NLPL_shared PROPERTIES OUTPUT_NAME NLPL)
    target_include_directories(NLPL_shared
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(NLPL_shared PUBLIC m)
    target_compile_options(NLPL_shared PRIVATE -fPIC)
endif()

# ---------------------------------------------------------------------------
# Simple smoke-test executable (only built when testing)
# ---------------------------------------------------------------------------

option(NLPL_BUILD_TESTS "Build the stdlib_native smoke-test program" OFF)

if(NLPL_BUILD_TESTS)
    add_executable(nlpl_stdlib_test tests/smoke_test.c)
    target_link_libraries(nlpl_stdlib_test PRIVATE NLPL)
    target_include_directories(nlpl_stdlib_test PRIVATE include)

    enable_testing()
    add_test(NAME smoke_test COMMAND nlpl_stdlib_test)
endif()

# ---------------------------------------------------------------------------
# Install rules
# ---------------------------------------------------------------------------

install(TARGETS NLPL
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES include/nxl_runtime.h
    DESTINATION include
)

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------

message(STATUS "NLPL Runtime v${PROJECT_VERSION}")
message(STATUS "  Sources    : ${NLPL_SOURCES}")
message(STATUS "  Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install    : ${CMAKE_INSTALL_PREFIX}")
