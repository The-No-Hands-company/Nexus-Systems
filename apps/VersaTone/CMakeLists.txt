# DAWG Audio Workstation - Professional Project Structure
# Revolutionary audio engine with standalone editor and examples
# Engine is completely GUI-independent and modular

cmake_minimum_required(VERSION 3.20)
project(DAWG 
    VERSION 1.0.0
    DESCRIPTION "DAWG Audio Workstation - Revolutionary Multi-Tier Voice System"
    LANGUAGES CXX
)

# Global project settings
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Project build options
option(DAWG_BUILD_ENGINE "Build the audio engine" ON)
option(DAWG_BUILD_EDITOR "Build the GUI editor (requires Qt6)" ON)
option(DAWG_BUILD_EXAMPLES "Build example applications" ON)
option(DAWG_BUILD_TESTS "Build unit tests" OFF)
option(DAWG_BUILD_TOOLS "Build development tools" ON)

# Platform-specific compiler settings
if(WIN32 AND MINGW)
    set(CMAKE_C_COMPILER "D:/msys64/mingw64/bin/gcc.exe")
    set(CMAKE_CXX_COMPILER "D:/msys64/mingw64/bin/g++.exe")
    set(CMAKE_MAKE_PROGRAM "D:/msys64/mingw64/bin/ninja.exe")
endif()

# Set build output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

message(STATUS "=== DAWG Audio Workstation Build Configuration ===")
message(STATUS "Engine: ${DAWG_BUILD_ENGINE}")
message(STATUS "Editor: ${DAWG_BUILD_EDITOR}")
message(STATUS "Examples: ${DAWG_BUILD_EXAMPLES}")
message(STATUS "Tests: ${DAWG_BUILD_TESTS}")
message(STATUS "Tools: ${DAWG_BUILD_TOOLS}")
message(STATUS "=================================================")

# Always build the engine first (core dependency)
if(DAWG_BUILD_ENGINE)
    message(STATUS "Building DAWG Audio Engine...")
    add_subdirectory(engine)
endif()

# Build editor (depends on engine + Qt6)
if(DAWG_BUILD_EDITOR AND DAWG_BUILD_ENGINE)
    message(STATUS "Building DAWG Editor...")
    find_package(Qt6 COMPONENTS Core Widgets Multimedia QUIET)
    if(Qt6_FOUND)
        add_subdirectory(editor)
    else()
        message(WARNING "Qt6 not found, editor will not be built")
        set(DAWG_BUILD_EDITOR OFF)
    endif()
endif()

# Build examples (showcase engine independence)
if(DAWG_BUILD_EXAMPLES AND DAWG_BUILD_ENGINE)
    message(STATUS "Building DAWG Examples...")
    add_subdirectory(examples)
endif()

# Build tests
if(DAWG_BUILD_TESTS AND DAWG_BUILD_ENGINE)
    message(STATUS "Building DAWG Tests...")
    add_subdirectory(tests)
endif()

# Build tools
if(DAWG_BUILD_TOOLS AND DAWG_BUILD_ENGINE)
    message(STATUS "Building DAWG Tools...")
    add_subdirectory(tools)
endif()

# Project summary
message(STATUS "")
message(STATUS "=== DAWG Build Summary ===")
message(STATUS "Revolutionary 292,864+ Voice Audio Engine: ${DAWG_BUILD_ENGINE}")
if(DAWG_BUILD_EDITOR)
    message(STATUS "Professional DAW Editor: ENABLED")
else()
    message(STATUS "Professional DAW Editor: DISABLED (Qt6 not found)")
endif()
message(STATUS "Engine Examples: ${DAWG_BUILD_EXAMPLES}")
message(STATUS "Unit Tests: ${DAWG_BUILD_TESTS}")
message(STATUS "Development Tools: ${DAWG_BUILD_TOOLS}")
message(STATUS "===========================")

# Install configuration
if(DAWG_BUILD_ENGINE)
    # Engine can be distributed standalone
    install(DIRECTORY engine/include/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
    )
endif()

# Package configuration
set(CPACK_PACKAGE_NAME "DAWG")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Revolutionary Audio Engine with 292,864+ Voice Capacity")
set(CPACK_PACKAGE_VENDOR "The No-hands Company")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

include(CPack)
